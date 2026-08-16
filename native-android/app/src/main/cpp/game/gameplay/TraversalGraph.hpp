#pragma once

#include <array>
#include <algorithm>
#include <cmath>

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
    Unknown,
    Automatic,
    Comfortable,
    Engaged,
    Demanding,
    Expert
};

enum class TraversalRole : unsigned char {
    Required,
    Shortcut,
    Resource,
    CombatPerch
};

inline const char* traversalActionName(TraversalAction action){switch(action){case TraversalAction::Walk:return "WALK";case TraversalAction::Jump:return "JUMP";case TraversalAction::DoubleJump:return "DOUBLE_JUMP";case TraversalAction::Lunge:return "LUNGE";case TraversalAction::JumpLunge:return "JUMP_LUNGE";case TraversalAction::Drop:return "DROP";case TraversalAction::LedgeRecover:return "LEDGE_RECOVER";}return "UNKNOWN";}
inline const char* traversalDifficultyName(TraversalDifficulty difficulty){switch(difficulty){case TraversalDifficulty::Unknown:return "UNCALIBRATED";case TraversalDifficulty::Automatic:return "AUTOMATIC";case TraversalDifficulty::Comfortable:return "COMFORTABLE";case TraversalDifficulty::Engaged:return "ENGAGED";case TraversalDifficulty::Demanding:return "DEMANDING";case TraversalDifficulty::Expert:return "EXPERT";}return "UNCALIBRATED";}
inline const char* traversalRoleName(TraversalRole role){switch(role){case TraversalRole::Required:return "REQUIRED";case TraversalRole::Shortcut:return "SHORTCUT";case TraversalRole::Resource:return "RESOURCE";case TraversalRole::CombatPerch:return "COMBAT_PERCH";}return "REQUIRED";}

struct TraversalSurface {
    Vec3 center{};
    Vec3 halfSize{};
    bool required = true;
};

struct TraversalEdge {
    int from = 0;
    int to = 0;
    TraversalAction action = TraversalAction::Walk;
    TraversalDifficulty difficulty = TraversalDifficulty::Unknown;
    TraversalRole role = TraversalRole::Required;
};

constexpr bool isRequired(const TraversalEdge& edge){return edge.role==TraversalRole::Required;}

struct TraversalEdgeMeasurement {
    float gap=0.0f;
    float heightDelta=0.0f;
    float landingWidth=0.0f;
    float approachTolerance=0.0f;
    TraversalAction movement=TraversalAction::Walk;
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

inline TraversalEdgeMeasurement measureTraversalEdge(const TraversalGraph& graph,const TraversalEdge& edge,float clearanceRadius){
    if(edge.from<0||edge.from>=graph.surfaceCount||edge.to<0||edge.to>=graph.surfaceCount)return {};
    const auto& from=graph.surfaces[edge.from];const auto& to=graph.surfaces[edge.to];
    const float dx=to.center.x-from.center.x,dz=to.center.z-from.center.z,distance=std::sqrt(dx*dx+dz*dz);
    const float ux=distance>0.0001f?std::abs(dx/distance):0.0f,uz=distance>0.0001f?std::abs(dz/distance):0.0f;
    const float sourceReach=from.halfSize.x*ux+from.halfSize.z*uz,targetReach=to.halfSize.x*ux+to.halfSize.z*uz;
    TraversalEdgeMeasurement result;result.gap=std::max(0.0f,distance-sourceReach-targetReach);result.heightDelta=to.center.y-from.center.y;result.landingWidth=std::min(to.halfSize.x,to.halfSize.z)*2.0f;result.approachTolerance=std::max(0.0f,result.landingWidth-clearanceRadius*2.0f);result.movement=edge.action;return result;
}

inline TraversalDifficulty resolvedTraversalDifficulty(const TraversalGraph& graph,const TraversalEdge& edge,float clearanceRadius){
    if(edge.difficulty!=TraversalDifficulty::Unknown)return edge.difficulty;
    const auto measured=measureTraversalEdge(graph,edge,clearanceRadius);
    if(edge.action==TraversalAction::Walk&&measured.gap<=0.001f&&std::abs(measured.heightDelta)<=0.001f)return TraversalDifficulty::Automatic;
    return TraversalDifficulty::Unknown;
}

} // namespace gameplay
