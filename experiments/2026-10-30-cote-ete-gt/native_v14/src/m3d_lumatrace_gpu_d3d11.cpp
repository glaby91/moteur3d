#if defined(_WIN32)

#include "m3d_lumatrace_gpu_d3d11.h"
#include "m3d_lumatrace_shader.h"


#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace m3d { namespace lumatrace_gpu {

void Compute::reset() noexcept {
    bvhBuffer_.Reset();orderBuffer_.Reset();albedoBuffer_.Reset();radianceBuffer_.Reset();paramsBuffer_.Reset();
    bvhSrv_.Reset();orderSrv_.Reset();albedoSrv_.Reset();radianceSrv_.Reset();radianceUav_.Reset();shader_.Reset();
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

bool Compute::prepare(ID3D11Device* device,const LumaTraceBvh4& bvh,
                      const LumaTraceCpuScene& cpuScene,std::wstring& error) {
    const uint32_t triangleCount=cpuScene.triangleCount;
    if(!device||!triangleCount||bvh.nodes.empty()||
       bvh.primitiveCount!=cpuScene.primitiveOrder.size()||
       cpuScene.triangleAlbedo.size()!=triangleCount){
        error=L"LumaTrace BVH4 : géométrie ou table de primitives incompatible.";
        return false;
    }
    if(sceneIdentity_==cpuScene.sceneIdentity&&sceneRevision_==cpuScene.sceneRevision&&
       triangleCount_==triangleCount&&bvhSrv_&&orderSrv_&&albedoSrv_&&radianceSrv_&&radianceUav_&&
       shader_&&paramsBuffer_)return true;

    reset();
    if(!makeStructuredBuffer(device,bvh.nodes.data(),bvh.nodes.size(),
            sizeof(LumaTraceBvh4Node),D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_IMMUTABLE,bvhBuffer_)||
       !makeStructuredBuffer(device,cpuScene.primitiveOrder.data(),cpuScene.primitiveOrder.size(),sizeof(uint32_t),
            D3D11_BIND_SHADER_RESOURCE,D3D11_USAGE_IMMUTABLE,orderBuffer_)||
       !makeStructuredBuffer(device,cpuScene.triangleAlbedo.data(),cpuScene.triangleAlbedo.size(),sizeof(std::array<float,4>),
            D3D11_BIND_SHADER_RESOURCE,D3D11_USAGE_IMMUTABLE,albedoBuffer_)||
       !makeStructuredBuffer(device,nullptr,triangleCount,16,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,radianceBuffer_)){
        error=L"Allocation des tables GPU LumaTrace impossible.";
        reset();
        return false;
    }
    if(!makeSrv(device,bvhBuffer_.Get(),UINT(bvh.nodes.size()),bvhSrv_)||
       !makeSrv(device,orderBuffer_.Get(),UINT(cpuScene.primitiveOrder.size()),orderSrv_)||
       !makeSrv(device,albedoBuffer_.Get(),triangleCount,albedoSrv_)||
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
    orderCount_=uint32_t(cpuScene.primitiveOrder.size());
    sceneIdentity_=cpuScene.sceneIdentity;
    sceneRevision_=cpuScene.sceneRevision;
    return true;
}

bool Compute::dispatch(ID3D11DeviceContext* context,ID3D11ShaderResourceView* vertexStream,
                       const Scene& scene,std::wstring& error) {
    if(!context||!vertexStream||!shader_||!paramsBuffer_||!radianceUav_||
       !bvhSrv_||!orderSrv_||!albedoSrv_||!triangleCount_){
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
    // Turn luminous scene surfaces into a small set of cached local-light proxies.
    // The existing BVH shadow queries and four deterministic bounce samples then
    // handle their contribution once per unchanged light state, not per frame.
    struct EmitterCell {
        V weightedPosition{}, color{};
        float area=0.f, intensity=0.f;
    };
    auto emissionOf=[](const Material& material,V& color,float& intensity)->bool {
        if(material.emissionAuthored) {
            color=material.emissionColor;
            intensity=material.emissionIntensity;
            return intensity>0.f;
        }
        const std::string& name=material.name;
        if(name=="cse_pool_strip") {
            color={.83f,.91f,1.f};intensity=.24f;
        } else if(name=="cse_neon_red") {
            color={1.f,.09f,.035f};intensity=1.65f;
        } else if(name=="cse_neon_cyan") {
            color={.03f,.89f,1.f};intensity=1.5f;
        } else if(name=="cse_neon_gold") {
            color={1.f,.72f,.24f};intensity=1.35f;
        } else if(name=="lamp") {
            color={1.f,166.f/255.f,91.f/255.f};intensity=2.65f*lampFactor;
        } else if(name=="led") {
            color={1.f,217.f/255.f,165.f/255.f};intensity=1.33f*lampFactor;
        } else if(name=="taillight") {
            color={1.f,28.f/255.f,12.f/255.f};intensity=1.25f*lampFactor;
        } else return false;
        return intensity>0.f;
    };
    using EmitterKey=std::tuple<uint32_t,int,int,int>;
    std::map<EmitterKey,EmitterCell> emitterCells;
    constexpr float emitterCellSize=4.f;
    for(const Obj& object:scene.obj) {
        if(object.kind!=Kind::Triangle || object.mat>=scene.mat.size())continue;
        V sourceColor;float sourceIntensity=0.f;
        if(!emissionOf(scene.mat[object.mat],sourceColor,sourceIntensity))continue;
        const V areaVector=cross(object.b-object.a,object.c-object.a);
        const float area=.5f*length(areaVector);
        if(area<1.e-8f)continue;
        const V centerPoint=(object.a+object.b+object.c)/3.f;
        const EmitterKey key{
            uint32_t(object.mat),
            int(std::floor(centerPoint.x/emitterCellSize)),
            int(std::floor(centerPoint.y/emitterCellSize)),
            int(std::floor(centerPoint.z/emitterCellSize))};
        EmitterCell& cell=emitterCells[key];
        cell.weightedPosition=cell.weightedPosition+centerPoint*area;
        cell.color=sourceColor;
        cell.area+=area;
        cell.intensity=sourceIntensity;
    }
    std::vector<std::pair<V,EmitterCell>> emitters;
    emitters.reserve(emitterCells.size());
    for(const auto& entry:emitterCells) {
        const EmitterCell& cell=entry.second;
        if(cell.area>0.f)emitters.push_back({cell.weightedPosition/cell.area,cell});
    }
    // Evenly retain at most sixteen samples along long strips. The scene's
    // emissive meshes stay intact; only their low-cost lighting proxies are limited.
    const size_t emitterCount=std::min<size_t>(16,emitters.size());
    for(size_t i=0;i<emitterCount;++i) {
        const size_t sourceIndex=emitterCount==1?0:
            (i*(emitters.size()-1))/(emitterCount-1);
        const auto& source=emitters[sourceIndex];
        const float spread=std::sqrt(std::max(source.second.area,0.f));
        const float radius=clampf(2.f+2.2f*spread,2.5f,8.f);
        const float intensity=clampf(source.second.intensity*4.5f*
            clampf(spread,.65f,1.5f),.5f,4.f);
        appendLight(source.first,radius,intensity,source.second.color*255.f);
    }
    for(const Light& light:scene.lights)
        appendLight(light.p,light.radius,light.intensity,light.color);

    // Legacy world fixtures are used only when the scene has no authored source.
    if(params.counts[3]==0) {
        for(const Light& light:pointLights)
            appendLight(light.p,light.radius,light.intensity,light.color);
    }
    if(params.counts[3]==0) {
        appendLight(center+V{1.4f,.75f,0.f},12.f,1.15f,{31,158,255});
        appendLight(center+V{-1.4f,.75f,0.f},12.f,1.15f,{255,99,26});
    }

    // Trace the same sun direction used by the raster renderer through the BVH.
    // This makes LumaTrace's direct term produce real cast shadows and lets
    // the one-bounce rays carry daylight into the pool and covered surfaces.
    const float daylight=std::max(0.f,std::sin(3.14159265359f*
        clampf((solarHour-6.f)/13.f,0.f,1.f)));
    params.sunDirection[0]=sun.x;params.sunDirection[1]=sun.y;
    params.sunDirection[2]=sun.z;params.sunDirection[3]=0.f;
    params.sunColor[0]=1.f;params.sunColor[1]=.97f;params.sunColor[2]=.91f;
    params.sunColor[3]=.58f*daylight;

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
    ID3D11ShaderResourceView* srvs[]={vertexStream,orderSrv_.Get(),bvhSrv_.Get(),albedoSrv_.Get()};
    ID3D11UnorderedAccessView* uavs[]={radianceUav_.Get()};
    ID3D11Buffer* constants[]={paramsBuffer_.Get()};
    context->CSSetShader(shader_.Get(),nullptr,0);
    context->CSSetShaderResources(0,4,srvs);
    context->CSSetUnorderedAccessViews(0,1,uavs,nullptr);
    context->CSSetConstantBuffers(0,1,constants);
    context->Dispatch((triangleCount_+63)/64,1,1);
    ID3D11UnorderedAccessView* noUavs[]={nullptr};
    ID3D11ShaderResourceView* noSrvs[]={nullptr,nullptr,nullptr,nullptr};
    context->CSSetUnorderedAccessViews(0,1,noUavs,nullptr);
    context->CSSetShaderResources(0,4,noSrvs);
    context->CSSetShader(nullptr,nullptr,0);
    radianceKey_=key;
    radianceCacheValid_=true;
    return true;
}

} } // namespace m3d::lumatrace_gpu

#endif // _WIN32
