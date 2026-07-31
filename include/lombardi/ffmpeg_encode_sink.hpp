#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/frame_context.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/render_targets.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/subprocess_pipe.hpp"
#include "liblombardi/node_base.hpp"
#include "lombardi/ffmpeg_record_config.hpp"
#include "lombardi/nodes.hpp"

namespace lombardi {
namespace nodes {

class ffmpeg_encode_sink : public render_node {
public:
  using ptr = std::shared_ptr<ffmpeg_encode_sink>;

  enum class input_port { source };

  static ptr create() { return std::make_shared<ffmpeg_encode_sink>(); }

  ~ffmpeg_encode_sink() override { finalize(); }

  void set_input(input_port,
                 const lewitt::resources::texture_input<lewitt::resources::color_sampled> &input) {
    _source = input;
  }

  bool active() const {
#if defined(_WIN32)
    return false;
#else
    return _pipe != nullptr;
#endif
  }

  // Host calls this when a new sim SceneFrame was applied; idle presents skip encode.
  void request_encode() { _encode_requested.store(true, std::memory_order_relaxed); }

  void init(wgpu::Device device, const lewitt::render_targets::extent2d &extent,
            const ffmpeg_record_config &config) {
    finalize();
    _device = device;
    _extent = extent;
    _config = config;
    _encode_requested.store(false, std::memory_order_relaxed);

    // yuv420p (libx264) requires even dimensions; crop the odd edge if needed.
    _encode_width = extent.width & ~1u;
    _encode_height = extent.height & ~1u;
    _row_bytes = _encode_width * 4;
    // WebGPU copyTextureToBuffer requires bytesPerRow multiple of 256.
    _padded_row_bytes = ((_row_bytes + 255u) / 256u) * 256u;
    if (_padded_row_bytes < 256u)
      _padded_row_bytes = 256u;
    _frame_bytes = static_cast<uint64_t>(_padded_row_bytes) * _encode_height;
    _rgba_row.resize(_row_bytes);

#if !defined(_WIN32)
    if (!config.enabled || !_encode_width || !_encode_height) {
      return;
    }

    if (std::system("command -v ffmpeg >/dev/null 2>&1") != 0) {
      std::cerr << "ffmpeg_encode_sink: ffmpeg not found on PATH\n";
      return;
    }

    const auto parent = std::filesystem::path(config.output_path).parent_path();
    if (!parent.empty()) {
      std::error_code ec;
      std::filesystem::create_directories(parent, ec);
    }

    // One continuous mp4 (not -f segment): mp4 segment without frag flags yields
    // tiny/broken one-frame files. Playback rate is config.fps sim-frames/sec.
    const std::string out = config.output_path + ".mp4";
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -y"
        << " -f rawvideo -pix_fmt rgba -s " << _encode_width << "x" << _encode_height
        << " -r " << config.fps << " -i -"
        << " -an -c:v libx264 -threads 0 -preset fast -crf " << config.crf
        << " -pix_fmt yuv420p -movflags +faststart " << out;

    _pipe = lewitt::subprocess::open_pipe_write(cmd.str());
    if (_pipe == nullptr) {
      std::cerr << "ffmpeg_encode_sink: could not launch ffmpeg\n";
      return;
    }

    std::cerr << "ffmpeg_encode_sink: recording to " << out
              << " (sim frames only @ " << config.fps << " fps playback)\n";

    wgpu::BufferDescriptor buffer_desc{};
    buffer_desc.size = _frame_bytes;
    buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    _staging = device.createBuffer(buffer_desc);
#endif
  }

  void resize(wgpu::Device device, const lewitt::render_targets::extent2d &extent) {
    const uint32_t ew = extent.width & ~1u;
    const uint32_t eh = extent.height & ~1u;
    if (ew == _encode_width && eh == _encode_height && active()) {
      _extent = extent;
      return;
    }
    init(device, extent, _config);
  }

  bool terminal_sink() const override { return true; }

  std::vector<resource_use> resource_uses() const override {
    const lewitt::render_targets::target_id source_id = _source.source.id;
    if (source_id == lewitt::render_targets::invalid_target_id) {
      return {};
    }
    return {{resource_use_kind::read, source_id}};
  }

  void render(lewitt::frame_context &ctx) override {
    encode_frame(ctx, ctx.texture_targets);
  }

  void encode_frame(lewitt::frame_context &ctx, const lewitt::render_targets::target_pool &pool) {
#if defined(_WIN32)
    (void)ctx;
    (void)pool;
    return;
#else
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::ffmpeg_encode_sink::encode_frame");
    if (!_encode_requested.exchange(false, std::memory_order_relaxed)) {
      return;
    }
    if (_pipe == nullptr || !_staging || !_device) {
      return;
    }

    const lewitt::render_targets::target_id source_id = _source.source.id;
    if (source_id == lewitt::render_targets::invalid_target_id || !pool.valid(source_id)) {
      return;
    }

    const lewitt::render_targets::render_target &target = pool.get(source_id);
    wgpu::Texture texture = target.texture();
    if (!texture) {
      return;
    }

    // Copy only the even-sized region we encode (matches ffmpeg -s).
    wgpu::CommandEncoder encoder = _device.createCommandEncoder(wgpu::CommandEncoderDescriptor{});
    wgpu::ImageCopyTexture src{};
    src.texture = texture;
    wgpu::ImageCopyBuffer dst{};
    dst.buffer = _staging;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = _padded_row_bytes;
    dst.layout.rowsPerImage = _encode_height;
    encoder.copyTextureToBuffer(src, dst, {_encode_width, _encode_height, 1});
    wgpu::CommandBuffer commands = encoder.finish(wgpu::CommandBufferDescriptor{});
    ctx.queue.submit(1, &commands);

    bool done = false;
    // CRITICAL: keep the callback handle alive until mapAsync completes. Dropping
    // it leaves userdata dangling and the wait loop hangs forever.
    auto map_handle = _staging.mapAsync(wgpu::MapMode::Read, 0, _frame_bytes,
                                        [&](wgpu::BufferMapAsyncStatus status) {
                                          if (status == wgpu::BufferMapAsyncStatus::Success) {
                                            const auto *mapped = static_cast<const uint8_t *>(
                                                _staging.getConstMappedRange(0, _frame_bytes));
                                            if (mapped != nullptr) {
                                              write_frame_rows(mapped);
                                              _staging.unmap();
                                            }
                                          }
                                          done = true;
                                        });

    while (!done) {
#ifdef WEBGPU_BACKEND_WGPU
      ctx.queue.submit(0, nullptr);
#elif defined(WEBGPU_BACKEND_DAWN)
      wgpuDeviceTick(_device);
#else
      ctx.queue.submit(0, nullptr);
#endif
    }
    (void)map_handle;
#endif
  }

  void finalize() {
#if !defined(_WIN32)
    if (_pipe != nullptr) {
      std::fflush(_pipe);
      lewitt::subprocess::close_pipe(_pipe);
      _pipe = nullptr;
    }
    if (_staging) {
      _staging.destroy();
      _staging = nullptr;
    }
#endif
  }

private:
#if !defined(_WIN32)
  void write_frame_rows(const uint8_t *mapped) {
    for (uint32_t row = 0; row < _encode_height; ++row) {
      const uint8_t *src_row = mapped + static_cast<size_t>(row) * _padded_row_bytes;
      std::memcpy(_rgba_row.data(), src_row, _row_bytes);
      if (std::fwrite(_rgba_row.data(), 1, _row_bytes, _pipe) != _row_bytes) {
        std::cerr << "ffmpeg_encode_sink: pipe write failed\n";
        finalize();
        return;
      }
    }
  }
#endif

  lewitt::resources::texture_input<lewitt::resources::color_sampled> _source{};
  ffmpeg_record_config _config{};
  lewitt::render_targets::extent2d _extent{};
  wgpu::Device _device = nullptr;
  uint32_t _encode_width = 0;
  uint32_t _encode_height = 0;
  uint32_t _row_bytes = 0;
  uint32_t _padded_row_bytes = 0;
  uint64_t _frame_bytes = 0;
  std::vector<uint8_t> _rgba_row;
  std::atomic<bool> _encode_requested{false};
#if !defined(_WIN32)
  wgpu::Buffer _staging = nullptr;
  FILE *_pipe = nullptr;
#endif
};

} // namespace nodes
} // namespace lombardi
