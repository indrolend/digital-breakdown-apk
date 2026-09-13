#pragma once

#include "Game.hpp"
#include "HumanVisual.hpp"

#include <cmath>
#include <limits>

struct DeveloperInspectionTarget {
    int targetIndex = -1;
    float screenDistance = std::numeric_limits<float>::max();
    float depth = 0.0f;

    bool valid() const noexcept {
        return targetIndex >= 0 && targetIndex < TARGET_COUNT;
    }
};

inline DeveloperInspectionTarget developerHumanAtReticle(
    const GameState& state,
    float aspect = 16.0f / 9.0f
) {
    DeveloperInspectionTarget result{};

    const Vec3 viewForward = normalized(
        state.camera.lookTarget - state.camera.pos
    );
    const auto cross = [](const Vec3& a, const Vec3& b) {
        return Vec3{
            a.y*b.z-a.z*b.y,
            a.z*b.x-a.x*b.z,
            a.x*b.y-a.y*b.x
        };
    };
    const Vec3 viewRight = normalized(cross(viewForward, {0,1,0}));
    const Vec3 viewUp = cross(viewRight, viewForward);

    const float tanHalf = std::tan(
        state.camera.verticalFovDegrees * 3.14159265358979323846f / 360.0f
    );

    for (int i = 0; i < TARGET_COUNT; ++i) {
        const TargetState& target = state.targets[i];

        if (!target.alive || target.slurpable)
            continue;

        const Vec3 world{
            target.pos.x,
            (PASS7_HUMAN_VISUAL_SPEC.totalHeight -
             PASS7_HUMAN_VISUAL_SPEC.headRadius) * target.scale,
            target.pos.z
        };

        const Vec3 delta = world - state.camera.pos;
        const float depth = dot3(delta, viewForward);

        if (depth <= 0.18f || depth > 16.0f)
            continue;

        const float nx =
            dot3(delta, viewRight) / (depth * tanHalf * aspect);
        const float ny =
            dot3(delta, viewUp) / (depth * tanHalf);

        if (std::abs(nx) > 1.04f || std::abs(ny) > 1.04f)
            continue;

        const float distance = nx * nx + ny * ny;

        if (distance < result.screenDistance) {
            result.targetIndex = i;
            result.screenDistance = distance;
            result.depth = depth;
        }
    }

    return result;
}
