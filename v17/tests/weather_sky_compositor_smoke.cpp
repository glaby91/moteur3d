#include "../weather/sky_compositor.h"
#include <cassert>
int main(){
    m3d::Camera c{};
    c.f={0.f,0.f,-1.f};c.r={1.f,0.f,0.f};c.u={0.f,1.f,0.f};
    c.tanHalf=.55f;c.aspect=16.f/9.f;
    cse::weather::SkyCompositor compositor;
    QImage night,moved,day;
    compositor.render(night,c,23.f,0.,true,false);
    compositor.render(moved,c,23.f,6.,true,false);
    compositor.render(day,c,12.5f,6.,true,false);
    assert(!night.isNull()&&night.size()==QSize(512,288));
    assert(night.format()==QImage::Format_RGB32);
    assert(night!=moved); // Clouds are genuinely animated, not fixed pixels.
    assert(day!=night); // Dawn/day/night color and star changes are real.
    return 0;
}
