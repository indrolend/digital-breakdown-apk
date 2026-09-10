#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

struct HumanModelVertex {
  float position[3];
  std::uint16_t bones[4];
  float weights[4];
};
struct HumanModelBone {
  int parent = -1;
  unsigned char flags = 0, kind = 0;
  signed char side = 0;
  float inverse[16]{};
};
struct HumanModelPose {
  float position[3];
  float quaternion[4];
  float scale[3];
};

struct HumanModelData {
  std::vector<HumanModelVertex> vertices;
  std::vector<HumanModelBone> bones;
  std::vector<HumanModelPose> poses;
  unsigned int frameCount = 0;
  float minY = 0, unitScale = 1,
        color[4]{0.60382736f, 0.60382736f, 0.60382736f, 1};
  float bindMatrix[16]{}, bindMatrixInverse[16]{}, rootParentMatrix[16]{};

  bool load(const std::string &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
      return false;
    const auto end = input.tellg();
    if (end < 236)
      return false;
    input.seekg(0);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(end));
    if (!input.read(reinterpret_cast<char *>(bytes.data()), end))
      return false;
    std::size_t at = 0;
    auto take = [&](void *output, std::size_t count) {
      if (at + count > bytes.size())
        return false;
      std::memcpy(output, bytes.data() + at, count);
      at += count;
      return true;
    };
    char magic[4];
    std::uint32_t vertexCount = 0, boneCount = 0, reserved = 0;
    if (!take(magic, 4) || std::memcmp(magic, "DBH1", 4) != 0 ||
        !take(&vertexCount, 4) || !take(&boneCount, 4) ||
        !take(&frameCount, 4) || !take(&reserved, 4))
      return false;
    if (vertexCount > 100000u || boneCount > 256u || frameCount == 0 ||
        frameCount > 240u)
      return false;
    if (!take(&minY, 4) || !take(&unitScale, 4) || !take(color, 16) ||
        !take(bindMatrix, 64) || !take(bindMatrixInverse, 64) ||
        !take(rootParentMatrix, 64))
      return false;
    vertices.resize(vertexCount);
    for (auto &v : vertices)
      if (!take(v.position, 12) || !take(v.bones, 8) || !take(v.weights, 16))
        return false;
    bones.resize(boneCount);
    for (auto &b : bones) {
      unsigned char packed[4];
      if (!take(&b.parent, 4) || !take(packed, 4) || !take(b.inverse, 64))
        return false;
      b.flags = packed[0];
      b.kind = packed[1];
      b.side = static_cast<signed char>(packed[2]);
      if (b.parent >= static_cast<int>(boneCount))
        return false;
    }
    poses.resize(static_cast<std::size_t>(frameCount) * boneCount);
    for (auto &p : poses)
      if (!take(p.position, 12) || !take(p.quaternion, 16) ||
          !take(p.scale, 12))
        return false;
    if (at != bytes.size())
      return false;
    for (const auto &v : vertices)
      for (int k = 0; k < 4; ++k)
        if (v.bones[k] >= boneCount && v.weights[k] > 0)
          return false;
    return valid();
  }
  bool valid() const {
    return !vertices.empty() && !bones.empty() &&
           poses.size() == static_cast<std::size_t>(frameCount) * bones.size();
  }

  void skin(float animationTime, float attackTimer, int attackVariant,
            std::vector<float> &output) const {
    if (!valid()) {
      output.clear();
      return;
    }
    const std::size_t boneCount = bones.size();
    const float frame = std::fmod(std::max(0.0f, animationTime) *
                                      static_cast<float>(frameCount),
                                  static_cast<float>(frameCount));
    const unsigned int f0 = static_cast<unsigned int>(frame) % frameCount,
                       f1 = (f0 + 1) % frameCount;
    const float blend = frame - static_cast<float>(f0);
    std::vector<float> worlds(boneCount * 16u), skinMatrices(boneCount * 16u);
    for (std::size_t i = 0; i < boneCount; ++i) {
      const HumanModelPose &a =
          poses[static_cast<std::size_t>(f0) * boneCount + i];
      const HumanModelPose &b =
          poses[static_cast<std::size_t>(f1) * boneCount + i];
      float p[3], s[3], q[4];
      for (int k = 0; k < 3; ++k) {
        p[k] = a.position[k] + (b.position[k] - a.position[k]) * blend;
        s[k] = a.scale[k] + (b.scale[k] - a.scale[k]) * blend;
      }
      slerp(a.quaternion, b.quaternion, blend, q);
      if (attackTimer > 0.0f)
        applyAttack(bones[i], attackTimer, attackVariant, q);
      float local[16];
      compose(p, q, s, local);
      float *world = worlds.data() + i * 16u;
      if (bones[i].parent >= 0)
        multiply(world,
                 worlds.data() +
                     static_cast<std::size_t>(bones[i].parent) * 16u,
                 local);
      else
        multiply(world, rootParentMatrix, local);
      multiply(skinMatrices.data() + i * 16u, world, bones[i].inverse);
    }
    output.resize(vertices.size() * 3u);
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      float bound[3];
      point(bindMatrix, vertices[i].position, bound);
      float sum[3]{0, 0, 0};
      for (int k = 0; k < 4; ++k) {
        const float w = vertices[i].weights[k];
        if (w <= 0)
          continue;
        float transformed[3];
        point(skinMatrices.data() +
                  static_cast<std::size_t>(vertices[i].bones[k]) * 16u,
              bound, transformed);
        for (int axis = 0; axis < 3; ++axis)
          sum[axis] += transformed[axis] * w;
      }
      output[i * 3] = sum[0] * unitScale;
      output[i * 3 + 1] = (sum[1] - minY) * unitScale;
      output[i * 3 + 2] = sum[2] * unitScale;
    }
  }

private:
  static void multiply(float *out, const float *a, const float *b) {
    float v[16]{};
    for (int c = 0; c < 4; ++c)
      for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 4; ++k)
          v[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    std::memcpy(out, v, sizeof(v));
  }
  static void point(const float *m, const float *p, float *out) {
    out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
  }
  static void compose(const float *p, const float *q, const float *s,
                      float *m) {
    const float x = q[0], y = q[1], z = q[2], w = q[3], x2 = x + x, y2 = y + y,
                z2 = z + z, xx = x * x2, xy = x * y2, xz = x * z2, yy = y * y2,
                yz = y * z2, zz = z * z2, wx = w * x2, wy = w * y2, wz = w * z2;
    m[0] = (1 - (yy + zz)) * s[0];
    m[1] = (xy + wz) * s[0];
    m[2] = (xz - wy) * s[0];
    m[3] = 0;
    m[4] = (xy - wz) * s[1];
    m[5] = (1 - (xx + zz)) * s[1];
    m[6] = (yz + wx) * s[1];
    m[7] = 0;
    m[8] = (xz + wy) * s[2];
    m[9] = (yz - wx) * s[2];
    m[10] = (1 - (xx + yy)) * s[2];
    m[11] = 0;
    m[12] = p[0];
    m[13] = p[1];
    m[14] = p[2];
    m[15] = 1;
  }
  static void slerp(const float *a, const float *b, float t, float *q) {
    float bx = b[0], by = b[1], bz = b[2], bw = b[3],
          dot = a[0] * bx + a[1] * by + a[2] * bz + a[3] * bw;
    if (dot < 0) {
      dot = -dot;
      bx = -bx;
      by = -by;
      bz = -bz;
      bw = -bw;
    }
    if (dot > 0.9995f) {
      q[0] = a[0] + (bx - a[0]) * t;
      q[1] = a[1] + (by - a[1]) * t;
      q[2] = a[2] + (bz - a[2]) * t;
      q[3] = a[3] + (bw - a[3]) * t;
    } else {
      const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f)),
                  sinTheta = std::sin(theta),
                  wa = std::sin((1 - t) * theta) / sinTheta,
                  wb = std::sin(t * theta) / sinTheta;
      q[0] = a[0] * wa + bx * wb;
      q[1] = a[1] * wa + by * wb;
      q[2] = a[2] * wa + bz * wb;
      q[3] = a[3] * wa + bw * wb;
    }
    const float inv =
        1 / std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int k = 0; k < 4; ++k)
      q[k] *= inv;
  }
  static void quaternionToEuler(const float *q, float &x, float &y, float &z) {
    float m[16], p[3]{}, s[3]{1, 1, 1};
    compose(p, q, s, m);
    y = std::asin(std::clamp(m[8], -1.0f, 1.0f));
    if (std::abs(m[8]) < 0.9999999f) {
      x = std::atan2(-m[9], m[10]);
      z = std::atan2(-m[4], m[0]);
    } else {
      x = std::atan2(m[6], m[5]);
      z = 0;
    }
  }
  static void eulerToQuaternion(float x, float y, float z, float *q) {
    const float c1 = std::cos(x / 2), c2 = std::cos(y / 2),
                c3 = std::cos(z / 2), s1 = std::sin(x / 2),
                s2 = std::sin(y / 2), s3 = std::sin(z / 2);
    q[0] = s1 * c2 * c3 + c1 * s2 * s3;
    q[1] = c1 * s2 * c3 - s1 * c2 * s3;
    q[2] = c1 * c2 * s3 + s1 * s2 * c3;
    q[3] = c1 * c2 * c3 - s1 * s2 * s3;
  }
  enum RigRegion {
    RigOther = 0,
    RigSpine = 1,
    RigHead = 2,
    RigShoulder = 3,
    RigUpperArm = 4,
    RigForearm = 5,
    RigHand = 6
  };
  static RigRegion rigRegion(const HumanModelBone &bone) {
    if (bone.kind == 1)
      return RigShoulder;
    if (bone.kind == 2)
      return RigUpperArm;
    if (bone.kind == 3)
      return RigForearm;
    if (bone.kind == 4)
      return RigHand;
    if (bone.flags & 2)
      return RigHead;
    if (bone.flags & 1)
      return RigSpine;
    return RigOther;
  }
  static bool rigIsLeftArm(const HumanModelBone &bone) { return bone.flags & 4; }
  static bool rigIsRightArm(const HumanModelBone &bone) { return bone.flags & 8; }
  static void applyAttack(const HumanModelBone &bone, float timer, int variant,
                          float *q) {
    constexpr float duration = 0.86f;
    const float t = 1 - std::clamp(timer / duration, 0.0f, 1.0f);
    const float pi = 3.14159265358979323846f;
    const float windup = std::sin(std::clamp(t / 0.30f, 0.0f, 1.0f) * pi * 0.5f) * (t < 0.30f ? 1.0f : 0.0f);
    const float sweepT = std::clamp((t - 0.30f) / 0.42f, 0.0f, 1.0f);
    const float strike = std::sin(sweepT * pi);
    const float reach = std::clamp(sweepT * 1.35f, 0.0f, 1.0f);
    const float recover = std::sin(std::clamp((t - 0.72f) / 0.28f, 0.0f, 1.0f) * pi);
    const float side = (variant % 2 == 0) ? 1.0f : -1.0f,
                low = variant >= 2 ? 1.0f : 0.0f;
    const RigRegion region = rigRegion(bone);
    float dx = 0, dy = 0, dz = 0;
    if (region == RigSpine) {
      dx += windup * (0.10f + low * 0.03f) - reach * (0.22f + low * 0.08f) + recover * 0.08f;
      dy += side * (windup * -0.10f + strike * 0.12f);
      dz += side * (windup * -0.22f + strike * 0.20f - recover * 0.08f);
    }
    if (region == RigHead) {
      dx += -reach * 0.10f + windup * 0.04f;
      dy += side * (windup * 0.06f + strike * 0.04f);
      dz += side * (windup * -0.08f + strike * 0.06f);
    }
    const bool lead =
        (side > 0 && rigIsRightArm(bone)) || (side < 0 && rigIsLeftArm(bone));
    const bool back =
        (side > 0 && rigIsLeftArm(bone)) || (side < 0 && rigIsRightArm(bone));
    if (lead || back) {
      const float armSide = lead ? side : -side;
      const float wind = windup;
      const float deadReach = lead ? (reach * 1.28f + strike * 0.34f) : (reach * 0.22f);
      const float drag = lead ? 1.0f : 0.34f;
      dx += lead ? wind * 0.54f : -wind * 0.16f;
      dy += lead ? -side * wind * 0.62f : side * wind * 0.16f;
      dz += lead ? -side * wind * 0.66f : side * wind * 0.18f;
      switch (region) {
      case RigShoulder:
        dx += lead ? -deadReach * (0.28f + low * 0.12f) : reach * 0.08f;
        dy += armSide * deadReach * (0.30f + low * 0.08f);
        dz += armSide * (strike * 0.24f - recover * 0.10f) * drag;
        break;
      case RigUpperArm:
        dx += lead ? -deadReach * (0.92f + low * 0.24f) : reach * 0.16f;
        dy += armSide * deadReach * (0.46f + low * 0.10f);
        dz += armSide * (strike * 0.32f - recover * 0.14f) * drag;
        break;
      case RigForearm:
        dx += lead ? -deadReach * (1.18f + low * 0.18f) : reach * 0.18f;
        dy += armSide * deadReach * 0.24f;
        dz += armSide * (strike * 0.22f - recover * 0.10f) * drag;
        break;
      case RigHand:
        dx += lead ? -deadReach * (0.68f + low * 0.12f) : reach * 0.08f;
        dy += armSide * deadReach * 0.18f;
        dz += armSide * (strike * 0.38f - recover * 0.18f) * drag;
        break;
      default:
        dx += lead ? -deadReach * 0.45f : reach * 0.10f;
        dz += armSide * strike * 0.18f * drag;
        break;
      }
    }
    if (dx != 0 || dy != 0 || dz != 0) {
      float x, y, z;
      quaternionToEuler(q, x, y, z);
      eulerToQuaternion(x + dx, y + dy, z + dz, q);
    }
  }
};
