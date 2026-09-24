#include "../game/gameplay/EnemyPerception.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace gameplay;
int main(){
 const float dry=environmentalCueTransmission(0.0f,true);
 const float rain=environmentalCueTransmission(1.0f,true);
 const float sheltered=environmentalCueTransmission(1.0f,false);
 assert(std::abs(dry-1.0f)<0.0001f); assert(std::abs(sheltered-1.0f)<0.0001f);
 assert(rain<dry && rain>0.70f);
 const float shot=0.72f*rain,vacuum=0.56f*rain,social=0.52f*rain,proximity=0.35f*rain;
 assert(shot>vacuum && vacuum>social && social>proximity);
 std::printf("ENEMY_WEATHER_CUE_TRANSMISSION_OK rain=%.3f\n",rain);
}
