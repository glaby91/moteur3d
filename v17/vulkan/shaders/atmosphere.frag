#version 450
layout(location=0) out vec4 result;
layout(set=0,binding=0,std140) uniform Frame {
 vec4 eye,forward,right,up,sun,sky;
 vec4 shadowBase,shadowRight,shadowUp;
 vec4 materialProps[256],emission[256],lightPos[64],lightRgb[64],misc,textureDetail[256];
 vec4 skyZenith,skyHorizon,skyWind,skyViewport,giOrigin,giInvSpan,giInfo;
 vec4 atmosphereInfo,fogPos[4],fogRgb[4];
} f;
layout(set=0,binding=3) uniform sampler2D shadowMap;
#ifdef ZATM_MSAA
layout(input_attachment_index=0,set=0,binding=8) uniform subpassInputMS depthInput;
#else
layout(input_attachment_index=0,set=0,binding=8) uniform subpassInput depthInput;
#endif
float sunVisible(vec3 p){
 if(f.shadowUp.w<2.0)return 0.0;
 vec3 q=p-f.shadowBase.xyz;
 float size=f.shadowUp.w;
 vec2 xy=vec2(dot(q,f.shadowRight.xyz),dot(q,f.shadowUp.xyz))*(size/(2.0*max(.1,f.shadowBase.w)))+size*.5-.5;
 ivec2 at=ivec2(round(xy));
 if(any(lessThan(at,ivec2(0)))||any(greaterThanEqual(at,ivec2(int(size)))))return 0.0;
 return f.shadowRight.w-dot(q,f.sun.xyz)<=texelFetch(shadowMap,at,0).r+.12?1.0:0.0;
}

// Meme noyau que DX11/LumaTrace : deux couches et grain compact fixe dans le monde.
float grain(vec3 p,float cellSize){
 vec3 g=p/cellSize,v=(fract(g)-.5)/.38;
 float b=clamp(1.0-dot(v,v),0.0,1.0);
 if(b<=0.0)return 0.0;
 uvec3 cell=uvec3(ivec3(floor(g)));
 uint h=(cell.x*73856093u)^(cell.y*19349663u)^(cell.z*83492791u);
 h^=h>>16;h*=0x7feb352du;h^=h>>15;
 return (h&3u)==0u?b*b:0.0;
}
vec3 dustLayer(vec3 ray,float visibleDistance,float distance,float cellSize){
 if(visibleDistance<=distance)return vec3(0);
 float footprint=2.0*distance*f.right.w/f.skyViewport.x;
 float aa=smoothstep(1.0,2.5,.76*cellSize/max(.00001,footprint));
 if(aa<=0.0)return vec3(0);
 vec3 p=f.eye.xyz+ray*distance;
 float g=grain(p,cellSize)*aa*smoothstep(0.0,.25,visibleDistance-distance);
 if(g<=0.0)return vec3(0);
 vec3 illumination=vec3(0);
 if(f.sun.w>.001)illumination+=vec3(1,.9,.75)*(1.5*f.sun.w*sunVisible(p));
 for(int k=0;k<4;++k){
   vec3 delta=f.fogPos[k].xyz-p;float radius=f.fogPos[k].w;
   float d2=dot(delta,delta);
   if(f.fogRgb[k].w>0.0&&radius>0.0&&d2<radius*radius){
     float fade=clamp(1.0-sqrt(d2)/radius,0.0,1.0);
     illumination+=f.fogRgb[k].rgb*(f.fogRgb[k].w*fade*fade);
   }
 }
 return g*illumination;
}
void main(){
#ifdef ZATM_MSAA
 float depth=subpassLoad(depthInput,0).r;
#else
 float depth=subpassLoad(depthInput).r;
#endif
 vec2 uv=gl_FragCoord.xy/f.skyViewport.xy;
 vec2 ndc=vec2(2.0*uv.x-1.0,1.0-2.0*uv.y);
 vec3 ray=normalize(f.forward.xyz+f.right.xyz*ndc.x*f.right.w+f.up.xyz*ndc.y*f.right.w/f.forward.w);
 float z=.08*10000.0/max(.08,10000.0-depth*(10000.0-.08));
 float visibleDistance=z/max(.05,dot(ray,f.forward.xyz));
 vec3 dust=dustLayer(ray,visibleDistance,2.5,.06)+dustLayer(ray,visibleDistance,6.5,.14);
 dust=clamp(dust*(f.atmosphereInfo.x*.116),vec3(0),vec3(.00077));
 // Le mode UNORM historique est deja en espace affiche; borne 1% preservee.
 if(f.misc.y<.5)dust*=12.92;
 result=vec4(dust,0);
}
