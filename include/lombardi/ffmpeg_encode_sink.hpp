#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
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

  void init(wgpu::Device device, const lewitt::render_targets::extent2d &extent,
            const ffmpeg_record_config &config) {
    finalize();
    _device = device;
    _extent = extent;
    _config = config;
    _row_bytes = extent.width * 4;
    _padded_row_bytes = std::max(256u, _row_bytes);
    _frame_bytes = _padded_row_bytes * extent.height;
    _rgba_row.resize(_row_bytes);

#if !defined(_WIN32)
    if (!config.enabled || !extent.width || !extent.height) {
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

    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -y"
        << " -r " << config.fps << " -f rawvideo -pix_fmt rgba -s " << extent.width << "x"
        << extent.height << " -i -"
        << " -threads 0 -preset fast -crf " << config.crf << " -pix_fmt yuv420p"
        << " -f segment -segment_time " << config.segment_seconds << " -reset_timestamps 1 "
        << config.output_path << "_%03d.mp4";

    _pipe = lewitt::subprocess::open_pipe_write(cmd.str());
    if (_pipe == nullptr) {
      std::cerr << "ffmpeg_encode_sink: could not launch ffmpeg\n";
      return;
    }

    std::cout << "ffmpeg_encode_sink: recording to " << config.output_path << "_NNN.mp4\n";

    wgpu::BufferDescriptor buffer_desc{};
    buffer_desc.size = _frame_bytes;
    buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    _staging = device.createBuffer(buffer_desc);
#endif
  }

  void resize(wgpu::Device device, const lewitt::render_targets::extent2d &extent) {
    if (_extent.width == extent.width && _extent.height == extent.height && active()) {
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

    wgpu::CommandEncoder encoder = _device.createCommandEncoder(wgpu::CommandEncoderDescriptor{});
    wgpu::ImageCopyTexture src{};
    src.texture = texture;
    wgpu::ImageCopyBuffer dst{};
    dst.buffer = _staging;
    dst.layout.offset = 0;
    dst.layout.bytesPerRow = _padded_row_bytes;
    dst.layout.rowsPerImage = _extent.height;
    encoder.copyTextureToBuffer(src, dst, {_extent.width, _extent.height, 1});
    wgpu::CommandBuffer commands = encoder.finish(wgpu::CommandBufferDescriptor{});
    ctx.queue.submit(1, &commands);

    bool done = false;
    _staging.mapAsync(wgpu::MapMode::Read, 0, _frame_bytes,
                      [&](wgpu::BufferMapAsyncStatus status) {
                        if (status == wgpu::BufferMapAsyncStatus::Success) {
                          const auto *mapped =
                              static_cast<const uint8_t *>(_staging.getConstMappedRange(0, _frame_bytes));
                          if (mapped != nullptr) {
                            write_frame_rows(mapped);
                            _staging.unmap();
                          }
                        }
                        done = true;
                      });

    while (!done) {
      ctx.queue.submit(0, nullptr);
    }
#endif
  }

  void finalize() {
#if !defined(_WIN32)
    if (_pipe != nullptr) {
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
    for (uint32_t row = 0; row < _extent.height; ++row) {
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
  uint32_t _row_bytes = 0;
  uint32_t _padded_row_bytes = 0;
  uint32_t _frame_bytes = 0;
  std::vector<uint8_t> _rgba_row;
#if !defined(_WIN32)
  wgpu::Buffer _staging = nullptr;
  FILE *_pipe = nullptr;
#endif
};

} // namespace nodes
} // namespace lombardi
