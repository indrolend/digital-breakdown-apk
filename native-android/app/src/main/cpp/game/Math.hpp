#pragma once

#include <algorithm>
#include <cmath>

constexpr float DB_PI = 3.14159265358979323846f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) {}

    Vec3 operator+(const Vec3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vec3 operator-(const Vec3& other) const { return {x - other.x, y - other.y, z - other.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    Vec3& operator+=(const Vec3& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& other) {
        x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    Vec3& operator*=(float s) {
        x *= s; y *= s; z *= s;
        return *this;
    }
};

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline float lengthSq(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float length(const Vec3& v) {
    return std::sqrt(lengthSq(v));
}

inline Vec3 normalized(const Vec3& v) {
    const float len = length(v);
    if (len <= 0.00001f) return {0.0f, 0.0f, 0.0f};
    return {v.x / len, v.y / len, v.z / len};
}

inline float horizontalLength(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

inline void limitHorizontal(Vec3& v, float maxSpeed) {
    const float speed = horizontalLength(v);
    if (speed <= maxSpeed || speed <= 0.00001f) return;
    const float s = maxSpeed / speed;
    v.x *= s;
    v.z *= s;
}

inline float approachAngle(float current, float target, float rate) {
    const float diff = std::atan2(std::sin(target - current), std::cos(target - current));
    return current + diff * clampf(rate, 0.0f, 1.0f);
}
