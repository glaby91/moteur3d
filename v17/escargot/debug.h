#pragma once
#include "runtime.h"
#include <ostream>
namespace m3d::escargot::debug {
enum Flags:uint32_t {Snails=1,Footprints=2,Spirals=4,Rays=8,MatrixPoints=16};
struct Line {V a,b;uint32_t object=0;State state=State::Unknown;};
inline std::vector<Line> geometry(const Result& result,uint32_t flags){
    std::vector<Line> lines;
    auto line=[&](V a,V b,uint32_t id,State state){lines.push_back({a,b,id,state});};
    for(const auto& sf:result.surfaces){
        if(sf.deferred)continue;const uint32_t id=sf.candidate.object;
        const auto first=std::find_if(sf.samples.begin(),sf.samples.end(),[](const Sample& s){return s.receiver==State::Measured;});
        if(first==sf.samples.end())continue;
        const V n=first->normal,t=norm(cross(std::abs(n.y)<.9f?V{0,1,0}:V{1,0,0},n)),b=norm(cross(n,t));
        const float size=clampf(length(sf.candidate.hi-sf.candidate.lo)*.06f,.025f,.4f);
        const V origin=first->position+n*.015f;
        const State state=sf.complete?State::Measured:State::Unknown;
        auto p=[&](float x,float y){return origin+(t*x+b*y)*size;};
        if(flags&Snails){
            V prev=p(-.25f,0);
            for(int i=1;i<=64;++i){const float f=float(i)/64,a=f*5*PI;V q=p(-.25f+.65f*f*std::cos(a),.65f*f*std::sin(a));line(prev,q,id,state);prev=q;}
            line(p(-1,-.65f),p(1,-.65f),id,state);line(p(1,-.65f),p(1,.12f),id,state);
            line(p(1,.12f),p(.7f,.5f),id,state);line(p(1,.12f),p(1.3f,.5f),id,state);
        }
        if(flags&Footprints){
            // World-space measured lattice adjacency, never invent a point for a miss.
            for(int y=0;y<sf.rows;++y)for(int x=0;x<sf.cols;++x){const auto& s=sf.samples[size_t(y)*sf.cols+x];if(s.receiver!=State::Measured)continue;
                for(auto xy:{std::pair<int,int>{x+1,y},{x,y+1}})if(xy.first<sf.cols&&xy.second<sf.rows){const auto& q=sf.samples[size_t(xy.second)*sf.cols+xy.first];if(q.receiver==State::Measured)line(s.position,q.position,id,state);}
            }
        }
        if(flags&Spirals){
            const Sample* prev=nullptr;
            for(auto k:spiral(sf.cols,sf.rows)){const auto& s=sf.samples[k];if(s.receiver!=State::Measured){prev=nullptr;continue;}if(prev)line(prev->position+n*.003f,s.position+n*.003f,id,state);prev=&s;}
        }
        for(const auto& s:sf.samples)if(s.receiver==State::Measured){
            if((flags&Rays)&&s.directionValid){
                float far=0;for(const auto& v:s.values)far=std::max(far,v.second.depth);
                // Actual observed optical segment, or a short direction marker on a miss.
                line(s.position,s.position+s.direction*(far>0?far:size*3.f),id,s.complete?State::Measured:State::Unknown);
            }
            if(flags&MatrixPoints)line(s.position,s.position+s.normal*.02f,id,state);
        }
    }return lines;
}
// Debug is exported separately, NEVER inserted into Scene::obj or BVH4.
inline void obj(std::ostream& out,const std::vector<Line>& lines){
    size_t k=1;uint32_t last=0;State lastState=State::Zero;
    for(const auto& l:lines){
        if(l.object!=last||l.state!=lastState){out<<"g escargot_"<<l.object<<"_state_"<<int(l.state)<<'\n';last=l.object;lastState=l.state;}
        out<<"v "<<l.a.x<<' '<<l.a.y<<' '<<l.a.z<<"\nv "<<l.b.x<<' '<<l.b.y<<' '<<l.b.z<<"\nl "<<k<<' '<<k+1<<'\n';k+=2;
    }
}
}
