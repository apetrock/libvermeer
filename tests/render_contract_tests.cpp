#include "lewitt/render_contract.hpp"
#include "lewitt/render_targets.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/g_buffer_node.hpp"
#include "mondrian/draw_schema.hpp"
#include "mondrian/render_contract.hpp"

#include <cassert>
#include <iostream>

namespace {

void test_attachment_state_initialized() {
  const auto color_state = lewitt::render_targets::attachment_state::color_clear();
  assert(lewitt::render_contract::attachment_load_store_initialized(color_state));

  const auto depth_state = lewitt::render_targets::attachment_state::depth_clear_write();
  assert(lewitt::render_contract::attachment_load_store_initialized(depth_state));
  assert(depth_state.depth_clear == 1.0f);
}

void test_blendable_formats() {
  assert(lewitt::render_contract::is_blendable_format(
      lewitt::resources::position_attachment::format()));
  assert(lewitt::render_contract::is_blendable_format(
      lewitt::resources::ssao_attachment::format()));
  assert(lewitt::render_contract::is_blendable_format(
      lewitt::resources::albedo_spec_attachment::format()));
}

void test_gbuffer_pass_count() {
  assert(lewitt::render_contract::gbuffer_pass_count(false, false) == 0);
  assert(lewitt::render_contract::gbuffer_pass_count(true, false) == 1);
  assert(lewitt::render_contract::gbuffer_pass_count(false, true) == 1);
  assert(lewitt::render_contract::gbuffer_pass_count(true, true) == 1);
  assert(lewitt::nodes::g_buffer_node::k_pass_count_with_geometry == 1);
}

void test_ssao_uniform_size() {
  assert(lewitt::render_contract::ssao_uniform_size_matches(80));
  assert(!lewitt::render_contract::ssao_uniform_size_matches(64));
}

void test_albedo_schema_depth_compare() {
  const auto mesh_schema = mondrian::mesh_albedo_schema();
  const auto line_schema = mondrian::debug_line_albedo_schema();
  assert(mondrian::render_contract::albedo_schema_uses_depth_read(mesh_schema.depth_compare,
                                                                  mesh_schema.depth_write_enabled));
  assert(mondrian::render_contract::albedo_schema_uses_depth_read(line_schema.depth_compare,
                                                                  line_schema.depth_write_enabled));
}

void test_ssao_projected_uv_y_flip() {
  assert(mondrian::render_contract::ssao_projected_uv_y(1.0f) == 0.0f);
  assert(mondrian::render_contract::ssao_projected_uv_y(-1.0f) == 1.0f);
  assert(mondrian::render_contract::ssao_projected_uv_y(0.0f) == 0.5f);
}

} // namespace

int main() {
  test_attachment_state_initialized();
  test_blendable_formats();
  test_gbuffer_pass_count();
  test_ssao_uniform_size();
  test_albedo_schema_depth_compare();
  test_ssao_projected_uv_y_flip();
  std::cout << "render_contract_tests passed\n";
  return 0;
}
