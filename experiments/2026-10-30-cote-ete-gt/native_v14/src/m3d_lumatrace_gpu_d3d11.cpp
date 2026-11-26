#if defined(_WIN32)

#include "m3d_lumatrace_gpu_d3d11.h"
#include "m3d_lumatrace_shader.h"


#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace m3d { namespace lumatrace_gpu {

void Compute::reset() noexcept {
    bvhBuffer_.Reset();orderBuffer_.Reset();radianceBuffer_.Reset();paramsBuffer_.Reset();
    bvhSrv_.Reset();orderSrv_.Reset();radianceSrv_.Reset();radianceUav_.Reset();shader_.Reset();
    probeSrv_.Reset();coarseSrv_.Reset();probeUav_.Reset();coarseUav_.Reset();
    probeTexture_.Reset();coarseTexture_.Reset();probeShader_.Reset();probeCacheValid_=false;
    triangleCount_=nodeCount_=orderCount_=0;
    sceneIdentity_=sceneRevision_=0;
    radianceCacheValid_=false;
    adaptiveBounds_.clear();zeroMask_.fill(0);zeroTriangles_=0;
    adaptiveEnabled_=adaptiveMaskValid_=adaptiveBoundsPrepared_=false;
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
                      const LumaTraceCpuScene& cpuScene,const Scene& scene,bool adaptive,std::wstring& error) {
    const uint32_t triangleCount=cpuScene.triangleCount;
    if(!device||!triangleCount||bvh.nodes.empty()||
       bvh.primitiveCount!=cpuScene.primitiveOrder.size()||
       scene.root<0||size_t(scene.root)>=scene.node.size()){
        error=L"LumaTrace BVH4 : géométrie ou table de primitives incompatible.";
        return false;
    }
    if(sceneIdentity_==cpuScene.sceneIdentity&&sceneRevision_==cpuScene.sceneRevision&&
       triangleCount_==triangleCount&&bvhSrv_&&orderSrv_&&radianceSrv_&&radianceUav_&&
       shader_&&probeShader_&&probeSrv_&&paramsBuffer_){
        if(adaptive&&!adaptiveBoundsPrepared_){adaptiveBounds_=lumatrace_adaptive::prepare(scene,cpuScene);adaptiveBoundsPrepared_=true;}
        return true;
    }

    reset();
    if(!makeStructuredBuffer(device,bvh.nodes.data(),bvh.nodes.size(),
            sizeof(LumaTraceBvh4Node),D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_IMMUTABLE,bvhBuffer_)||
       !makeStructuredBuffer(device,cpuScene.primitiveOrder.data(),cpuScene.primitiveOrder.size(),sizeof(uint32_t),
            D3D11_BIND_SHADER_RESOURCE,D3D11_USAGE_IMMUTABLE,orderBuffer_)||
       !makeStructuredBuffer(device,nullptr,triangleCount,16,
            D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,
            D3D11_USAGE_DEFAULT,radianceBuffer_)){
        error=L"Allocation des tables GPU LumaTrace impossible.";
        reset();
        return false;
    }
    if(!makeSrv(device,bvhBuffer_.Get(),UINT(bvh.nodes.size()),bvhSrv_)||
       !makeSrv(device,orderBuffer_.Get(),UINT(cpuScene.primitiveOrder.size()),orderSrv_)||
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

    auto compileShader=[&](const char* entry,ComPtr<ID3D11ComputeShader>& output){
        ComPtr<ID3DBlob> code,diagnostics;
        HRESULT hr=D3DCompile(lumatrace_detail::kComputeShader,sizeof(lumatrace_detail::kComputeShader)-1,
            "m3d_lumatrace_compute",nullptr,nullptr,entry,"cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&diagnostics);
        if(FAILED(hr)){
            error=L"Compilation LumaTrace impossible (HRESULT "+std::to_wstring(uint32_t(hr))+L")";
            if(diagnostics&&diagnostics->GetBufferPointer()){
                error+=L": ";const char* msg=static_cast<const char*>(diagnostics->GetBufferPointer());
                for(size_t i=0;i<std::min<size_t>(diagnostics->GetBufferSize(),1024);++i)
                    error.push_back(msg[i]>=32&&msg[i]<127?wchar_t(msg[i]):L' ');
            }
            return false;
        }
        hr=device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&output);
        if(FAILED(hr)){error=L"Création du shader LumaTrace impossible.";return false;}return true;
    };
    if(!compileShader("CSShadeTriangles",shader_)||!compileShader("CSBuildProbeVolume",probeShader_)){reset();return false;}
    D3D11_TEXTURE3D_DESC td{};td.Width=12;td.Height=5;td.Depth=12;td.MipLevels=1;
    td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.Usage=D3D11_USAGE_DEFAULT;
    td.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    auto volume=[&](ComPtr<ID3D11Texture3D>& texture,ComPtr<ID3D11ShaderResourceView>& srv,
                    ComPtr<ID3D11UnorderedAccessView>& uav){
        return SUCCEEDED(device->CreateTexture3D(&td,nullptr,&texture))&&
            SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&srv))&&
            SUCCEEDED(device->CreateUnorderedAccessView(texture.Get(),nullptr,&uav));
    };
    if(!volume(probeTexture_,probeSrv_,probeUav_)||!volume(coarseTexture_,coarseSrv_,coarseUav_)){
        error=L"Allocation des volumes de sondes LumaTrace impossible.";reset();return false;
    }
    const Node& root=scene.node[size_t(scene.root)];
    const V size=root.mx-root.mn;
    const V padding={std::max(1.f,size.x*.01f),std::max(1.f,size.y*.03f),std::max(1.f,size.z*.01f)};
    origin_=root.mn-padding;span_=size+padding*2.f;
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
    if(adaptive){adaptiveBounds_=lumatrace_adaptive::prepare(scene,cpuScene);adaptiveBoundsPrepared_=true;}
    triangleCount_=triangleCount;
    nodeCount_=uint32_t(bvh.nodes.size());
    orderCount_=uint32_t(cpuScene.primitiveOrder.size());
    sceneIdentity_=cpuScene.sceneIdentity;
    sceneRevision_=cpuScene.sceneRevision;
    return true;
}

bool Compute::dispatch(ID3D11DeviceContext* context,ID3D11ShaderResourceView* vertexStream,
                       ID3D11ShaderResourceView* indices,const Scene& scene,ID3D11ShaderResourceView* albedo,
                       ID3D11ShaderResourceView* preserved,ID3D11SamplerState* sampler,
                       const std::array<int32_t,256>& layers,bool probes,bool adaptive,std::wstring& error) {
    if(!context||!vertexStream||!shader_||!paramsBuffer_||!radianceUav_||!bvhSrv_||!orderSrv_||
       !triangleCount_||(probes&&(!albedo||!sampler||!probeShader_))){
        error=L"Ressources compute LumaTrace non préparées.";return false;
    }
    Params p{};p.geometryLayout[0]=indices?1u:0u;
    p.counts[0]=triangleCount_;p.counts[1]=orderCount_;p.counts[2]=nodeCount_;
    p.probeDims[0]=12;p.probeDims[1]=5;p.probeDims[2]=12;
    p.probeOrigin[0]=origin_.x;p.probeOrigin[1]=origin_.y;p.probeOrigin[2]=origin_.z;
    p.probeSpan[0]=span_.x;p.probeSpan[1]=span_.y;p.probeSpan[2]=span_.z;
    auto linear=[](float v){v=clampf(v/255.f,0.f,1.f);return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
    p.sunDirection[0]=sun.x;p.sunDirection[1]=sun.y;p.sunDirection[2]=sun.z;
    p.sunColor[0]=linear(sunTint.x);p.sunColor[1]=linear(sunTint.y);p.sunColor[2]=linear(sunTint.z);
    p.sunColor[3]=daylight*.88f;
    for(auto& row:p.preservedLayers)std::fill(std::begin(row),std::end(row),0xffffffffu);
    for(size_t m=0;m<layers.size();++m)if(layers[m]>=0)p.preservedLayers[m>>2][m&3]=uint32_t(layers[m]);
    auto append=[&](const Light& light,float scale){
        if(p.counts[3]>=64)return;const uint32_t i=p.counts[3]++;
        p.lights[i][0]=light.p.x;p.lights[i][1]=light.p.y;p.lights[i][2]=light.p.z;p.lights[i][3]=light.radius;
        p.colors[i][0]=linear(light.color.x);p.colors[i][1]=linear(light.color.y);p.colors[i][2]=linear(light.color.z);
        p.colors[i][3]=light.intensity*scale;
    };
    for(const Light& l:pointLights)append(l,1.f);
    for(const Light& l:scene.lights)if(l.exterior)append(l,lampFactor);
    for(const Light& l:scene.lights)if(!l.exterior)append(l,.42f+.58f*(1.f-daylight));
    adaptiveEnabled_=adaptive&&!adaptiveBounds_.empty();
    if(adaptiveEnabled_){
        // Only light position/radius and on/off state affect the group mask.
        // The original ordered light list, including its 64-entry cap, is preserved.
        std::array<uint32_t,513> maskKey{};maskKey[0]=p.counts[3];
        std::memcpy(maskKey.data()+1,p.lights,sizeof(p.lights));
        for(uint32_t i=0;i<p.counts[3];++i){
            const auto& c=p.colors[i];bool finite=true;
            for(float x:c)finite=finite&&std::isfinite(x);
            maskKey[257+i]=finite?((c[3]!=0.f&&(c[0]!=0.f||c[1]!=0.f||c[2]!=0.f))?1u:0u):2u;
        }
        if(!adaptiveMaskValid_||maskKey!=adaptiveLightKey_){
            zeroTriangles_=lumatrace_adaptive::classify(adaptiveBounds_,triangleCount_,p.lights,p.colors,p.counts[3],zeroMask_);
            adaptiveLightKey_=maskKey;adaptiveMaskValid_=true;
        }
        p.geometryLayout[1]=1u;std::memcpy(p.adaptiveZero,zeroMask_.data(),sizeof(p.adaptiveZero));
    }
    std::array<uint32_t,sizeof(Params)/sizeof(uint32_t)> key{};std::memcpy(key.data(),&p,sizeof(p));
    auto upload=[&](const Params& params){
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(paramsBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){
            error=L"Upload constantes LumaTrace impossible.";return false;}
        std::memcpy(mapped.pData,&params,sizeof(params));context->Unmap(paramsBuffer_.Get(),0);return true;
    };
    auto unbind=[&](){
        ID3D11ShaderResourceView* ns[8]{};ID3D11UnorderedAccessView* nu[3]{};ID3D11SamplerState* sm[1]{};
        context->CSSetUnorderedAccessViews(0,3,nu,nullptr);context->CSSetShaderResources(0,8,ns);
        context->CSSetSamplers(0,1,sm);context->CSSetShader(nullptr,nullptr,0);
    };
    ID3D11Buffer* constants[]={paramsBuffer_.Get()};
    if(!radianceCacheValid_||key!=radianceKey_){
        if(!upload(p))return false;
        ID3D11ShaderResourceView* noRadiance[]={nullptr};context->PSSetShaderResources(7,1,noRadiance);
        ID3D11ShaderResourceView* srvs[8]={vertexStream,orderSrv_.Get(),bvhSrv_.Get(),nullptr,nullptr,nullptr,nullptr,indices};
        ID3D11UnorderedAccessView* uavs[]={radianceUav_.Get()};
        context->CSSetShader(shader_.Get(),nullptr,0);context->CSSetConstantBuffers(0,1,constants);
        context->CSSetShaderResources(0,8,srvs);context->CSSetUnorderedAccessViews(0,1,uavs,nullptr);
        context->Dispatch((triangleCount_+63)/64,1,1);unbind();
        radianceKey_=key;radianceCacheValid_=true;probeCacheValid_=false;
    }
    if(probes&&!probeCacheValid_){
        ID3D11ShaderResourceView* noProbes[2]{};context->PSSetShaderResources(4,2,noProbes);
        for(uint32_t mode=0;mode<2;++mode){
            p.probeDims[3]=mode;if(!upload(p)){unbind();return false;}
            ID3D11ShaderResourceView* srvs[8]={vertexStream,orderSrv_.Get(),bvhSrv_.Get(),
                radianceSrv_.Get(),albedo,preserved?preserved:albedo,mode?coarseSrv_.Get():nullptr,indices};
            ID3D11UnorderedAccessView* uavs[3]={nullptr,mode?probeUav_.Get():nullptr,mode?nullptr:coarseUav_.Get()};
            context->CSSetShader(probeShader_.Get(),nullptr,0);context->CSSetConstantBuffers(0,1,constants);
            context->CSSetSamplers(0,1,&sampler);context->CSSetShaderResources(0,8,srvs);
            context->CSSetUnorderedAccessViews(0,3,uavs,nullptr);
            context->Dispatch((12u*5u*12u+63u)/64u,1,1);unbind();
        }
        probeCacheValid_=true;
    }return true;
}
} } // namespace m3d::lumatrace_gpu

#endif // _WIN32
