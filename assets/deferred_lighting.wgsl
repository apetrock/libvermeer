struct VertexOutput {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};

@group(0) @binding(0) var t_position: texture_2d<f32>;
@group(0) @binding(1) var t_normal: texture_2d<f32>;
@group(0) @binding(2) var t_albedo: texture_2d<f32>;
@group(0) @binding(3) var t_ssao: texture_2d<f32>;
@group(0) @binding(4) var s_gbuffer: sampler;

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
  let fragPos = textureSample(t_position, s_gbuffer, in.uv).rgb;
  let normal = normalize(textureSample(t_normal, s_gbuffer, in.uv).rgb);
  let diffuse = textureSample(t_albedo, s_gbuffer, in.uv).rgb;
  let ambientOcclusion = textureSample(t_ssao, s_gbuffer, in.uv).r;

  let specularStrength = 0.5;
  let ambient = diffuse;
  var lighting = vec3f(0.0);
  let viewDir = normalize(-fragPos);

  let lcol = 5.0 * vec3f(1.0, 1.0, 1.0);
  let lpos = normalize(vec3f(1.0, 1.0, 1.0));
  let llin = 0.05;
  let lquad = 0.8;

  let lightDir = normalize(lpos - fragPos);
  var diffuseTerm = max(dot(normal, lightDir), 0.0) * diffuse * lcol;

  let halfwayDir = normalize(lightDir + viewDir);
  let spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);
  let specular = lcol * spec * specularStrength;

  let distance = length(lpos - fragPos);
  let attenuation = 1.0 / (1.0 + llin * distance + lquad * distance * distance);
  diffuseTerm *= attenuation;
  let specularTerm = specular * attenuation;
  lighting += diffuseTerm + specularTerm;
  //return vec4f(ambientOcclusion);
  return ambientOcclusion * vec4f(lighting + 0.01 * ambient, 1.0);
}
