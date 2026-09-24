#include "VisualIdentity.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
    PhoneVisualState previous{};
    for(int frame=0;frame<24;++frame){auto next=makePhoneVisualState(1,1,1,frame/60.0f,false);advancePhoneIngestBulge(next,previous,1.0f/60.0f);previous=next;}
    assert(previous.ingestBulge>0.70f&&previous.bodyScale.x>1.05f&&previous.bodyScale.y>1.05f&&previous.bodyScale.z>previous.bodyScale.x);
    for(int frame=0;frame<90;++frame){auto next=makePhoneVisualState(0,0,0,(24+frame)/60.0f,false);advancePhoneIngestBulge(next,previous,1.0f/60.0f);previous=next;}
    assert(std::abs(previous.ingestBulge)<0.02f&&std::abs(previous.bodyScale.x-1.0f)<0.01f);
    std::puts("EXPRESSIVE_VISUAL_OK phone=INGEST_BULGE_SETTLE enemy=IMPACT_SQUASH_ELASTIC_ARMS");
}
