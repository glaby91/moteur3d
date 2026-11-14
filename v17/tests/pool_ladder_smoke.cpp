#include "../pool_ladder.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace cse::pool_ladder;
int main(){
    assert(anchors.size()==2);
    assert(nearest({25.7f,.55f,-44.05f})==0);
    assert(nearest({25.7f,.55f,-19.95f})==1);
    assert(nearest({0.f,0.f,0.f})==-1);
    assert(nearest({-25.7f,.55f,-44.05f})==-1);
    assert(!inWater({26.f,0.f,-41.5f}));
    assert(inWater({24.6f,-1.80f,-41.5f}));
    for(int i=0;i<2;i++){
        const auto& a=anchors[i];
        Motion descend{};
        assert(descend.begin({a.side*deckX,deckFeetY,a.z}));
        assert(!descend.up);
        assert(!descend.begin({a.side*deckX,deckFeetY,a.z}));
        Point prev=path(i,0.f,false);
        for(int step=0;step<100&&descend.active();++step){
            const auto p=descend.advance(.04f);
            assert(std::abs(p.z-a.z)<.00001f);
            assert(p.y<=prev.y+1e-5f);
            prev=p;
        }
        assert(!descend.active());
        assert(std::abs(prev.y-bottomFeetY)<.0001f);
        assert(std::abs(prev.x-a.side*submergedX)<.0001f);
        Motion ascend{};
        assert(ascend.begin(prev));
        assert(ascend.up);
        Point current=prev;
        for(int step=0;step<100&&ascend.active();++step){
            auto p=ascend.advance(.04f);
            assert(p.y>=current.y-1e-5f);
            current=p;
        }
        assert(!ascend.active());
        assert(std::abs(current.y-deckFeetY)<.0001f);
        assert(std::abs(current.x-a.side*deckX)<.0001f);
    }
    const auto bounded=walkInWater({24.60f,-1.80f,-41.5f},10.f,-10.f);
    assert(bounded.x<=24.72f&&bounded.z>=-44.24f);
    assert(std::abs(bounded.y-bottomFeetY)<.00001f);
    std::cout<<"V17.438 pool ladder PASS: two actual V8 ladders, up/down, dry/wet boundary, no escape through wall\n";
}
