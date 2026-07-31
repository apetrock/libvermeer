const KSIZE: u32 = 32u;
const PSI: f32 = 1.533751168755204288118041;
const GPHI: f32 = 1.618033988749895;
const PHI: f32 = 1.618033988749895;

struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

struct SsaoParams {
  projectionMatrix: mat4x4f,
  params: vec4f,
}

struct quat {
  s: f32,
  v: vec3f,
}

@group(0) @binding(0) var t_position: texture_2d<f32>;
@group(0) @binding(1) var t_normal: texture_2d<f32>;
@group(0) @binding(2) var s_gbuffer: sampler;
@group(0) @binding(3) var<uniform> u_ssao: SsaoParams;

fn rotate(q: quat, p: vec3f) -> vec3f {
  return p + 2.0 * cross(q.v, cross(q.v, p) + q.s * p);
}

fn getSphere(i: i32) -> vec3f {
  let N = f32(KSIZE);
  let t = (f32(i) + 0.5) / N;
  let sqti = sqrt(t);
  let sqt1i = sqrt(1.0 - t);
  let thet = 2.0 * 3.14159265 * N * t;
  let NtP = (N * t) / PSI;
  let t0 = NtP - floor(NtP);
  let x0 = sqti * sin(thet / PHI);
  let x1 = sqti * cos(thet / PHI);
  let y0 = sqt1i * sin(thet / PSI);
  let y1 = sqt1i * cos(thet / PSI);
  let q = quat(y0, vec3f(y1, x0, x1));
  let p0 = pow(t0, 2.5 / 3.0) * vec3f(1.0, 0.0, 0.0);
  return rotate(q, p0);
}

fn gold_noise(xy: vec2f, seed: f32) -> f32 {
  return fract(tan(distance(xy * GPHI, xy) * seed) * xy.x);
}

fn rvec(t: vec2f) -> vec3f {
  return vec3f(
    gold_noise(t, 0.5 * GPHI),
    gold_noise(t, 1.0 * GPHI),
    gold_noise(t, 1.5 * GPHI),
  );
}

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

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let fragPos = textureSample(t_position, s_gbuffer, in.uv).xyz;
  let normal = normalize(textureSample(t_normal, s_gbuffer, in.uv).rgb);
  let randomVec = 0.5 - normalize(rvec(500.0 * in.uv));
  let tangent = normalize(randomVec - normal * dot(randomVec, normal));

  let bitangent = cross(normal, tangent);
  let TBN = mat3x3f(tangent, bitangent, normal);

  // params: (radius_scale, bias, ao_power, fade_start)
  // Radius scales with view depth so kernel stays roughly screen-stable when dollying out.
  let viewDist = max(abs(fragPos.z), 1.0e-3);
  let radius = u_ssao.params.x * viewDist;
  let bias = u_ssao.params.y;
  let ao_power = max(u_ssao.params.z, 1.0e-3);
  let fade_start = max(u_ssao.params.w, 1.0e-3);

  var occlusion = 0.0;
  for (var i = 0; i < i32(KSIZE); i++) {
    var sphere = getSphere(i);
    sphere.z = abs(sphere.z);
    var samplePos = TBN * sphere;
    samplePos = fragPos + samplePos * radius;

    var offset = u_ssao.projectionMatrix * vec4f(samplePos, 1.0);
    offset = offset / offset.w;
    offset = vec4f(offset.x * 0.5 + 0.5, 1.0 - (offset.y * 0.5 + 0.5), offset.z, 1.0);

    let sampleDepth = textureSample(t_position, s_gbuffer, offset.xy).z;
    let rangeCheck =
      smoothstep(0.0, 1.0, radius / max(abs(fragPos.z - sampleDepth), 1.0e-5));
    occlusion +=
      select(0.0, 1.0, sampleDepth >= samplePos.z + bias) * rangeCheck;
  }

  occlusion = 1.0 - (occlusion / f32(KSIZE));
  var ao = pow(occlusion, ao_power);
  // Fade AO toward unoccluded as we pull away from the scene.
  let fade = smoothstep(fade_start, fade_start * 3.0, viewDist);
  ao = mix(ao, 1.0, fade);
  return vec4f(ao, ao, ao, 1.0);
}
