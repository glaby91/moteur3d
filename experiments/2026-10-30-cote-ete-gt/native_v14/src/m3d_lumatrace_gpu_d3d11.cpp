#include "m3d_lumatrace_gpu_d3d11.h"

#if defined(_WIN32)

#include "m3d_lumatrace_shader.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace m3d::lumatrace_gpu {

void Compute::reset() noexcept {
    bvhBuffer_.Reset();orderBuffer_.Reset();radianceBuffer_.Reset();paramsBuffer_.Reset();
    bvhSrv_.Reset();orderSrv_.Reset();radianceSrv_.Reset();radianceUav_.Reset();shader_.Reset();
    triangleCount_=nodeCount_=orderCount_=0;
    sceneIdentity_=sceneRevision_=0;
    radianceCacheValid_=false;
}

bool Compute::makeStructuredBuffer(ID3D11Device* device,const void* data,size_t count,
                                   UINT stride,UINT bind,D3D11_USAGE usage,
                                   ComPtr<ID3D11Buffer>& output) {
    if(!device||!count||!stride||count>std::numeric_limits<UINT>::max()/stride)return false;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth=UINT(count*stride);
    desc.Usage=usage;
    desc.BindFlags=bind;
    desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride=stride;
    D3D11_SUBRESOURCE_DATA initial{};
    initial.pSysMem=data;
    return SUCCEEDED(device->CreateBuffer(&desc,data?&initial:nullptr,&output));
}

bool Compute::makeSrv(ID3D11Device* device,ID3D11Buffer* buffer,UINT count,
                      ComPtr<ID3D11ShaderResourceView>& output) {
    if(!device||!buffer||!count)return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format=DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
    desc.Buffer.NumElements=count;
    return SUCCEEDED(device->CreateShaderResourceView(buffer,&desc,&output));
}

bool Compute::prepare(ID3D11Device* device,const Scene& scene,const LumaTraceBvh4& bvh,
                      const std::vector<uint32_t>& objectToTriangle,uint32_t triangleCount,
                      std::wstring& error) {
    if(!device||!triangleCount||bvh.nodes.empty()||
       bvh.primitiveCount!=scene.order.size()){
        error=L"LumaTrace BVH4 : géométrie ou table de primitives incompatible.";
        return false;
    }
    if(sceneIdentity_==scene.gpuCacheIdentity&&sceneRevision_==scene.gpuCacheRevision&&
       triangleCount_==triangleCount&&bvhSrv_&&orderSrv_&&radianceSrv_&&radianceUav_&&
       shader_&&paramsBuffer_)return true;

    std::vector<uint32_t> order;
    order.reserve(scene.order.size());
    for(uint32_t id:scene.order){
        if(id>=objectToTriangle.size()){
            error=L"LumaTrace BVH4 : indice de primitive hors limites.";
            return false;
        }
        order.push_back(objectToTriangle[id]);
    }

    reset();
    if(!makeStructuredBuffer(device,bvh.nodes.data(),bvh.nodes.size(),
            sizeof(LumaTraceBvh4Node),D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_IMMUTABLE,bvhBuffer_)||
       !makeStructuredBuffer(device,order.data(),order.size(),sizeof(uint32_t),
            D3D11_BIND_SHADER_RESOURCE,D3D11_USAGE_IMMUTABLE,orderBuffer_)||
       !makeStructuredBuffer(device,nullptr,triangleCount,16,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,radianceBuffer_)){
        error=L"Allocation des tables GPU LumaTrace impossible.";
        reset();
        return false;
    }
    if(!makeSrv(device,bvhBuffer_.Get(),UINT(bvh.nodes.size()),bvhSrv_)||
       !makeSrv(device,orderBuffer_.Get(),UINT(order.size()),orderSrv_)||
       !makeSrv(device,radianceBuffer_.Get(),triangleCount,radianceSrv_)){
        error=L"Création des vues GPU LumaTrace impossible.";
        reset();
        return false;
    }
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format=DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements=triangleCount;
    if(FAILED(device->CreateUnorderedAccessView(radianceBuffer_.Get(),&uavDesc,&radianceUav_))){
        error=L"UAV de radiance LumaTrace impossible.";
        reset();
        return false;
    }

    ComPtr<ID3DBlob> bytecode,diagnostics;
    const HRESULT compile=D3DCompile(lumatrace_detail::kComputeShader,
        sizeof(lumatrace_detail::kComputeShader)-1,"m3d_lumatrace_compute",nullptr,nullptr,
        "CSShadeTriangles","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,
        &bytecode,&diagnostics);
    if(FAILED(compile)){
        error=L"Compilation du compute shader LumaTrace impossible (HRESULT "+
              std::to_wstring(uint32_t(compile))+L")";
        if(diagnostics&&diagnostics->GetBufferPointer()){
            error+=L": ";
            const char* message=static_cast<const char*>(diagnostics->GetBufferPointer());
            const size_t length=std::min<size_t>(diagnostics->GetBufferSize(),1024);
            for(size_t i=0;i<length;++i)
                error.push_back(message[i]>=32&&message[i]<127?wchar_t(message[i]):L' ');
        }
        reset();
        return false;
    }
    if(FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(),
                                          bytecode->GetBufferSize(),nullptr,&shader_))){
        error=L"Création du compute shader LumaTrace impossible.";
        reset();
        return false;
    }
    D3D11_BUFFER_DESC paramsDesc{};
    paramsDesc.ByteWidth=sizeof(Params);
    paramsDesc.Usage=D3D11_USAGE_DYNAMIC;
    paramsDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    paramsDesc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(device->CreateBuffer(&paramsDesc,nullptr,&paramsBuffer_))){
        error=L"Constantes compute LumaTrace impossibles.";
        reset();
        return false;
    }
    triangleCount_=triangleCount;
    nodeCount_=uint32_t(bvh.nodes.size());
    orderCount_=uint32_t(order.size());
    sceneIdentity_=scene.gpuCacheIdentity;
    sceneRevision_=scene.gpuCacheRevision;
    return true;
}

bool Compute::dispatch(ID3D11DeviceContext* context,ID3D11ShaderResourceView* vertexStream,
                       const Scene& scene,std::wstring& error) {
    if(!context||!vertexStream||!shader_||!paramsBuffer_||!radianceUav_||
       !bvhSrv_||!orderSrv_||!triangleCount_){
        error=L"Ressources compute LumaTrace non préparées.";
        return false;
    }
    if(scene.root<0||size_t(scene.root)>=scene.node.size()){
        error=L"Racine BVH4 invalide.";
        return false;
    }
    Params params{};
    params.counts[0]=triangleCount_;
    params.counts[1]=orderCount_;
    params.counts[2]=nodeCount_;
    const Node& root=scene.node[size_t(scene.root)];
    const V center=(root.mn+root.mx)*.5f;
    auto appendLight=[&](V position,float radius,float intensity,V color){
        if(params.counts[3]>=64)return;
        const uint32_t i=params.counts[3]++;
        params.lights[i][0]=position.x;params.lights[i][1]=position.y;
        params.lights[i][2]=position.z;params.lights[i][3]=radius;
        params.colors[i][0]=color.x/255.f;params.colors[i][1]=color.y/255.f;
        params.colors[i][2]=color.z/255.f;params.colors[i][3]=intensity;
    };
    // Keep the test pair, then prioritize authored exterior fixtures.
    appendLight(center+V{1.4f,.75f,0.f},12.f,1.15f,{31,158,255});
    appendLight(center+V{-1.4f,.75f,0.f},12.f,1.15f,{255,99,26});
    for(const Light& light:pointLights)
        appendLight(light.p,light.radius,light.intensity,light.color);
    for(const Light& light:scene.lights)if(light.exterior)
        appendLight(light.p,light.radius,light.intensity*lampFactor,light.color);
    for(const Light& light:scene.lights)if(!light.exterior)
        appendLight(light.p,light.radius,light.intensity,light.color);

    std::array<uint32_t,sizeof(Params)/sizeof(uint32_t)> key{};
    std::memcpy(key.data(),&params,sizeof(params));
    if(radianceCacheValid_&&key==radianceKey_)return true;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(paramsBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
        error=L"Upload constantes LumaTrace impossible.";
        return false;
    }
    std::memcpy(mapped.pData,&params,sizeof(params));
    context->Unmap(paramsBuffer_.Get(),0);
    ID3D11ShaderResourceView* srvs[]={vertexStream,orderSrv_.Get(),bvhSrv_.Get()};
    ID3D11UnorderedAccessView* uavs[]={r