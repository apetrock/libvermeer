struct VertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) color: vec3f,
};

struct AlbedoVertexOutput {
  @builtin(position) position: vec4f,
  @location(0) albedo: vec3f,
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
fn vs_albedo(in: VertexInput) -> AlbedoVertexOutput {
  var out: AlbedoVertexOutput;
  let worldPosition = u_object.modelMatrix * vec4f(in.position, 1.0);
  out.position = u_object.projectionMatrix * u_object.viewMatrix * worldPosition;
  out.albedo = in.color;
  return out;
}

@fragment
fn fs_albedo(in: AlbedoVertexOutput) -> @location(0) vec4f {
  return vec4f(in.albedo, 1.0);
}
