#pragma once

#include "FacetedRock.hpp"
#include "SlopeSupport.hpp"

#include <algorithm>

enum class RoomColliderKind : unsigned char { Generic, TreeTrunk, RockAuthoritySlot };

struct RoomCollider {
    float minX = 0.0f;
    float maxX = 0.0f;
    float minZ = 0.0f;
    float maxZ = 0.0f;
    float bottomY = 0.0f;
    float topY = 0.0f;
    float width = 0.0f;
    float depth = 0.0f;
    float height = 0.0f;
    Vec3 center;
    RoomColliderKind kind = RoomColliderKind::Generic;
    float climbTopY = 0.0f;
};

enum class SupportSource : unsigned char { Ground, Collider, Slope, RockFacet };

struct PlayerSupportSample {
    float height = 0.08f;
    Vec3 normal{0.0f, 1.0f, 0.0f};
    SupportClassification classification = SupportClassification::Ordinary;
    SupportSource source = SupportSource::Ground;
    int sourceIndex = -1;
};

struct WorldContactView {
    const RoomCollider* colliders = nullptr;
    int colliderCount = 0;
    const SlopeSupport* slopes = nullptr;
    int slopeCount = 0;
    const faceted_rock::Support* rocks = nullptr;
    int rockCount = 0;
};

inline PlayerSupportSample queryPlayerSupport(const WorldContactView& world,
                                              float x, float localZ,
                                              float supportRadius,
                                              float bodyRadius,
                                              float groundHeight,
                                              float ceilingLimit) {
    PlayerSupportSample result{};
    result.height = groundHeight;
    const auto accept = [&](float height, const Vec3& normal,
                            SupportClassification classification,
                            SupportSource source, int sourceIndex) {
        if (height <= result.height + 0.0001f) return;
        result.height = height;
        result.normal = normal;
        result.classification = classification;
        result.source = source;
        result.sourceIndex = sourceIndex;
    };

    for (int i = 0; i < world.colliderCount; ++i) {
        const RoomCollider& collider = world.colliders[i];
        if (collider.kind == RoomColliderKind::RockAuthoritySlot) continue;
        if (x > collider.minX - supportRadius && x < collider.maxX + supportRadius &&
            localZ > collider.minZ - supportRadius && localZ < collider.maxZ + supportRadius) {
            accept(collider.topY + groundHeight, {0.0f, 1.0f, 0.0f},
                   SupportClassification::Ordinary, SupportSource::Collider, i);
        }
    }
    for (int i = 0; i < world.slopeCount; ++i) {
        const auto sample = sampleSlopeSupport(world.slopes[i], x, localZ);
        if (!sample.inside || sample.classification == SupportClassification::Steep) continue;
        accept(sample.height + groundHeight, sample.normal, sample.classification,
               SupportSource::Slope, i);
    }
    for (int i = 0; i < world.rockCount; ++i) {
        const auto sample = faceted_rock::sampleSupportFootprint(world.rocks[i], x, localZ, bodyRadius);
        if (!sample.inside) continue;
        accept(sample.height + groundHeight, sample.normal,
               SupportClassification::TraversableSlope, SupportSource::RockFacet, i);
    }
    result.height = std::min(result.height, ceilingLimit);
    return result;
}
