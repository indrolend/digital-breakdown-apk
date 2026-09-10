#pragma once

#include <cmath>

namespace world {

struct RoomCoordinates {
    float depth = 42.0f;

    float wrapLocalZ(float worldZ) const noexcept {
        if (depth <= 0.0f) return worldZ;
        float wrapped = std::fmod(worldZ + depth * 0.5f, depth);
        if (wrapped < 0.0f) wrapped += depth;
        return wrapped - depth * 0.5f;
    }

    int tileIndex(float worldZ) const noexcept {
        if (depth <= 0.0f) return 0;
        return static_cast<int>(std::floor((worldZ + depth * 0.5f) / depth));
    }

    float tileOriginZ(int tile) const noexcept {
        return static_cast<float>(tile) * depth;
    }

    float canonicalZ(float worldZ, int targetTile) const noexcept {
        return wrapLocalZ(worldZ) + tileOriginZ(targetTile);
    }

    float nearestPeriodicZ(float worldZ, float referenceZ) const noexcept {
        if (depth <= 0.0f) return worldZ;
        const float local = wrapLocalZ(worldZ);
        const int centerTile = tileIndex(referenceZ);
        float best = local + tileOriginZ(centerTile);
        float bestDistance = std::abs(best - referenceZ);
        for (int offset : {-1, 1}) {
            const float candidate = local + tileOriginZ(centerTile + offset);
            const float distance = std::abs(candidate - referenceZ);
            if (distance < bestDistance) {
                best = candidate;
                bestDistance = distance;
            }
        }
        return best;
    }
};

} // namespace world
