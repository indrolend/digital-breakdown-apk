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

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

inline Quat quatAxisAngle(const Vec3& axis, float angle) {
    const float half = angle * 0.5f;
    const float s = std::sin(half);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
}

inline Quat operator*(const Quat& a, const Quat& b) {
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}

inline Quat quatNormalized(const Quat& q) {
    const float n = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (n <= 0.00001f) return {};
    return {q.x/n, q.y/n, q.z/n, q.w/n};
}

inline Quat quatSlerp(Quat a, Quat b, float t) {
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (dot < 0.0f) { b = {-b.x,-b.y,-b.z,-b.w}; dot = -dot; }
    if (dot > 0.9995f) return quatNormalized({a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, a.w+(b.w-a.w)*t});
    const float theta = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    const float sinTheta = std::sin(theta);
    const float wa = std::sin((1.0f-t)*theta)/sinTheta;
    const float wb = std::sin(t*theta)/sinTheta;
    return {a.x*wa+b.x*wb, a.y*wa+b.y*wb, a.z*wa+b.z*wb, a.w*wa+b.w*wb};
}

inline Vec3 rotate(const Quat& q, const Vec3& v) {
    const Vec3 u{q.x,q.y,q.z};
    const Vec3 uv{u.y*v.z-u.z*v.y, u.z*v.x-u.x*v.z, u.x*v.y-u.y*v.x};
    const Vec3 uuv{u.y*uv.z-u.z*uv.y, u.z*uv.x-u.x*uv.z, u.x*uv.y-u.y*uv.x};
    return v + uv*(2.0f*q.w) + uuv*2.0f;
}

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
