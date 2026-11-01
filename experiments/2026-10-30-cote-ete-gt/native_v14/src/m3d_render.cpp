#include "m3d_engine.h"

namespace m3d {
Camera camera(float angleDeg,float elevationDeg,float distance,int w,int h){float a=angleDeg*PI/180,e=elevationDeg*PI/180;V look{0,1,-8.0f};Camera c;c.eye={look.x+distance*std::sin(a)*std::cos(e),look.y+distance*std::sin(e),look.z+distance*std::cos(a)*std::cos(e)};c.f=norm(look-c.eye);c.r=norm(cross(c.f,{0,1,0}));c.u=cross(c.r,c.f);c.tanHalf=std::tan(25*PI/180);c.aspect=float(w)/h;c.pixelAngle=2*c.tanHalf/w;return c;}
static uint32_t pack(V c){uint32_t r=(uint32_t)clampf(c.x,0,255),g=(uint32_t)clampf(c.y,0,255),b=(uint32_t)clampf(c.z,0,255);return 0xff000000u|(r<<16)|(g<<8)|b;}
Stats render(const Scene&s,const ShadowMap&sm,const Camera&cam,int w,int h,std::vector<uint32_t>&pixels,unsigned threads,bool fog,const std::atomic<uint64_t>&generation,uint64_t token,unsigned samplesPerPixel,bool depthOfField){
 auto start=std::chrono::steady_clock::now();std::atomic<int>next{0};std::atomic<uint64_t>rays{0},tests{0},aabb{0},nodes{0},hits{0},skies{0},texels{0},depth{0};
 int nx=(w+15)/16,ny=(h+15)/16,total=nx*ny;const unsigned grid=samplesPerPixel==16?4u:samplesPerPixel==4?2u:1u;const unsigned sampleCount=grid*grid;
 static constexpr float pattern[16][2]={{.125f,.125f},{.625f,.375f},{.375f,.625f},{.875f,.875f},{.375f,.125f},{.875f,.375f},{.125f,.625f},{.625f,.875f},{.875f,.125f},{.375f,.375f},{.625f,.625f},{.125f,.875f},{.625f,.125f},{.125f,.375f},{.875f,.625f},{.375f,.875f}};
 auto worker=[&]{Stats local;for(;;){if(generation.load()!=token)return;int tile=next.fetch_add(1);if(tile>=total)break;int bx=(tile%nx)*16,by=(tile/nx)*16;
  for(int y=by;y<std::min(h,by+16);y++)for(int x=bx;x<std::min(w,bx+16);x++){V accumulated{0,0,0};unsigned sampleIndex=0;
   for(unsigned sy=0;sy<grid;sy++)for(unsigned sx=0;sx<grid;sx++,sampleIndex++){if(generation.load()!=token)return;
    float ox=sampleCount==1?.5f:pattern[sampleIndex][0],oy=sampleCount==1?.5f:pattern[sampleIndex][1];
    float fx=x+ox,fy=y+oy,rayX=(2*fx/w-1)*cam.tanHalf,rayY=(1-2*fy/h)*cam.tanHalf/cam.aspect;
    Ray r{cam.eye,norm(cam.f+cam.r*rayX+cam.u*rayY)};
    if(depthOfField&&cam.apertureRadius>0){const float golden=2.39996323f,rad=cam.apertureRadius*std::sqrt((sampleIndex+.5f)/sampleCount),ang=sampleIndex*golden;V lens=cam.eye+cam.r*(rad*std::cos(ang))+cam.u*(rad*std::sin(ang));float focusT=cam.focusDistance/std::max(.05f,dot(r.d,cam.f));V focal=cam.eye+r.d*focusT;r={lens,norm(focal-lens)};}
    Hit hit;closest(s,r,hit,&local);local.rays++;if(hit.id==-1)local.sky++;else local.hits++;accumulated=accumulated+shade(s,sm,cam,r,hit,local,fog);
   }pixels[size_t(y)*w+x]=pack(accumulated*(1.f/sampleCount));
  }}rays.fetch_add(local.rays);tests.fetch_add(local.tests);aabb.fetch_add(local.aabb);nodes.fetch_add(local.nodes);hits.fetch_add(local.hits);skies.fetch_add(local.sky);texels.fetch_add(local.texels);depth.fetch_add(local.depthReads);};
 threads=std::max(1u,std::min(threads,(unsigned)total));std::vector<std::thread>pool;for(unsigned i=0;i<threads;i++)pool.emplace_back(worker);for(auto&t:pool)t.join();Stats st;st.rays=rays;st.tests=tests;st.aabb=aabb;st.nodes=nodes;st.hits=hits;st.sky=skies;st.texels=texels;st.depthReads=depth;st.threads=threads;st.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return st;
}

}
