#if defined(_WIN32)
#ifndef M3D_LUMATRACE_GPU_D3D11_H
#define M3D_LUMATRACE_GPU_D3D11_H

#include "m3d_lumatrace_cpu.h"
#include "m3d_lumatrace_adaptive.h"

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
                 const LumaTraceCpuScene& cpuScene,const Scene& scene,bool adaptive,std::wstring& error);
    bool dispatch(ID3D11DeviceContext* context,ID3D11ShaderResourceView* vertexStream,
                  ID3D11ShaderResourceView* indices,const Scene& scene,ID3D11ShaderResourceView* albedo,
                  ID3D11ShaderResourceView* preserved,ID3D11SamplerState* sampler,
                  const std::array<int32_t,256>& layers,bool probes,bool adaptive,std::wstring& error);
    const lumatrace_adaptive::Mask& zeroMask() const noexcept { return zeroMask_; }
    uint32_t zeroTriangles() const noexcept { return zeroTriangles_; }
    uint32_t triangleCount() const noexcept { return triangleCount_; }
    bool adaptiveEnabled() const noexcept { return adaptiveEnabled_; }
    ID3D11ShaderResourceView* radianceSrv() const noexcept { return radianceSrv_.Get(); }

    ID3D11ShaderResourceView* probeSrv() const noexcept { return probeSrv_.Get(); }
    V probeOrigin() const noexcept { return origin_; }
    V probeInvSpan() const noexcept { return {1.f/span_.x,1.f/span_.y,1.f/span_.z}; }
    void reset() noexcept;
private:
    struct alignas(16) Params {
        uint32_t counts[4];
        float lights[64][4],colors[64][4];
        uint32_t probeDims[4];
        float probeOrigin[4],probeSpan[4],sunDirection[4],sunColor[4];
        uint32_t preservedLayers[64][4];
        uint32_t geometryLayout[4];
        uint32_t adaptiveZero[512];
    };
    static_assert(sizeof(Params)==5232,"LumaTrace HLSL constant packing");

    bool makeStructuredBuffer(ID3D11Device* device,const void* data,size_t count,
                              UINT stride,UINT bind,D3D11_USAGE usage,
                              ComPtr<ID3D11Buffer>& output);
    bool makeSrv(ID3D11Device* device,ID3D11Buffer* buffer,UINT count,
                 ComPtr<ID3D11ShaderResourceView>& output);
    ComPtr<ID3D11Buffer> bvhBuffer_,orderBuffer_,radianceBuffer_,paramsBuffer_;
    ComPtr<ID3D11ShaderResourceView> bvhSrv_,orderSrv_,radianceSrv_,probeSrv_,coarseSrv_;
    ComPtr<ID3D11UnorderedAccessView> radianceUav_,probeUav_,coarseUav_;
    ComPtr<ID3D11Texture3D> probeTexture_,coarseTexture_;
    V origin_{},span_{1,1,1};
    ComPtr<ID3D11ComputeShader> shader_,probeShader_;
    std::array<uint32_t,sizeof(Params)/sizeof(uint32_t)> radianceKey_{};
    uint32_t triangleCount_=0,nodeCount_=0,orderCount_=0;
    uint64_t sceneIdentity_=0,sceneRevision_=0;
    bool radianceCacheValid_=false,probeCacheValid_=false;
    std::vector<lumatrace_adaptive::Bounds> adaptiveBounds_;
    lumatrace_adaptive::Mask zeroMask_{};
    std::array<uint32_t,513> adaptiveLightKey_{};
    bool adaptiveEnabled_=false,adaptiveMaskValid_=false,adaptiveBoundsPrepared_=false;
    uint32_t zeroTriangles_=0;
};

} } // namespace m3d::lumatrace_gpu

#endif // M3D_LUMATRACE_GPU_D3D11_H
#endif // _WIN32
