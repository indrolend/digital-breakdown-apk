#pragma once

#include <array>

#include "Math.hpp"

namespace gameplay {

enum class TraversalAction : unsigned char {
    Walk,
    Jump,
    DoubleJump,
    Lunge,
    JumpLunge,
    Drop,
    LedgeRecover
};

enum class TraversalDifficulty : unsigned char {
    Automatic,
    Comfortable,
    Engaged,
    Demanding,
    Expert
};

struct TraversalSurface {
    Vec3 center{};
    Vec3 halfSize{};
    bool required = true;
};

struct TraversalEdge {
    int from = 0;
    int to = 0;
    TraversalAction action = TraversalAction::Walk;
    TraversalDifficulty difficulty = TraversalDifficulty::Automatic;
    bool required = true;
};

struct TraversalGraph {
    static constexpr int SurfaceCapacity = 8;
    static constexpr int EdgeCapacity = 10;
    std::array<TraversalSurface, SurfaceCapacity> surfaces{};
    std::array<TraversalEdge, EdgeCapacity> edges{};
    int surfaceCount = 0;
    int edgeCount = 0;
};

constexpr bool validTraversalGraphTopology(const TraversalGraph& graph) {
    if (graph.surfaceCount < 2 || graph.surfaceCount > TraversalGraph::SurfaceCapacity ||
        graph.edgeCount < 1 || graph.edgeCount > TraversalGraph::EdgeCapacity) return false;
    for (int i = 0; i < graph.edgeCount; ++i) {
        const TraversalEdge& edge = graph.edges[i];
        if (edge.from < 0 || edge.from >= graph.surfaceCount ||
            edge.to < 0 || edge.to >= graph.surfaceCount || edge.from == edge.to) return false;
    }
    return true;
}

} // namespace gameplay
