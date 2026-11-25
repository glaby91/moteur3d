#pragma once
namespace m3d::lumatrace_detail {
inline constexpr char kComputeShader[] = R"HLSL(
struct Vertex { float3 p; uint normalPacked; float2 uv; uint material; uint primitiveId; };
struct Node { float3 lo; uint c0; float3 hi; uint c1; uint c2,c3,first,count,children,pad; };
float snorm10(uint bits){return (float)(bits&1023u)*(2.0/1023.0)-1.0;}
float3 decodeNormal(uint packed){return normalize(float3(snorm10(packed),snorm10(packed>>10),snorm10(packed>>20)));}
ByteAddressBuffer V : register(t0);
Vertex loadVertex(uint id){uint byteOffset=id*32u;Vertex v;v.p=float3(asfloat(V.Load(byteOffset)),asfloat(V.Load(byteOffset+4u)),asfloat(V.Load(byteOffset+8u)));v.normalPacked=V.Load(byteOffset+12u);v.uv=float2(asfloat(V.Load(byteOffset+16u)),asfloat(V.Load(byteOffset+20u)));v.material=V.Load(byteOffset+24u);v.primitiveId=V.Load(byteOffset+28u);return v;}
StructuredBuffer<uint> Order : register(t1);
StructuredBuffer<Node> Bvh : register(t2);
StructuredBuffer<float4> DirectRadiance : register(t3);
Texture2DArray<float4> Albedo : register(t4);
Texture2DArray<float4> PreservedAlbedo : register(t5);
SamplerState WrapSampler : register(s0);
RWStructuredBuffer<float4> Radiance : register(u0);
RWTexture3D<float4> ProbeOutput : register(u1);
RWTexture3D<float4> CoarseProbeOutput : register(u2);
Texture3D<float4> CoarseProbeTexture : register(t6);
cbuffer Params : register(b0) {
 uint4 Counts;
 float4 Lights[64];
 float4 Colors[64];
 uint4 ProbeDims;
 float4 ProbeOrigin;
 float4 ProbeSpan;
 float4 SunDirection;
 float4 SunColor;
 uint4 PreservedLayers[64];
};
static const float kPi=3.14159265359;
static const uint kProbeCoarseRayCount=2;
static const uint kProbeRefineRayCount=4;
static const uint kProbeTotalRayCount=kProbeCoarseRayCount+kProbeRefineRayCount;
bool bounds(Node n,float3 o,float3 invD,float maxT,out float nearT){
 float3 a=(n.lo-o)*invD,b=(n.hi-o)*invD,lo=min(a,b),hi=max(a,b);
 nearT=max(max(lo.x,lo.y),max(lo.z,0));return min(min(hi.x,hi.y),min(hi.z,maxT))>=nearT;}
bool triangleHit(uint id,float3 o,float3 d,float maxT,out float hitT,out float hitU,out float hitV){
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);float3 e1=b.p-a.p,e2=c.p-a.p,p=cross(d,e2);
 float det=dot(e1,p);if(abs(det)<1.e-8){hitT=0;hitU=0;hitV=0;return false;}float inv=rcp(det);float3 t=o-a.p;
 hitU=dot(t,p)*inv;if(hitU<0||hitU>1){hitT=0;hitV=0;return false;}float3 q=cross(t,e1);hitV=dot(d,q)*inv;
 if(hitV<0||hitU+hitV>1){hitT=0;return false;}hitT=dot(e2,q)*inv;return hitT>.015&&hitT<maxT;}
bool traceClosest(float3 o,float3 d,float maxT,uint source,out uint hitId,out float hitT,out float hitU,out float hitV){
 float3 sd=float3(abs(d.x)<1.e-8?1.e-8:d.x,abs(d.y)<1.e-8?1.e-8:d.y,abs(d.z)<1.e-8?1.e-8:d.z);float3 invD=rcp(sd);
 uint stack[64];uint top=1;stack[0]=0;hitId=0xffffffff;hitT=maxT;hitU=0;hitV=0;
 [loop]while(top){Node n=Bvh[stack[--top]];float nt;if(!bounds(n,o,invD,hitT,nt))continue;
  if(n.count){[loop]for(uint j=0;j<n.count;++j){uint id=Order[n.first+j];float t,u,v;
   if(id!=0xffffffff&&id!=source&&id<Counts.x&&triangleHit(id,o,d,hitT,t,u,v)&&t>.20){hitId=id;hitT=t;hitU=u;hitV=v;}}}
  else{uint kids[4]={n.c0,n.c1,n.c2,n.c3};[loop]for(uint k=0;k<n.children&&top<60;++k)stack[top++]=kids[k];}}
 return hitId!=0xffffffff;}
bool occluded(float3 o,float3 d,float maxT,uint source){
 float3 sd=float3(abs(d.x)<1.e-8?1.e-8:d.x,abs(d.y)<1.e-8?1.e-8:d.y,abs(d.z)<1.e-8?1.e-8:d.z);float3 invD=rcp(sd);
 uint stack[64];uint top=1;stack[0]=0;
 [loop]while(top){Node n=Bvh[stack[--top]];float nt;if(!bounds(n,o,invD,maxT,nt))continue;
  if(n.count){[loop]for(uint j=0;j<n.count;++j){uint id=Order[n.first+j];float t,u,v;
   if(id!=0xffffffff&&id!=source&&id<Counts.x&&triangleHit(id,o,d,maxT,t,u,v))return true;}}
  else{uint kids[4]={n.c0,n.c1,n.c2,n.c3};[loop]for(uint k=0;k<n.children&&top<60;++k)stack[top++]=kids[k];}}
 return false;}
float3 light(uint source,float3 p,float3 n,float4 l,float4 c){
 float3 dv=l.xyz-p;float d=length(dv),radius=max(l.w,.05);if(d<1.e-4||d>=radius)return 0;
 float3 dir=dv/d;float ndl=saturate(dot(n,dir));if(ndl<=0||occluded(p+n*.018,dir,max(.001,d-.035),source))return 0;
 float edge=saturate(1-d/radius);return c.rgb*(c.a*edge*edge*ndl*.27);}
float3 sunlight(uint source,float3 p,float3 n){
 if(SunColor.a<=0)return 0;float3 dir=SunDirection.xyz;float q=dot(dir,dir);if(q<1.e-8)return 0;dir*=rsqrt(q);
 float ndl=saturate(dot(n,dir));if(ndl<=0||occluded(p+n*.018,dir,1.e20,source))return 0;return SunColor.rgb*(SunColor.a*ndl);}
float3 normalAtHit(uint id,float u,float v,float3 rayDirection){
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);
 float3 n=normalize(decodeNormal(a.normalPacked)*(1-u-v)+decodeNormal(b.normalPacked)*u+decodeNormal(c.normalPacked)*v);
 return dot(n,rayDirection)>0?-n:n;}
float3 albedoAtHit(uint id,float u,float v){
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);
 float2 uv=a.uv*(1-u-v)+b.uv*u+c.uv*v;uint material=min(a.material,255u);
 uint layer=PreservedLayers[material>>2][material&3u];
 float3 color=layer==0xffffffffu?Albedo.SampleLevel(WrapSampler,float3(uv,(float)material),0).rgb:
     PreservedAlbedo.SampleLevel(WrapSampler,float3(uv,(float)layer),0).rgb;
 return saturate(color);}
float3 directAtHit(uint id,float3 p,float3 n){
 float3 sum=sunlight(id,p,n);[loop]for(uint i=0;i<min(Counts.w,64u);++i)sum+=light(id,p,n,Lights[i],Colors[i]);return sum;}
[numthreads(64,1,1)] void CSShadeTriangles(uint3 tid:SV_DispatchThreadID){uint id=tid.x;if(id>=Counts.x)return;
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);float3 p=(a.p+b.p+c.p)/3;
 float3 n=normalize(decodeNormal(a.normalPacked)+decodeNormal(b.normalPacked)+decodeNormal(c.normalPacked));float3 sum=0;
 [loop]for(uint i=0;i<min(Counts.w,64u);++i)sum+=light(id,p,n,Lights[i],Colors[i]);Radiance[id]=float4(sum,1);}
[numthreads(64,1,1)] void CSBuildProbeVolume(uint3 tid:SV_DispatchThreadID){
 uint id=tid.x;uint nx=ProbeDims.x,ny=ProbeDims.y,nz=ProbeDims.z;if(id>=nx*ny*nz)return;
 uint3 cell=uint3(id%nx,(id/nx)%ny,id/(nx*ny));float3 uv=(float3(cell)+.5)/float3(nx,ny,nz);
 const bool refine=ProbeDims.w!=0;
 float4 coarse=0;
 if(refine){coarse=CoarseProbeTexture.Load(int4(cell,0));if(coarse.a<=0.0)return;}
 float3 origin=ProbeOrigin.xyz+uv*ProbeSpan.xyz;float3 sum=0;uint hits=0;
 float rotation=(float)(id*747796405u+2891336453u)*2.3283064365386963e-10;
 const uint sampleCount=refine?kProbeRefineRayCount:kProbeCoarseRayCount;
 [unroll]for(uint s=0;s<sampleCount;++s){
  // Interleave coarse and refinement samples across the same deterministic
  // six-direction set so the first pass covers both hemispheres.
  uint sampleIndex=refine?(s==0?0u:s==1?2u:s==2?3u:5u):(s==0?1u:4u);
  float y=1-2*((sampleIndex+.5)/kProbeTotalRayCount);
  float r=sqrt(max(0,1-y*y));float phi=2*kPi*frac(rotation+sampleIndex*.61803398875);
  float3 d=float3(cos(phi)*r,y,sin(phi)*r);uint hit;float t,u,v;
  if(!traceClosest(origin,d,1.e20,0xffffffff,hit,t,u,v))continue;
  float3 p=origin+d*t,n=normalAtHit(hit,u,v,d);float facing=saturate(dot(n,-d));if(facing<=0)continue;
  // The raster shader lights surfaces with ambient fill, lamps and sun.
  // Record the hit surface's outgoing energy and its receiver zone.
  float3 irradiance=DirectRadiance[hit].rgb+sunlight(hit,p,n)+float3(.24,.24,.24);
  if(max(irradiance.x,max(irradiance.y,irradiance.z))<=1.e-5)continue;
  sum+=albedoAtHit(hit,u,v)*irradiance*facing;++hits;
 }
 if(!refine){
  // Coarse estimate; alpha stores the lit-hit fraction for the refinement gate.
  CoarseProbeOutput[cell]=float4(sum*(4.0/kProbeCoarseRayCount),
                                  (float)hits/kProbeCoarseRayCount);
 }else{
  // Restore the coarse sum from its normalized estimate, add the local samples,
  // then write one combined estimate for all six directions.
  float coarseHits=coarse.a*(float)kProbeCoarseRayCount;
  float coarseWeight=(float)kProbeCoarseRayCount*.25;
  float3 total=coarse.rgb*coarseWeight+sum;
  ProbeOutput[cell]=float4(total*(4.0/kProbeTotalRayCount),
                           (coarseHits+(float)hits)/(float)kProbeTotalRayCount);
 }
}
)HLSL";
}
