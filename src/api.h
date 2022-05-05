#pragma once

#include <string>
#include <vector>

struct ParsecHost {
  std::string peer_id;
  std::string name;
  bool self;
};

bool ParsecAuth(const std::string& email,
                const std::string& password,
                const std::string& tfa,
                int& out_user_id,
                std::string& out_session_id,
                std::string& out_peer_id);

bool ParsecHosts(const std::string& session_id,
                 std::vector<ParsecHost>& out_hosts);
