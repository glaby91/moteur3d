#pragma once
namespace m3d::dx11 {
// Authored emission takes precedence over legacy aliases. Negative strength
// is the existing water surface sentinel; preserve its separate shading path.
template<class Material>
inline void applyAuthoredEmission(const Material& material,float& r,float& g,float& b,float& strength,float& glow) noexcept {
    if(!material.emissionAuthored || strength<0.f)return;
    r=material.emissionColor.x;g=material.emissionColor.y;b=material.emissionColor.z;
    strength=material.emissionIntensity;glow=0.f;
}
}
