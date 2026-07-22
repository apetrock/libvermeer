#pragma once

// Lombardi — Mark Lombardi: graph-like drawings mapping relationships and flows.
// This layer schedules render graph nodes, allocates graph-owned targets, and
// wires resource dependencies. Lombardi invokes Mondrian record systems; it
// does not set pipelines or bind groups directly.

#include "lombardi/gbuffer_graph_helpers.hpp"
#include "lombardi/render_graph.hpp"
