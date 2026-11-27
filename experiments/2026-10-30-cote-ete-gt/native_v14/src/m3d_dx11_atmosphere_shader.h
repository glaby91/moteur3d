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
float4 VS(uint id:SV_VertexID):SV_Position{
 return float4(id==2?3:-1,id==1?3:-1,0,1);
}
float zAt(int2 p){
 float d=rawDepth(clamp(p,int2(0,0),int2(dimensions.xy)-1));
 return .08*10000/max(.08,10000-d*(10000-.08));
}
float sunVisible(float3 p){
 if(settings.w<2)return 0;
 float3 q=p-shadowBase.xyz;
 float2 xy=float2(dot(q,shadowRight.xyz),dot(q,shadowUp.xyz))*(settings.w/(2*max(.1,shadowBase.w)))+settings.w*.5-.5;
 int2 at=int2(round(xy));
 if(any(at<0)||any(at>=int(settings.w)))return 0;
 return shadowRight.w-dot(q,sun.xyz)<=shadowTex.Load(int3(at,0))+.12?1:0;
}

// Bruit compact dans l'espace monde : nul au bord de chaque cellule.
// Le hash ne change jamais avec le temps ni avec les pixels de l'ecran.
float grain(float3 p,float cellSize){
 float3 g=p/cellSize;
 float3 v=(frac(g)-.5)/.38;
 float b=saturate(1-dot(v,v));
 if(b<=0)return 0;
 uint3 cell=asuint(int3(floor(g)));
 uint h=(cell.x*73856093u)^(cell.y*19349663u)^(cell.z*83492791u);
 h^=h>>16;h*=0x7feb352du;h^=h>>15;
 return (h&3u)==0u?b*b:0;
}
float3 dustLayer(float3 ray,float visibleDistance,float distance,float cellSize){
 if(visibleDistance<=distance)return 0;
 float footprint=2*distance*right.w/dimensions.x;
 float aa=smoothstep(1,2.5,.76*cellSize/max(.00001,footprint));
 if(aa<=0)return 0;
 float3 p=eye.xyz+ray*distance;
 float g=grain(p,cellSize)*aa*smoothstep(0,.25,visibleDistance-distance);
 [branch] if(g<=0)return 0;
 float3 illumination=0;
 [branch] if(settings.y>.001)illumination+=float3(1,.9,.75)*(1.5*settings.y*sunVisible(p));
 [unroll]for(int k=0;k<4;++k){
   float3 delta=lightPos[k].xyz-p;float radius=lightPos[k].w;
   float d2=dot(delta,delta);
   [branch]if(lightRgb[k].w>0&&radius>0&&d2<radius*radius){
     float fade=saturate(1-sqrt(d2)/radius);
     illumination+=lightRgb[k].rgb*(lightRgb[k].w*fade*fade);
   }
 }
 return g*illumination;
}
float4 CompositePS(float4 position:SV_Position):SV_Target{
 float2 uv=position.xy/dimensions.xy;
 float2 ndc=float2(2*uv.x-1,1-2*uv.y);
 float3 ray=normalize(forward.xyz+right.xyz*ndc.x*right.w+up.xyz*ndc.y*right.w/forward.w);
 float visibleDistance=zAt(int2(position.xy))/max(.05,dot(ray,forward.xyz));
 float3 dust=dustLayer(ray,visibleDistance,2.5,.06)+dustLayer(ray,visibleDistance,6.5,.14);
 return float4(clamp(dust*(settings.x*.116),0,.00077),0);
}
)";
}
