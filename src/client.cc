#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <shlwapi.h>

#include "client.h"

#include "matoya.h"
#include "parsec-dso.h"

#include "mtymap.h"

#define PARSEC_APP_CLIPBOARD_MSG 7

struct context {
  bool done;
  MTY_App* app;
  MTY_Audio* audio;
  ParsecDSO* parsec;
};

static void logCallback(ParsecLogLevel level, const char* msg, void* opaque) {
  opaque;

  printf("[%s] %s\n", level == LOG_DEBUG ? "D" : "I", msg);
}

static void audio_func(const int16_t* pcm, uint32_t frames, void* opaque) {
  struct context* ctx = (context*)opaque;

  if (ctx->audio)
    MTY_AudioQueue(ctx->audio, pcm, frames);
}

static void userData(struct context* ctx, uint32_t id, uint32_t bufferKey) {
  char* msg = (char*)ParsecGetBuffer(ctx->parsec, bufferKey);

  if (msg && id == PARSEC_APP_CLIPBOARD_MSG)
    MTY_AppSetClipboard(ctx->app, msg);

  ParsecFree(ctx->parsec, msg);
}

static void cursor_func(struct context* ctx,
                        const ParsecCursor* cursor,
                        uint32_t bufferKey) {
  if (cursor->imageUpdate) {
    uint8_t* image = (uint8_t*)ParsecGetBuffer(ctx->parsec, bufferKey);

    if (image) {
      MTY_AppSetPNGCursor(ctx->app, image, cursor->size, cursor->hotX, cursor->hotY);
      ParsecFree(ctx->parsec, image);
    }
  }

  bool relative = cursor->relative || cursor->hidden;
  if (MTY_AppGetRelativeMouse(ctx->app) && !relative) {
    MTY_AppSetRelativeMouse(ctx->app, false);
    MTY_WindowWarpCursor(ctx->app, 0, cursor->positionX, cursor->positionY);
  } else if (!MTY_AppGetRelativeMouse(ctx->app) && relative) {
    MTY_AppSetRelativeMouse(ctx->app, true);
    MTY_WindowWarpCursor(ctx->app, 0, cursor->positionX, cursor->positionY);
  }
}

static void* audioThread(void* opaque) {
  struct context* ctx = (context*)opaque;

  while (!ctx->done)
    ParsecClientPollAudio(ctx->parsec, audio_func, 100, ctx);

  return NULL;
}

static void* renderThread(void* opaque) {
  struct context* ctx = (context*)opaque;

  MTY_WindowMakeCurrent(ctx->app, 0, true);

  while (!ctx->done) {
    uint32_t w = 0, h = 0;
    MTY_WindowGetSize(ctx->app, 0, &w, &h);

    float scale = MTY_WindowGetScreenScale(ctx->app, 0);

    ParsecClientSetDimensions(ctx->parsec, DEFAULT_STREAM, w, h, scale);
    ParsecClientGLRenderFrame(ctx->parsec, DEFAULT_STREAM, NULL, NULL, 100);

    MTY_WindowPresent(ctx->app, 0, 1);
  }

  ParsecClientGLDestroy(ctx->parsec, DEFAULT_STREAM);
  MTY_WindowSetGFX(ctx->app, 0, MTY_GFX_NONE, false);

  return NULL;
}

static void event_func(const MTY_Event* evt, void* opaque) {
  struct context* ctx = (context*)opaque;

  if (evt->type == MTY_EVENT_CLIPBOARD) {
    char* text = MTY_AppGetClipboard(ctx->app);

    if (text)
      ParsecClientSendUserData(ctx->parsec, PARSEC_APP_CLIPBOARD_MSG, text);

    MTY_Free(text);

  } else {
    ParsecMessage msg = {0};
    MTY_EVENT_TO_PARSEC(evt, &msg);

    if (msg.type != 0)
      ParsecClientSendMessage(ctx->parsec, &msg);
  }

  if (evt->type == MTY_EVENT_CLOSE)
    ctx->done = true;
}

static bool app_func(void* opaque) {
  struct context* ctx = (context*)opaque;

  for (ParsecClientEvent event;
       ParsecClientPollEvents(ctx->parsec, 0, &event);) {
    switch (event.type) {
      case CLIENT_EVENT_CURSOR:
        cursor_func(ctx, &event.cursor.cursor, event.cursor.key);
        break;
      case CLIENT_EVENT_USER_DATA:
        userData(ctx, event.userData.id, event.userData.key);
        break;
      case CLIENT_EVENT_RUMBLE:
        break;
      default:
        break;
    }
  }

  ParsecStatus e = ParsecClientGetStatus(ctx->parsec, NULL);

  if (e != PARSEC_CONNECTING && e != PARSEC_OK) {
    MTY_ShowMessageBox("Parsec Error", "Parsec error: %d", e);
    ctx->done = true;
  }

  return !ctx->done;
}

Context::Context(const ContextConfig& config) : config_(config) {}

Context::~Context() {}

int Context::Start() {
  struct context ctx = {0};

  char cwd[MAX_PATH];
  ::GetModuleFileNameA(NULL, cwd, MAX_PATH);
  ::PathRemoveFileSpecA(cwd);
  std::string dllpath = std::string(cwd) + "\\parsec.dll";
  if (!std::filesystem::exists(dllpath)) {
    dllpath = "parsec-sdk\\sdk\\windows\\parsec.dll";
  }

  ParsecConfig pc{};
  ParsecStatus e = ParsecInit(&pc, NULL, (char*)dllpath.c_str(), &ctx.parsec);
  if (e != PARSEC_OK) {
    MTY_ShowMessageBox("Parsec Error", "Parsec error: %d", e);
    return 1;
  }

  ctx.audio = MTY_AudioCreate(48000, 75, 150);
  ctx.app = MTY_AppCreate(app_func, event_func, &ctx);

  MTY_WindowDesc desc = {
      .title = config_.name.c_str(),
      .api = MTY_GFX_GL,
      .width = 1280,
      .height = 720,
  };

  MTY_WindowCreate(ctx.app, &desc);

  ParsecSetLogCallback(ctx.parsec, logCallback, NULL);

  ParsecClientConfig cfg = PARSEC_CLIENT_DEFAULTS;
  cfg.pngCursor = true;
  ParsecClientConnect(ctx.parsec, &cfg, config_.session_id.c_str(),
                      config_.host_peer_id.c_str());

  MTY_Thread* render_thread = MTY_ThreadCreate(renderThread, &ctx);
  MTY_Thread* audio_thread = MTY_ThreadCreate(audioThread, &ctx);

  MTY_AppSetTimeout(ctx.app, 1);
  MTY_AppRun(ctx.app);

  MTY_ThreadDestroy(&audio_thread);
  MTY_ThreadDestroy(&render_thread);

  ParsecDestroy(ctx.parsec);

  MTY_AppDestroy(&ctx.app);
  MTY_AudioDestroy(&ctx.audio);

  return 0;
}
