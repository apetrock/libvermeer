#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/shaders.hpp"
#include "mondrian/render_contract.hpp"

namespace mondrian {

struct draw_schema {
  std::string shader_path;
  std::string vertex_entry = "vs_gbuffer";
  std::string fragment_entry = "fs_gbuffer";
  std::vector<wgpu::TextureFormat> color_formats{};
  wgpu::TextureFormat depth_format = wgpu::TextureFormat::Undefined;
  bool depth_write_enabled = true;
  wgpu::CompareFunction depth_compare = wgpu::CompareFunction::Less;
  std::vector<wgpu::VertexBufferLayout> vertex_layouts{};
};

class pipeline_state {
public:
  wgpu::RenderPipeline get_or_create(wgpu::Device device, const draw_schema &schema,
                                     lewitt::bindings::group::ptr bindings) {
    if (!bindings) {
      return nullptr;
    }

    const cache_key key{schema.shader_path, schema.vertex_entry, schema.fragment_entry,
                        schema.color_formats, schema.depth_format, schema.depth_write_enabled,
                        schema.depth_compare, bindings.get()};

    if (const auto it = _entries.find(key); it != _entries.end()) {
      return it->second.pipeline;
    }

    auto shader = lewitt::shaders::render_shader::create_from_path(schema.shader_path, device);
    for (const auto &layout : schema.vertex_layouts) {
      shader->add_layout(layout);
    }

    bool ready = false;
    if (schema.color_formats.size() > 1) {
      ready = shader->init(device, bindings->get_layout(), schema.color_formats,
                           schema.depth_format, schema.vertex_entry, schema.fragment_entry,
                           schema.depth_write_enabled, schema.depth_compare);
    } else if (!schema.color_formats.empty()) {
      ready = shader->init(device, bindings->get_layout(), schema.color_formats.front(),
                           schema.depth_format, schema.vertex_entry, schema.fragment_entry,
                           schema.depth_write_enabled, schema.depth_compare);
    }

    if (!ready) {
      return nullptr;
    }

    cached_entry entry{};
    entry.shader = std::move(shader);
    entry.pipeline = entry.shader->render_pipe_line();
    _entries.emplace(key, std::move(entry));
    return _entries.at(key).pipeline;
  }

  void clear() { _entries.clear(); }

private:
  struct cached_entry {
    lewitt::shaders::render_shader::ptr shader;
    wgpu::RenderPipeline pipeline = nullptr;
  };

  struct cache_key {
    std::string shader_path;
    std::string vertex_entry;
    std::string fragment_entry;
    std::vector<wgpu::TextureFormat> color_formats;
    wgpu::TextureFormat depth_format;
    bool depth_write_enabled;
    wgpu::CompareFunction depth_compare;
    void *layout_handle;

    bool operator==(const cache_key &other) const {
      return shader_path == other.shader_path && vertex_entry == other.vertex_entry &&
             fragment_entry == other.fragment_entry && color_formats == other.color_formats &&
             depth_format == other.depth_format &&
             depth_write_enabled == other.depth_write_enabled &&
             depth_compare == other.depth_compare && layout_handle == other.layout_handle;
    }
  };

  struct cache_key_hash {
    std::size_t operator()(const cache_key &key) const {
      std::size_t hash = std::hash<std::string>{}(key.shader_path);
      hash ^= std::hash<std::string>{}(key.vertex_entry) << 1;
      hash ^= std::hash<std::string>{}(key.fragment_entry) << 2;
      hash ^= std::hash<void *>{}(key.layout_handle) << 3;
      hash ^= std::hash<int>{}(static_cast<int>(key.depth_compare)) << 4;
      return hash;
    }
  };

  std::unordered_map<cache_key, cached_entry, cache_key_hash> _entries;
};

using pipeline_cache = pipeline_state;

inline draw_schema mesh_gbuffer_mrt_schema() {
  draw_schema schema{};
  schema.shader_path = RESOURCE_DIR "/gaudi_gbuffer.wgsl";
  schema.vertex_entry = "vs_gbuffer";
  schema.fragment_entry = "fs_gbuffer";
  schema.color_formats = {lewitt::resources::position_attachment::format(),
                          lewitt::resources::normal_attachment::format()};
  schema.depth_format = lewitt::resources::depth_attachment::format();
  schema.depth_write_enabled = true;
  schema.depth_compare = wgpu::CompareFunction::Less;
  schema.vertex_layouts = {lewitt::mesh_buffer::layout()};
  return schema;
}

inline draw_schema debug_line_gbuffer_mrt_schema() {
  draw_schema schema{};
  schema.shader_path = RESOURCE_DIR "/debug_line_gbuffer.wgsl";
  schema.vertex_entry = "vs_gbuffer";
  schema.fragment_entry = "fs_gbuffer";
  schema.color_formats = {lewitt::resources::position_attachment::format(),
                          lewitt::resources::normal_attachment::format()};
  schema.depth_format = lewitt::resources::depth_attachment::format();
  schema.depth_write_enabled = true;
  schema.depth_compare = wgpu::CompareFunction::Less;
  return schema;
}

inline draw_schema mesh_albedo_schema() {
  draw_schema schema{};
  schema.shader_path = RESOURCE_DIR "/gaudi_albedo.wgsl";
  schema.vertex_entry = "vs_albedo";
  schema.fragment_entry = "fs_albedo";
  schema.color_formats = {lewitt::resources::albedo_spec_attachment::format()};
  schema.depth_format = lewitt::resources::depth_attachment::format();
  schema.depth_write_enabled = false;
  schema.depth_compare = render_contract::albedo_depth_compare();
  schema.vertex_layouts = {lewitt::mesh_buffer::layout()};
  return schema;
}

inline draw_schema debug_line_albedo_schema() {
  draw_schema schema{};
  schema.shader_path = RESOURCE_DIR "/debug_line_albedo.wgsl";
  schema.vertex_entry = "vs_albedo";
  schema.fragment_entry = "fs_albedo";
  schema.color_formats = {lewitt::resources::albedo_spec_attachment::format()};
  schema.depth_format = lewitt::resources::depth_attachment::format();
  schema.depth_write_enabled = false;
  schema.depth_compare = render_contract::albedo_depth_compare();
  return schema;
}

inline draw_schema mesh_forward_schema() {
  draw_schema schema{};
  schema.shader_path = RESOURCE_DIR "/gaudi_snapshot.wgsl";
  schema.vertex_entry = "vs_main";
  schema.fragment_entry = "fs_main";
  schema.color_formats = {lewitt::resources::lit_color_attachment::format()};
  schema.depth_format = lewitt::resources::depth_attachment::format();
  schema.depth_write_enabled = true;
  schema.depth_compare = wgpu::CompareFunction::Less;
  schema.vertex_layouts = {lewitt::mesh_buffer::layout()};
  return schema;
}

inline draw_schema debug_line_forward_schema() {
  draw_schema schema{};
  schema.shader_path = RESOURCE_DIR "/debug_line_albedo.wgsl";
  schema.vertex_entry = "vs_albedo";
  schema.fragment_entry = "fs_albedo";
  schema.color_formats = {lewitt::resources::lit_color_attachment::format()};
  schema.depth_format = lewitt::resources::depth_attachment::format();
  schema.depth_write_enabled = false;
  schema.depth_compare = render_contract::albedo_depth_compare();
  return schema;
}

inline draw_schema fullscreen_schema(std::string shader_path,
                                     wgpu::TextureFormat color_format,
                                     std::string vertex_entry = "vs_main",
                                     std::string fragment_entry = "fs_main") {
  draw_schema schema{};
  schema.shader_path = std::move(shader_path);
  schema.vertex_entry = std::move(vertex_entry);
  schema.fragment_entry = std::move(fragment_entry);
  schema.color_formats = {color_format};
  schema.depth_format = wgpu::TextureFormat::Undefined;
  schema.depth_write_enabled = false;
  schema.depth_compare = wgpu::CompareFunction::Always;
  return schema;
}

} // namespace mondrian
