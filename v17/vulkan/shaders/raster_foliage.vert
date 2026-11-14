#version 450
layout(location=0) in vec3 templatePosition;
layout(location=1) in vec3 templateNormal;
layout(location=2) in vec2 templateUV;
layout(location=3) in uint ignoredMaterial;
layout(location=4) in vec3 center;
layout(location=5) in uint instanceMaterial;
layout(location=6) in vec3 radii;
layout(location=7) in float instancePadding;
layout(set=0,binding=0,std140) uniform Frame {
 vec4 eye,forward,right,up,sun,sky;
 vec4 shadowBase,shadowRight,shadowUp;
 vec4 material[256], emission[256];
 vec4 lightPos[16],lightRgb[16];
 vec4 misc;
 vec4 textureDetail[256];
 vec4 skyZenith,skyHorizon,skyWind,skyViewport;
} f;
layout(location=0) out vec2 uv;
layout(location=1) out vec3 normal;
layout(location=2) out vec3 world;
layout(location=3) flat out uint material;
void main(){
 vec3 p=center+templatePosition*radii;
 vec3 n=normalize(templateNormal/max(abs(radii),vec3(0.00001)));
 vec3 q=p-f.eye.xyz;float z=dot(q,f.forward.xyz);
 gl_Position=vec4(dot(q,f.right.xyz)/max(0.001,f.right.w),
                 -dot(q,f.up.xyz)*f.forward.w/max(0.001,f.right.w),
                 f.eye.w/(f.eye.w-f.up.w)*z-f.eye.w*f.up.w/(f.eye.w-f.up.w),z);
 uv=templateUV;normal=n;world=p;material=min(instanceMaterial,255u);
}
