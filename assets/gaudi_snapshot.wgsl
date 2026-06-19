struct VertexInput {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) color: vec3f,
};

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) color: vec3f,
  @location(1) normal: vec3f,
  @location(2) viewDirection: vec3f,
};

struct uniforms {
  projectionMatrix: mat4x4f,
  viewMatrix: mat4x4f,
  modelMatrix: mat4x4f,
  color: vec4f,
  cameraWorldPosition: vec3f,
  time: f32,
}

struct LightingUniforms {
  directions: array<vec4f, 2>,
  colors: array<vec4f, 2>,
  hardness: f32,
  kd: f32,
  ks: f32,
}

@group(0) @binding(0) var<uniform> u_object: uniforms;
@group(0) @binding(1) var<uniform> u_lighting: LightingUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
  var out: VertexOutput;
  let worldPosition = u_object.modelMatrix * vec4f(in.position, 1.0);
  out.position = u_object.projectionMatrix * u_object.viewMatrix * worldPosition;
  out.normal = normalize((u_object.modelMatrix * vec4f(in.normal, 0.0)).xyz);
  out.color = in.color;
  out.viewDirection = u_object.cameraWorldPosition - worldPosition.xyz;
  return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let N = normalize(in.normal);
  let V = normalize(in.viewDirection);
  let kd = u_lighting.kd;
  let ks = u_lighting.ks;
  let hardness = u_lighting.hardness;

  var color = 0.12 * in.color;
  for (var i: i32 = 0; i < 2; i++) {
    let lightColor = u_lighting.colors[i].rgb;
    let L = normalize(u_lighting.directions[i].xyz);
    let R = reflect(-L, N);
    let diffuse = max(0.0, dot(L, N)) * lightColor;
    let specular = pow(max(0.0, dot(R, V)), hardness);
    color += in.color * kd * diffuse + ks * specular;
  }

  return vec4f(pow(color, vec3f(1.0 / 2.2)), u_object.color.a);
}
