#pragma once

// Vermeer — rendering library named after Johannes Vermeer, whose work is
// associated with light, camera obscura speculation, and careful optical
// construction. This header frames the three internal layers:
//
//   lewitt/    — low-level WebGPU wrappers (buffers, bindings, shaders, targets)
//   mondrian/  — compositional draw bundles, schemas, and record systems
//   lombardi/  — render graph nodes, scheduling, and pipeline wiring
//
// Design guardrail: avoid OpenSceneGraph-style fat per-object GPU state.
// Pipeline and target state live in schemas and graph resources; drawables
// carry sparse component bundles that Mondrian systems compose at record time.

#include "lewitt/lewitt.hpp"
#include "lombardi/lombardi.hpp"
#include "mondrian/mondrian.hpp"
