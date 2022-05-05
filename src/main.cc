#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

// Windows API
#include <shlobj.h>

// WinRT for HTTPS GET/POST
#pragma comment(lib, "windowsapp")
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <string_view>
#include "winrt/Windows.Web.Http.Filters.h"
#include "winrt/Windows.Web.Http.Headers.h"
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Filters;
using namespace Windows::Web::Http::Headers;
using namespace Windows::Foundation::Collections;

#include "json.hpp"

#include "client.h"
#include "resource.h"
#include "tray.h"

int g_user_id;
std::string g_session_id, g_peer_id;

void Login();
tray CreateTray();
void RefreshTray();

struct ParsecHost {
  std::string peer_id;
  std::string name;
  bool self;
};
std::vector<ParsecHost> g_hosts;
tray g_tray;
bool g_run_auth = false;

void RefreshTray() {
  g_tray = CreateTray();
  tray_update(g_tray);
}

tray CreateTray() {
  tray t;
  t.icon = IDI_ICON1;
  t.tooltip = "parsec-tray";
  if (g_session_id != "" && g_peer_id != "") {
    t.menu.push_back(tray_menu("Refresh"));
    if (g_hosts.empty()) {
      t.menu.push_back(tray_menu("Hosts not found."));
    } else {
      for (const ParsecHost& host : g_hosts) {
        std::string title;
        if (host.self) {
          title = "(self) " + host.name + " (" + host.peer_id + ")";
        } else {
          title = "Connect to " + host.name + " (" + host.peer_id + ")";
        }
        t.menu.push_back(tray_menu(title, [=](const tray_menu*) {
          char buf[MAX_PATH]{};
          ::GetModuleFileNameA(NULL, buf, MAX_PATH);
          char cmd[1024]{};
          sprintf(cmd, "%s %s %s %s", buf, g_session_id.c_str(),
                  g_peer_id.c_str(), host.peer_id.c_str());
          STARTUPINFOA si{sizeof(si)};
          PROCESS_INFORMATION pi{};
          if (!::CreateProcess(NULL, cmd, NULL, NULL, FALSE, DETACHED_PROCESS,
                               NULL, NULL, &si, &pi)) {
            ::MessageBoxA(NULL, "parsec-tray", "failed to CreateProcess().",
                          MB_OK);
          }
        }));
      }
    }
    t.menu.push_back(tray_menu());
  }
  if (g_session_id != "" && g_peer_id != "") {
    t.menu.push_back(tray_menu("Login to different account ...",
                               [&](const tray_menu*) { g_run_auth = true; }));
    t.menu.push_back(tray_menu("Logout", [&](const tray_menu*) {
      g_session_id.clear();
      g_peer_id.clear();
      RefreshTray();
    }));
  } else {
    t.menu.push_back(tray_menu("Login to Parsec ...",
                               [&](const tray_menu*) { g_run_auth = true; }));
  }
  t.menu.push_back(tray_menu("Exit", [&](const tray_menu*) { tray_exit(); }));
  return t;
}

bool ParsecAuth(const std::string& email,
                const std::string& password,
                const std::string& tfa,
                int& out_user_id,
                std::string& out_session_id,
                std::string& out_peer_id) {
  try {
    nlohmann::json request_json;
    request_json["email"] = email;
    request_json["password"] = password;
    request_json["tfa"] = tfa;

    Uri uri(L"https://kessel-api.parsecgaming.com/v1/auth");
    HttpRequestMessage request(HttpMethod::Post(), uri);
    request.Content(HttpStringContent(winrt::to_hstring(request_json.dump())));

    HttpClient httpClient;
    HttpResponseMessage response = httpClient.SendRequestAsync(request).get();
    if (!response) {
      std::cout << "failed to login.";
      return false;
    }

    nlohmann::json response_json = nlohmann::json::parse(
        winrt::to_string(response.Content().ReadAsStringAsync().get()));
    std::cout << response_json.dump() << std::endl;
    out_user_id = response_json["user_id"];
    out_session_id = response_json["session_id"];
    out_peer_id = response_json["host_peer_id"];
    return true;
  } catch (winrt::hresult_error const& ex) {
    auto message = ex.message();
    std::wcout << std::wstring(message) << std::endl;
    return false;
  } catch (std::exception& ex) {
    std::cout << ex.what() << std::endl;
    return false;
  }
}

bool ParsecHosts(const std::string& session_id,
                 std::vector<ParsecHost>& out_hosts) {
  std::wstring query = L"?mode=desktop&public=false";
  Uri uri(L"https://kessel-api.parsecgaming.com/v2/hosts" + query);
  HttpRequestMessage request(HttpMethod::Get(), uri);
  Headers::HttpCredentialsHeaderValue header(L"Bearer",
                                             winrt::to_hstring(session_id));
  request.Headers().Authorization(header);

  HttpClient httpClient;
  HttpResponseMessage response = httpClient.SendRequestAsync(request).get();
  if (!response) {
    return false;
  }

  std::string res =
      winrt::to_string(response.Content().ReadAsStringAsync().get());
  nlohmann::json response_json = nlohmann::json::parse(res);
  if (!response_json.contains("data")) {
    return false;
  }

  for (const auto& host : response_json["data"]) {
    out_hosts.push_back({
        host["peer_id"],
        host["name"],
        host["self"],
    });
  }
  return true;
}

std::string GetIniPath() {
  WCHAR* wbuf = NULL;
  ::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, NULL, &wbuf);
  if (!wbuf) {
    throw std::runtime_error("failed SHGetKnownFolderPath().");
  }
  std::string path = winrt::to_string(wbuf) + "\\parsec-tray.ini";
  return path;
}

void Login() {
  // too lazy to create GUI ...
  std::string path = GetIniPath();

  g_user_id = 0;
  g_session_id.clear();
  g_peer_id.clear();

  if (!::AllocConsole()) {
    throw std::runtime_error("failed AllocConsole().");
  }

  FILE* fd;
  freopen_s(&fd, "CONIN$", "r", stdin);
  freopen_s(&fd, "CONOUT$", "w", stdout);
  freopen_s(&fd, "CONOUT$", "w", stderr);
  std::cout.clear();
  std::cerr.clear();
  std::cin.clear();

  std::cout << "login to parsec ..." << std::endl;
  while (g_session_id.empty() || g_peer_id.empty()) {
    ::Sleep(1000);

    std::string email, password, tfa;
    std::cout << "input email: ";
    std::cin >> email;
    std::cout << "input password: ";
    std::cin >> password;
    std::cout << "input tfa token (just press enter if not set): ";
    std::cin >> tfa;

    bool ret =
        ParsecAuth(email, password, tfa, g_user_id, g_session_id, g_peer_id);
    if (ret && g_user_id != 0 && g_session_id != "" && g_peer_id != "") {
      ::WritePrivateProfileStringA("parsec-tray", "session_id",
                                   g_session_id.c_str(), path.c_str());
      ::WritePrivateProfileStringA("parsec-tray", "peer_id", g_peer_id.c_str(),
                                   path.c_str());
      break;
    } else {
      std::cout << "failed to login.";
      continue;
    }
  }
  ::fclose(stdin);
  ::fclose(stdout);
  ::fclose(stderr);
  ::FreeConsole();
}

int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR lpCmdLine,
                    int nCmdShow) {
  int argc;
  LPWSTR* argv = ::CommandLineToArgvW(lpCmdLine, &argc);
  if (argc >= 3) {
    g_session_id = winrt::to_string(argv[0]);
    g_peer_id = winrt::to_string(argv[1]);
    ParsecHosts(g_session_id, g_hosts);

    std::string host_peer_id = winrt::to_string(argv[2]);
    auto it = std::find_if(
        g_hosts.begin(), g_hosts.end(),
        [=](const ParsecHost& host) { return host.peer_id == host_peer_id; });

    if (it != g_hosts.end()) {
      ContextConfig config;
      config.name = it->name;
      config.session_id = g_session_id;
      config.peer_id = g_peer_id;
      config.host_peer_id = host_peer_id;

      Context context(config);
      if (!context.Start()) {
        ::MessageBoxA(NULL, "parsec-tray", "Failed to start Parsec client.",
                      MB_OK);
      }
    }
  } else {
    std::string path = GetIniPath();
    char buf[1024]{};
    ::GetPrivateProfileStringA("parsec-tray", "session_id", "", buf,
                               sizeof(buf), path.c_str());
    g_session_id = buf;
    ::GetPrivateProfileStringA("parsec-tray", "peer_id", "", buf, sizeof(buf),
                               path.c_str());
    g_peer_id = buf;

    if (g_session_id != "" && g_peer_id != "") {
      ParsecHosts(g_session_id, g_hosts);
    }
    g_tray = CreateTray();
    tray_init(g_tray);
    while (tray_loop(true) == 0) {
      if (g_run_auth) {
        Login();
        ParsecHosts(g_session_id, g_hosts);
        g_run_auth = false;
      }
    }
  }

  return 0;
}
