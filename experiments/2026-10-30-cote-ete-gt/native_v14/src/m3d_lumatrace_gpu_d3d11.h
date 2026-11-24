#pragma message("M3D_DIAG GPU header reached")
#if defined(_WIN32)
#pragma message("M3D_DIAG Windows guard active")
#ifndef M3D_LUMATRACE_GPU_D3D11_H
#define M3D_LUMATRACE_GPU_D3D11_H

#include "m3d_lumatrace_cpu.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <vector>

namespace m3d { namespace lumatrace_gpu {
using Microsoft::WRL::ComPtr;

// Owns only LumaTrace's D3D11 compute resources and dispatch. Raster geometry,
// swapchain, presentation and CPU BVH construction remain separate modules.
class Compute {
public:
    bool prepare(ID3D11Device* device,const LumaTraceBvh4& bvh,
                 const LumaTraceCpuScene& cpuScene,std::wstring& error);
    bool dispatch(ID3D11DeviceContext* context,ID3D11ShaderResourceView* vertexStream,
                  const Scene& scene,std::wstring& error);
    ID3D11ShaderResourceView* radianceSrv() const noexcept { return radianceSrv_.Get(); }

private:
    struct alignas(16) Params {
        uint32_t counts[4];
        float lights[64][4],colors[64][4];
    };
    static_assert(sizeof(Params)==2064,"LumaTrace HLSL constant packing");

    bool makeStructuredBuffer(ID3D11Device* device,const void* data,size_t count,
                              UINT stride,UINT bind,D3D11_USAGE usage,
                              ComPtr<ID3D11Buffer>& output);
    bool makeSrv(ID3D11Device* device,ID3D11Buffer* buffer,UINT count,
                 ComPtr<ID3D11ShaderResourceView>& output);
    void reset() noexcept;

    ComPtr<ID3D11Buffer> bvhBuffer_,orderBuffer_,albedoBuffer_,radianceBuffer_,paramsBuffer_;
    ComPtr<ID3D11ShaderResourceView> bvhSrv_,orderSrv_,albedoSrv_,radianceSrv_;
    ComPtr<ID3D11UnorderedAccessView> radianceUav_;
    ComPtr<ID3D11ComputeShader> shader_;
    std::array<uint32_t,sizeof(Params)/sizeof(uint32_t)> radianceKey_{};
    uint32_t triangleCount_=0,nodeCount_=0,orderCount_=0;
    uint64_t sceneIdentity_=0,sceneRevision_=0;
    bool radianceCacheValid_=false;
};

} } // namespace m3d::lumatrace_gpu

#endif // M3D_LUMATRACE_GPU_D3D11_H
#endif // _WIN32
