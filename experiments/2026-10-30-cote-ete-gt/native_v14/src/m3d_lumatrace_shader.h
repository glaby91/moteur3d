#pragma once
namespace m3d::lumatrace_detail {
inline constexpr char kComputeShader[] = R"HLSL(
struct Vertex { float3 p; uint normalPacked; float2 uv; uint material; uint primitiveId; };
struct Node { float3 lo; uint c0; float3 hi; uint c1; uint c2,c3,first,count,children,pad; };
float snorm10(uint bits){return (float)(bits&1023u)*(2.0/1023.0)-1.0;}
float3 decodeNormal(uint packed){return normalize(float3(snorm10(packed),snorm10(packed>>10),snorm10(packed>>20)));}
ByteAddressBuffer V : register(t0);
Vertex loadVertex(uint id){
 uint byteOffset=id*32u;Vertex v;
 v.p=float3(asfloat(V.Load(byteOffset)),asfloat(V.Load(byteOffset+4u)),asfloat(V.Load(byteOffset+8u)));
 v.normalPacked=V.Load(byteOffset+12u);
 v.uv=float2(asfloat(V.Load(byteOffset+16u)),asfloat(V.Load(byteOffset+20u)));
 v.material=V.Load(byteOffset+24u);v.primitiveId=V.Load(byteOffset+28u);return v;
}
StructuredBuffer<uint> Order : register(t1);
StructuredBuffer<Node> Bvh : register(t2);
StructuredBuffer<float4> Albedo : register(t3);
RWStructuredBuffer<float4> Radiance : register(u0);
cbuffer Params : register(b0) { uint4 Counts; float4 Lights[64]; float4 Colors[64]; float4 SunDirection; float4 SunColor; };
static const float kPi=3.14159265359;
static const uint kBounceSamples=4;
bool bounds(Node n,float3 o,float3 invD,float maxT,out float nearT){
 float3 a=(n.lo-o)*invD,b=(n.hi-o)*invD,lo=min(a,b),hi=max(a,b);
 nearT=max(max(lo.x,lo.y),max(lo.z,0));
 return min(min(hi.x,hi.y),min(hi.z,maxT))>=nearT;
}
bool triangleHit(uint id,float3 o,float3 d,float maxT,out float hitT,out float hitU,out float hitV){
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);
 float3 e1=b.p-a.p,e2=c.p-a.p,p=cross(d,e2);
 float det=dot(e1,p);if(abs(det)<1.e-8){hitT=0;hitU=0;hitV=0;return false;}
 float inv=rcp(det);float3 t=o-a.p;
 hitU=dot(t,p)*inv;if(hitU<0||hitU>1){hitT=0;hitV=0;return false;}
 float3 q=cross(t,e1);hitV=dot(d,q)*inv;
 if(hitV<0||hitU+hitV>1){hitT=0;return false;}
 hitT=dot(e2,q)*inv;return hitT>.015&&hitT<maxT;
}
bool traceClosest(float3 o,float3 d,float maxT,uint source,
                  out uint hitId,out float hitT,out float hitU,out float hitV){
 float3 safeD=float3(abs(d.x)<1.e-8?1.e-8:d.x,abs(d.y)<1.e-8?1.e-8:d.y,abs(d.z)<1.e-8?1.e-8:d.z);
 float3 invD=rcp(safeD);uint stack[64];uint top=1;stack[0]=0;
 hitId=0xffffffff;hitT=maxT;hitU=0;hitV=0;
 [loop]while(top){
  Node n=Bvh[stack[--top]];float nearT;
  if(!bounds(n,o,invD,hitT,nearT))continue;
  if(n.count){
   [loop]for(uint j=0;j<n.count;++j){
    uint id=Order[n.first+j];float t,u,v;
    if(id!=0xffffffff&&id!=source&&id<Counts.x&&triangleHit(id,o,d,hitT,t,u,v)){
     hitId=id;hitT=t;hitU=u;hitV=v;
    }
   }
  }else{
   uint kids[4]={n.c0,n.c1,n.c2,n.c3};
   [loop]for(uint k=0;k<n.children&&top<60;++k)stack[top++]=kids[k];
  }
 }
 return hitId!=0xffffffff;
}
bool occluded(float3 o,float3 d,float maxT,uint source){
 float3 safeD=float3(abs(d.x)<1.e-8?1.e-8:d.x,abs(d.y)<1.e-8?1.e-8:d.y,abs(d.z)<1.e-8?1.e-8:d.z);
 float3 invD=rcp(safeD);uint stack[64];uint top=1;stack[0]=0;
 [loop]while(top){
  Node n=Bvh[stack[--top]];float nearT;
  if(!bounds(n,o,invD,maxT,nearT))continue;
  if(n.count){
   [loop]for(uint j=0;j<n.count;++j){
    uint id=Order[n.first+j];float t,u,v;
    if(id!=0xffffffff&&id!=source&&id<Counts.x&&triangleHit(id,o,d,maxT,t,u,v))return true;
   }
  }else{
   uint kids[4]={n.c0,n.c1,n.c2,n.c3};
   [loop]for(uint k=0;k<n.children&&top<60;++k)stack[top++]=kids[k];
  }
 }
 return false;
}
float3 light(uint source,float3 p,float3 n,float4 l,float4 color){
 float3 dv=l.xyz-p;float d2=dot(dv,dv),r=max(l.w,.05),r2=r*r;
 if(d2<1.e-8||d2>=r2)return 0;
 float d=sqrt(d2);float3 dir=dv/d;float ndl=saturate(dot(n,dir));
 if(ndl<=0||occluded(p+n*.018,dir,max(.001,d-.035),source))return 0;
 float edge=saturate(1-d/r);
 return color.rgb*(ndl*color.a*edge*edge/(1+d2*.28));
}
float3 sunlight(uint source,float3 p,float3 n){
 float3 direction=normalize(SunDirection.xyz);
 float ndl=saturate(dot(n,direction));
 if(ndl<=0||SunColor.a<=0||occluded(p+n*.018,direction,1.e20,source))return 0;
 return SunColor.rgb*(SunColor.a*ndl);
}
float3 directAt(uint source,float3 p,float3 n){
 float3 sum=sunlight(source,p,n);
 [loop]for(uint i=0;i<min(Counts.w,64u);++i)
  sum+=light(source,p,n,Lights[i],Colors[i]);
 return sum;
}
float3 directBounceAt(uint source,float3 p,float3 n){
 // Keep the eight strongest local contributions for secondary-hit shading.
 uint selected[8];float scores[8];
 [unroll]for(uint k=0;k<8;++k){selected[k]=0xffffffff;scores[k]=0;}
 [loop]for(uint i=0;i<min(Counts.w,64u);++i){
  float3 dv=Lights[i].xyz-p;float d2=dot(dv,dv),r=max(Lights[i].w,.05);
  if(d2<1.e-8||d2>=r*r)continue;
  float d=sqrt(d2),cosLight=saturate(dot(n,dv/d));
  float edge=saturate(1-d/r);
  float score=Colors[i].a*cosLight*edge*edge/(1+d2*.28);
  uint weakest=0;
  [unroll]for(uint k=1;k<8;++k)if(scores[k]<scores[weakest])weakest=k;
  if(score>scores[weakest]){scores[weakest]=score;selected[weakest]=i;}
 }
 float3 sum=0;
 [unroll]for(uint k=0;k<8;++k)
  if(selected[k]!=0xffffffff)sum+=light(source,p,n,Lights[selected[k]],Colors[selected[k]]);
 sum+=sunlight(source,p,n);
 return sum;
}
uint hash32(uint x){
 x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;
}
float radicalInverse(uint bits){
 bits=(bits<<16)|(bits>>16);
 bits=((bits&0x55555555u)<<1)|((bits&0xAAAAAAAAu)>>1);
 bits=((bits&0x33333333u)<<2)|((bits&0xCCCCCCCCu)>>2);
 bits=((bits&0x0F0F0F0Fu)<<4)|((bits&0xF0F0F0F0u)>>4);
 bits=((bits&0x00FF00FFu)<<8)|((bits&0xFF00FF00u)>>8);
 return (float)bits*2.3283064365386963e-10;
}
float3 cosineDirection(float3 n,uint sampleIndex,uint seed){
 float u=(sampleIndex+.5)/kBounceSamples;
 float v=frac(radicalInverse(sampleIndex)+((seed&0xffffu)*(1.0/65536.0)));
 float r=sqrt(u),phi=2*kPi*v;
 float3 helper=abs(n.y)<.999?float3(0,1,0):float3(1,0,0);
 float3 tangent=normalize(cross(helper,n)),bitangent=cross(n,tangent);
 return normalize(tangent*(r*cos(phi))+bitangent*(r*sin(phi))+n*sqrt(max(0,1-u)));
}
float3 normalAtHit(uint id,float u,float v,float3 rayDirection){
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);
 float3 n=normalize(decodeNormal(a.normalPacked)*(1-u-v)+decodeNormal(b.normalPacked)*u+decodeNormal(c.normalPacked)*v);
 return dot(n,rayDirection)>0?-n:n;
}
[numthreads(64,1,1)] void CSShadeTriangles(uint3 tid:SV_DispatchThreadID){
 uint id=tid.x;if(id>=Counts.x)return;
 Vertex a=loadVertex(id*3u),b=loadVertex(id*3u+1u),c=loadVertex(id*3u+2u);
 float3 p=(a.p+b.p+c.p)/3;
 float3 n=normalize(decodeNormal(a.normalPacked)+decodeNormal(b.normalPacked)+decodeNormal(c.normalPacked));
 float3 direct=directAt(id,p,n),indirect=0;
 uint seed=hash32(id^0x68bc21ebu);
 [unroll]for(uint sampleIndex=0;sampleIndex<kBounceSamples;++sampleIndex){
  float3 direction=cosineDirection(n,sampleIndex,seed);
  uint hitId;float hitT,u,v;
  if(!traceClosest(p+n*.018,direction,1.e20,id,hitId,hitT,u,v))continue;
  float3 hitPosition=p+n*.018+direction*hitT;
  float3 hitNormal=normalAtHit(hitId,u,v,direction);
  float3 hitDirect=directBounceAt(hitId,hitPosition,hitNormal);
  // One diffuse bounce. Four deterministic cosine-weighted rays are cached
  // with the light state, so unchanged scenes pay this cost only once.
  indirect+=Albedo[id].rgb*Albedo[hitId].rgb*(hitDirect/kPi);
 }
 indirect/=kBounceSamples;
 Radiance[id]=float4(direct+indirect,1);
}
)HLSL";

} // namespace m3d::lumatrace_detail
