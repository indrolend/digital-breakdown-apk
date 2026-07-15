#include "../../main/cpp/game/VisualIdentity.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b, float e = 0.001f) { return std::fabs(a - b) <= e; }

int main() {
    const PhoneVisualState idle = makePhoneVisualState(0, 0, 0, 1, false);
    assert(near(idle.vacuumPose, 0) && near(idle.screenGlow, 0.75f));
    assert(near(idle.bodyScale.x, 1) && idle.visible);
    const PhoneVisualState active = makePhoneVisualState(1, 1, 0.6f, 1, false);
    assert(active.actionLift > 0.64f && active.actionForward > 0.24f);
    assert(active.screenGlow > idle.screenGlow && active.screenScale.x > 1);
    assert(!makePhoneVisualState(1, 1, 0, 1, true).visible);
    assert(near(active.screenGlow, makePhoneVisualState(1, 1, 0.6f, 1, false).screenGlow));

    const SoulVisualState freeSoul = makeSoulVisualState(0, 0, 0, 0, 1, 0, true);
    assert(near(freeSoul.color.r, Pass7Visual::SoulBase.r));
    const SoulVisualState attracted = makeSoulVisualState(1, 0.8f, 0, 0, 1, 0, true);
    assert(attracted.pullAmount > 0.79f && attracted.scale.y > freeSoul.scale.y);
    const SoulVisualState latched = makeSoulVisualState(2, 1, 0.2f, 0, 1, 0, true);
    assert(latched.latchAmount == 1 && latched.emission > attracted.emission);
    const SoulVisualState ingesting = makeSoulVisualState(3, 1, 0.8f, 0, 1, 0, true);
    assert(ingesting.ingestAmount > 0.79f && ingesting.scale.x > 0);
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
