#include <cmath>
#include <cstdio>

#include "Game.hpp"

struct SoulProjectileLifecycleAccess {
    static void bullets(Game& game, float dt) { game.updateBullets(dt); }
    static int melee(Game& game) { return game.applyMeleeHits(); }
};

namespace {

constexpr float kDt = 1.0f / 60.0f;

float speed(const Vec3& v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
bool near(float a,float b,float e=0.002f){return std::fabs(a-b)<=e;}

void isolate(Game& game) {
    game.reset();
    GameState& state=game.networkMutableState();
    state.roomClear=true;
    for(auto& target:state.targets)target.alive=false;
    for(auto& bullet:state.bullets)bullet=BulletState{};
    state.player.souls=0;
    state.player.storedSoulBrute.fill(false);
    state.player.storedSouls.fill(SoulRecord{});
    state.vacuum=VacuumState{};
}

BulletState& soul(Game& game,std::uint64_t id=77) {
    BulletState& bullet=game.networkMutableState().bullets[0];
    bullet=BulletState{};bullet.alive=true;bullet.life=3.0f;
    bullet.soul={id,false,4};
    return bullet;
}

} // namespace

int main(){
    bool ok=true;
    Game wall;isolate(wall);auto& wb=soul(wall);wb.pos={14.70f,2.0f,0};wb.vel={12,0,0};
    SoulProjectileLifecycleAccess::bullets(wall,kDt);
    ok&=wb.alive&&!wb.dropped&&wb.vel.x<0&&speed(wb.vel)<12.1f&&speed(wb.vel)>8.0f&&wb.soul.id==77;

    Game ceiling;isolate(ceiling);auto& cb=soul(ceiling);cb.pos={0,7.9f,0};cb.vel={0,12,0};
    SoulProjectileLifecycleAccess::bullets(ceiling,kDt);
    ok&=cb.vel.y<0&&cb.alive&&cb.soul.id==77;

    Game floor;isolate(floor);auto& fb=soul(floor);fb.pos={0,0.20f,0};fb.vel={0,-8,0};
    SoulProjectileLifecycleAccess::bullets(floor,kDt);
    ok&=fb.alive&&fb.dropped&&near(speed(fb.vel),0.0f)&&fb.soul.id==77;

    Game melee;isolate(melee);auto& mb=soul(melee);mb.pos={0,0.45f,-1.0f};mb.vel={0,0,1};
    auto& ms=melee.networkMutableState();ms.player.pos={0,0.08f,0};ms.phoneTransform.position={0,0.45f,0};
    ms.meleeVisual=MeleeVisualState{};ms.meleeVisual.direction={0,0,-1};ms.meleeVisual.range=2.0f;ms.meleeVisual.hitRadius=0.6f;
    const int meleeHits=SoulProjectileLifecycleAccess::melee(melee);
    ok&=meleeHits==1&&mb.vel.z<0&&speed(mb.vel)>14.0f&&speed(mb.vel)<20.0f&&mb.contactCooldown>0;

    Game lunge;isolate(lunge);auto& lb=soul(lunge);lb.pos={0,0.45f,-0.2f};lb.vel={0,0,1};
    auto& ls=lunge.networkMutableState();ls.player.pos={0,0.08f,0};ls.phoneTransform.position={0,0.45f,-0.4f};
    ls.meleeVisual=MeleeVisualState{};ls.meleeVisual.direction={0,0,-1};ls.meleeVisual.locomotionLunge=true;
    ls.meleeVisual.contactPositionValid=true;ls.meleeVisual.previousContactPosition={0,0.45f,0.2f};
    const int lungeHits=SoulProjectileLifecycleAccess::melee(lunge);
    ok&=lungeHits==1&&lb.vel.z<0&&speed(lb.vel)>25.0f&&ls.hud.criticalHitPulse>0.9f;

    Game recovery;isolate(recovery);auto& rb=soul(recovery,991);rb.pos={0,1.0f,0.35f};rb.dropped=true;
    auto& rs=recovery.networkMutableState();rs.vacuum.active=true;rs.phoneTransform.vacuumPullPoint={0,1.0f,0};rs.phoneTransform.screenNormal={0,0,1};
    SoulProjectileLifecycleAccess::bullets(recovery,kDt);
    ok&=!rb.alive&&rs.player.souls==1&&rs.player.storedSouls[0].id==991&&rs.player.storedSouls[0].originRoom==4;

    Game replayA;Game replayB;isolate(replayA);isolate(replayB);
    auto& a=soul(replayA,1234);auto& b=soul(replayB,1234);a.pos=b.pos={14.7f,2.0f,0};a.vel=b.vel={9,3,-2};
    for(int i=0;i<90;++i){SoulProjectileLifecycleAccess::bullets(replayA,kDt);SoulProjectileLifecycleAccess::bullets(replayB,kDt);}
    ok&=a.alive==b.alive&&a.dropped==b.dropped&&a.soul.id==b.soul.id&&near(a.pos.x,b.pos.x)&&near(a.pos.y,b.pos.y)&&near(a.pos.z,b.pos.z)&&near(a.vel.x,b.vel.x)&&near(a.vel.y,b.vel.y)&&near(a.vel.z,b.vel.z);

    if(!ok){std::fprintf(stderr,"SOUL_PROJECTILE_LIFECYCLE_FAILED wall=%.2f ceiling=%.2f melee=%.2f lunge=%.2f recovered=%d replay=%llu/%llu\n",wb.vel.x,cb.vel.y,speed(mb.vel),speed(lb.vel),rs.player.souls,static_cast<unsigned long long>(a.soul.id),static_cast<unsigned long long>(b.soul.id));return 1;}
    std::printf("SOUL_PROJECTILE_LIFECYCLE_OK wall=REFLECT ceiling=REFLECT floor=DROP melee=%.2f lunge=%.2f recovery_id=%llu deterministic=MATCH\n",speed(mb.vel),speed(lb.vel),static_cast<unsigned long long>(rs.player.storedSouls[0].id));
    return 0;
}
