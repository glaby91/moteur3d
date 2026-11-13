#pragma once
// V17.311 — deterministic first-person walking controller.
// Independent of Qt/DirectX; BVH ray queries use the SAME scene geometry as
// the renderers. All coordinates are metres, Y up, origin at the feet.
#include "m3d_engine.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace m3d { namespace walk {

struct Settings {
    float eyeHeight=1.70f;
    float bodyHeight=1.76f;
    float radius=.23f;
    float maxStepUp=.34f;
    float maxStepDown=.48f;
    float minWalkableNormalY=.64f; // ~50 degrees, stair treads are flatter
    float gravity=9.81f;
    float maxSubstep=.085f; // avoid tunnelling through thin obstacles
};
struct State {
    V feet{};
    float verticalVelocity=0.f;
    bool grounded=false;
    // Keep a known safe landing point; a lost finite-ground contact must not
    // accelerate the player forever into empty world space.
    V lastSafeFeet{};
    bool hasSafeGround=false;
};
inline V eye(const State& state,const Settings& cfg={}) {
    return state.feet+V{0.f,cfg.eyeHeight,0.f};
}
// Return the highest *walkable* hit within a small height interval, stepping
// past non-walkable intersections (walls, underside of stair, ceiling).
inline bool support(const Scene& scene,float x,float z,float top,float bottom,
                    V& point,const Settings& cfg={}){
    if(!std::isfinite(x)||!std::isfinite(z)||top<=bottom)return false;
    V origin{x,top,z};
    for(unsigned attempt=0;attempt<16 && origin.y>bottom+.0001f;++attempt){
        Hit hit;
        closest(scene,Ray{origin,{0.f,-1.f,0.f}},hit,nullptr,false,-1,false);
        if(hit.id<0||hit.p.y<bottom-.001f)break;
        if(hit.n.y>=cfg.minWalkableNormalY){
            point=hit.p;
            return true;
        }
        origin.y=hit.p.y-.003f;
    }
    // For old V14 scenes where a virtual plane is requested intentionally.
    if(!scene.finiteGround && bottom<=0.f && top>=0.f){
        point={x,0.f,z};return true;
    }
    return false;
}
// V17.442 / ANO-014. Outdoor paving stones sit about 15 cm above their
// backing slab, separated by decorative 5-6 cm joints. The character's
// centre ray must not snap down to the backing slab while one foot is still
// over a neighbouring paver. Keep the previous footing only when there is
// a REAL nearby, walkable and almost coplanar surface. This is not a
// generalized step-height increase and cannot bridge a wide stair/ledge.
inline bool continuousPaverFooting(const Scene& scene,V next,float previousY,
                                   float detectedY,const Settings& cfg={}){
    const float drop=previousY-detectedY;
    if(drop<.018f || drop>.18f)return false;
    constexpr float offsets[4][2]={{.13f,0.f},{-.13f,0.f},
                                    {0.f,.13f},{0.f,-.13f}};
    for(const auto& offset:offsets){
        V nearby{};
        if(support(scene,next.x+offset[0],next.z+offset[1],
                   previousY+.035f,previousY-.035f,nearby,cfg)
           && std::abs(nearby.y-previousY)<.022f)
            return true;
    }
    return false;
}
inline bool spawn(const Scene& scene,V cameraEye,State& state,
                  const Settings& cfg={}){
    state=State{};
    state.feet=cameraEye-V{0.f,cfg.eyeHeight,0.f};
    V floor{};
    // Aerial preset may sit 65-85 m above the site. Scan far enough down
    // to the true terrain instead of leaving the player flying at 80 m.
    // Never snap UP through a ceiling or choose a floor above eye position.
    if(support(scene,cameraEye.x,cameraEye.z,
               cameraEye.y-.05f,cameraEye.y-320.f,floor,cfg)){
        state.feet.y=floor.y;
        state.grounded=true;
        state.lastSafeFeet=state.feet;
        state.hasSafeGround=true;
    }
    return state.grounded;
}
inline bool sideBlocked(const Scene& scene,V from,V to,
                        float feetY,const Settings& cfg={}){
    const V delta{to.x-from.x,0.f,to.z-from.z};
    const float d=length(delta);
    if(d<1e-6f)return false;
    const V direction=delta/d;
    const V side=norm(cross(direction,V{0.f,1.f,0.f}));
    // Sweep three rays across the person's width, at three heights. Exclude
    // stair risers below 34 cm so the stair-step support resolver can climb.
    for(float h:{.38f,.98f,1.63f}){
        for(float lateral:{-0.80f,0.f,0.80f}){
            V start=V{from.x,feetY+h,from.z}
                    +direction*(cfg.radius*.90f)+side*(lateral*cfg.radius);
            Hit wall{};
            closest(scene,Ray{start,direction},wall,nullptr,false,-1,false);
            if(wall.id>=0&&wall.t<d+.015f &&
               wall.n.y<cfg.minWalkableNormalY){
                return true;
            }
        }
    }
    return false;
}
inline bool ceilingBlocked(const Scene& scene,V feet,
                           const Settings& cfg={}){
    Hit top{};
    closest(scene,Ray{feet+V{0.f,.42f,0.f},{0.f,1.f,0.f}},top,
            nullptr,false,-1,false);
    return top.id>=0 && top.t<(cfg.bodyHeight-.42f);
}
inline void advance(const Scene& scene,State& state,V motion,float dt,
                    const Settings& cfg={}){
    dt=clampf(dt,0.f,.1f);
    // Some view presets start outside authored terrain (e.g. panorama).
    // Do not enable gravity without ever establishing a real surface.
    // Preserve horizontal movement so the user can enter the walkable site.
    if(!state.hasSafeGround){
        V firstFloor{};
        if(support(scene,state.feet.x,state.feet.z,
                   state.feet.y+cfg.maxStepUp+.01f,
                   state.feet.y-320.f,firstFloor,cfg)){
            state.feet.y=firstFloor.y;
            state.verticalVelocity=0.f;
            state.grounded=true;
            state.lastSafeFeet=state.feet;
            state.hasSafeGround=true;
        }else{
            state.verticalVelocity=0.f;
            state.grounded=false;
            state.feet.x+=motion.x;
            state.feet.z+=motion.z;
            return;
        }
    }
    const float distance=length(V{motion.x,0.f,motion.z});
    const unsigned substeps=std::max(1u,unsigned(std::ceil(distance/cfg.maxSubstep)));
    const V delta{motion.x/float(substeps),0.f,motion.z/float(substeps)};
    for(unsigned i=0;i<substeps;++i){
        // Axis-separated fallback permits sliding along walls.
        auto tryStep=[&](float dx,float dz)->bool{
            if(std::abs(dx)+std::abs(dz)<1e-7f)return false;
            const V from=state.feet;
            V next=from+V{dx,0.f,dz};
            V floor{};
            const bool supported=support(scene,next.x,next.z,
                from.y+cfg.maxStepUp+.01f,from.y-cfg.maxStepDown-.01f,
                floor,cfg);
            if(supported&&state.grounded){
                if(floor.y-from.y>cfg.maxStepUp+.001f)return false;
                // Preserve eye height over narrow paving joints only;
                // retain the actual lower floor for stairs and true ledges.
                next.y=continuousPaverFooting(scene,next,from.y,floor.y,cfg)
                           ?from.y:floor.y;
            }
            // While airborne keep falling; never teleport upwards to a
            // higher landing without the grounded step-up sequence.
            if(sideBlocked(scene,from,next,std::max(from.y,next.y),cfg)||
               ceilingBlocked(scene,next,cfg))return false;
            state.feet=next;
            if(supported&&state.grounded){
                state.verticalVelocity=0.f;
                state.lastSafeFeet=state.feet;
                state.hasSafeGround=true;
            }else if(!supported){
                state.grounded=false;
            }
            return true;
        };
        if(!tryStep(delta.x,delta.z)){
            if(std::abs(delta.x)>1e-7f)tryStep(delta.x,0.f);
            if(std::abs(delta.z)>1e-7f)tryStep(0.f,delta.z);
        }
    }
    // Ground contact must also be updated while standing still.
    V floor{};
    bool landed=support(scene,state.feet.x,state.feet.z,
        state.feet.y+cfg.maxStepUp+.01f,
        state.feet.y-cfg.maxStepDown-.01f,floor,cfg);
    if(landed&&state.grounded){
        state.feet.y=continuousPaverFooting(scene,state.feet,state.feet.y,
                                              floor.y,cfg)
                          ?state.feet.y:floor.y;
        state.verticalVelocity=0.f;
        state.lastSafeFeet=state.feet;
        state.hasSafeGround=true;
    }else{
        state.grounded=false;
        const float previousY=state.feet.y;
        state.verticalVelocity=std::max(-22.f,state.verticalVelocity-cfg.gravity*dt);
        const float proposedY=previousY+state.verticalVelocity*dt;
        V landing{};
        if(support(scene,state.feet.x,state.feet.z,
                   previousY+.02f,proposedY-.02f,landing,cfg)){
            state.feet.y=landing.y;
            state.verticalVelocity=0.f;
            state.grounded=true;
            state.lastSafeFeet=state.feet;
            state.hasSafeGround=true;
        }else{
            state.feet.y=proposedY;
            // Leaving a real ledge is allowed. Recover only once the player
            // falls below the *whole scene* rather than running away at -22 m/s.
            if(state.hasSafeGround && scene.root>=0 &&
               size_t(scene.root)<scene.node.size() &&
               state.feet.y<scene.node[size_t(scene.root)].mn.y-3.f){
                state.feet=state.lastSafeFeet;
                state.verticalVelocity=0.f;
                state.grounded=true;
            }
        }
    }
}
}} // namespace m3d::walk
