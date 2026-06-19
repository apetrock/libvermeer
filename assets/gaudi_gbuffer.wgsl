struct VertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) color: vec3f,
};

struct GBufferVertexOutput {
  @builtin(position) position: vec4f,
  @location(0) viewPosition: vec3f,
  @location(1) viewNormal: vec3f,
};

struct uniforms {
  projectionMatrix: mat4x4f,
  viewMatrix: mat4x4f,
  modelMatrix: mat4x4f,
  color: vec4f,
  cameraWorldPosition: vec3f,
  time: f32,
}

@group(0) @binding(0) var<uniform> u_object: uniforms;

@vertex
fn vs_gbuffer(in: VertexInput) -> GBufferVertexOutput {
  var out: GBufferVertexOutput;
  let worldPosition = u_object.modelMatrix * vec4f(in.position, 1.0);
  let viewPosition = (u_object.viewMatrix * worldPosition).xyz;
  let viewNormal =
    normalize((u_object.viewMatrix * u_object.modelMatrix * vec4f(in.normal, 0.0)).xyz);
  out.position = u_object.projectionMatrix * u_object.viewMatrix * worldPosition;
  out.viewPosition = viewPosition;
  out.viewNormal = viewNormal;
  return out;
}

@fragment
fn fs_position(in: GBufferVertexOutput) -> @location(0) vec4f {
  return vec4f(in.viewPosition, 1.0);
}

@fragment
fn fs_normal(in: GBufferVertexOutput) -> @location(0) vec4f {
  return vec4f(normalize(in.viewNormal), 1.0);
}
