#pragma once
namespace m3d::lumatrace_detail {
inline constexpr char kComputeShader[] = R"HLSL(
struct Vertex { float3 p; float3 n; float2 uv; uint material; uint primitiveId; };
struct Node { float3 lo; uint c0; float3 hi; uint c1; uint c2,c3,first,count,children,pad; };
StructuredBuffer<Vertex> V : register(t0);
StructuredBuffer<uint> Order : register(t1);
StructuredBuffer<Node> Bvh : register(t2);
RWStructuredBuffer<float4> Radiance : register(u0);
cbuffer Params : register(b0) { uint4 Counts; float4 LightA; float4 ColorA; float4 LightB; float4 ColorB; };
bool bounds(Node n,float3 o,float3 invD,float maxT,out float nearT){
 float3 a=(n.lo-o)*invD,b=(n.hi-o)*invD,lo=min(a,b),hi=max(a,b);
 nearT=max(max(lo.x,lo.y),max(lo.z,0));return min(min(hi.x,hi.y),min(hi.z,maxT))>=nearT;}
bool triangle(uint id,float3 o,float3 d,float maxT){
 Vertex a=V[id*3],b=V[id*3+1],c=V[id*3+2];float3 e1=b.p-a.p,e2=c.p-a.p,p=cross(d,e2);
 float det=dot(e1,p);if(abs(det)<1.e-8)return false;float inv=rcp(det);float3 t=o-a.p;
 float u=dot(t,p)*inv;if(u<0||u>1)return false;float3 q=cross(t,e1);float v=dot(d,q)*inv;
 if(v<0||u+v>1)return false;float hit=dot(e2,q)*inv;return hit>.015&&hit<maxT;}
bool occluded(float3 o,float3 d,float maxT,uint source){
 float3 sd=float3(abs(d.x)<1.e-8?1.e-8:d.x,abs(d.y)<1.e-8?1.e-8:d.y,abs(d.z)<1.e-8?1.e-8:d.z);
 float3 invD=rcp(sd);uint stack[64];uint top=1;stack[0]=0;
 [loop]while(top){Node n=Bvh[stack[--top]];float nt;if(!bounds(n,o,invD,maxT,nt))continue;
  if(n.count){[loop]for(uint j=0;j<n.count;++j){uint id=Order[n.first+j];
   if(id!=0xffffffff&&id!=source&&id<Counts.x&&triangle(id,o,d,maxT))return true;}}
  else{uint kids[4]={n.c0,n.c1,n.c2,n.c3};[loop]for(uint k=0;k<n.children&&top<60;++k)stack[top++]=kids[k];}}
 return false;}
float3 light(uint id,float3 p,float3 n,float4 l,float3 color){float3 dv=l.xyz-p;float d=length(dv);
 if(d<1.e-4)return 0;float3 dir=dv/d;float ndl=saturate(dot(n,dir));
 if(ndl<=0||occluded(p+n*.018,dir,d,id))return 0;
 return color*(ndl*l.w/(1+d*d*.28));}
[numthreads(64,1,1)] void CSShadeTriangles(uint3 tid:SV_DispatchThreadID){uint id=tid.x;if(id>=Counts.x)return;
 Vertex a=V[id*3],b=V[id*3+1],c=V[id*3+2];float3 p=(a.p+b.p+c.p)/3,n=normalize(a.n+b.n+c.n);
 Radiance[id]=float4(light(id,p,n,LightA,ColorA.rgb)+light(id,p,n,LightB,ColorB.rgb),1);}
)HLSL";
}
