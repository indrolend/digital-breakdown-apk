#pragma once

#include "../HumanVisual.hpp"
#include "TraversalCapabilities.hpp"

namespace gameplay {

// Presentation scale answers what architecture should look like beside a
// person. TraversalCapabilities remains the authority for whether the phone
// can move through or onto that architecture.
struct WorldScale {
    float phoneHeight = 0.16f;
    float humanHeight = PASS7_HUMAN_VISUAL_SPEC.totalHeight;
    float doorwayHeight = humanHeight * 1.9f;
    float storyHeight = humanHeight * 2.6f;
    float lowCoverHeight = humanHeight * 0.45f;
    float highCoverHeight = humanHeight * 0.80f;
    float lowTraversalHeight = 0.35f;
    float comfortableTraversalHeight = TRAVERSAL_CAPABILITIES.comfortableGroundJumpHeight();
    float narrowPassageHalfWidth = TRAVERSAL_CAPABILITIES.comfortableClearanceRadius;
};

inline constexpr WorldScale WORLD_SCALE{};

static_assert(WORLD_SCALE.humanHeight > WORLD_SCALE.phoneHeight * 7.0f,
    "architecture must remain human-relative rather than phone-sized");

} // namespace gameplay
