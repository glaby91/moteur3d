#include "m3d_engine.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace m3d {
RendererCapabilities detectRendererCapabilities() noexcept {
    RendererCapabilities capabilities{};
#if defined(_WIN32)
    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0,
                                 IID_PPV_ARGS(&device)))) return capabilities;
    ComPtr<ID3D12Device5> rayTracingDevice;
    if (FAILED(device.As(&rayTracingDevice))) return capabilities;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,
                                          &options, sizeof(options)))) return capabilities;
    capabilities.dxrHardware = options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
#endif
    // A DXR-capable adapter is not usable until this engine ships its own DXR backend.
    capabilities.dxrBackend = false;
    return capabilities;
}
}
