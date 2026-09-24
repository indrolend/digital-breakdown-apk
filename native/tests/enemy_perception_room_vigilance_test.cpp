#include <cassert>
#include <cmath>
#include <cstdio>
#include "gameplay/EnemyPerception.hpp"

int main(){
    gameplay::EnemyPerceptionState calm{}, pressured{};
    calm.lastSeenPosition=pressured.lastSeenPosition={0.0f,1.0f,-1.6f};
    calm.confidence=pressured.confidence=0.42f;
    calm.uncertainty=pressured.uncertainty=0.86f;
    calm.searchPhase=pressured.searchPhase=1.05f;

    gameplay::EnemyPerceptionInput input{};
    input.observerPosition={0.0f,1.0f,0.0f};
    input.dt=1.0f/60.0f;
    input.individuality=0.23f;
    const auto calmOut=gameplay::updateEnemyPerception(input,calm);
    input.roomMemoryPressure=1.0f;
    const auto pressuredOut=gameplay::updateEnemyPerception(input,pressured);

    // Pressure makes unresolved vigilance more legible through gaze without
    // manufacturing evidence or confirmation.
    assert(std::abs(pressuredOut.headYaw)>std::abs(calmOut.headYaw)+0.001f);
    assert(pressuredOut.confidence>=calmOut.confidence);
    assert(!pressuredOut.confirmed&&!calmOut.confirmed);
    assert(pressuredOut.hasSpatialBelief&&calmOut.hasSpatialBelief);
    std::printf("ENEMY_ROOM_VIGILANCE_OK calmYaw=%.4f pressuredYaw=%.4f confidence=%.3f/%.3f\\n",calmOut.headYaw,pressuredOut.headYaw,calmOut.confidence,pressuredOut.confidence);
}
