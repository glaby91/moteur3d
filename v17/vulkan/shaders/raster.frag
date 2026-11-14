#version 450
layout(location=0) in vec2 uv;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 world;
layout(location=3) flat in uint material;
layout(location=0) out vec4 result;
layout(set=0,binding=0,std140) uniform Frame {
 vec4 eye,forward,right,up,sun,sky;
 vec4 shadowBase,shadowRight,shadowUp;
 vec4 materialProps[256], emission[256];
 vec4 lightPos[16],lightRgb[16];
 vec4 misc;
 vec4 textureDetail[256];
} f;
layout(set=0,binding=1) uniform sampler2DArray albedo;
layout(set=0,binding=2) uniform sampler2DArray normalRough;
layout(set=0,binding=3) uniform sampler2D shadowMap;
layout(set=0,binding=4) uniform sampler2DArray preservedAlbedo;
layout(set=0,binding=5) uniform sampler2DArray preservedNormalRough;
const float pi=3.14159265;
float visibility(vec3 p) {
 if(f.shadowUp.w<2.0) return 1.0;
 vec3 d=p-f.shadowBase.xyz;
 vec2 sc=vec2(dot(d,f.shadowRight.xyz),dot(d,f.shadowUp.xyz))
       *(f.shadowUp.w/(2.0*max(0.1,f.shadowBase.w)))+vec2(f.shadowUp.w*0.5-0.5);
 vec2 stepUV=vec2(1.0/f.shadowUp.w);
 vec2 at=(sc+0.5)*stepUV;
 if(any(lessThan(at,stepUV*2.0)) || any(greaterThan(at,vec2(1.0)-stepUV*2.0)))return 1.0;
 float dscene=f.shadowRight.w-dot(d,normalize(f.sun.xyz));
 float factor=0.0;
 for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++)
   factor+= dscene <= texture(shadowMap,at+vec2(x,y)*stepUV).r+0.095?1.0:0.0;
 return 0.16+0.84*factor/9.0;
}
void main() {
 int m=int(material);
 vec3 base;
 vec4 nm;
 float protectedLayer=f.textureDetail[m].x;
 if(protectedLayer<0.0) {
   // Preserve the exact automatic path for all existing materials.
   base=texture(albedo,vec3(uv,float(m))).rgb;
   nm=texture(normalRough,vec3(uv,float(m)));
 } else {
   // Compute derivatives before divergent texture selection. Protected
   // material always samples its full-resolution tier, even at distance.
   vec2 du=dFdx(uv), dv=dFdy(uv);
   base=textureGrad(preservedAlbedo,vec3(uv,protectedLayer),du,dv).rgb;
   nm=textureGrad(preservedNormalRough,vec3(uv,protectedLayer),du,dv);
 }
 vec3 n=normalize(normal);
 // Only compute tangent basis if a normal map exists.
 if(f.materialProps[m].w>0.5) {
   vec3 dx=dFdx(world),dy=dFdy(world);
   vec2 tx=dFdx(uv),ty=dFdy(uv);
   float det=tx.x*ty.y-tx.y*ty.x;
   if(abs(det)>1e-7) {
     vec3 t=normalize((dx*ty.y-dy*tx.y)/det);
     t=normalize(t-n*dot(n,t));
     vec3 b=normalize(cross(n,t))*sign(det);
     n=normalize(mat3(t,b,n)*(nm.xyz*2.0-1.0));
   }
 }
 vec3 L=normalize(f.sun.xyz);
 float ndotl=max(dot(n,L),0.0);
 // Preserve original shading when the experimental fast profile is disabled.
#ifdef CSE_WARP_VK_FAST_FRAGMENT
 // A back-facing surface has zero direct sun and zero solar specular;
 // its 9-tap shadow filter cannot change the output.
 float shade=ndotl>0.0?visibility(world):1.0;
#else
 float shade=visibility(world);
#endif
 float hemi=mix(0.75,1.1,clamp(n.y*0.5+0.5,0.0,1.0));
 vec3 color=base*((0.28+0.34*f.sun.w)*hemi+(0.56*ndotl)*shade);
#ifdef CSE_WARP_VK_FAST_FRAGMENT
 // Specular is exactly zero for ndotl=0. Skip normalization, Fresnel and pow.
 if(ndotl>0.0) {
#endif
 float rough=clamp(nm.a*f.materialProps[m].y,0.065,1.0);
 float metal=clamp(f.materialProps[m].x,0.0,1.0);
 vec3 V=normalize(f.eye.xyz-world);
 vec3 H=normalize(L+V);
 float nh=max(dot(n,H),0.0);
 float vh=max(dot(V,H),0.0);
 vec3 F0=mix(vec3(.04),base,metal);
 vec3 F=F0+(1.0-F0)*pow(1.0-vh,5.0);
 float a=rough*rough;
 float D=a*a/(pi*pow(max(0.0001,nh*nh*(a*a-1.0)+1.0),2.0));
 color+=F*D*ndotl*shade*0.24;
#ifdef CSE_WARP_VK_FAST_FRAGMENT
 }
#endif
 for(int i=0;i<16;i++){
   if(i>=int(f.misc.x))break;
   vec3 delta=f.lightPos[i].xyz-world;
#ifdef CSE_WARP_VK_FAST_FRAGMENT
   // Radius rejection uses dot(), avoiding sqrt() and normalization outside
   // influence. It retains the original attenuation inside the light radius.
   float radius=max(0.05,f.lightPos[i].w);
   float distSquared=dot(delta,delta);
   if(distSquared>=radius*radius)continue;
   float dist=sqrt(distSquared);
   float fade=clamp(1.0-dist/radius,0.0,1.0);
   float falloff=fade*fade;
#else
   float dist=length(delta);
   float radius=max(0.05,f.lightPos[i].w);
   float falloff=pow(clamp(1.0-dist/radius,0.0,1.0),2.0);
#endif
   color+=base*f.lightRgb[i].rgb*f.lightRgb[i].a*falloff*max(0.0,dot(n,normalize(delta)))*0.38;
 }
 color+=f.emission[m].rgb*max(0.0,f.emission[m].a);
 color=mix(color,f.sky.rgb,clamp(length(world-f.eye.xyz)/1800.0,0.0,0.28));
 if(f.misc.y<0.5){
   // Original Vulkan: legacy UNORM framebuffer, old tone/gamma path.
   color=color/(color+vec3(1.0));
   color=pow(max(color,vec3(0.0)),vec3(1.0/2.2));
 }else{
   // DX11 parity experiment: DX11 color RTV is sRGB, and its default
   // shader emits LINEAR lighting with no manual gamma/pow.
   // The Vulkan _SRGB attachment performs the encode exactly once.
   color=max(color,vec3(0.0));
 }
 result=vec4(color,clamp(f.materialProps[m].z,0.02,1.0));
}
