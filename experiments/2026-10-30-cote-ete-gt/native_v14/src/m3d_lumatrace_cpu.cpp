#include "m3d_lumatrace_cpu.h"

#include <limits>

namespace m3d {

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
    for(std::size_t i=0;i<scene.obj.size();++i) {
        const Obj& object=scene.obj[i];
        if(object.kind!=Kind::Triangle) {
            out={};
            error=L"LumaTrace CPU : les primitives analytiques ne sont pas encore prises en charge.";
            return false;
        }
        const bool transparent=object.mat<scene.mat.size()&&
            scene.mat[object.mat].transmissionPercent>0;
        if(!transparent) {
            if(out.triangleCount==noTriangle) {
                out={};
                error=L"LumaTrace CPU : dépassement du nombre de triangles.";
                return false;
            }
            out.objectToTriangle[i]=out.triangleCount++;
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
