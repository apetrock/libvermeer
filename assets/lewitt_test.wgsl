struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
	@location(4) tangent: vec3f,
	@location(5) bitangent: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) viewDirection: vec3<f32>,
	@location(4) tangent: vec3f,
	@location(5) bitangent: vec3f,
};

/**
 * A structure holding the value of our uniforms
 */

struct material_uniforms {
	modelMatrix:mat4x4f,
	color: vec4f
} // material_uniforms_group

struct scene_uniforms  {
	// We add transform matrices
	projectionMatrix: mat4x4f,
	viewMatrix: mat4x4f,
	cameraWorldPosition: vec3f,
	time: f32
} // scene_uniforms_group

/**
 * A structure holding the lighting settings
 */
struct LightingUniforms {
	directions: array<vec4f, 2>,
	colors: array<vec4f, 2>,
	hardness: f32,
	kd: f32,
	ks: f32,
}

@group(0) @binding(0) var<uniform> u_material: material_uniforms;
@group(0) @binding(1) var<uniform> u_scene: scene_uniforms;
@group(0) @binding(2) var<uniform> u_lighting: LightingUniforms;
@group(0) @binding(3) var baseColorTexture: texture_2d<f32>;
@group(0) @binding(4) var normalTexture: texture_2d<f32>;
@group(0) @binding(5) var textureSampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	let worldPosition = u_material.modelMatrix * vec4<f32>(in.position, 1.0);
	out.position = u_scene.projectionMatrix * u_scene.viewMatrix * worldPosition;
	out.tangent = (u_material.modelMatrix * vec4f(in.tangent, 0.0)).xyz;
	out.bitangent = (u_material.modelMatrix * vec4f(in.bitangent, 0.0)).xyz;
	out.normal = (u_material.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
	out.uv = in.uv;
	out.viewDirection = u_scene.cameraWorldPosition - worldPosition.xyz;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// Sample normal
	let normalMapStrength = 1.0; // could be a uniform
	let encodedN = textureSample(normalTexture, textureSampler, in.uv).rgb;
	let localN = encodedN * 2.0 - 1.0;
	// The TBN matrix converts directions from the local space to the world space
	let localToWorld = mat3x3f(
		normalize(in.tangent),
		normalize(in.bitangent),
		normalize(in.normal),
	);
	let worldN = localToWorld * localN;
	let N = normalize(mix(in.normal, worldN, normalMapStrength));

	let V = normalize(in.viewDirection);

	// Sample texture
	let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;
	let kd = u_lighting.kd;
	let ks = u_lighting.ks;
	let hardness = u_lighting.hardness;

	// Compute shading
	var color = vec3f(0.0);
	for (var i: i32 = 0; i < 2; i++) {
		let lightColor = u_lighting.colors[i].rgb;
		let L = normalize(u_lighting.directions[i].xyz);
		let R = reflect(-L, N); // equivalent to 2.0 * dot(N, L) * N - L

		let diffuse = max(0.0, dot(L, N)) * lightColor;

		// We clamp the dot product to 0 when it is negative
		let RoV = max(0.0, dot(R, V));
		let specular = pow(RoV, hardness);

		color += baseColor * kd * diffuse + ks * specular;
	}

	//color = N * 0.5 + 0.5;
	
	// Gamma-correction
	let corrected_color = pow(color, vec3f(2.2));
	return vec4f(corrected_color, u_material.color.a);
}
