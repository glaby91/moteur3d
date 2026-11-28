#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <chrono>
#include <cstring>
#include <limits>
#include <algorithm>
namespace m3d::escargot_gpu {
using Microsoft::WRL::ComPtr;
// Each thread owns one tile. Header copied once; dynamic ranges allocated
// atomically with an explicit non-wrapping allocation bound. No host readback before shading.
inline constexpr char kBuildPacket[] = R"(
StructuredBuffer<float4> src:register(t0);
RWStructuredBuffer<float4> dst:register(u0);
RWStructuredBuffer<uint> state:register(u1); // next record, error flags, tile count
cbuffer Params:register(b0){uint jobBase,jobCount,capacity,sourceCount;}
uint reserveRange(uint count){
    // Host plan bounds the total attempted allocations below UINT_MAX.
    uint old=0;InterlockedAdd(state[0],count,old);
    if(old>capacity||count>capacity-old){InterlockedOr(state[1],1u);return 0u;}
    return old;
}
uint minimumDistance(int2 p,int4 r){
    int2 d=max(int2(0,0),max(r.xy-p,p-(r.xy+r.zw-1)));
    return uint(d.x*d.x+d.y*d.y);
}
uint maximumDistance(int2 p,int4 r){
    int2 d=max(abs(r.xy-p),abs(r.xy+r.zw-1-p));
    return uint(d.x*d.x+d.y*d.y);
}
uint shortlistLimit(uint start,uint count,int4 r){
    if(count<=4u)return 0xffffffffu;
    uint4 best=0xffffffffu;
    for(uint j=0;j<count;++j){
        uint v=maximumDistance(int2(src[start+3*j].xy),r);
        if(v<best.x)best=uint4(v,best.xyz);
        else if(v<best.y)best=uint4(best.x,v,best.yz);
        else if(v<best.z)best=uint4(best.xy,v,best.z);
        else if(v<best.w)best.w=v;
    }
    return best.w;
}
// FXC cs_5_0 cannot dynamically address vector components as l-values.
void markSelected(inout uint4 mask,uint index){
    uint bit=1u<<(index&31u);
    if(index<32u)mask.x|=bit;
    else if(index<64u)mask.y|=bit;
    else if(index<96u)mask.z|=bit;
    else mask.w|=bit;
}
uint popSelected(inout uint4 mask){
    if(mask.x){uint bit=uint(firstbitlow(mask.x));mask.x&=mask.x-1u;return bit;}
    if(mask.y){uint bit=uint(firstbitlow(mask.y));mask.y&=mask.y-1u;return 32u+bit;}
    if(mask.z){uint bit=uint(firstbitlow(mask.z));mask.z&=mask.z-1u;return 64u+bit;}
    if(mask.w){uint bit=uint(firstbitlow(mask.w));mask.w&=mask.w-1u;return 96u+bit;}
    return 128u; // unreachable while the selected-count invariant holds
}
bool overlaps(int4 a,int4 b){return all(a.xy<b.xy+b.zw)&&all(b.xy<a.xy+a.zw);}
[numthreads(32,1,1)]
void CSBuild(uint3 tid:SV_DispatchThreadID){
    uint job=tid.x;if(job>=jobCount)return;
    uint base=jobBase+3*job;
    float4 a=src[base],b=src[base+1],c=src[base+2];
    uint tileId=uint(a.x),receiverStart=uint(a.y),receiverCount=uint(a.z),layerStart=uint(a.w),layerCount=uint(b.x);
    bool common=b.y>.5;
    int4 rect=int4(int2(b.zw),int2(c.xy));
    if(!receiverCount||!layerCount)return;
    uint receiverLimit=shortlistLimit(receiverStart,receiverCount,rect),chosenReceivers=0;
    // The shared receiver shortlist is also the exact common-layer stencil.
    // Cache the selected source indices in four registers for small regions.
    const bool maskable=receiverCount<=128u;uint4 selected=0;
    for(uint j=0;j<receiverCount;++j)if(minimumDistance(int2(src[receiverStart+3*j].xy),rect)<=receiverLimit){
        ++chosenReceivers;if(maskable)markSelected(selected,j);
    }
    if(!chosenReceivers)return;
    uint overlapsCount=0;
    for(uint l=0;l<layerCount;++l)if(overlaps(int4(src[layerStart+7*l+1]),rect))++overlapsCount;
    if(!overlapsCount)return;
    uint receiverDst=reserveRange(3*chosenReceivers+7*min(overlapsCount,64u));
    if(!receiverDst)return;uint layerDst=receiverDst+3*chosenReceivers;
    uint wr=0;uint4 copyMask=selected;
    for(uint j=0;j<(maskable?chosenReceivers:receiverCount);++j){
        uint index=j;if(maskable)index=popSelected(copyMask);
        uint s=receiverStart+3*index;if(!maskable&&minimumDistance(int2(src[s].xy),rect)>receiverLimit)continue;
        uint t=receiverDst+3*wr++;dst[t]=src[s];dst[t+1]=src[s+1];dst[t+2]=src[s+2];
    }
    uint active=0;
    for(uint l=0;l<layerCount;++l){
        uint lb=layerStart+7*l;
        if(!overlaps(int4(src[lb+1]),rect))continue;
        float4 info=src[lb];
        uint obsStart=uint(info.x),obsCount=uint(info.y);
        if(common&&obsCount!=receiverCount){InterlockedOr(state[1],4u);return;}
        uint positions=common?receiverStart:obsStart;
        uint limit=common?receiverLimit:shortlistLimit(positions,obsCount,rect);
        uint stride=common?2u:3u,count=0;bool hit=false;
        const bool sharedMask=common&&maskable;uint4 layerMask=selected;
        if(sharedMask){
            count=chosenReceivers;
            for(uint k=0;k<count;++k){
                uint index=popSelected(layerMask);
                if(src[obsStart+2*index+1].w>.5){hit=true;break;}
            }
        }else for(uint k=0;k<obsCount;++k){
            if(minimumDistance(int2(src[positions+3*k].xy),rect)>limit)continue;
            ++count;
            if(src[obsStart+stride*k+stride-1].w>.5)hit=true;
        }
        if(!count||!hit)continue;
        if(active>=64u){InterlockedOr(state[1],2u);return;}
        uint obsDst=reserveRange(count*stride);if(!obsDst)return;
        uint layer=layerDst+7*active++;
        dst[layer]=float4(float(obsDst),float(count),info.zw);
        for(uint k=1;k<7;++k)dst[layer+k]=src[lb+k];
        uint at=0;layerMask=selected;
        for(uint k=0;k<(sharedMask?count:obsCount);++k){
            uint index=k;if(sharedMask)index=popSelected(layerMask);
            if(!sharedMask&&minimumDistance(int2(src[positions+3*index].xy),rect)>limit)continue;
            uint from=obsStart+stride*index,to=obsDst+stride*at++;
            dst[to]=src[from];dst[to+1]=src[from+1];if(!common)dst[to+2]=src[from+2];
        }
    }
    if(active){dst[tileId]=float4(float(receiverDst),float(chosenReceivers),float(layerDst),float(active));InterlockedAdd(state[2],1u);}
}
)";
class PacketBuilder {
    using Clock=std::chrono::steady_clock;
    ID3D11Device* device_=nullptr;
    ComPtr<ID3D11ComputeShader> shader_;
    ComPtr<ID3D11Buffer> input_,output_,state_,readback_,constants_;
    ComPtr<ID3D11ShaderResourceView> inputSrv_,outputSrv_;
    ComPtr<ID3D11UnorderedAccessView> outputUav_,stateUav_;
    ComPtr<ID3D11Query> disjoint_,begin_,end_;
    size_t inputCapacity_=0,outputCapacity_=0;
    bool timestamp_=false,pending_=false,capture_=false;
    static double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
    static bool buffer(ID3D11Device* d,UINT count,UINT stride,UINT bind,ComPtr<ID3D11Buffer>& b){
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=count*stride;desc.StructureByteStride=stride;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;desc.BindFlags=bind;
        return SUCCEEDED(d->CreateBuffer(&desc,nullptr,b.ReleaseAndGetAddressOf()));
    }
    static bool srv(ID3D11Device* d,ID3D11Buffer* b,UINT count,ComPtr<ID3D11ShaderResourceView>& v){
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};desc.Format=DXGI_FORMAT_UNKNOWN;desc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;desc.Buffer.NumElements=count;
        return SUCCEEDED(d->CreateShaderResourceView(b,&desc,v.ReleaseAndGetAddressOf()));
    }
    static bool uav(ID3D11Device* d,ID3D11Buffer* b,UINT count,ComPtr<ID3D11UnorderedAccessView>& v){
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};desc.Format=DXGI_FORMAT_UNKNOWN;desc.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;desc.Buffer.NumElements=count;
        return SUCCEEDED(d->CreateUnorderedAccessView(b,&desc,v.ReleaseAndGetAddressOf()));
    }
public:
    ID3D11ShaderResourceView* view()const{return outputSrv_.Get();}
    bool build(ID3D11Device* device,ID3D11DeviceContext* context,const EscargotPacketBuild& p,EscargotBuildTiming& timing,std::wstring& error){
        timing={};pending_=false;auto start=Clock::now();const size_t n=p.data.size()/4;
        if(!device||!context||p.data.size()%4||n<2||n>4u*1024u*1024u||p.prefixRecords<2||p.prefixRecords>n||
           p.jobBase<p.prefixRecords||uint64_t(p.jobBase)+3ull*p.jobCount>n||p.jobCount>65535u*32u||
           p.maxRecords<=p.prefixRecords||p.maxRecords>4u*1024u*1024u){error=L"Invalid GPU optical build plan";return false;}
        if(device_!=device){*this=PacketBuilder{};device_=device;}
        if(!shader_){
            ComPtr<ID3DBlob> code,log;
            auto hr=D3DCompile(kBuildPacket,sizeof(kBuildPacket)-1,"EscargotBuildPacket",nullptr,nullptr,"CSBuild","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&log);
            if(FAILED(hr)){error=L"Escargot GPU packet shader: ";if(log){const char* s=static_cast<const char*>(log->GetBufferPointer());for(size_t i=0;i<std::min<size_t>(4096,log->GetBufferSize());++i)error.push_back(wchar_t(static_cast<unsigned char>(s[i])));}return false;}
            if(FAILED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader_))){error=L"Escargot GPU packet shader creation";return false;}
        }
        if(!inputSrv_||inputCapacity_<n){
            inputCapacity_=0;inputSrv_.Reset();
            if(!buffer(device,UINT(n),16,D3D11_BIND_SHADER_RESOURCE,input_)||!srv(device,input_.Get(),UINT(n),inputSrv_)){error=L"Escargot build input allocation";return false;}
            inputCapacity_=n;
        }
        if(!outputSrv_||!outputUav_||outputCapacity_!=p.maxRecords){
            outputCapacity_=0;outputSrv_.Reset();outputUav_.Reset();
            if(!buffer(device,p.maxRecords,16,D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,output_)||
               !srv(device,output_.Get(),p.maxRecords,outputSrv_)||!uav(device,output_.Get(),p.maxRecords,outputUav_)){error=L"Escargot build output allocation";return false;}
            outputCapacity_=p.maxRecords;
        }
        if(!stateUav_||!readback_||!constants_){
            if(!buffer(device,4,4,D3D11_BIND_UNORDERED_ACCESS,state_)||!uav(device,state_.Get(),4,stateUav_)){error=L"Escargot build counters";return false;}
            D3D11_BUFFER_DESC d{};d.ByteWidth=16;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            if(FAILED(device->CreateBuffer(&d,nullptr,readback_.ReleaseAndGetAddressOf()))){error=L"Escargot build counter readback";return false;}
            d.Usage=D3D11_USAGE_DEFAULT;d.CPUAccessFlags=0;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            if(FAILED(device->CreateBuffer(&d,nullptr,constants_.ReleaseAndGetAddressOf()))){error=L"Escargot build constants";return false;}
        }
        if(!disjoint_||!begin_||!end_){
            D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
            if(FAILED(device->CreateQuery(&q,disjoint_.ReleaseAndGetAddressOf())))disjoint_.Reset();
            q.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(device->CreateQuery(&q,begin_.ReleaseAndGetAddressOf())))begin_.Reset();
            if(FAILED(device->CreateQuery(&q,end_.ReleaseAndGetAddressOf())))end_.Reset();
        }
        timestamp_=disjoint_&&begin_&&end_;
        capture_=p.capturePacket;
        timing.prepareMs=ms(start);start=Clock::now();
        ID3D11ShaderResourceView* clear=nullptr;context->PSSetShaderResources(13,1,&clear);
        D3D11_BOX dataBox{0,0,0,UINT(n*16),1,1},headerBox{0,0,0,p.prefixRecords*16,1,1};
        context->UpdateSubresource(input_.Get(),0,&dataBox,p.data.data(),0,0);
        context->UpdateSubresource(output_.Get(),0,&headerBox,p.data.data(),0,0);
        const uint32_t counters[]={p.prefixRecords,0,0,0},params[]={p.jobBase,p.jobCount,p.maxRecords,uint32_t(n)};
        context->UpdateSubresource(state_.Get(),0,nullptr,counters,0,0);context->UpdateSubresource(constants_.Get(),0,nullptr,params,0,0);
        ID3D11ShaderResourceView* in=inputSrv_.Get();ID3D11UnorderedAccessView* out[]={outputUav_.Get(),stateUav_.Get()};ID3D11Buffer* cb=constants_.Get();
        context->CSSetShader(shader_.Get(),nullptr,0);context->CSSetShaderResources(0,1,&in);context->CSSetUnorderedAccessViews(0,2,out,nullptr);context->CSSetConstantBuffers(0,1,&cb);
        if(timestamp_){context->Begin(disjoint_.Get());context->End(begin_.Get());}
        if(p.jobCount)context->Dispatch((p.jobCount+31)/32,1,1);
        if(timestamp_){context->End(end_.Get());context->End(disjoint_.Get());}
        ID3D11UnorderedAccessView* nullUav[2]{};ID3D11Buffer* nullCb=nullptr;
        context->CSSetUnorderedAccessViews(0,2,nullUav,nullptr);context->CSSetShaderResources(0,1,&clear);context->CSSetConstantBuffers(0,1,&nullCb);context->CSSetShader(nullptr,nullptr,0);
        // Only a 16-byte diagnostic is copied. Do not Map before rendering.
        context->CopyResource(readback_.Get(),state_.Get());pending_=true;timing.submitMs=ms(start);return true;
    }
    bool finish(ID3D11DeviceContext* context,EscargotBuildTiming& timing,std::wstring& error){
        if(!pending_){error=L"No GPU packet build pending";return false;}pending_=false;auto start=Clock::now();
        D3D11_MAPPED_SUBRESOURCE m{};
        if(FAILED(context->Map(readback_.Get(),0,D3D11_MAP_READ,0,&m))){error=L"Escargot GPU packet status readback";return false;}
        uint32_t state[4]{};std::memcpy(state,m.pData,sizeof(state));context->Unmap(readback_.Get(),0);
        timing.records=state[0];timing.tiles=state[2];
        if(timestamp_){D3D11_QUERY_DATA_TIMESTAMP_DISJOINT q{};UINT64 a=0,b=0;
            if(context->GetData(disjoint_.Get(),&q,sizeof(q),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK&&
               context->GetData(begin_.Get(),&a,sizeof(a),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK&&
               context->GetData(end_.Get(),&b,sizeof(b),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK&&!q.Disjoint&&q.Frequency&&b>=a){
                timing.gpuMs=1000.*double(b-a)/double(q.Frequency);timing.gpuTimingValid=true;
            }
        }
        timing.statusMs=ms(start);
        if(state[1]||state[0]>outputCapacity_){error=L"Escargot GPU packet build overflow/invalid layout, flags="+std::to_wstring(state[1]);return false;}
        if(capture_){
            // Untimed diagnostic only: read logical records after the image.
            D3D11_BUFFER_DESC d{};d.ByteWidth=UINT(outputCapacity_*16);d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Buffer> capture;
            if(FAILED(device_->CreateBuffer(&d,nullptr,capture.GetAddressOf()))){error=L"Escargot packet diagnostic allocation";return false;}
            context->CopyResource(capture.Get(),output_.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(context->Map(capture.Get(),0,D3D11_MAP_READ,0,&mapped))){error=L"Escargot packet diagnostic read";return false;}
            timing.diagnosticPacket.resize(size_t(state[0])*4);std::memcpy(timing.diagnosticPacket.data(),mapped.pData,timing.diagnosticPacket.size()*sizeof(float));
            context->Unmap(capture.Get(),0);
        }
        return true;
    }
};
}
