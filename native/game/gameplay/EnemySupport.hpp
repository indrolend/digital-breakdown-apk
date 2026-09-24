#pragma once

#include <algorithm>

#include "Math.hpp"

namespace gameplay {

enum class EnemySupportState : unsigned char {
    None,
    Left,
    Right,
    Double
};

struct EnemySupportRegion {
    EnemySupportState state = EnemySupportState::None;
    Vec3 closestPoint{};
    Vec3 error{};
    float distance = 0.0f;
    float margin = 0.0f;
    float totalLoad = 0.0f;
};

inline EnemySupportRegion enemySupportRegion(
    const Vec3& leftFoot,
    float leftLoad,
    float leftContact,
    const Vec3& rightFoot,
    float rightLoad,
    float rightContact,
    const Vec3& projectedPoint)
{
    constexpr float supportThreshold = 0.08f;
    constexpr float singleFootMargin = 0.19f;
    constexpr float doubleSupportMargin = 0.20f;

    const float leftAuthority = std::max(0.0f, leftLoad * leftContact);
    const float rightAuthority = std::max(0.0f, rightLoad * rightContact);
    const bool leftSupported = leftAuthority > supportThreshold;
    const bool rightSupported = rightAuthority > supportThreshold;

    EnemySupportRegion support{};
    support.totalLoad = leftAuthority + rightAuthority;
    if (!leftSupported && !rightSupported) {
        support.closestPoint = projectedPoint;
        return support;
    }

    if (leftSupported && rightSupported) {
        support.state = EnemySupportState::Double;
        support.margin = doubleSupportMargin;
        const Vec3 span{rightFoot.x - leftFoot.x, 0.0f, rightFoot.z - leftFoot.z};
        const float spanLengthSq = lengthSq(span);
        const Vec3 fromLeft{projectedPoint.x - leftFoot.x, 0.0f, projectedPoint.z - leftFoot.z};
        const float along = spanLengthSq > 0.000001f
            ? std::max(0.0f, std::min(1.0f, dot3(fromLeft, span) / spanLengthSq))
            : 0.5f;
        support.closestPoint = leftFoot + span * along;
    } else {
        support.state = leftSupported ? EnemySupportState::Left : EnemySupportState::Right;
        support.margin = singleFootMargin;
        support.closestPoint = leftSupported ? leftFoot : rightFoot;
    }

    support.error = {
        projectedPoint.x - support.closestPoint.x,
        0.0f,
        projectedPoint.z - support.closestPoint.z
    };
    support.distance = horizontalLength(support.error);
    return support;
}

inline Vec3 enemyCapturePoint(
    const Vec3& projectedCenterOfMass,
    const Vec3& bodyVelocity,
    float centerOfMassHeight)
{
    // Low-fidelity inverted-pendulum prediction: enough to decide where the
    // support must move, without pretending to be a full ZMP/MPC solver.
    constexpr float gravity = 9.81f;
    const float height = std::max(0.10f, centerOfMassHeight);
    const float horizon = std::max(0.15f, std::min(0.35f, std::sqrt(height / gravity)));
    return projectedCenterOfMass
        + Vec3{bodyVelocity.x, 0.0f, bodyVelocity.z} * horizon;
}

} // namespace gameplay
