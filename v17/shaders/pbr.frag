#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;
flat in int vMaterial;
out vec4 fragColor;

uniform sampler2DArray uAlbedo;
uniform sampler2DArray uNormalMap;
uniform sampler2DArray uRoughnessMap;
uniform sampler1D uMaterialProps; // metallic, roughness multiplier, transmission, IOR
uniform sampler2D uShadowMap;
uniform mat4 uLightSpace;
uniform vec3 uCamera;
uniform vec3 uSun;            // direction towards the sun, world space
uniform vec3 uSunColor;       // linear radiance: warm direct sun
uniform vec3 uSkyColor;       // linear ambient environment
uniform vec3 uGroundFill;
uniform int uLightCount;
uniform vec4 uLightPositionRadius[8];
uniform vec4 uLightColorIntensity[8];
uniform float uExposure;
uniform float uTime;

// Direct PBR follows GGX/Smith/Schlick. This is a raster preview, not offline GI.
const float PI=3.14159265358979323846;
float dGGX(float NoH,float roughness){
    float a=max(0.045,roughness*roughness);
    float a2=a*a;
    float d=NoH*NoH*(a2-1.0)+1.0;
    return a2/max(0.0001,PI*d*d);
}
float gSchlick(float NoV,float k){
    return NoV/max(0.001,NoV*(1.0-k)+k);
}
float geometrySmith(float NoV,float NoL,float roughness){
    float r=roughness+1.0;
    float k=(r*r)/8.0;
    return gSchlick(NoV,k)*gSchlick(NoL,k);
}
vec3 fresnel(float VoH,vec3 f0){
    return f0+(1.0-f0)*pow(1.0-clamp(VoH,0.0,1.0),5.0);
}
vec3 brdf(vec3 base,float metallic,float roughness,vec3 N,vec3 V,vec3 L,vec3 Li){
    float NoL=max(dot(N,L),0.0),NoV=max(dot(N,V),0.001);
    if(NoL<=0.0)return vec3(0.0);
    vec3 H=normalize(V+L);
    float NoH=max(dot(N,H),0.0),VoH=max(dot(V,H),0.0);
    vec3 f0=mix(vec3(0.04),base,metallic);
    vec3 F=fresnel(VoH,f0);
    float D=dGGX(NoH,roughness);
    float G=geometrySmith(NoV,NoL,roughness);
    vec3 specular=D*G*F/max(0.001,4.0*NoV*NoL);
    vec3 diffuse=(1.0-F)*(1.0-metallic)*base/PI;
    return (diffuse+specular)*NoL*Li;
}
float visibility(vec3 world,vec3 N){
    vec4 projected=uLightSpace*vec4(world,1.0);
    vec3 coord=projected.xyz/max(0.0001,projected.w)*0.5+0.5;
    if(coord.x<0.0||coord.y<0.0||coord.x>1.0||coord.y>1.0||coord.z>1.0||coord.z<0.0)return 1.0;
    float bias=max(0.0012,0.0028*(1.0-max(dot(N,uSun),0.0)));
    vec2 texel=1.0/vec2(textureSize(uShadowMap,0));
    float lit=0.0;
    for(int y=-1;y<=1;++y)
      for(int x=-1;x<=1;++x){
        float nearest=texture(uShadowMap,coord.xy+vec2(x,y)*texel).r;
        lit+=coord.z-bias<=nearest?1.0:0.0;
      }
    return lit/9.0;
}
vec3 toneACES(vec3 x){
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
}
void main(){
    int layer=clamp(vMaterial,0,255);
    vec3 uv=vec3(fract(vUv),float(layer));
    vec3 base=texture(uAlbedo,uv).rgb; // GL_SRGB8_ALPHA8 -> linear
    vec4 props=texelFetch(uMaterialProps,layer,0);
    float metallic=clamp(props.x,0.0,1.0);
    float roughness=clamp(texture(uRoughnessMap,uv).r*props.y,0.06,0.99);
    float transmission=clamp(props.z,0.0,1.0);

    vec3 N=normalize(vNormal);
    vec3 tx=texture(uNormalMap,uv).xyz*2.0-1.0;
    vec3 dpdx=dFdx(vWorld),dpdy=dFdy(vWorld);
    vec2 duvdx=dFdx(vUv),duvdy=dFdy(vUv);
    float determinant=duvdx.x*duvdy.y-duvdx.y*duvdy.x;
    if(abs(determinant)>0.000001){
        vec3 tangent=normalize((dpdx*duvdy.y-dpdy*duvdx.y)/determinant);
        tangent=normalize(tangent-N*dot(N,tangent));
        vec3 rawBitangent=(-dpdx*duvdy.x+dpdy*duvdx.x)/determinant;
        vec3 bitangent=normalize(cross(N,tangent));
        if(dot(bitangent,rawBitangent)<0.0)bitangent=-bitangent;
        N=normalize(mat3(tangent,bitangent,N)*tx);
    }
    vec3 V=normalize(uCamera-vWorld);
    vec3 L=normalize(uSun);
    float shadow=visibility(vWorld,N);
    float hemisphere=clamp(N.y*0.5+0.5,0.0,1.0);
    vec3 ambient=mix(uGroundFill,uSkyColor,hemisphere);
    vec3 color=base*(1.0-metallic)*ambient*0.66;
    color+=brdf(base,metallic,roughness,N,V,L,uSunColor)*shadow*2.0;
    for(int i=0;i<8;++i){
        if(i>=uLightCount)break;
        vec3 delta=uLightPositionRadius[i].xyz-vWorld;
        float radius=max(0.01,uLightPositionRadius[i].w);
        float distanceToLight=length(delta);
        if(distanceToLight>=radius||distanceToLight<0.0001)continue;
        float falloff=clamp(1.0-distanceToLight/radius,0.0,1.0);
        vec3 intensity=uLightColorIntensity[i].rgb*uLightColorIntensity[i].w*
                       falloff*falloff*2.0;
        color+=brdf(base,metallic,roughness,N,V,delta/distanceToLight,intensity);
    }
    float fres=pow(1.0-max(dot(N,V),0.0),5.0);
    float alpha=1.0;
    if(transmission>0.01){
        // Thin architectural glass: bright environmental Fresnel, with the
        // opaque geometry still depth tested and visible through the pane.
        vec3 skyReflection=mix(uSkyColor,uSunColor*0.4,0.18+0.35*fres);
        color=mix(color,skyReflection,0.30+0.45*fres);
        alpha=clamp(1.0-transmission,0.08,0.82);
    }
    vec3 outputColor=pow(toneACES(max(vec3(0.0),color*uExposure)),vec3(1.0/2.2));
    fragColor=vec4(outputColor,alpha);
}
