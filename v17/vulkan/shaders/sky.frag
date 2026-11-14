#version 450
// Port of V17 DirectX11 kCloudSkyShader: shared world-locked sky, no scene IDs.
layout(early_fragment_tests) in; // Skip shader for pixels covered by prepass depth.
layout(location=0) out vec4 outColor;
layout(set=0,binding=0,std140) uniform Frame {
 vec4 eye,forward,right,up,sun,sky;
 vec4 shadowBase,shadowRight,shadowUp;
 vec4 material[256], emission[256];
 vec4 lightPos[16],lightRgb[16];
 vec4 misc;
 vec4 textureDetail[256];
 vec4 skyZenith,skyHorizon,skyWind,skyViewport;
} f;
float skyHash(vec2 p){
 vec3 q=fract(vec3(p.x,p.y,p.x)*.1031);
 q+=dot(q,q.yzx+33.33);
 return fract((q.x+q.y)*q.z);
}
float skyNoise(vec2 p){
 vec2 k=floor(p),t=fract(p);
 t=t*t*(3.0-2.0*t);
 return mix(mix(skyHash(k),skyHash(k+vec2(1,0)),t.x),
            mix(skyHash(k+vec2(0,1)),skyHash(k+vec2(1,1)),t.x),t.y);
}
float cirrus(vec3 d){
 vec2 dome=d.xz/max(d.y+.18,.18);
 vec2 p=vec2(dome.x*3.0,dome.y*2.0)+f.skyWind.xy*18.0;
 return .56*skyNoise(p*.73)+.30*skyNoise(p*1.53+vec2(7.1,11.3))
        +.14*skyNoise(p*3.11-vec2(3.7,8.2));
}
void main(){
 if(f.skyViewport.z>.5){outColor=vec4(f.sky.rgb,1.0);return;}
 // Vulkan viewport height is positive (Y points downward); reconstruct
 // D3D11 NDC Y-up for the sky ray from window-space pixel centers.
 vec2 ndc=vec2(gl_FragCoord.x*2.0/max(f.skyViewport.x,1.0)-1.0,
               1.0-gl_FragCoord.y*2.0/max(f.skyViewport.y,1.0));
 vec3 d=normalize(f.forward.xyz+f.right.xyz*(ndc.x*f.right.w)
                      +f.up.xyz*(ndc.y*f.right.w/max(f.forward.w,.001)));
 float gradient=smoothstep(-.06,.60,d.y);
 vec3 col=mix(f.skyHorizon.rgb,f.skyZenith.rgb,gradient);
 float coverage=f.skyHorizon.a, opacity=f.skyZenith.a, day=f.sun.w;
 if(d.y>-.02){
   float n=cirrus(d);
   float cloud=smoothstep(.60-coverage*.27,.73-coverage*.22,n)
              *smoothstep(.01,.16,d.y);
   vec3 cloudColor=mix(vec3(.018,.025,.045),vec3(.78,.84,.91),day);
   col=mix(col,cloudColor,cloud*opacity);
 }
 if(d.y>0.0 && day<.20){
   vec2 starUV=vec2(atan(d.z,d.x)/6.28318530718+.5+f.skyWind.z/24.0,
     asin(clamp(d.y,-1.0,1.0))/3.14159265359+.5)*vec2(360.0,180.0);
   vec2 starCell=floor(starUV);
   float chance=skyHash(starCell+vec2(19.79,91.73));
   vec2 center=vec2(skyHash(starCell+vec2(41.11,12.77)),
                     skyHash(starCell+vec2(13.47,59.19)));
   float radius=mix(.10,.18,skyHash(starCell+vec2(29.5,97.3)));
   float footprint=max(max(fwidth(starUV.x),fwidth(starUV.y))*.5,.018);
   float disc=1.0-smoothstep(radius-footprint,radius+footprint,
                             length(fract(starUV)-center));
   float bright=mix(.60,1.55,skyHash(starCell+vec2(2.19,6.37)));
   float horizonFade=smoothstep(.09,.29,d.y);
   float night=1.0-smoothstep(.01,.18,day);
   float cloudMask=smoothstep(.60-coverage*.27,.73-coverage*.22,cirrus(d))*opacity;
   vec3 hue=mix(vec3(.65,.79,1.0),vec3(1.0,.93,.82),
                 skyHash(starCell+vec2(73.9,5.8)));
   col+=hue*disc*step(.987,chance)*bright*horizonFade*night*
        (1.0-clamp(cloudMask,0.0,1.0));
 }
 if(f.sun.y>0.0){
   float x=clamp(dot(d,normalize(f.sun.xyz)),0.0,1.0);
   float disc=smoothstep(.99984,.99997,x);
   float halo=pow(x,128.0)*.20;
   float warm=clamp(1.0-f.sun.y*3.2,0.0,1.0);
   vec3 sunColor=mix(vec3(.95,.96,1.0),vec3(1.0,.63,.32),warm);
   col+=sunColor*(disc*.75+halo)*day;
 }
 // Default sRGB framebuffer performs hardware encoding once.
 outColor=vec4(max(col,vec3(0.0)),1.0);
}
