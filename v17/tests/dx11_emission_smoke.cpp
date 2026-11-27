#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_dx11_emission.h"
#include <iostream>
struct V{float x,y,z;};
struct Material{bool emissionAuthored;V emissionColor;float emissionIntensity;};
int main(){
    float r=0,g=0,b=0,s=0,glow=0;
    auto apply=[&](Material m){m3d::dx11::applyAuthoredEmission(m,r,g,b,s,glow);};
    apply({true,{1,.76f,.5f},.65f});
    if(r!=1||g!=.76f||b!=.5f||s!=.65f||glow!=0)return 1;
    std::cout<<"PASS 1 authored rib\n";
    apply({true,{1,.76f,.5f},.30f});if(s!=.30f)return 2;
    std::cout<<"PASS 2 authored groove\n";
    r=.83f;g=.91f;b=1;s=.24f;glow=.38f;
    apply({false,{0,0,0},0});if(r!=.83f||g!=.91f||b!=1||s!=.24f||glow!=.38f)return 3;
    std::cout<<"PASS 3 legacy unchanged\n";
    apply({true,{1,1,1},0});if(s!=0||glow!=0)return 4;
    std::cout<<"PASS 4 explicit off overrides legacy\n";
    s=-1; r=.1f;g=.2f;b=.3f;glow=.4f;
    apply({true,{1,1,1},2});if(s!=-1||r!=.1f||g!=.2f||b!=.3f||glow!=.4f)return 5;
    std::cout<<"PASS 5 water sentinel preserved\n";
}
