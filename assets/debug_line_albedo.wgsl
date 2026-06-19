struct VertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) flag: u32,
  @location(3) radius: f32,
  @location(4) p0: vec3f,
  @location(5) p1: vec3f,
  @location(6) color: vec3f,
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

  var pos = in.position;
  let dp = normalize(in.p1 - in.p0);
  let Z = vec3f(0.0, 1.001, 0.0);
  let m0 = dp;
  let m1 = normalize(cross(m0, Z));
  let m2 = normalize(cross(m0, m1));
  let M = mat3x3f(
    m1[0], m1[1], m1[2],
    m2[0], m2[1], m2[2],
    m0[0], m0[1], m0[2],
  );

  pos = M * in.radius * pos;
  if (in.flag == u32(0)) {
    pos += in.p0;
  }
  if (in.flag == u32(1)) {
    pos += in.p1;
  }

  let worldPosition = u_object.modelMatrix * vec4f(pos, 1.0);
  out.position = u_object.projectionMatrix * u_object.viewMatrix * worldPosition;
  out.albedo = in.color;
  return out;
}

@fragment
fn fs_albedo(in: AlbedoVertexOutput) -> @location(0) vec4f {
  return vec4f(in.albedo, 1.0);
}
