// CPU-only regression for the mouse-held FPS sniper-scope projection.
#include "../scope_zoom.h"
#include <cassert>
#include <cmath>
#include <cstdio>
static bool near(float a,float b,float tolerance=.0005f){
    return std::abs(a-b)<tolerance;
}
int main(){
    using namespace cse::scope;
    static_assert(magnification==3.f,"Scope magnification regression");
    const float base=65.f;
    const float narrow=targetFov(base);
    assert(narrow>20.f&&narrow<30.f);
    assert(near(effectiveFov(base,0.f),base));
    assert(near(effectiveFov(base,1.f),narrow));
    assert(effectiveFov(base,.5f)<base&&effectiveFov(base,.5f)>narrow);
    assert(effectiveFov(100.f,1.f)<100.f);
    assert(mouseSensitivity(1.f)<mouseSensitivity(0.f));
    float progress=0.f;
    for(int i=0;i<30;++i)progress=advance(progress,true,1.f/60.f);
    assert(progress>.95f&&progress<=1.f);
    float fov=effectiveFov(base,progress);
    assert(fov<base);
    for(int i=0;i<60;++i)progress=advance(progress,false,1.f/60.f);
    assert(progress<.005f);
    for(int i=0;i<12;++i)progress=advance(progress,false,.08f);
    assert(progress==0.f);
    assert(effectiveFov(base,progress)==base);
    std::printf("SCOPE PASS: hold RMB %.2f -> %.2f deg, smooth return, mouse precision stable\\n",base,narrow);
    return 0;
}
