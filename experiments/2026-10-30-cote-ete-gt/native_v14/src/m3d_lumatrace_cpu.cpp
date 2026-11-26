#include "m3d_lumatrace_cpu.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace m3d {
namespace {
float srgbToLinear(float x) {
    x=clampf(x,0.f,1.f);
    return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);
}
V representativeAlbedo(const Scene& scene,std::size_t materialIndex) {
    if(materialIndex>=scene.mat.size())return {.45f,.45f,.45f};
    const Material& material=scene.mat[materialIndex];
    V color{.55f,.55f,.55f};
    if(!material.colorMip.empty()&&!material.colorMip.back().p.empty()) {
        const std::uint32_t rgb=material.colorMip.back().p.front();
        color={srgbToLinear(float((rgb>>16)&255u)/255.f),
               srgbToLinear(float((rgb>>8)&255u)/255.f),
               srgbToLinear(float(rgb&255u)/255.f)};
    } else if(!material.mip.empty()&&!material.mip.back().p.empty()) {
        const std::uint16_t rgb=material.mip.back().p.front();
        color={srgbToLinear(float((rgb>>11)&31u)/31.f),
               srgbToLinear(float((rgb>>5)&63u)/63.f),
               srgbToLinear(float(rgb&31u)/31.f)};
    }
    return color*(1.f-clampf(material.metallic,0.f,1.f))*.82f;
}
}

bool prepareLumaTraceCpuScene(const Scene& scene,const LumaTraceBvh4& bvh,
                              LumaTraceCpuScene& out,std::wstring& error) {
    out={};
    if(bvh.nodes.empty()||bvh.primitiveCount!=scene.order.size()) {
        error=L"LumaTrace CPU : le BVH4 ne correspond pas à la scène.";
        return false;
    }
    if(scene.obj.size()>std::numeric_limits<std::uint32_t>::max()) {
        error=L"LumaTrace CPU : trop de primitives pour les indices 32 bits.";
        return false;
    }

    constexpr std::uint32_t noTriangle=0xffffffffu;
    out.objectToTriangle.assign(scene.obj.size(),noTriangle);
    out.triangleAlbedo.reserve(scene.obj.size());
    for(std::size_t i=0;i<scene.obj.size();++i) {
        const Obj& object=scene.obj[i];
        if(object.kind!=Kind::Triangle) {
            out={};
            error=L"LumaTrace CPU : les primitives analytiques ne sont pas encore prises en charge.";
            return false;
        }
        const bool transparent=object.mat<scene.mat.size()&&
            (scene.mat[object.mat].transmissionPercent>0 || scene.mat[object.mat].waterSurface ||
             (!scene.mat[object.mat].surfaceAuthored && scene.mat[object.mat].name=="cse_pool_water"));
        if(!transparent) {
            if(out.triangleCount==noTriangle) {
                out={};
                error=L"LumaTrace CPU : dépassement du nombre de triangles.";
                return false;
            }
            out.objectToTriangle[i]=out.triangleCount++;
            const V albedo=representativeAlbedo(scene,std::size_t(object.mat));
            out.triangleAlbedo.push_back({albedo.x,albedo.y,albedo.z,1.f});
        }
    }

    out.primitiveOrder.reserve(scene.order.size());
    for(const std::uint32_t objectId:scene.order) {
        if(objectId>=out.objectToTriangle.size()) {
            out={};
            error=L"LumaTrace CPU : indice de primitive hors limites dans l’ordre BVH.";
            return false;
        }
        out.primitiveOrder.push_back(out.objectToTriangle[objectId]);
    }
    out.sceneIdentity=scene.gpuCacheIdentity;
    out.sceneRevision=scene.gpuCacheRevision;
    error.clear();
    return true;
}

} // namespace m3d
