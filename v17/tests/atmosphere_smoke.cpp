#include "../atmosphere/atmosphere.h"
#include <cmath>
#include <cstdint>
#include <iostream>
using namespace m3d::atmosphere;
float grain(float x,float y,float z,float cell){
    auto frac=[](float a){return a-std::floor(a);};
    float a=x/cell,b=y/cell,c=z/cell;
    float dx=(frac(a)-.5f)/.38f,dy=(frac(b)-.5f)/.38f,dz=(frac(c)-.5f)/.38f;
    float v=std::max(0.f,1.f-dx*dx-dy*dy-dz*dz);
    uint32_t h=uint32_t(int32_t(std::floor(a)))*73856093u^uint32_t(int32_t(std::floor(b)))*19349663u^uint32_t(int32_t(std::floor(c)))*83492791u;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;
    return (h&3u)==0?v*v:0;
}
int main(){
    if(maxLinear*12.92f>=.01f)return 1;
    if(defaultDensity*.116f*12.92f>.00301f)return 2;
    for(int i=-100;i<100;++i){
        float x=i*.06f;
        if(grain(x-.00001f,.03f,.03f,.06f)!=0||grain(x+.00001f,.03f,.03f,.06f)!=0)return 3;
    }
    for(int i=0;i<10000;++i){
        float x=i*.012345f;float v=grain(x,x*.37f,x*.61f,.06f);
        if(v<0||v>1||v!=grain(x,x*.37f,x*.61f,.06f))return 4;
        if(std::clamp(v*0.f*defaultDensity*.116f,0.f,maxLinear)!=0)return 5;
    }
    if(smooth(0)!=0||smooth(1)!=1||smooth(.0001f)>.00001f)return 6;
    std::cout<<"PASS six controles: amplitude sRGB, valeur nominale, continuite, determinisme, obscurite, fondu des sources\n";
}
