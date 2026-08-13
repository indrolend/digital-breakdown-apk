#pragma once

namespace gameplay {

// Canonical movement values consumed by both the simulation and deterministic
// room validation. Keep generation conservative: maximum values describe the
// physical envelope, while comfortable values leave input/timing margin.
struct TraversalCapabilities {
    float gravity = 14.0f;
    float groundJumpSpeed = 4.5f;
    float airJumpSpeed = 4.25f;
    float airLungeDistance = 5.40f;
    float walkClearanceRadius = 0.40f;
    float comfortableClearanceRadius = 0.55f;

    constexpr float maximumGroundJumpHeight() const {
        return groundJumpSpeed * groundJumpSpeed / (2.0f * gravity);
    }
    constexpr float maximumDoubleJumpAddedHeight() const {
        return airJumpSpeed * airJumpSpeed / (2.0f * gravity);
    }
    constexpr float comfortableGroundJumpHeight() const {
        return maximumGroundJumpHeight() * 0.72f;
    }
    constexpr float comfortableDoubleJumpAddedHeight() const {
        return maximumDoubleJumpAddedHeight() * 0.72f;
    }
    constexpr float comfortableAirLungeDistance() const {
        return airLungeDistance * 0.72f;
    }
};

inline constexpr TraversalCapabilities TRAVERSAL_CAPABILITIES{};

} // namespace gameplay
