// V17.311 — walk / collision / stair traversal CPU-only deterministic test.
#include "src/m3d_walk_controller.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace m3d;
using namespace m3d::walk;

static void tri(Scene& s,V a,V b,V c){
    Obj o{};
    o.kind=Kind::Triangle;o.a=a;o.b=b;o.c=c;
    o.n0=o.n1=o.n2=norm(cross(b-a,c-a));
    o.mn={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    o.mx={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    s.obj.push_back(o);
}
static void top(Scene& s,float x0,float x1,float z0,float z1,float y){
    V a{x0,y,z0},b{x0,y,z1},c{x1,y,z1},d{x1,y,z0};
    tri(s,a,b,c);tri(s,a,c,d);
}
static void wall(Scene& s,float x,float z0,float z1,float topY){
    V a{x,0,z0},b{x,0,z1},c{x,topY,z1},d{x,topY,z0};
    tri(s,a,b,c);tri(s,a,c,d);
}
static void done(Scene& s){
    s.finiteGround=true;
    Node leaf{};
    leaf.start=0;leaf.count=int(s.obj.size());
    leaf.mn={-7,-1,-7};leaf.mx={7,5,7};
    s.node.push_back(leaf);s.root=0;
    for(unsigned i=0;i<s.obj.size();++i)s.order.push_back(i);
}
static void require(bool condition,const char* reason){
    if(!condition){std::fprintf(stderr,"WALK TEST FAILED: %s\n",reason);std::exit(1);}
}
int main(){
    Scene scene;
    top(scene,-5,5,-5,5,0.f);
    // 3 x 20 cm steps in a short stairway (up and down).
    top(scene,-.65f,.65f,-.5f,0.f,.2f);
    top(scene,-.65f,.65f,-1.f,-.5f,.4f);
    top(scene,-.65f,.65f,-1.5f,-1.f,.6f);
    wall(scene,2.f,-5.f,5.f,3.f);
    done(scene);
    State state{};
    require(spawn(scene,{0.f,1.7f,2.f},state),"no starting floor");
    require(std::abs(eye(state).y-1.7f)<.01f,"incorrect 1.70m eye height");
    for(int i=0;i<33;++i)advance(scene,state,{0.f,0.f,-.10f},.04f);
    require(state.grounded,"lost contact while climbing stairs");
    require(state.feet.y>.55f&&state.feet.y<.65f,"unable to climb 20cm stair treads");
    for(int i=0;i<33;++i)advance(scene,state,{0.f,0.f,.10f},.04f);
    require(state.grounded&&std::abs(state.feet.y)<.02f,"unable to descend stairs");
    for(int i=0;i<30;++i)advance(scene,state,{.10f,0.f,0.f},.04f);
    require(state.feet.x<1.81f,"walking camera crossed solid wall");
    require(std::abs(eye(state).y-1.7f)<.02f,"eye height changed on flat floor");
    const float xBefore=state.feet.x;
    for(int i=0;i<10;++i)advance(scene,state,{0.f,0.f,-.10f},.04f);
    require(std::abs(state.feet.x-xBefore)<.02f,"wall sliding drifted into obstacle");
    // Keep original presets and fly navigation independent from walker.
    std::puts("V17 WALK PASS: 1.70m eye, 20cm stair ascent/descent, wall collision and slide");
    return 0;
}
