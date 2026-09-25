#include <cassert>
#include <cstdio>
#include "gameplay/EnemyBehaviorState.hpp"

int main() {
    using namespace gameplay;
    EnemyBehaviorState state{};
    EnemyBehaviorInput in{};
    in.dt = 1.0f / 60.0f;

    EnemyBehaviorOutput out{};
    for (int i = 0; i < 90; ++i) out = updateEnemyBehavior(state, in);
    assert(out.mode == EnemyBehaviorMode::Rest);
    assert(out.travelScale == 0.0f && out.settled);

    in.vagueAwareness = 0.32f;
    for (int i = 0; i < 30; ++i) out = updateEnemyBehavior(state, in);
    assert(out.mode == EnemyBehaviorMode::Orient);
    assert(out.travelScale == 0.0f);

    in.vagueAwareness = 0.0f;
    in.hasSpatialBelief = true;
    in.confidence = 0.24f;
    for (int i = 0; i < 30; ++i) out = updateEnemyBehavior(state, in);
    assert(out.mode == EnemyBehaviorMode::Investigate);
    assert(out.travelScale > 0.0f && !out.mayAttack);

    in.confirmed = true;
    in.confidence = 0.80f;
    out = updateEnemyBehavior(state, in);
    assert(out.mode == EnemyBehaviorMode::Engage && out.mayAttack);

    in.confirmed = false;
    in.confidence = 0.20f;
    in.uncertainty = 0.72f;
    for (int i = 0; i < 30; ++i) out = updateEnemyBehavior(state, in);
    assert(out.mode == EnemyBehaviorMode::Search);
    assert(out.travelScale > 0.0f && out.travelScale < 1.0f);

    in.hasSpatialBelief = false;
    in.confidence = 0.0f;
    in.uncertainty = 1.0f;
    for (int i = 0; i < 500; ++i) out = updateEnemyBehavior(state, in);
    assert(out.mode == EnemyBehaviorMode::Rest);
    assert(out.settled && out.travelScale == 0.0f);

    std::puts("ENEMY_BEHAVIOR_STATE_OK discrete=READABLE continuous=EVIDENCE rest=SETTLED");
}
