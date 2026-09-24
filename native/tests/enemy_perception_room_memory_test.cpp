#include <cassert>
#include <cstdio>
#include "gameplay/EnemyPerception.hpp"

int main(){
    gameplay::EnemyPerceptionState calm{}, pressured{}, empty{};
    gameplay::EnemyPerceptionInput seed{};
    seed.observerPosition={0,1,0};
    seed.environmentalCuePosition={6,0,-2};
    seed.environmentalCueStrength=0.72f;
    seed.dt=1.0f/60.0f;
    gameplay::updateEnemyPerception(seed,calm);
    gameplay::updateEnemyPerception(seed,pressured);
    const float seeded=calm.confidence;

    gameplay::EnemyPerceptionInput quiet{};
    quiet.observerPosition={0,1,0};
    quiet.dt=1.0f/60.0f;
    for(int frame=0;frame<45;++frame) gameplay::updateEnemyPerception(quiet,calm);
    quiet.roomMemoryPressure=1.0f;
    for(int frame=0;frame<45;++frame) gameplay::updateEnemyPerception(quiet,pressured);

    assert(seeded>0.10f);
    assert(pressured.confidence>calm.confidence+0.025f);
    assert(pressured.uncertainty>=calm.uncertainty-0.0001f);
    assert(!pressured.confirmed);

    quiet.roomMemoryPressure=1.0f;
    gameplay::EnemyPerceptionOutput emptyOut{};
    for(int frame=0;frame<120;++frame) emptyOut=gameplay::updateEnemyPerception(quiet,empty);
    assert(!emptyOut.hasSpatialBelief);
    assert(empty.confidence==0.0f);
    assert(!empty.confirmed);
    std::printf("ENEMY_ROOM_MEMORY_OK seeded=%.3f calm=%.3f pressured=%.3f\
",seeded,calm.confidence,pressured.confidence);
}
