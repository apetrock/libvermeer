#pragma once

#include <cstdio>
#include <string>

namespace lewitt {
namespace subprocess {

inline FILE *open_pipe_write(const std::string &cmd) {
#if defined(_WIN32)
  return _popen(cmd.c_str(), "w");
#else
  return popen(cmd.c_str(), "w");
#endif
}

inline int close_pipe(FILE *pipe) {
  if (pipe == nullptr) {
    return 0;
  }
#if defined(_WIN32)
  return _pclose(pipe);
#else
  return pclose(pipe);
#endif
}

} // namespace subprocess
} // namespace lewitt
