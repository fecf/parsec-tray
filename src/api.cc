#include "api.h"

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
      std::cout << "failed to login." << std::endl;
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
  out_hosts.clear();

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

