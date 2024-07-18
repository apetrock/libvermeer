struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
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

	modelMatrix:mat4x4f,
	color: vec4f,

	cameraWorldPosition: vec3f,
	time: f32
	//implicit padding
} // material_uniforms_group


/**
 * A structure holding the lighting settings
 */
struct LightingUniforms {
	directions: array<vec4f, 2>,
	colors: array<vec4f, 2>,
	hardness: f32,
	kd: f32,
	ks: f32
}

@group(0) @binding(0) var<uniform> u_object: uniforms;
@group(0) @binding(1) var<uniform> u_lighting: LightingUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	let worldPosition = u_object.modelMatrix * vec4<f32>(in.position, 1.0);
	out.position = u_object.projectionMatrix * u_object.viewMatrix * worldPosition;
	out.normal = (u_object.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = vec3f(1.0, 0.0, 0.0);
	out.viewDirection = u_object.cameraWorldPosition - worldPosition.xyz;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let N = in.normal;
	let V = normalize(in.viewDirection);

	// Sample texture
	let baseColor = in.color;
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
	return vec4f(corrected_color, u_object.color.a);
	//return vec4f(1.0, 0.0, 0.0, 1.0);
	
}
