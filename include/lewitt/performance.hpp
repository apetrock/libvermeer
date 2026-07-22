#pragma once

#include <cstdint>
#include <iostream>
#include <string_view>

namespace lewitt {
namespace performance {

class profiler {
public:
  struct sample {
    uint64_t count = 0;
    double total_ms = 0.0;
    double sum_squares_ms = 0.0;
    double min_ms = 0.0;
    double max_ms = 0.0;
    double last_ms = 0.0;
  };

  static profiler &instance();

  bool enabled() const;
  void set_enabled(bool enabled);
  void start(std::string_view path);
  void stop(std::string_view path);
  void record(std::string_view path, double elapsed_ms);
  void report(std::ostream &out = std::cout);
};

bool enabled();
void set_enabled(bool enabled);
void start(std::string_view path);
void stop(std::string_view path);
void record(std::string_view path, double elapsed_ms);
void report(std::ostream &out = std::cout);

class scope {
public:
  explicit scope(std::string_view path);
  ~scope();

  scope(const scope &) = delete;
  scope &operator=(const scope &) = delete;

private:
  std::string_view _path;
};

} // namespace performance
} // namespace lewitt

namespace performance = ::lewitt::performance;

#define LEWITT_PERF_CONCAT_IMPL(x, y) x##y
#define LEWITT_PERF_CONCAT(x, y) LEWITT_PERF_CONCAT_IMPL(x, y)
#define LEWITT_PERF_SCOPE_PATH(path) \
  ::lewitt::performance::scope LEWITT_PERF_CONCAT(_lewitt_perf_scope_, __LINE__)(path)
#define LEWITT_PERF_SCOPE() LEWITT_PERF_SCOPE_PATH(__PRETTY_FUNCTION__)
