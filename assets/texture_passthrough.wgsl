struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
  var positions = array<vec2f, 3>(
    vec2f(-1.0, -1.0),
    vec2f(3.0, -1.0),
    vec2f(-1.0, 3.0),
  );
  var out: VertexOutput;
  let pos = positions[vertex_index];
  out.position = vec4f(pos, 0.0, 1.0);
  let uv = pos * 0.5 + vec2f(0.5, 0.5);
  out.uv = vec2f(uv.x, 1.0 - uv.y);
  return out;
}

@group(0) @binding(0) var t_input: texture_2d<f32>;
@group(0) @binding(1) var s_input: sampler;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let value = textureSample(t_input, s_input, in.uv);
  return vec4f(value.rgb, 1.0);
}
