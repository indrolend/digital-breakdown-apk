#include <cassert>

#include "gameplay/TargetLifecycle.hpp"

int main() {
    TargetState target;

    gameplay::initializeActiveHuman(target, false, 2.0f, 4.0f, 1.7f);
    assert(gameplay::isActiveHuman(target));
    assert(target.armor == 2.0f);
    assert(target.scale == 1.0f);

    assert(gameplay::convertHumanToLooseSoul(target));
    assert(gameplay::isLooseSoul(target));
    assert(target.armor == 0.0f);
    assert(target.soulState == SoulState::Free);

    assert(gameplay::queueCapture(target, 0.92f));
    assert(target.captureQueued);
    assert(target.ingestProgress >= 0.92f);

    assert(gameplay::commitCapture(target));
    assert(!target.captureQueued);
    assert(target.captureCommitted);

    assert(gameplay::deactivateCapturedSoul(target));
    assert(!target.alive);
    assert(gameplay::isReusableTargetSlot(target));

    gameplay::initializeActiveHuman(target, true, 2.0f, 4.0f, 1.7f);
    assert(target.brute);
    assert(target.armor == 4.0f);
    assert(target.scale == 1.7f);

    TargetState invalid;
    assert(!gameplay::convertHumanToLooseSoul(invalid));
    assert(!gameplay::queueCapture(invalid, 0.92f));
    assert(!gameplay::commitCapture(invalid));
    assert(!gameplay::deactivateCapturedSoul(invalid));

    return 0;
}
