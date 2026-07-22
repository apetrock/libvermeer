#pragma once

// Lewitt — Sol LeWitt: systems, instructions, and modular structure.
// This layer wraps WebGPU state and lifetime: buffers, bindings, shaders,
// render targets, passes, and frame/session primitives. Unwrapping to wgpu::*
// is intentional here; this is the API boundary.
