#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

// Windows API
#include <shlobj.h>

#include "json.hpp"

#include "api.h"
#include "client.h"
#include "resource.h"
#include "tray.h"

int g_user_id;
std::string g_session_id, g_peer_id;

void Login();
void RefreshHosts();
tray CreateTray();
void RefreshTray();

std::vector<ParsecHost> g_hosts;
tray g_tray;

std::string GetIniPath() {
  WCHAR* wbuf = NULL;
  ::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, NULL, &wbuf);
  if (!wbuf) {
    throw std::runtime_error("failed SHGetKnownFolderPath().");
  }
  std::string path = winrt::to_string(wbuf) + "\\parsec-tray.ini";
  return path;
}

void RefreshTray() {
  g_tray = CreateTray();
  tray_update(g_tray);
}

tray CreateTray() {
  tray t;
  t.icon = IDI_ICON1;
  t.tooltip = "parsec-tray";
  if (g_session_id != "" && g_peer_id != "") {
    t.menu.push_back(tray_menu("Refresh", [&](const tray_menu*) {
      RefreshHosts();
      RefreshTray();
    }));
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
    t.menu.push_back(
        tray_menu("Login to different account ...", [&](const tray_menu*) {
          Login();
          RefreshHosts();
          RefreshTray();
        }));
    t.menu.push_back(tray_menu("Logout", [&](const tray_menu*) {
      g_session_id.clear();
      g_peer_id.clear();
      std::string path = GetIniPath();
      ::WritePrivateProfileStringA("parsec-tray", "session_id", "",
                                   path.c_str());
      ::WritePrivateProfileStringA("parsec-tray", "peer_id", "", path.c_str());
      RefreshTray();
    }));
  } else {
    t.menu.push_back(tray_menu("Login to Parsec ...", [&](const tray_menu*) {
      Login();
      ParsecHosts(g_session_id, g_hosts);
      RefreshTray();
    }));
  }
  t.menu.push_back(tray_menu("-"));
  t.menu.push_back(tray_menu(
      "Visit Website (https://github.com/fecf/parsec-tray)",
      [&](const tray_menu*) {
        ::ShellExecuteA(NULL, "open", "https://github.com/fecf/parsec-tray",
                        NULL, NULL, SW_SHOWNORMAL);
      }));
  t.menu.push_back(tray_menu("Exit", [&](const tray_menu*) { tray_exit(); }));
  return t;
}

void Login() {
  // too lazy to create GUI ...
  std::string path = GetIniPath();

  g_user_id = 0;
  g_session_id.clear();
  g_peer_id.clear();

  if (!::AllocConsole()) {
    throw std::runtime_error("failed to AllocConsole().");
  }
  FILE* fd;
  freopen_s(&fd, "CONIN$", "r", stdin);
  freopen_s(&fd, "CONOUT$", "w", stdout);
  freopen_s(&fd, "CONOUT$", "w", stderr);
  std::cout.clear();
  std::cerr.clear();
  std::cin.clear();

  std::cout << "Login to Parsec ..." << std::endl;
  while (g_session_id.empty() || g_peer_id.empty()) {
    std::cout << std::endl;
    ::Sleep(1000);

    std::string email, password, tfa;
    std::cout << "Input email: ";
    std::cin >> email;
    std::cout << "Input password: ";
    std::cin >> password;
    std::cout << "Input tfa token (input \"no\" if not set): ";
    std::cin >> tfa;

    if (tfa == "no") {
      tfa = "";
    }

    bool ret =
        ParsecAuth(email, password, tfa, g_user_id, g_session_id, g_peer_id);
    if (ret && g_user_id != 0 && g_session_id != "" && g_peer_id != "") {
      ::WritePrivateProfileStringA("parsec-tray", "session_id",
                                   g_session_id.c_str(), path.c_str());
      ::WritePrivateProfileStringA("parsec-tray", "peer_id", g_peer_id.c_str(),
                                   path.c_str());
      break;
    } else {
      std::cout << "Failed to login to Parsec.";
      continue;
    }
  }
  ::fclose(stdin);
  ::fclose(stdout);
  ::fclose(stderr);
  ::FreeConsole();
}

void RefreshHosts() {
  g_hosts.clear();
  if (g_session_id.empty()) {
    return;
  }
  bool success = ParsecHosts(g_session_id, g_hosts);
  if (!success) {
    ::MessageBoxA(NULL, "parsec-tray", "Failed to refresh Parsec hosts.", MB_OK);
  }
}

int WINAPI wWinMain(HINSTANCE hInstance,
                    HINSTANCE hPrevInstance,
                    PWSTR lpCmdLine,
                    int nCmdShow) {
  winrt::init_apartment();

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
    }
  }

  return 0;
}
