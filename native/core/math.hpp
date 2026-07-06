#pragma once

#include <cmath>

namespace db {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Vec3 makeVec3(float x, float y, float z) {
    return Vec3{x, y, z};
}

inline Vec3 add(Vec3 a, Vec3 b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 sub(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 mul(Vec3 a, float s) {
    return Vec3{a.x * s, a.y * s, a.z * s};
}

inline float lengthSqXZ(Vec3 a) {
    return a.x * a.x + a.z * a.z;
}

inline float lengthXZ(Vec3 a) {
    return std::sqrt(lengthSqXZ(a));
}

inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline int clampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float dampFactor(float rate, float dt) {
    float f = 1.0f - rate * dt;
    return f < 0.0f ? 0.0f : f;
}

inline float shortestAngle(float from, float to) {
    constexpr float PI = 3.14159265358979323846f;
    float diff = std::fmod(to - from + PI, PI * 2.0f);
    if (diff < 0.0f) diff += PI * 2.0f;
    return diff - PI;
}

} // namespace db
