#pragma once

// Mondrian — Piet Mondrian: abstract composition from simple geometric relations.
// This layer composes Lewitt wrappers into draw bundles, schemas (archetypes),
// pipeline state, and record systems. Nodes should not hand-roll encoder calls;
// they invoke Mondrian systems instead.

#include "mondrian/draw_bundles.hpp"
#include "mondrian/draw_schema.hpp"
#include "mondrian/render_contract.hpp"
