#pragma once
namespace m3d::dx11::shaders::atmosphere {
inline constexpr char code[]=R"(
cbuffer Fog : register(b2) {
 float4 eye,forward,right,up,sun,shadowBase,shadowRight,shadowUp;
 float4 dimensions,settings;
 float4 lightPos[4],lightRgb[4];
};
#ifdef ZATM_MSAA
Texture2DMS<float> depthTex : register(t0);
float rawDepth(int2 p){return depthTex.Load(p,0);}
#else
Texture2D<float> depthTex : register(t0);
float rawDepth(int2 p){return depthTex.Load(int3(p,0));}
#endif
Texture2D<float> shadowTex : register(t1);
Texture2D<float4> fogTex : register(t2);
float4 VS(uint id:SV_VertexID):SV_Position{
 return float4(id==2?3:-1,id==1?3:-1,0,1);
}
float zAt(int2 p){
 float d=rawDepth(clamp(p,int2(0,0),int2(dimensions.xy)-1));
 return .08*10000/max(.08,10000-d*(10000-.08));
}
float sunVisible(float3 p){
 if(settings.w<2)return 1;
 float3 q=p-shadowBase.xyz;
 float2 xy=float2(dot(q,shadowRight.xyz),dot(q,shadowUp.xyz))*(settings.w/(2*max(.1,shadowBase.w)))+settings.w*.5-.5;
 int2 at=int2(round(xy));
 if(any(at<0)||any(at>=int(settings.w)))return 1;
 return shadowRight.w-dot(q,sun.xyz)<=shadowTex.Load(int3(at,0))+.12?1:0;
}
float4 integrate(float2 uv,float z){
 float2 ndc=float2(2*uv.x-1,1-2*uv.y);
 float3 ray=normalize(forward.xyz+right.xyz*ndc.x*right.w+up.xyz*ndc.y*right.w/forward.w);
 float len=min(settings.z,z/max(.05,dot(ray,forward.xyz)));
 float stepLen=len*.25,a=1-exp(-settings.x*stepLen),trans=1;
 float3 scattered=0;
 float phase=saturate(dot(ray,sun.xyz));phase=.25+.75*phase*phase*phase*phase;
 [unroll] for(int j=0;j<4;++j){
   float3 p=eye.xyz+ray*((j+.5)*stepLen);
   float3 illumination=float3(.026,.029,.034)*(.35+.65*settings.y);
   if(settings.y>.001)illumination+=float3(1,.9,.75)*(.85*settings.y*phase*sunVisible(p));
   [unroll] for(int k=0;k<4;++k){
      float3 delta=lightPos[k].xyz-p;float radius=lightPos[k].w;
      if(radius>0 && dot(delta,delta)<radius*radius){
         float fade=saturate(1-length(delta)/radius);
         illumination+=lightRgb[k].rgb*(lightRgb[k].w*fade*fade*.20);
      }
   }
   scattered+=trans*a*illumination;trans*=1-a;
 }
 return float4(scattered,1-trans);
}
float4 IntegratePS(float4 position:SV_Position):SV_Target{
 float2 uv=position.xy/dimensions.zw;
 return integrate(uv,zAt(int2(uv*dimensions.xy)));
}
float4 CompositePS(float4 position:SV_Position):SV_Target{
 float2 uv=position.xy/dimensions.xy;
 float center=zAt(int2(position.xy));
 float2 p=uv*dimensions.zw-.5;int2 base=int2(floor(p));float2 fracPart=frac(p);
 float4 sum=0;float weight=0;
 [unroll]for(int j=0;j<2;++j)[unroll]for(int i=0;i<2;++i){
    int2 at=clamp(base+int2(i,j),int2(0,0),int2(dimensions.zw)-1);
    float2 u=(float2(at)+.5)/dimensions.zw;
    float z=zAt(int2(u*dimensions.xy));
    float w=(i?fracPart.x:1-fracPart.x)*(j?fracPart.y:1-fracPart.y);
    w*=1/(1+abs(min(z,settings.z)-min(center,settings.z))*12);
    sum+=fogTex.Load(int3(at,0))*w;weight+=w;
 }
 if(weight<.075)return integrate(uv,center);
 return sum/max(weight,.00001);
}
)";
}
