#include <cassert>
#include <cmath>

#include "../game/Game.hpp"

namespace {
float distanceToData(const GameState& state) {
    const Vec3 delta=state.player.pos-state.targets[0].pos;
    return std::sqrt(delta.x*delta.x+delta.z*delta.z);
}
}

int main() {
    Game game;
    game.setEnemyIntentionMode(gameplay::EnemyIntentionMode::RelentlessZombie);
    game.setEnemyLabVariant(gameplay::EnemyLabVariant::FeralHybrid);
    game.debugStartZombieV1Benchmark();
    const float initialDistance=distanceToData(game.state());
    float previous=initialDistance;
    int retreatFrames=0;
    for(int frame=0;frame<150;++frame){
        game.update(1.0f/60.0f);
        const float current=distanceToData(game.state());
        if(current>previous+0.003f)++retreatFrames;
        previous=current;
    }
    const float beforeShove=distanceToData(game.state());
    assert(beforeShove<initialDistance-2.0f);
    assert(retreatFrames<8);
    assert(game.zombieV1Telemetry()[0].active);
    assert(game.zombieV1Telemetry()[0].relentless);

    game.debugApplyEnemyImpulse(0,{3.2f,0.0f,0.0f});
    for(int frame=0;frame<45;++frame)game.update(1.0f/60.0f);
    const auto& telemetry=game.zombieV1Telemetry()[0];
    const Vec3 toData=game.state().player.pos-game.state().targets[0].pos;
    assert(telemetry.active&&telemetry.relentless);
    assert(telemetry.requestedSteering.x*toData.x+telemetry.requestedSteering.z*toData.z>=-0.0001f);

    Game moving;
    moving.setEnemyIntentionMode(gameplay::EnemyIntentionMode::RelentlessZombie);
    moving.setEnemyLabVariant(gameplay::EnemyLabVariant::FeralHybrid);
    moving.debugStartZombieV1Benchmark();
    for(int frame=0;frame<150;++frame){
        moving.networkMutableState().player.pos.x=4.0f*static_cast<float>(frame)/149.0f;
        moving.update(1.0f/60.0f);
    }
    assert(moving.state().targets[0].pos.x>0.45f);
    const Vec3 movingToData=moving.state().player.pos-moving.state().targets[0].pos;
    const auto& movingTelemetry=moving.zombieV1Telemetry()[0];
    assert(movingTelemetry.requestedSteering.x*movingToData.x+
           movingTelemetry.requestedSteering.z*movingToData.z>0.0f);
    return 0;
}
