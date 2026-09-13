#pragma once
#include <array>
#include "Math.hpp"

namespace shopping_cart_geometry {
enum class PartKind : unsigned char { Metal, Basket, Handle, Wheel };
struct Part { Vec3 offset; Vec3 size; PartKind kind; int wheel=-1; };
inline const std::array<Part,11> Parts{{
    {{0,0.42f,0.02f},{0.78f,0.07f,1.08f},PartKind::Metal,-1},
    {{0,0.82f,-0.10f},{0.82f,0.62f,0.82f},PartKind::Basket,-1},
    {{-0.40f,0.72f,0.42f},{0.05f,0.82f,0.05f},PartKind::Metal,-1},{{0.40f,0.72f,0.42f},{0.05f,0.82f,0.05f},PartKind::Metal,-1},
    {{0,1.13f,0.50f},{1.05f,0.06f,0.06f},PartKind::Handle,-1},
    {{-0.43f,0.12f,-0.62f},{0.10f,0.24f,0.24f},PartKind::Wheel,0},{{0.43f,0.12f,-0.62f},{0.10f,0.24f,0.24f},PartKind::Wheel,1},
    {{-0.43f,0.12f,0.62f},{0.10f,0.24f,0.24f},PartKind::Wheel,2},{{0.43f,0.12f,0.62f},{0.10f,0.24f,0.24f},PartKind::Wheel,3},
    {{-0.36f,0.68f,-0.53f},{0.05f,0.70f,0.05f},PartKind::Metal,-1},{{0.36f,0.68f,-0.53f},{0.05f,0.70f,0.05f},PartKind::Metal,-1}
}};
}
