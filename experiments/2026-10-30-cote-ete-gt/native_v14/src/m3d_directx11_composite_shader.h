#pragma once

// Versioned DX11 shader feature module.
namespace m3d::dx11::shaders::composite {

inline constexpr char kPeripheralComposite[] = R"(
Texture2D<float4> lo : register(t0);
Texture2D<float4> hi : register(t1);
SamplerState bilinear : register(s0);
struct CV {float4 p:SV_POSITION;float2 uv:TEXCOORD;};
CV VSC(uint id:SV_VertexID) {
    CV v;v.uv=float2(id==2?2.:0.,id==1?2.:0.);
    v.p=float4(v.uv.x*2.-1.,1.-v.uv.y*2.,0.,1.);return v;
}
float4 PSC(CV v):SV_TARGET {
    // Camera-centred elliptical focus: approximately half the picture stays
    // razor sharp, followed by a soft 16%-radius transition like a headlamp.
    // The edge always uses fresh pixels from this frame (never reprojection).
    float2 focus=(v.uv-.5)/float2(.47,.48);
    float radius=length(focus);
    float fade=smoothstep(.84,1.0,radius);
    return lerp(hi.Sample(bilinear,v.uv),lo.Sample(bilinear,v.uv),fade);
}
)";

} // namespace m3d::dx11::shaders::composite
