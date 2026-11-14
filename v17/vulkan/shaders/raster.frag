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
} f;
layout(set=0,binding=1) uniform sampler2DArray albedo;
layout(set=0,binding=2) uniform sampler2DArray normalRough;
layout(set=0,binding=3) uniform sampler2D shadowMap;
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
 vec3 base=texture(albedo,vec3(uv,float(m))).rgb;
 vec4 nm=texture(normalRough,vec3(uv,float(m)));
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
 float shade=visibility(world);
 float hemi=mix(0.75,1.1,clamp(n.y*0.5+0.5,0.0,1.0));
 vec3 color=base*((0.28+0.34*f.sun.w)*hemi+(0.56*ndotl)*shade);
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
 for(int i=0;i<16;i++){
   if(i>=int(f.misc.x))break;
   vec3 delta=f.lightPos[i].xyz-world;
   float dist=length(delta);
   float radius=max(0.05,f.lightPos[i].w);
   float falloff=pow(clamp(1.0-dist/radius,0.0,1.0),2.0);
   color+=base*f.lightRgb[i].rgb*f.lightRgb[i].a*falloff*max(0.0,dot(n,normalize(delta)))*0.38;
 }
 color+=f.emission[m].rgb*max(0.0,f.emission[m].a);
 color=mix(color,f.sky.rgb,clamp(length(world-f.eye.xyz)/1800.0,0.0,0.28));
 color=color/(color+vec3(1.0));
 color=pow(max(color,vec3(0.0)),vec3(1.0/2.2));
 result=vec4(color,clamp(f.materialProps[m].z,0.02,1.0));
}
