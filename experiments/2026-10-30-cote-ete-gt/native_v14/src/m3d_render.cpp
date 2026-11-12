#include "m3d_engine.h"
#include "m3d_primary_quarter.h"

namespace m3d {
Camera camera(float angleDeg,float elevationDeg,float distance,int w,int h,V look){float a=angleDeg*PI/180,e=elevationDeg*PI/180;Camera c;c.eye={look.x+distance*std::sin(a)*std::cos(e),look.y+distance*std::sin(e),look.z+distance*std::cos(a)*std::cos(e)};c.f=norm(look-c.eye);c.r=norm(cross(c.f,{0,1,0}));c.u=cross(c.r,c.f);c.tanHalf=std::tan(25*PI/180);c.aspect=float(w)/h;c.pixelAngle=2*c.tanHalf/w;return c;}
static uint32_t pack(V c){uint32_t r=(uint32_t)clampf(c.x,0,255),g=(uint32_t)clampf(c.y,0,255),b=(uint32_t)clampf(c.z,0,255);return 0xff000000u|(r<<16)|(g<<8)|b;}
static uint32_t packHdr(V c){
 auto encode=[](float x){x=std::max(0.f,x);const float mapped=clampf((x*(2.51f*x+.03f))/(x*(2.43f*x+.59f)+.14f),0.f,1.f);const float srgb=mapped<=.0031308f?12.92f*mapped:1.055f*std::pow(mapped,1.f/2.4f)-.055f;return uint32_t(std::lround(clampf(srgb,0.f,1.f)*255.f));};
 return 0xff000000u|(encode(c.x)<<16)|(encode(c.y)<<8)|encode(c.z);
}
Stats render(const Scene&s,const ShadowMap&sm,const Camera&cam,int w,int h,std::vector<uint32_t>&pixels,unsigned threads,bool fog,const std::atomic<uint64_t>&generation,uint64_t token,unsigned samplesPerPixel,bool depthOfField,bool useAvx2,bool offlinePathTrace,unsigned maxBounces){
 if(w<=0||h<=0||size_t(w)>SIZE_MAX/size_t(h))return {};
 pixels.resize(size_t(w)*size_t(h));
 // This opt-in mode runs one real primary BVH ray per 2x2 pixels.
 // Multi-sample, DOF and offline path tracing retain their reference paths.
 const bool primaryQuarter=primaryQuarterEnabled()&&samplesPerPixel==1u&&
                           !depthOfField&&!offlinePathTrace;
 auto start=std::chrono::steady_clock::now();std::atomic<int>next{0};std::atomic<uint64_t>rays{0},tests{0},aabb{0},nodes{0},hits{0},skies{0},texels{0},depth{0},simdRayPackets{0},simdTrianglePackets{0},simdTextureSamples{0},simdLightBatches{0};
 int nx=(w+15)/16,ny=(h+15)/16,total=nx*ny;const unsigned grid=samplesPerPixel==16?4u:samplesPerPixel==4?2u:1u;const unsigned sampleCount=offlinePathTrace?std::clamp(samplesPerPixel,1u,4096u):grid*grid;
 static constexpr float pattern[16][2]={{.125f,.125f},{.625f,.375f},{.375f,.625f},{.875f,.875f},{.375f,.125f},{.875f,.375f},{.125f,.625f},{.625f,.875f},{.875f,.125f},{.375f,.375f},{.625f,.625f},{.125f,.875f},{.625f,.125f},{.125f,.375f},{.875f,.625f},{.375f,.875f}};
 auto worker=[&]{Stats local;local.avx2=useAvx2;for(;;){if(generation.load()!=token)return;int tile=next.fetch_add(1);if(tile>=total)break;int bx=(tile%nx)*16,by=(tile/nx)*16;
  for(int y=by;y<std::min(h,by+16);y++){int endX=std::min(w,bx+16);
   if(primaryQuarter){
    if((y&1)!=0)continue;
    for(int x=bx;x<endX;x+=2){
     const float fx=float(x)+.5f,fy=float(y)+.5f;
     const float rx=(2.f*fx/w-1.f)*cam.tanHalf;
     const float ry=(1.f-2.f*fy/h)*cam.tanHalf/cam.aspect;
     const Ray ray{cam.eye,norm(cam.f+cam.r*rx+cam.u*ry)};
     pixels[size_t(y)*w+x]=pack(traceColor(s,sm,cam,ray,local,fog));
    }
   }else if(useAvx2&&!depthOfField&&!offlinePathTrace){for(int x=bx;x<endX;){int count=std::min(8,endX-x);V acc[8]{};unsigned sampleIndex=0;
    for(unsigned sy=0;sy<grid;sy++)for(unsigned sx=0;sx<grid;sx++,sampleIndex++){if(generation.load()!=token)return;float ox=sampleCount==1?.5f:pattern[sampleIndex][0],oy=sampleCount==1?.5f:pattern[sampleIndex][1];Ray packet[8];
     if(count==8){primaryRays8Avx2(cam,y,x,w,h,ox,oy,packet);local.simdRayPackets++;}else for(int k=0;k<count;k++){float fx=x+k+ox,fy=y+oy,rx=(2*fx/w-1)*cam.tanHalf,ry=(1-2*fy/h)*cam.tanHalf/cam.aspect;packet[k]={cam.eye,norm(cam.f+cam.r*rx+cam.u*ry)};}
     for(int k=0;k<count;k++)acc[k]=acc[k]+traceColor(s,sm,cam,packet[k],local,fog);
    }
    for(int k=0;k<count;k++)pixels[size_t(y)*w+x+k]=pack(acc[k]*(1.f/sampleCount));x+=count;
   }}else{for(int x=bx;x<endX;x++){V accumulated{0,0,0};
    for(unsigned sampleIndex=0;sampleIndex<sampleCount;sampleIndex++){if(generation.load()!=token)return;
     const uint64_t sampleSeed=(uint64_t(uint32_t(x+1))*0x9e3779b185ebca87ull)^(uint64_t(uint32_t(y+1))*0xc2b2ae3d27d4eb4full)^(uint64_t(sampleIndex+1)*0x165667b19e3779f9ull);
     uint64_t rng=sampleSeed;auto random01=[&](){rng+=0x9e3779b97f4a7c15ull;uint64_t z=rng;z=(z^(z>>30))*0xbf58476d1ce4e5b9ull;z=(z^(z>>27))*0x94d049bb133111ebull;z^=z>>31;return float((z>>40)&0xffffffu)/16777216.f;};
     float ox=sampleCount==1?.5f:pattern[sampleIndex%16][0],oy=sampleCount==1?.5f:pattern[sampleIndex%16][1];
     if(offlinePathTrace){ox=random01();oy=random01();}
     float fx=x+ox,fy=y+oy,rayX=(2*fx/w-1)*cam.tanHalf,rayY=(1-2*fy/h)*cam.tanHalf/cam.aspect;Ray r{cam.eye,norm(cam.f+cam.r*rayX+cam.u*rayY)};
     if(depthOfField&&cam.apertureRadius>0){V lens;if(offlinePathTrace){float rad=cam.apertureRadius*std::sqrt(random01()),ang=2.f*PI*random01();lens=cam.eye+cam.r*(rad*std::cos(ang))+cam.u*(rad*std::sin(ang));}else{const float golden=2.39996323f,rad=cam.apertureRadius*std::sqrt((sampleIndex+.5f)/sampleCount),ang=sampleIndex*golden;lens=cam.eye+cam.r*(rad*std::cos(ang))+cam.u*(rad*std::sin(ang));}float focusT=cam.focusDistance/std::max(.05f,dot(r.d,cam.f));V focal=cam.eye+r.d*focusT;r={lens,norm(focal-lens)};}
     accumulated=accumulated+(offlinePathTrace?traceOfflineColor(s,sm,cam,r,local,fog,sampleSeed,maxBounces):traceColor(s,sm,cam,r,local,fog));
    }pixels[size_t(y)*w+x]=offlinePathTrace?packHdr(accumulated*(1.f/sampleCount)):pack(accumulated*(1.f/sampleCount));
   }}
  }}rays.fetch_add(local.rays);tests.fetch_add(local.tests);aabb.fetch_add(local.aabb);nodes.fetch_add(local.nodes);hits.fetch_add(local.hits);skies.fetch_add(local.sky);texels.fetch_add(local.texels);depth.fetch_add(local.depthReads);simdRayPackets.fetch_add(local.simdRayPackets);simdTrianglePackets.fetch_add(local.simdTrianglePackets);simdTextureSamples.fetch_add(local.simdTextureSamples);simdLightBatches.fetch_add(local.simdLightBatches);};
 threads=std::max(1u,std::min(threads,(unsigned)total));std::vector<std::thread>pool;for(unsigned i=0;i<threads;i++)pool.emplace_back(worker);for(auto&t:pool)t.join();
 if(primaryQuarter&&generation.load()==token)reconstructQuarterPixels(pixels,w,h);
 Stats st;st.primarySamples=primaryQuarter?primaryQuarterSampleCount(w,h):
                          uint64_t(w)*uint64_t(h)*sampleCount;
 st.rays=rays;st.tests=tests;st.aabb=aabb;st.nodes=nodes;st.hits=hits;st.sky=skies;st.texels=texels;st.depthReads=depth;st.simdRayPackets=simdRayPackets;st.simdTrianglePackets=simdTrianglePackets;st.simdTextureSamples=simdTextureSamples;st.simdLightBatches=simdLightBatches;st.threads=threads;st.avx2=useAvx2;st.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return st;
}

/* V16 progressive global illumination: preserve HDR linear radiance across
 * passes, then apply ACES/sRGB only to the preview. Every sample has a unique
 * per-pixel seed, regardless of pass partitioning or thread scheduling. */
Stats renderGiAccumulatePass(const Scene& scene,const ShadowMap& shadow,
    const Camera& cam,int w,int h,std::vector<V>& accum,
    std::vector<uint32_t>& preview,uint32_t firstSample,uint32_t batchSamples,
    unsigned threads,const std::atomic<uint64_t>& generation,uint64_t token,
    unsigned maxBounces){
 if(w<=0||h<=0||size_t(w)>SIZE_MAX/size_t(h)||
    !batchSamples||firstSample>4096||batchSamples>4096-firstSample)return {};
 const size_t n=size_t(w)*h;
 if(firstSample==0)accum.assign(n,V{});
 else if(accum.size()!=n)return {};
 preview.resize(n);
 const auto start=std::chrono::steady_clock::now();
 std::atomic<int> nextTile{0};
 std::atomic<uint64_t> rays{0},tests{0},aabb{0},hits{0};
 const int nx=(w+15)/16,ny=(h+15)/16,total=nx*ny;
 auto worker=[&]{
    Stats local;
    for(;;){
        if(generation.load(std::memory_order_relaxed)!=token)break;
        const int tile=nextTile.fetch_add(1);
        if(tile>=total)break;
        const int x0=(tile%nx)*16,y0=(tile/nx)*16;
        for(int y=y0;y<std::min(h,y0+16);++y)
        for(int x=x0;x<std::min(w,x0+16);++x){
            const size_t at=size_t(y)*w+x;
            V sum=accum[at];
            // A per-pixel Cranley-Patterson rotation of the R2 sequence gives
            // more uniform subpixel coverage than fresh uncorrelated jitter.
            const uint32_t pixSeed=uint32_t(x+1)*0x9e3779b9u^uint32_t(y+1)*0x85ebca6bu;
            auto mix=[](uint32_t v){v^=v>>16;v*=0x7feb352du;v^=v>>15;v*=0x846ca68bu;v^=v>>16;return v;};
            const float shiftX=float(mix(pixSeed)&0xffffffu)*(1.f/16777216.f);
            const float shiftY=float(mix(pixSeed^0xc2b2ae35u)&0xffffffu)*(1.f/16777216.f);
            for(uint32_t j=0;j<batchSamples;++j){
                const uint32_t sample=firstSample+j;
                const float u=float(sample)*.7548776662f+.5f+shiftX;
                const float v=float(sample)*.5698402909f+.5f+shiftY;
                const float ox=u-std::floor(u),oy=v-std::floor(v);
                const float rx=(2.f*(x+ox)/w-1.f)*cam.tanHalf;
                const float ry=(1.f-2.f*(y+oy)/h)*cam.tanHalf/cam.aspect;
                Ray ray{cam.eye,norm(cam.f+cam.r*rx+cam.u*ry)};
                const uint64_t seed=(uint64_t(uint32_t(x+1))*0x9e3779b185ebca87ull)^
                                    (uint64_t(uint32_t(y+1))*0xc2b2ae3d27d4eb4full)^
                                    (uint64_t(sample+1)*0x165667b19e3779f9ull);
                sum=sum+traceOfflineColor(scene,shadow,cam,ray,local,false,seed,maxBounces);
            }
            accum[at]=sum;
        }
    }
    rays.fetch_add(local.rays);tests.fetch_add(local.tests);
    aabb.fetch_add(local.aabb);hits.fetch_add(local.hits);
 };
 threads=std::max(1u,std::min(threads,unsigned(total)));
 std::vector<std::thread> pool;
 for(unsigned i=1;i<threads;++i)pool.emplace_back(worker);
 worker();
 for(auto& t:pool)t.join();
 if(generation.load(std::memory_order_relaxed)!=token)return {};
 const float inv=1.f/float(firstSample+batchSamples);
 for(size_t i=0;i<n;++i)preview[i]=packHdr(accum[i]*inv);
 Stats st;st.rays=rays;st.tests=tests;st.aabb=aabb;st.hits=hits;st.threads=threads;
 st.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 return st;
}

}
