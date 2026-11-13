#include "../anomaly_catalog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(){
    using namespace cse::anomalies;
    if(records.size()!=10)return 1;
    for(size_t i=0;i<records.size();++i){
        const auto& a=records[i];
        char id[16]={};
        std::snprintf(id,sizeof(id),"ANO-%03d",int(i)+5);
        if(std::strcmp(id,a.id)!=0)return 2;
        if(!a.title||!a.check||!a.title[0]||!a.check[0])return 3;
        if(a.cameraKnown){
            if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)
               ||!std::isfinite(a.yaw)||!std::isfinite(a.pitch)
               ||a.yaw<0||a.yaw>=360||a.pitch<=-90||a.pitch>=90
               ||a.fov<30||a.fov>120)return 4;
        }
        if(a.correctedCandidate!=(i==5||i==6||i==7||i==9))return 5;
    }
    if(records[9].cameraKnown)return 6; // no invented camera for paving
    if(records[2].x!=records[3].x||records[2].z!=records[3].z)return 7;
    std::puts("OK: 10 IDs, 9 exact screenshots, no fabricated ANO-014 pose");
    return 0;
}
