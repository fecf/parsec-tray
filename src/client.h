#pragma once

#include <string>

struct ContextConfig {
  std::string name;
  std::string session_id;
  std::string peer_id;
  std::string host_peer_id;
};

class Context {
 public:
  Context(const ContextConfig& config);
  ~Context();

  int Start();

 private:
  ContextConfig config_;
};
