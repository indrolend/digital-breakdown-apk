#include <cassert>
#include <cmath>

#include "gameplay/SoulMotion.hpp"
#include "gameplay/TargetRoles.hpp"
#include "VisualIdentity.hpp"

namespace {

TargetState makeHuman() {
    TargetState target{};
    target.alive = true;
    target.slurpable = false;
    target.soulState = SoulState::Free;
    return target;
}

TargetState makeSoul(SoulState state = SoulState::Free) {
    TargetState target{};
    target.alive = true;
    target.slurpable = true;
    target.soulState = state;
    return target;
}

void testRolePredicates() {
    TargetState human = makeHuman();
    assert(gameplay::isActiveHuman(human));
    assert(gameplay::isCombatTarget(human));
    assert(!gameplay::isLooseSoul(human));
    assert(!gameplay::isVacuumTarget(human));

    TargetState soul = makeSoul();
    assert(!gameplay::isActiveHuman(soul));
    assert(!gameplay::isCombatTarget(soul));
    assert(gameplay::isLooseSoul(soul));
    assert(gameplay::isVacuumTarget(soul));
    assert(gameplay::isFreeVacuumOffer(soul));

    soul.captureQueued = true;
    assert(!gameplay::isLooseSoul(soul));
    assert(!gameplay::isVacuumTarget(soul));

    TargetState recoiling = makeSoul(SoulState::Recoiling);
    assert(gameplay::isLooseSoul(recoiling));
    assert(!gameplay::isVacuumTarget(recoiling));

    TargetState revolving = makeSoul(SoulState::Revolving);
    assert(gameplay::isLooseSoul(revolving));
    assert(!gameplay::isVacuumTarget(revolving));
}

void testFreeSoulMotion() {
    TargetState soul = makeSoul();
    soul.pos = {0.0f, 1.0f, 0.0f};
    soul.vel = {2.0f, 0.0f, -1.0f};

    gameplay::updateLooseSoulMotion(soul, 0.1f);

    assert(soul.pos.x > 0.0f);
    assert(soul.pos.y < 1.0f);
    assert(soul.pos.z < 0.0f);
    assert(soul.vel.y < 0.0f);
    assert(std::abs(soul.vel.x) < 2.0f);
    assert(std::abs(soul.vel.z) < 1.0f);
}

void testRecoilTimeout() {
    TargetState soul = makeSoul(SoulState::Recoiling);
    soul.recoilTime = 0.05f;
    soul.networkOwnerPlayerId = 2;
    soul.pos = {0.0f, 0.5f, 0.0f};

    gameplay::updateLooseSoulMotion(soul, 0.1f);

    assert(soul.soulState == SoulState::Free);
    assert(soul.networkOwnerPlayerId == -1);
    assert(soul.recoilTime == 0.0f);
}

void testGroundClampAndStop() {
    TargetState soul = makeSoul();
    soul.pos = {0.0f, 0.081f, 0.0f};
    soul.vel = {0.01f, -1.0f, -0.01f};

    gameplay::updateLooseSoulMotion(soul, 0.1f);

    assert(std::abs(soul.pos.y - 0.08f) < 0.0001f);
    assert(soul.vel.y == 0.0f);
    assert(soul.vel.x == 0.0f);
    assert(soul.vel.z == 0.0f);
}

void testVacuumOwnedStatesDoNotMove() {
    for (const SoulState state : {SoulState::Attracted, SoulState::Latched, SoulState::Ingesting, SoulState::Revolving}) {
        TargetState soul = makeSoul(state);
        soul.pos = {1.0f, 2.0f, 3.0f};
        soul.vel = {4.0f, 5.0f, 6.0f};
        const Vec3 originalPos = soul.pos;
        const Vec3 originalVel = soul.vel;

        gameplay::updateLooseSoulMotion(soul, 0.25f);

        assert(soul.pos.x == originalPos.x && soul.pos.y == originalPos.y && soul.pos.z == originalPos.z);
        assert(soul.vel.x == originalVel.x && soul.vel.y == originalVel.y && soul.vel.z == originalVel.z);
    }
}

void testIngestingSoulShellContractsContinuously() {
    const SoulVisualState start = makeSoulVisualState(3, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, true);
    const SoulVisualState middle = makeSoulVisualState(3, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, true);
    const SoulVisualState late = makeSoulVisualState(3, 1.0f, 0.8f, 0.0f, 1.0f, 0.0f, true);

    assert(std::abs(start.morphScale - 1.0f) < 0.0001f);
    assert(std::abs(middle.morphScale - 0.5f) < 0.0001f);
    assert(late.morphScale < middle.morphScale);
    assert(late.morphScale > 0.0f);
}

} // namespace

int main() {
    testRolePredicates();
    testFreeSoulMotion();
    testRecoilTimeout();
    testGroundClampAndStop();
    testVacuumOwnedStatesDoNotMove();
    testIngestingSoulShellContractsContinuously();
    return 0;
}
