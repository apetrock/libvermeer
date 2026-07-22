#include "lewitt/performance.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace lewitt {
namespace performance {
namespace {

using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

struct profiler_state {
  bool enabled = std::getenv("VERMEER_PERF") != nullptr;
  bool reported = false;
  bool exit_report_registered = false;
  bool signal_handlers_registered = false;
  std::unordered_map<std::string, std::vector<time_point>> active;
  std::unordered_map<std::string, profiler::sample> samples;
  std::mutex mutex;
};

std::atomic<int> g_pending_signal{0};

profiler_state &state() {
  static profiler_state value;
  return value;
}

void report_at_exit() { profiler::instance().report(std::cerr); }

void signal_handler(int signal) { g_pending_signal.store(signal, std::memory_order_relaxed); }

void install_signal_handlers(profiler_state &profiler_state) {
  if (profiler_state.signal_handlers_registered) {
    return;
  }
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  profiler_state.signal_handlers_registered = true;
}

void ensure_exit_reporting(profiler_state &profiler_state) {
  if (profiler_state.exit_report_registered) {
    return;
  }
  std::atexit(report_at_exit);
  profiler_state.exit_report_registered = true;
}

void ensure_reporting_hooks(profiler_state &profiler_state) {
  ensure_exit_reporting(profiler_state);
  install_signal_handlers(profiler_state);
}

void record_locked(profiler_state &profiler_state, const std::string &path,
                   double elapsed_ms) {
  auto &entry = profiler_state.samples[path];
  const bool first_sample = entry.count == 0;
  entry.count += 1;
  entry.total_ms += elapsed_ms;
  entry.sum_squares_ms += elapsed_ms * elapsed_ms;
  entry.last_ms = elapsed_ms;
  if (first_sample || elapsed_ms < entry.min_ms) {
    entry.min_ms = elapsed_ms;
  }
  if (elapsed_ms > entry.max_ms) {
    entry.max_ms = elapsed_ms;
  }
}

void report_locked(profiler_state &profiler_state, std::ostream &out) {
  if (profiler_state.reported || profiler_state.samples.empty()) {
    profiler_state.reported = true;
    return;
  }

  std::vector<std::pair<std::string, profiler::sample>> ordered;
  ordered.reserve(profiler_state.samples.size());
  for (const auto &[path, entry] : profiler_state.samples) {
    ordered.push_back({path, entry});
  }
  std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
    return a.second.total_ms > b.second.total_ms;
  });

  out << "[performance]\n";
  out << std::fixed << std::setprecision(3);
  for (const auto &[path, entry] : ordered) {
    const double avg_ms =
        entry.count ? entry.total_ms / static_cast<double>(entry.count) : 0.0;
    const double variance_ms =
        entry.count ? (entry.sum_squares_ms / static_cast<double>(entry.count)) -
                          (avg_ms * avg_ms)
                    : 0.0;
    const double stddev_ms = std::sqrt(std::max(0.0, variance_ms));
    out << "  " << path << " count=" << entry.count << " total=" << entry.total_ms
        << "ms avg=" << avg_ms << "ms stddev=" << stddev_ms
        << "ms min=" << entry.min_ms << "ms max=" << entry.max_ms
        << "ms last=" << entry.last_ms << "ms\n";
  }
  profiler_state.reported = true;
}

void handle_pending_signal_if_needed() {
  const int signal = g_pending_signal.exchange(0, std::memory_order_relaxed);
  if (signal == 0) {
    return;
  }

  profiler::instance().report(std::cerr);
  std::signal(signal, SIG_DFL);
  std::raise(signal);
}

} // namespace

profiler &profiler::instance() {
  static profiler profiler;
  return profiler;
}

bool profiler::enabled() const { return state().enabled; }

void profiler::set_enabled(bool enabled) {
  auto &profiler_state = state();
  profiler_state.enabled = enabled;
  if (enabled) {
    ensure_reporting_hooks(profiler_state);
  }
}

void profiler::start(std::string_view path) {
  auto &profiler_state = state();
  if (!profiler_state.enabled) {
    return;
  }

  ensure_reporting_hooks(profiler_state);
  std::lock_guard<std::mutex> lock(profiler_state.mutex);
  profiler_state.active[std::string(path)].push_back(clock::now());
}

void profiler::stop(std::string_view path) {
  auto &profiler_state = state();
  if (!profiler_state.enabled) {
    return;
  }

  const auto end = clock::now();
  {
    std::lock_guard<std::mutex> lock(profiler_state.mutex);
    auto active_it = profiler_state.active.find(std::string(path));
    if (active_it == profiler_state.active.end() || active_it->second.empty()) {
      return;
    }

    const auto begin = active_it->second.back();
    active_it->second.pop_back();
    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(end - begin).count();
    record_locked(profiler_state, std::string(path), elapsed_ms);
  }

  handle_pending_signal_if_needed();
}

void profiler::record(std::string_view path, double elapsed_ms) {
  auto &profiler_state = state();
  if (!profiler_state.enabled) {
    return;
  }

  ensure_reporting_hooks(profiler_state);
  {
    std::lock_guard<std::mutex> lock(profiler_state.mutex);
    record_locked(profiler_state, std::string(path), elapsed_ms);
  }

  handle_pending_signal_if_needed();
}

void profiler::report(std::ostream &out) {
  auto &profiler_state = state();
  if (!profiler_state.enabled) {
    return;
  }

  std::lock_guard<std::mutex> lock(profiler_state.mutex);
  report_locked(profiler_state, out);
}

bool enabled() { return profiler::instance().enabled(); }
void set_enabled(bool enabled) { profiler::instance().set_enabled(enabled); }
void start(std::string_view path) { profiler::instance().start(path); }
void stop(std::string_view path) { profiler::instance().stop(path); }
void record(std::string_view path, double elapsed_ms) {
  profiler::instance().record(path, elapsed_ms);
}
void report(std::ostream &out) { profiler::instance().report(out); }

scope::scope(std::string_view path) : _path(path) { start(_path); }
scope::~scope() { stop(_path); }

} // namespace performance
} // namespace lewitt
