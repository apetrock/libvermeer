struct VertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) flag: u32,
  @location(3) radius: f32,
  @location(4) p0: vec3f,
  @location(5) p1: vec3f,
  @location(6) color: vec3f,
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

  var pos = in.position;
  let dp = in.p1 - in.p0;
  let m0 = normalize(dp);
  let up = vec3f(0.0, 1.0, 0.0);
  let ref_axis = select(up, vec3f(1.0, 0.0, 0.0), abs(dot(m0, up)) > 0.99);
  let m1 = normalize(cross(m0, ref_axis));
  let m2 = normalize(cross(m0, m1));
  let M = mat3x3f(m1, m2, m0);

  pos = M * in.radius * pos;
  let N = M * in.normal;

  if (in.flag == u32(0)) {
    pos += in.p0;
  }
  if (in.flag == u32(1)) {
    pos += in.p1;
  }

  let worldPosition = u_object.modelMatrix * vec4f(pos, 1.0);
  let viewPosition = (u_object.viewMatrix * worldPosition).xyz;
  let viewNormal =
    normalize((u_object.viewMatrix * u_object.modelMatrix * vec4f(N, 0.0)).xyz);
  out.position = u_object.projectionMatrix * u_object.viewMatrix * worldPosition;
  out.viewPosition = viewPosition;
  out.viewNormal = viewNormal;
  return out;
}

struct GBufferFragmentOutput {
  @location(0) position: vec4f,
  @location(1) normal: vec4f,
};

@fragment
fn fs_gbuffer(in: GBufferVertexOutput) -> GBufferFragmentOutput {
  var out: GBufferFragmentOutput;
  out.position = vec4f(in.viewPosition, 1.0);
  out.normal = vec4f(normalize(in.viewNormal), 1.0);
  return out;
}
