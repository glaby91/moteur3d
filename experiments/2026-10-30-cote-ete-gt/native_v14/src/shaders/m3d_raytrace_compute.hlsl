// Pripyat / Moteur3D - D3D11 Compute Shader Model 5.0, no DXR.
// Ray tracing on mixed analytic primitives with real albedo, normal and roughness maps.
// Single-bounce direct shading, NOT full CPU GI/refraction; magenta means BVH overflow.
struct Ray { float3 origin; float3 direction; };
struct GpuBvhNode {float3 boundsMin; uint left; float3 boundsMax; uint right; uint first; uint count; uint pad0; uint pad1;};
struct GpuObject {
 float3 mn; uint kind;
 float3 mx; uint material;
 float3 a; float r;
 float3 b; float r1;
 float3 c; float k;
 float3 n0; float len;
 float3 n1; float len2;
 float3 n2; float pad0;
 float3 center; float pad1;
 float3 core; float pad2;
 float3 uv0; float pad3;
 float3 uv1; float pad4;
 float3 uv2; float pad5;
};
struct GpuMaterial { float4 shade; float4 params; };
cbuffer Frame : register(b0) {
 float3 eye; float tanHalfFov;
 float3 forward; float aspect;
 float3 right; float nearPlane;
 float3 up; float farPlane;
 uint width; uint height; uint nodeCount; uint objectCount;
 uint rootIndex; uint maxTraversal; uint reserved0; uint reserved1;
 float3 sunDir; float ambient;
 float3 skyZenith; float sunIntensity;
 float3 skyHorizon; float reservedSky;
};
StructuredBuffer<GpuBvhNode> Nodes : register(t0);
StructuredBuffer<GpuObject> Objects : register(t1);
StructuredBuffer<uint> PrimitiveOrder : register(t2);
StructuredBuffer<GpuMaterial> Materials : register(t3);
Texture2DArray<float4> AlbedoAtlas : register(t4);
Texture2DArray<float4> NormalAtlas : register(t5);
Texture2DArray<float4> RoughAtlas : register(t6);
SamplerState AtlasSampler : register(s0);
RWTexture2D<float4> Output : register(u0);
bool hitBounds(Ray r,float3 mn,float3 mx,float limit){
 float lo=max(nearPlane,0.0001),hi=limit;
 [unroll]for(uint k=0;k<3;k++){
  float d=r.direction[k],o=r.origin[k];
  if(abs(d)<1.e-9){if(o<mn[k]||o>mx[k])return false;}
  else{float a=(mn[k]-o)/d,b=(mx[k]-o)/d;
   lo=max(lo,min(a,b));hi=min(hi,max(a,b));if(hi<lo)return false;}
 }
 return true;
}
float3 normalBox(GpuObject o,float3 p){
 float3 a=abs(p-o.mn),b=abs(p-o.mx);
 float d=a.x;float3 n=float3(-1,0,0);
 if(b.x<d){d=b.x;n=float3(1,0,0);}
 if(a.y<d){d=a.y;n=float3(0,-1,0);}
 if(b.y<d){d=b.y;n=float3(0,1,0);}
 if(a.z<d){d=a.z;n=float3(0,0,-1);}
 if(b.z<d)n=float3(0,0,1);
 return n;
}
float sdRoundedBox(GpuObject o,float3 p){
 float3 q=abs(p-o.center)-o.core;
 return length(max(q,0.0))+min(max(q.x,max(q.y,q.z)),0.0)-o.r;
}
bool hitObject(Ray ray,GpuObject o,float limit,out float t,out float3 n,out float2 uv){
 t=limit;n=float3(0,1,0);uv=float2(0,0);
 float dir2=dot(ray.direction,ray.direction);
 if(o.kind==0){ // triangle
  float3 e1=o.b-o.a,e2=o.c-o.a,p=cross(ray.direction,e2);
  float det=dot(e1,p);
  if(abs(det)<1.e-10)return false;
  float3 q=ray.origin-o.a;
  float u=dot(q,p)/det;if(u<0||u>1)return false;
  float3 v=cross(q,e1);float w=dot(ray.direction,v)/det;
  if(w<0||u+w>1)return false;
  t=dot(e2,v)/det;
  n=normalize(o.n0*(1-u-w)+o.n1*u+o.n2*w);
  uv=o.uv0.xy*(1-u-w)+o.uv1.xy*u+o.uv2.xy*w;
 }else if(o.kind==5){ // ellipsoid
  if(min(o.b.x,min(o.b.y,o.b.z))<=0)return false;
  float3 p=(ray.origin-o.a)/o.b,d=ray.direction/o.b;
  float A=dot(d,d),B=dot(p,d),C=dot(p,p)-1,D=B*B-A*C;
  if(D<0||A<1.e-15)return false;
  float q=(-B-sqrt(D))/A;t=q>.0001?q:(-B+sqrt(D))/A;
  float3 h=ray.origin+ray.direction*t;
  n=normalize((h-o.a)/(o.b*o.b));
 }else if(o.kind==1||o.kind==2){ // box, rounded box
  float lo=-1.e25,hi=limit;
  [unroll]for(uint k=0;k<3;k++){
   float p=ray.origin[k],d=ray.direction[k],mn=o.mn[k],mx=o.mx[k];
   if(abs(d)<1.e-12){if(p<mn||p>mx)return false;}
   else{float a=(mn-p)/d,b=(mx-p)/d;
    lo=max(lo,min(a,b));hi=min(hi,max(a,b));if(lo>hi)return false;}
  }
  if(o.kind==1){t=lo>.0001?lo:hi;n=normalBox(o,ray.origin+ray.direction*t);}
  else{
   t=max(lo,.0001);bool ok=false;
   [loop]for(uint it=0;it<64&&t<hi;it++){
    float d=sdRoundedBox(o,ray.origin+ray.direction*t);
    if(d<.001){ok=true;break;}t+=max(d*.82,.0005);
   }
   if(!ok||t>=hi)return false;
   float3 q=ray.origin+ray.direction*t-o.center;
   float3 d=abs(q)-o.core,v=max(d,0.0)*sign(q);
   if(dot(v,v)>1.e-12)n=normalize(v);
   else if(d.x>=d.y&&d.x>=d.z)n=float3(q.x>=0?1:-1,0,0);
   else if(d.y>=d.z)n=float3(0,q.y>=0?1:-1,0);
   else n=float3(0,0,q.z>=0?1:-1);
  }
 }else if(o.kind==3){ // capsule
  float3 ba=o.b-o.a,oa=ray.origin-o.a;
  float baba=max(o.len2,1.e-10),rdba=dot(ray.direction,ba),oaba=dot(oa,ba);
  float A=baba*dir2-rdba*rdba,B=baba*dot(ray.direction,oa)-oaba*rdba;
  float C=baba*dot(oa,oa)-oaba*oaba-o.r*o.r*baba;
  float h=B*B-A*C;
  if(A>1.e-12&&h>=0){float tc=(-B-sqrt(h))/A,y=oaba+tc*rdba;if(tc>.0001&&y>0&&y<baba)t=tc;}
  [unroll]for(uint i=0;i<2;i++){
   float3 q=ray.origin-(i==0?o.a:o.b);
   float bb=dot(q,ray.direction),cc=dot(q,q)-o.r*o.r,D=bb*bb-dir2*cc;
   if(D>=0){float sq=sqrt(D),t0=(-bb-sq)/dir2,t1=(-bb+sq)/dir2;
    if(t0>.0001&&t0<t)t=t0;else if(t1>.0001&&t1<t)t=t1;
   }
  }
  float3 q=ray.origin+ray.direction*t-o.a;
  float axis=saturate(dot(q,ba)/baba);
  n=normalize(q-ba*axis);
 }else if(o.kind==4){ // cone/frustum
  float3 p=ray.origin-o.a;
  float z=dot(p,o.n0),zd=dot(ray.direction,o.n0);
  float3 q=p-o.n0*z,v=ray.direction-o.n0*zd;
  float radius=o.r+o.k*z,rd=o.k*zd;
  float A=dot(v,v)-rd*rd,B=dot(q,v)-radius*rd,C=dot(q,q)-radius*radius;
  if(abs(A)>1.e-12){float D=B*B-A*C;
   if(D>=0){float sq=sqrt(D);
    [unroll]for(uint i=0;i<2;i++){float x=i==0?(-B-sq)/A:(-B+sq)/A,h=z+x*zd;
     if(x>.0001&&x<t&&h>=0&&h<=o.len)t=x;}
   }
  }
  if(abs(zd)>1.e-12){
   [unroll]for(uint i=0;i<2;i++){
    float h=i==0?0:o.len,rad=i==0?o.r:o.r1,x=(h-z)/zd;
    if(x>.0001&&x<t){float3 qq=q+v*x;if(dot(qq,qq)<=rad*rad)t=x;}
   }
  }
  float3 hit=ray.origin+ray.direction*t-o.a;float ax=dot(hit,o.n0);
  if(ax<.0002)n=-o.n0;
  else if(ax>o.len-.0002)n=o.n0;
  else n=normalize(hit-o.n0*(ax+o.k));
 }else return false;
 return t>.0001&&t<limit;
}
float4 tracePrimary(Ray ray){
 if(nodeCount==0||rootIndex>=nodeCount)return float4(.02,.03,.06,1);
 uint stack[64],sp=0;stack[sp++]=rootIndex;
 float closest=farPlane;float3 normal=float3(0,1,0);uint material=0;
 float2 hitUV=float2(0,0);GpuObject hitObj=(GpuObject)0;
 bool found=false,overflow=false;uint visits=0;
 [loop]while(sp>0){
  if(++visits>maxTraversal){overflow=true;break;}
  uint index=stack[--sp];
  if(index>=nodeCount){overflow=true;break;}
  GpuBvhNode node=Nodes[index];
  if(!hitBounds(ray,node.boundsMin,node.boundsMax,closest))continue;
  if(node.count>0){
   [loop]for(uint k=0;k<node.count;k++){
    uint id=PrimitiveOrder[node.first+k];if(id>=objectCount){overflow=true;break;}
    GpuObject obj=Objects[id];float t;float3 n;float2 uv;
    if(hitObject(ray,obj,closest,t,n,uv)&&t<closest){
     closest=t;normal=n;material=obj.material;hitObj=obj;hitUV=uv;found=true;
    }
   }
  }else{
   if(sp+2>64){overflow=true;break;}
   stack[sp++]=node.left;stack[sp++]=node.right;
  }
  if(overflow)break;
 }
 if(overflow)return float4(1,0,1,1);
 if(!found){
  float t=saturate(ray.direction.y*.65+.38);
  float3 sky=lerp(skyHorizon,skyZenith,t);
  // At night preserve faint cool gradients; no pure black backdrop.
  sky=max(sky,float3(.012,.018,.034));
  if(ray.direction.y<-.00001){
   float groundT=-ray.origin.y/ray.direction.y;
   if(groundT>.0001&&groundT<farPlane){
    float3 ground=float3(.19,.20,.16)*(.23+ambient*.55);
    return float4(ground,1);
   }
  }
  return float4(sky,1);
 }
 GpuMaterial m=Materials[material];
 float3 p=ray.origin+ray.direction*closest;
 float3 n=normalize(dot(normal,ray.direction)>0?-normal:normal);
 float2 uv=hitUV;
 if(hitObj.kind!=0){
   float scale=m.params.x;
   if(hitObj.kind==5){
    float3 q=(p-hitObj.a)/max(hitObj.b,float3(.001,.001,.001));
    uv=float2(atan2(q.z,q.x)/(6.2831853)*2.5,(.5-asin(clamp(q.y,-1,1))/3.14159265)*2.5);
   }else if(hitObj.kind==3){
    float3 ba=hitObj.b-hitObj.a,q=p-hitObj.a;
    float h=saturate(dot(q,ba)/max(hitObj.len2,.00001));
    float3 radial=q-ba*h;
    uv=float2(atan2(radial.z,radial.x)/6.2831853,
        (h*sqrt(max(hitObj.len2,.001))+asin(clamp(radial.y/max(hitObj.r,.001),-1,1))*hitObj.r)*.55);
   }else if(hitObj.kind==4){
    float3 q=p-hitObj.a;
    uv=float2(atan2(q.z,q.x)/6.2831853*3,dot(q,hitObj.n0)*.84);
   }else{
    float3 an=abs(n);
    if(an.y>=an.x&&an.y>=an.z)uv=p.xz*scale;
    else if(an.x>=an.z)uv=p.zy*scale;
    else uv=p.xy*scale;
   }
 }
 float3 base=AlbedoAtlas.SampleLevel(AtlasSampler,float3(uv,material),0).rgb;
 float3 tangent,bitangent;
 if(hitObj.kind==0){
   float3 e1=hitObj.b-hitObj.a,e2=hitObj.c-hitObj.a;
   float2 d1=hitObj.uv1.xy-hitObj.uv0.xy,d2=hitObj.uv2.xy-hitObj.uv0.xy;
   float den=d1.x*d2.y-d1.y*d2.x;
   tangent=abs(den)>1.e-7?normalize((e1*d2.y-e2*d1.y)/den):normalize(cross(abs(n.y)>.9?float3(1,0,0):float3(0,1,0),n));
   bitangent=normalize(cross(n,tangent));
 }else{
   float3 an=abs(n);
   tangent=an.y>=an.x&&an.y>=an.z?float3(1,0,0):an.x>=an.z?float3(0,0,1):float3(1,0,0);
   tangent=normalize(tangent-n*dot(n,tangent));
   bitangent=normalize(cross(n,tangent));
 }
 float3 tsNormal=NormalAtlas.SampleLevel(AtlasSampler,float3(uv,material),0).rgb*2-1;
 n=normalize(n*max(tsNormal.z,.12)+tangent*tsNormal.x+bitangent*tsNormal.y);
 float rough=clamp(RoughAtlas.SampleLevel(AtlasSampler,float3(uv,material),0).r,.08,.98);
 float3 L=normalize(sunDir),V=normalize(-ray.direction),H=normalize(L+V);
 float ndl=saturate(dot(n,L)),ndv=saturate(dot(n,V)),ndh=saturate(dot(n,H));
 float vdh=saturate(dot(V,H));
 float a=max(.05,rough*rough),a2=a*a;
 float d=a2/(3.14159265*pow(max(.0001,ndh*ndh*(a2-1)+1),2));
 float k=pow(rough+1,2)/8;
 float gv=ndv/max(.0001,ndv*(1-k)+k),gl=ndl/max(.0001,ndl*(1-k)+k);
 float F0=clamp(m.shade.a,.02,.20);
 float F=F0+(1-F0)*pow(1-vdh,5);
 float spec=d*gv*gl*F/max(.1,4*ndl*ndv);
 float fill=.40+.23*ambient;
 float3 rgb=base*(fill+sunIntensity*.95*ndl)+float3(spec,spec,spec)*sunIntensity*.55;
 return float4(saturate(rgb),1);
}
[numthreads(8,8,1)]
void CSMain(uint3 tid:SV_DispatchThreadID){
 if(tid.x>=width||tid.y>=height)return;
 float2 uv=((float2(tid.xy)+.5)/float2(width,height))*2-1;
 Ray ray;ray.origin=eye;
 // Match CPU m3d_render.cpp and D3D11 raster: tanHalfFov is HORIZONTAL.
 // Keep vertical field of view divided by aspect, never multiplied into X.
 ray.direction=normalize(forward+right*uv.x*tanHalfFov-up*uv.y*tanHalfFov/aspect);
 Output[tid.xy]=tracePrimary(ray);
}
