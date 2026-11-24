#pragma once

// Versioned DX11 shader feature module.
namespace m3d::dx11::shaders::sky {

inline constexpr char kCloudSkyShader[] = R"(
cbuffer Sky : register(b1) {
    float3 forward; float tanHalf;
    float3 right; float aspect;
    float3 up; float pad0;
    float3 sun; float day;
    float3 zenith; float opacity;
    float3 horizon; float coverage;
    float4 wind;
};
struct SkyOut {float4 pos:SV_POSITION; float2 ndc:TEXCOORD0;};
SkyOut VSSky(uint vid:SV_VertexID){
    SkyOut o;
    o.ndc=vid==0?float2(-1,-1):vid==1?float2(-1,3):float2(3,-1);
    o.pos=float4(o.ndc,0,1);
    return o;
}
float skyHash(float2 p){
    float3 q=frac(float3(p.xyx)*.1031);
    q+=dot(q,q.yzx+33.33);
    return frac((q.x+q.y)*q.z);
}
float skyNoise(float2 p){
    float2 k=floor(p),f=frac(p);f=f*f*(3-2*f);
    return lerp(lerp(skyHash(k),skyHash(k+float2(1,0)),f.x),
                lerp(skyHash(k+float2(0,1)),skyHash(k+1),f.x),f.y);
}
float4 PSSky(SkyOut i):SV_TARGET {
    if(wind.w>.5) return float4(0,0,0,0);
    float3 d=normalize(forward+right*(i.ndc.x*tanHalf)
                             +up*(i.ndc.y*tanHalf/aspect));
    float gradient=smoothstep(-.06,.60,d.y);
    float3 col=lerp(horizon,zenith,gradient);
    if(d.y>-.02){
        float2 dome=d.xz/max(d.y+.18,.18);
        float2 p=float2(dome.x*3.0,dome.y*2.0)+wind.xy*18.0;
        float n=.56*skyNoise(p*.73)+.30*skyNoise(p*1.53+float2(7.1,11.3))
               +.14*skyNoise(p*3.11-float2(3.7,8.2));
        float cloud=smoothstep(.60-coverage*.27,.73-coverage*.22,n)
                   *smoothstep(.01,.16,d.y);
        float3 cloudColor=lerp(float3(.018,.025,.045),
                                float3(.78,.84,.91),day);
        col=lerp(col,cloudColor,cloud*opacity);
    }
    // Stable world-space star field. It appears only in the night sky, with
    // soft subpixel points and natural occlusion from the animated clouds.
    if(d.y>0.0 && day<0.20) {
        float2 starUV=float2(atan2(d.z,d.x)/6.28318530718+0.5+wind.z/24.0,
            asin(clamp(d.y,-1.0,1.0))/3.14159265359+0.5)*float2(360.0,180.0);
        float2 starCell=floor(starUV);
        float chance=skyHash(starCell+float2(19.79,91.73));
        float2 center=float2(
            skyHash(starCell+float2(41.11,12.77)),
            skyHash(starCell+float2(13.47,59.19)));
        float radius=lerp(.10,.18,skyHash(starCell+float2(29.5,97.3)));
        float footprint=max(max(fwidth(starUV.x),fwidth(starUV.y))*.5,.018);
        float disc=1.0-smoothstep(radius-footprint,radius+footprint,
                                  length(frac(starUV)-center));
        float bright=lerp(.60,1.55,skyHash(starCell+float2(2.19,6.37)));
        float horizonFade=smoothstep(.09,.29,d.y);
        float night=1.0-smoothstep(.01,.18,day);
        float2 dome=d.xz/max(d.y+.18,.18);
        float2 p=float2(dome.x*3.0,dome.y*2.0)+wind.xy*18.0;
        float cloudNoise=.56*skyNoise(p*.73)
            +.30*skyNoise(p*1.53+float2(7.1,11.3))
            +.14*skyNoise(p*3.11-float2(3.7,8.2));
        float cloudMask=smoothstep(.60-coverage*.27,
                                   .73-coverage*.22,cloudNoise)*opacity;
        float3 hue=lerp(float3(.65,.79,1.0),float3(1.0,.93,.82),
            skyHash(starCell+float2(73.9,5.8)));
        col+=hue*disc*step(.987,chance)*bright*horizonFade*night*
            (1.0-saturate(cloudMask));
    }
    // The solar disc stays on the moving directional-light vector.
    if(sun.y>0) {
        float x=saturate(dot(d,normalize(sun)));
        float disc=smoothstep(.99984,.99997,x);
        float halo=pow(x,128.0)*.20;
        float warm=saturate(1.0-sun.y*3.2);
        float3 c=lerp(float3(.95,.96,1),float3(1,.63,.32),warm);
        col+=c*(disc*.75+halo)*day;
    }
    return float4(max(col,0),1);
}
)";

} // namespace m3d::dx11::shaders::sky
