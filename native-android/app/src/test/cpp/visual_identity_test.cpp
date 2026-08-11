#include "../../main/cpp/game/VisualIdentity.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b, float e = 0.001f) { return std::fabs(a - b) <= e; }

int main() {
    const Quat halfFlip = quatAxisAngle({1, 0, 0}, -DB_PI);
    const Vec3 flippedUp = rotate(halfFlip, {0, 1, 0});
    assert(near(flippedUp.y, -1.0f));
    const Quat turn = quatSlerp({}, quatAxisAngle({0, 1, 0}, DB_PI), 0.5f);
    const Vec3 turnedForward = rotate(turn, {0, 0, 1});
    assert(near(std::fabs(turnedForward.x), 1.0f) && near(turnedForward.z, 0.0f));
    const PhoneVisualState idle = makePhoneVisualState(0, 0, 0, 1, false);
    assert(near(idle.vacuumPose, 0) && near(idle.screenGlow, 0.75f));
    assert(near(idle.bodyScale.x, 1) && idle.visible);
    const PhoneVisualState active = makePhoneVisualState(1, 1, 0.6f, 1, false);
    assert(active.actionLift > 0.64f && active.actionForward > 0.24f);
    assert(active.screenGlow > idle.screenGlow && active.screenScale.x > 1);
    assert(!makePhoneVisualState(1, 1, 0, 1, true).visible);
    assert(near(active.screenGlow, makePhoneVisualState(1, 1, 0.6f, 1, false).screenGlow));

    const SoulVisualState freeSoul = makeSoulVisualState(0, 0, 0, 0, 1, 0, true, 1, 0.4f, 0.8f);
    assert(near(freeSoul.color.r, Pass7Visual::SoulBase.r));
    assert(near(freeSoul.scale.x, freeSoul.scale.y) && near(freeSoul.scale.y, freeSoul.scale.z));
    assert(near(freeSoul.rotationY, 0.8f) && std::fabs(freeSoul.verticalOffset) <= 0.18f);
    assert(near(makeSoulVisualState(0,0,0,0,1,0,true,0.5f).morphScale,0.5f));
    const SoulVisualState attracted = makeSoulVisualState(1, 0.8f, 0, 0, 1, 0, true);
    assert(attracted.pullAmount > 0.79f && near(attracted.scale.x, attracted.scale.y));
    const SoulVisualState latched = makeSoulVisualState(2, 1, 0.2f, 0, 1, 0, true);
    assert(latched.latchAmount == 1 && latched.emission > attracted.emission);
    const SoulVisualState ingesting = makeSoulVisualState(3, 1, 0.8f, 0, 1, 0, true);
    assert(ingesting.ingestAmount > 0.79f && ingesting.scale.x > 0);
    const SoulVisualState ingestStart = makeSoulVisualState(3, 1, 0.0f, 0, 1, 0, true);
    const SoulVisualState ingestMid = makeSoulVisualState(3, 1, 0.5f, 0, 1, 0, true);
    assert(near(ingestStart.morphScale, 1.0f));
    assert(near(ingestMid.morphScale, 0.5f));
    assert(ingesting.morphScale < ingestMid.morphScale);
    assert(!makeSoulVisualState(3, 1, 0.92f, 0, 1, 0, true).visible);
    for (float dt : {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f}) {
        float elapsed = 0;
        while (elapsed + dt <= 1.0f + 0.0001f) elapsed += dt;
        const auto sample = makePhoneVisualState(1, 1, 0.5f, elapsed, false);
        assert(std::fabs(sample.screenGlow - makePhoneVisualState(1, 1, 0.5f, 1.0f, false).screenGlow) < 0.002f);
    }
    std::printf(
        "phone pose=%.3f glow=%.3f screenScale=(%.3f,%.3f) "
        "soul color=(%.3f,%.3f,%.3f) emission=%.3f deformation=(%.3f,%.3f,%.3f) "
        "scale=(%.3f,%.3f,%.3f) capture=ingesting phase=%.3f\n",
        active.vacuumPose, active.screenGlow, active.screenScale.x, active.screenScale.y,
        ingesting.color.r, ingesting.color.g, ingesting.color.b, ingesting.emission,
        ingesting.deformation.x, ingesting.deformation.y, ingesting.deformation.z,
        ingesting.scale.x, ingesting.scale.y, ingesting.scale.z, ingesting.phase);
    return 0;
}
