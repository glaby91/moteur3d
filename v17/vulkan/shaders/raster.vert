#version 450
// Vulkan 1.0 native raster backend: world -> clip follows V17 camera exactly.
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUV;
layout(location=3) in uint inMaterial;
layout(set=0,binding=0,std140) uniform Frame {
 vec4 eye,forward,right,up,sun,sky;
 vec4 shadowBase,shadowRight,shadowUp;
 vec4 material[256], emission[256];
 vec4 lightPos[16],lightRgb[16];
 vec4 misc;
 vec4 textureDetail[256];
 vec4 skyZenith,skyHorizon,skyWind,skyViewport;
 vec4 giOrigin,giInvSpan,giInfo;
} f;
layout(location=0) out vec2 uv;
layout(location=1) out vec3 normal;
layout(location=2) out vec3 world;
layout(location=3) flat out uint material;
void main() {
 vec3 q=inPosition-f.eye.xyz;
 float z=dot(q,f.forward.xyz);
 float safeZ=max(z,0.08);
 // Vulkan positive-height viewport: Y convention is opposite DX11.
 gl_Position=vec4(dot(q,f.right.xyz)/max(0.001,f.right.w),
                 -dot(q,f.up.xyz)*f.forward.w/max(0.001,f.right.w),
                 f.eye.w/(f.eye.w-f.up.w)*z - f.eye.w*f.up.w/(f.eye.w-f.up.w),
                 z);
 normal=inNormal; uv=inUV;world=inPosition;material=min(inMaterial,255u);
}
