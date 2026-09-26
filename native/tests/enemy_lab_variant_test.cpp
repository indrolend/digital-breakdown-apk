#include "gameplay/EnemyLabVariant.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace gameplay;
    EnemyLabVariant parsed{};
    assert(parseEnemyLabVariant("rabid-animator",parsed)&&parsed==EnemyLabVariant::RabidAnimator);
    assert(parseEnemyLabVariant("euphoria-lite",parsed)&&parsed==EnemyLabVariant::EuphoriaLite);
    assert(parseEnemyLabVariant("traversal-predator",parsed)&&parsed==EnemyLabVariant::TraversalPredator);
    assert(parseEnemyLabVariant("feral-hybrid",parsed)&&parsed==EnemyLabVariant::FeralHybrid);
    assert(!parseEnemyLabVariant("unknown",parsed));

    const auto animator=enemyLabProfile(EnemyLabVariant::RabidAnimator);
    const auto euphoria=enemyLabProfile(EnemyLabVariant::EuphoriaLite);
    const auto predator=enemyLabProfile(EnemyLabVariant::TraversalPredator);
    const auto feral=enemyLabProfile(EnemyLabVariant::FeralHybrid);
    assert(animator.presentation.ordinaryGait==EnemyGaitAuthority::AuthoredAnimation);
    assert(euphoria.presentation.ordinaryGait==EnemyGaitAuthority::PhysicalContacts);
    assert(predator.traversal.aggressiveRouting&&predator.traversal.mayJumpGap);
    assert(feral.traversal.aggressiveRouting&&feral.consequences.impactResponse>euphoria.consequences.impactResponse);
    assert(predator.motor.speedScale>animator.motor.speedScale);
    std::cout<<"ENEMY_LAB_VARIANTS_OK selection=4 authorities=SEPARATE\n";
}
