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
 if(f.shadowUp.w<2.0)return 1.0;
 vec3 q=p-f.shadowBase.xyz;
 float size=f.shadowUp.w;
 vec2 xy=vec2(dot(q,f.shadowRight.xyz),dot(q,f.shadowUp.xyz))*(size/(2.0*max(.1,f.shadowBase.w)))+size*.5-.5;
 ivec2 at=ivec2(round(xy));
 if(any(lessThan(at,ivec2(0)))||any(greaterThanEqual(at,ivec2(int(size)))))return 1.0;
 return f.shadowRight.w-dot(q,f.sun.xyz)<=texelFetch(shadowMap,at,0).r+.12?1.0:0.0;
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
 float len=min(f.atmosphereInfo.y,z/max(.05,dot(ray,f.forward.xyz)));
 float stepLen=len*.25,a=1.0-exp(-f.atmosphereInfo.x*stepLen),trans=1.0;
 vec3 scattered=vec3(0);
 float phase=clamp(dot(ray,f.sun.xyz),0.0,1.0);phase=.25+.75*phase*phase*phase*phase;
 for(int j=0;j<4;++j){
   vec3 p=f.eye.xyz+ray*((float(j)+.5)*stepLen);
   vec3 illumination=vec3(.026,.029,.034)*(.35+.65*f.sun.w);
   if(f.sun.w>.001)illumination+=vec3(1,.9,.75)*(.85*f.sun.w*phase*sunVisible(p));
   for(int k=0;k<4;++k){
     vec3 delta=f.fogPos[k].xyz-p;float radius=f.fogPos[k].w;
     if(radius>0.0&&dot(delta,delta)<radius*radius){
       float fade=clamp(1.0-length(delta)/radius,0.0,1.0);
       illumination+=f.fogRgb[k].rgb*(f.fogRgb[k].w*fade*fade*.20);
     }
   }
   scattered+=trans*a*illumination;trans*=1.0-a;
 }
 if(f.misc.y<.5&&trans<1.0)scattered=pow(max(scattered/(1.0-trans),vec3(0)),vec3(1.0/2.2))*(1.0-trans);
 result=vec4(scattered,1.0-trans);
}
