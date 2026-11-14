#include "../frame_pacing.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main(){
    using namespace cse::pacing;
    // Strictly reject an obsolete scene, not an image from a camera that
    // moved while GPU raster was computing.
    assert(accept(false,false,true));
    assert(!accept(false,true,true));
    assert(!accept(true,false,true));
    assert(!accept(false,false,false));
    std::vector<Sample> frames;
    frames.push_back(Sample{0,0,12,0,0});
    for(int i=1;i<=100;i++){
        Sample x{};
        x.timestampMs=i*16;
        x.intervalMs=(i==93)?78:16;
        x.renderMs=12;
        frames.push_back(x);
    }
    const auto stats=summarize(frames,2,4);
    assert(stats.frames==101);
    assert(std::abs(stats.medianPresentMs-16)<1e-8);
    assert(stats.worstPresentMs==78);
    assert(stats.p99PresentMs>=16);
    assert(stats.stutterPercent==1.0);
    assert(stats.invalidated==2 && stats.queued==4);
    assert(stats.medianRenderMs==12);
    std::cout<<"PASS V17.437: independent visual cadence and render time, accepted moving camera\n";
}
