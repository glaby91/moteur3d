#version 450
// Instanced boxes: 36 shared cube vertices, 48 bytes per scene instance.
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in uint unusedMaterial;
layout(location=4) in vec3 boxLo;
layout(location=5) in float uvScale;
layout(location=6) in vec3 boxHi;
layout(location=7) in uint boxMaterial;
layout(location=8) in vec3 dimensions;
layout(location=9) in uint uvMode;
layout(set=0,binding=0,std140) uniform Frame {
 vec4 eye,forward,right,up,sun,sky;
 vec4 shadowBase,shadowRight,shadowUp;
 vec4 material[256], emission[256];
 vec4 lightPos[16],lightRgb[16];
 vec4 misc;
} f;
layout(location=0) out vec2 uv;
layout(location=1) out vec3 normal;
layout(location=2) out vec3 world;
layout(location=3) flat out uint material;
void main(){
 vec3 p=mix(boxLo,boxHi,step(vec3(0.5),inPosition));
 vec3 n=inNormal;
 if(uvMode==1u||uvMode==2u){
   vec2 localUV=inUV;
   if(n.z < -0.5 || n.x > 0.5)localUV.x=1.0-localUV.x;
   if(n.y > 0.5)localUV.y=1.0-localUV.y;
   vec2 extents=vec2((abs(n.x)>0.5?dimensions.z:dimensions.x)*0.5,
                      (abs(n.y)>0.5?dimensions.z:dimensions.y)*0.5);
   uv=uvMode==2u?localUV:localUV*extents;
 }else{
   uv=(abs(n.y)>=abs(n.x)&&abs(n.y)>=abs(n.z))
       ?p.xz*uvScale
       :(abs(n.x)>=abs(n.z)?p.zy*uvScale:p.xy*uvScale);
 }
 vec3 q=p-f.eye.xyz;
 float z=dot(q,f.forward.xyz);
 gl_Position=vec4(dot(q,f.right.xyz)/max(0.001,f.right.w),
                 -dot(q,f.up.xyz)*f.forward.w/max(0.001,f.right.w),
                 f.eye.w/(f.eye.w-f.up.w)*z-f.eye.w*f.up.w/(f.eye.w-f.up.w),
                 z);
 normal=n;world=p;material=min(boxMaterial,255u);
}
