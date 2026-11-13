#pragma once
// CSE WARP - Qt 2D independent celestial compositor.
// The unchanged 3D foreground is provided by the opt-in DX11 alpha mask;
// no geometry, texture atlas or shadow map is rebuilt for weather frames.
#include "m3d_engine.h"
#include "solar_cycle.h"
#include <QImage>
#include <QPainter>
#include <QColor>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cse::weather {
class SkyCompositor {
public:
    static constexpr int Width=512,Height=288,Tile=512;
    SkyCompositor() : cloudAtlas_(Tile*Tile) {
        // Seamless, octave-layered fair-weather cirrus sampled in world direction.
        // One-time CPU work; the per-frame path only samples an 8-bit atlas.
        for(int y=0;y<Tile;++y)for(int x=0;x<Tile;++x){
            const float u=float(x)/Tile,v=float(y)/Tile;
            const float n=.58f*noise(u*6.f,v*6.f,6)
                         +.29f*noise(u*14.f,v*14.f,14)
                         +.13f*noise(u*30.f,v*30.f,30);
            cloudAtlas_[size_t(y)*Tile+x]=uint8_t(std::clamp(int(n*255.f),0,255));
        }
        // Stable pseudo-star catalog. Independent of the moving cirrus.
        uint32_t seed=0x92b82acdu;
        for(int i=0;i<1300;++i){
            const float bearing=random(seed)*6.283185307f;
            const float elevation=.035f+random(seed)*.955f;
            const float scale=std::sqrt(1.f-elevation*elevation);
            stars_.push_back({{scale*std::cos(bearing),elevation,scale*std::sin(bearing)},
                .45f+random(seed)*.75f,random(seed),random(seed)});
        }
    }
    void render(QImage& out,const m3d::Camera& cam,float hour,double elapsedSeconds,
                bool cloudsEnabled,bool fastWeather){
        if(out.size()!=QSize(Width,Height)||out.format()!=QImage::Format_RGB32)
            out=QImage(Width,Height,QImage::Format_RGB32);
        const auto sun=solarMotion(hour);
        const float day=sun.daylight;
        const float night=1.f-smoothstep(.02f,.20f,day);
        const float speed=fastWeather?10.f:1.f;
        // ~8 texture pixels/real second normally; x10 during the accelerated
        // six-minute diurnal cycle. Enough motion for smooth 30Hz perception.
        const float drift=float(std::fmod(elapsedSeconds*speed*.25,1024.0));
        const m3d::V sunDir{sun.east,sun.up,sun.north};
        const auto mix=[](float a,float b,float t){return a+(b-a)*t;};
        for(int y=0;y<Height;++y){
            uint32_t* row=reinterpret_cast<uint32_t*>(out.scanLine(y));
            const float ny=1.f-2.f*(float(y)+.5f)/Height;
            for(int x=0;x<Width;++x){
                const float nx=2.f*(float(x)+.5f)/Width-1.f;
                m3d::V d=cam.f+cam.r*(nx*cam.tanHalf)
                                  +cam.u*(ny*cam.tanHalf/cam.aspect);
                const float inv=1.f/std::max(.0001f,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));
                d=d*inv;
                const float h=smoothstep(-.06f,.60f,d.y);
                const float skyR=mix(mix(29.f,11.f,h),mix(219.f,122.f,h),day);
                const float skyG=mix(mix(35.f,17.f,h),mix(225.f,172.f,h),day);
                const float skyB=mix(mix(53.f,32.f,h),mix(230.f,217.f,h),day);
                float r=skyR,g=skyG,b=skyB;
                if(cloudsEnabled&&d.y>-.02f){
                    float cloud=cloudAt(d,drift);
                    const float opacity=.65f*smoothstep(.01f,.16f,d.y)*cloud;
                    r=mix(r,mix(7.f,200.f,day),opacity);
                    g=mix(g,mix(10.f,215.f,day),opacity);
                    b=mix(b,mix(16.f,232.f,day),opacity);
                }
                if(sun.up>0.f){
                    const float dot=d.x*sunDir.x+d.y*sunDir.y+d.z*sunDir.z;
                    const float disk=smoothstep(.99975f,.99994f,dot);
                    const float halo=dot>.85f?std::pow(dot,128.f)*.16f:0.f;
                    const float warm=1.f-smoothstep(.04f,.32f,sun.up);
                    r+=255.f*(disk*.80f+halo)*day;
                    g+=mix(247.f,168.f,warm)*(disk*.80f+halo)*day;
                    b+=mix(240.f,87.f,warm)*(disk*.80f+halo)*day;
                }
                row[x]=0xff000000u|(uint32_t(std::clamp(int(r),0,255))<<16)
                      |(uint32_t(std::clamp(int(g),0,255))<<8)
                      |uint32_t(std::clamp(int(b),0,255));
            }
        }
        if(night>.01f){
            QPainter painter(&out);
            painter.setRenderHint(QPainter::Antialiasing,true);
            constexpr float twoPi=6.283185307f;
            const float rotation=float(cycleHour(hour,0.0))*twoPi/24.f;
            const float co=std::cos(rotation),si=std::sin(rotation);
            for(const auto& star:stars_){
                // Sidereal-style rotation around the vertical axis.
                const m3d::V d{star.direction.x*co-star.direction.z*si,
                    star.direction.y,star.direction.x*si+star.direction.z*co};
                const float f=d.x*cam.f.x+d.y*cam.f.y+d.z*cam.f.z;
                if(f<.08f)continue;
                const float right=d.x*cam.r.x+d.y*cam.r.y+d.z*cam.r.z;
                const float up=d.x*cam.u.x+d.y*cam.u.y+d.z*cam.u.z;
                const float nx=right/(f*cam.tanHalf);
                const float ny=up*cam.aspect/(f*cam.tanHalf);
                if(std::abs(nx)>1.015f||std::abs(ny)>1.015f)continue;
                const float visible=night*smoothstep(.07f,.26f,d.y)*
                    (cloudsEnabled?(1.f-cloudAt(d,drift)*.78f):1.f);
                const int alpha=std::clamp(int(195.f*visible*star.brightness),0,255);
                if(alpha<8)continue;
                const QColor c=star.temperature>.5f
                    ?QColor(247,235,214,alpha):QColor(200,224,255,alpha);
                painter.setPen(Qt::NoPen);
                painter.setBrush(c);
                const qreal xx=(nx+1.f)*Width*.5f,yy=(1.f-ny)*Height*.5f;
                const qreal radius=.30+star.radius*.45;
                painter.drawEllipse(QPointF(xx,yy),radius,radius);
            }
        }
    }
private:
    struct Star{m3d::V direction;float radius,temperature,brightness;};
    std::vector<uint8_t> cloudAtlas_;
    std::vector<Star> stars_;
    static float smoothstep(float lo,float hi,float v){
        float t=std::clamp((v-lo)/(hi-lo),0.f,1.f);
        return t*t*(3.f-2.f*t);
    }
    static uint32_t hash(uint32_t x,uint32_t y){
        uint32_t h=x*0x9e3779b1u+y*0x85ebca77u+0x7f4a7c15u;
        h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
        return h;
    }
    static float random(uint32_t& state){
        state=state*1664525u+1013904223u;
        return float((state>>8)&0xffffffu)/16777216.f;
    }
    static float noise(float x,float y,int period){
        int ix=int(std::floor(x)),iy=int(std::floor(y));
        const float fx=x-std::floor(x),fy=y-std::floor(y);
        const float tx=fx*fx*(3.f-2.f*fx),ty=fy*fy*(3.f-2.f*fy);
        const auto at=[period](int xx,int yy){
            const uint32_t a=uint32_t((xx%period+period)%period);
            const uint32_t b=uint32_t((yy%period+period)%period);
            return float(hash(a,b)&65535u)/65535.f;
        };
        const float a=at(ix,iy)+(at(ix+1,iy)-at(ix,iy))*tx;
        const float b=at(ix,iy+1)+(at(ix+1,iy+1)-at(ix,iy+1))*tx;
        return a+(b-a)*ty;
    }
    float cloudAt(const m3d::V& direction,float drift)const{
        const float inv=1.f/std::max(direction.y+.18f,.18f);
        const float u=direction.x*inv*2.6f+drift;
        const float v=direction.z*inv*1.8f+drift*.31f;
        const int x=int(std::floor(u*32.f)),y=int(std::floor(v*32.f));
        const uint8_t v0=cloudAtlas_[size_t(y&(Tile-1))*Tile+(x&(Tile-1))];
        return smoothstep(.50f,.72f,float(v0)/255.f);
    }
};
} // namespace cse::weather
