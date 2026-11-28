#pragma once
#include "m3d_lumatrace_bvh4.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
namespace m3d::escargot_gpu {
using Microsoft::WRL::ComPtr;
// A bounded nearest-hit compute kernel. Original primitive IDs, all triangles,
// no material filter. A stack overflow is explicit status=2, never a miss.
inline constexpr char kTrace[] = R"(
struct Node {float3 lo;uint c0;float3 hi;uint c1;uint c2,c3,first,count,children,pad;};
struct Triangle {float4 a,b,c;};
struct Query {float3 o;float low;float3 d;float pad;};
struct Hit {uint primitive,status;float depth,u,v;float3 pad;};
StructuredBuffer<Node> nodes:register(t0);
StructuredBuffer<uint> order:register(t1);
StructuredBuffer<Triangle> triangles:register(t2);
StructuredBuffer<Query> queries:register(t3);
RWStructuredBuffer<Hit> hits:register(u0);
cbuffer Params:register(b0){uint queryCount,nodeCount,triangleCount,orderCount;}
bool box(Node n,float3 o,float3 d,float low,float high,out float nearT){
    nearT=low;
    [unroll]for(uint a=0;a<3;++a){
        if(abs(d[a])<1e-12){if(o[a]<n.lo[a]||o[a]>n.hi[a])return false;continue;}
        float x=(n.lo[a]-o[a])/d[a],y=(n.hi[a]-o[a])/d[a];
        float l=min(x,y),h=max(x,y);
        l-=4.76837158203125e-7*max(1.0,abs(l));h+=4.76837158203125e-7*max(1.0,abs(h));
        nearT=max(nearT,l);high=min(high,h);if(high<nearT)return false;
    }return true;
}
bool hitTriangle(Triangle t,Query q,float high,out float depth,out float u,out float v){
    depth=0;u=0;v=0;
    float3 e=t.b.xyz-t.a.xyz,f=t.c.xyz-t.a.xyz,p=cross(q.d,f);
    float det=dot(e,p);if(abs(det)<1e-10)return false;
    float3 a=q.o-t.a.xyz;u=dot(a,p)/det;if(u<0||u>1)return false;
    float3 z=cross(a,e);v=dot(q.d,z)/det;if(v<0||u+v>1)return false;
    depth=dot(f,z)/det;return isfinite(depth)&&depth>q.low&&depth<=high;
}
// Four-way sorting network: fixed five comparisons, no nested dynamic loop.
// Push farthest first so the nearest node is popped first.
void compareFarFirst(inout uint4 child,inout float4 dist,uint a,uint b){
    if(dist[a]<dist[b]){float t=dist[a];dist[a]=dist[b];dist[b]=t;
        uint k=child[a];child[a]=child[b];child[b]=k;}
}
[numthreads(64,1,1)]
void CSTrace(uint3 tid:SV_DispatchThreadID){
    uint i=tid.x;if(i>=queryCount)return;
    Query q=queries[i];Hit h;h.primitive=0xffffffffu;h.status=0;h.depth=3.402823466e38;h.u=0;h.v=0;h.pad=0;
    uint stack[128];float stackNear[128];uint top=0,visited=0;float n;
    if(nodeCount>0&&box(nodes[0],q.o,q.d,q.low,h.depth,n)){stack[top]=0;stackNear[top++]=n;}
    [loop]while(top>0){
        --top;uint id=stack[top];float nearT=stackNear[top];
        if(id>=nodeCount||++visited>nodeCount){h.status=2;break;}
        if(nearT>h.depth)continue;
        Node node=nodes[id];
        if(node.count){
            if(node.first>orderCount||node.count>orderCount-node.first){h.status=2;break;}
            [loop]for(uint j=0;j<node.count;++j){
                uint p=order[node.first+j];if(p>=triangleCount){h.status=2;break;}
                float t,u,v;if(hitTriangle(triangles[p],q,h.depth,t,u,v)&&(t<h.depth||(t==h.depth&&p<h.primitive))){
                    h.primitive=p;h.status=1;h.depth=t;h.u=u;h.v=v;
                }
            }
            if(h.status==2)break;
        }else{
            if(node.children<2||node.children>4){h.status=2;break;}
            uint4 kids=uint4(node.c0,node.c1,node.c2,node.c3);
            uint4 child=0;float4 dist=-3.402823466e38;uint count=0;
            [loop]for(uint childIndex=0;childIndex<node.children;++childIndex){
                uint k=kids[childIndex];if(k>=nodeCount||k==id){h.status=2;break;}
                float childNear;if(box(nodes[k],q.o,q.d,q.low,h.depth,childNear)){
                    child[count]=k;dist[count]=childNear;++count;
                }
            }
            if(h.status==2||top+count>128){h.status=2;break;}
            compareFarFirst(child,dist,0,1);compareFarFirst(child,dist,2,3);
            compareFarFirst(child,dist,0,2);compareFarFirst(child,dist,1,3);
            compareFarFirst(child,dist,1,2);
            [loop]for(uint pushIndex=0;pushIndex<count;++pushIndex){stack[top]=child[pushIndex];stackNear[top++]=dist[pushIndex];}
        }
    }
    hits[i]=h;
}
)";
class TraceBatch {
    struct Triangle{float a[4],b[4],c[4];};
    static_assert(sizeof(Triangle)==48&&sizeof(EscargotRayQuery)==32&&sizeof(EscargotRayHit)==32,"Escargot compute ABI");
    ComPtr<ID3D11Buffer> geometry_,order_,queries_,hits_,readback_,constants_;
    ComPtr<ID3D11ShaderResourceView> geometrySrv_,orderSrv_,querySrv_;
    ComPtr<ID3D11UnorderedAccessView> hitUav_;
    ComPtr<ID3D11ComputeShader> shader_;
    uint64_t identity_=0,revision_=0;const Scene* source_=nullptr;size_t capacity_=0;
    ID3D11Device* device_=nullptr;
    static bool buffer(ID3D11Device* device,const void* data,size_t count,UINT stride,UINT bind,ComPtr<ID3D11Buffer>& out){
        if(!count||count>std::numeric_limits<UINT>::max()/stride)return false;
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(count*stride);desc.StructureByteStride=stride;
        desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;desc.Usage=data?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT;desc.BindFlags=bind;
        D3D11_SUBRESOURCE_DATA init{};init.pSysMem=data;
        return SUCCEEDED(device->CreateBuffer(&desc,data?&init:nullptr,out.ReleaseAndGetAddressOf()));
    }
    static bool srv(ID3D11Device* device,ID3D11Buffer* b,size_t count,ComPtr<ID3D11ShaderResourceView>& out){
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};desc.Format=DXGI_FORMAT_UNKNOWN;desc.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;desc.Buffer.NumElements=UINT(count);
        return SUCCEEDED(device->CreateShaderResourceView(b,&desc,out.ReleaseAndGetAddressOf()));
    }
public:
    bool run(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11ShaderResourceView* nodes,const Scene& scene,const LumaTraceBvh4& bvh,
             const std::vector<EscargotRayQuery>& input,std::vector<EscargotRayHit>& output,double& prepareMs,double& queryMs,std::wstring& error){
        using Clock=std::chrono::steady_clock;auto start=Clock::now();prepareMs=queryMs=0;
        if(!device||!context||!nodes||input.empty()||input.size()>1000000||scene.obj.empty()||scene.obj.size()>2000000||bvh.primitiveCount!=scene.order.size()){
            error=L"Escargot GPU trace: incompatible scene or diagnostic memory limit";return false;}
        if(device_!=device){*this=TraceBatch{};device_=device;}
        if(!shader_){
            ComPtr<ID3DBlob> code,log;
            HRESULT hr=D3DCompile(kTrace,sizeof(kTrace)-1,"EscargotTrace",nullptr,nullptr,"CSTrace","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&log);
            if(FAILED(hr)){error=L"Escargot trace shader: ";if(log){const char* p=static_cast<const char*>(log->GetBufferPointer());for(size_t j=0;j<std::min<size_t>(log->GetBufferSize(),2048);++j)error.push_back(wchar_t(static_cast<unsigned char>(p[j])));}return false;}
            if(FAILED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader_))){error=L"Escargot trace shader creation";return false;}
        }
        if(!geometrySrv_||!orderSrv_||source_!=&scene||identity_!=scene.gpuCacheIdentity||revision_!=scene.gpuGeometryRevision){
            std::vector<Triangle> all;all.reserve(scene.obj.size());
            for(const auto& o:scene.obj){if(o.kind!=Kind::Triangle){error=L"Escargot trace expects triangles";return false;}
                all.push_back({{o.a.x,o.a.y,o.a.z,0},{o.b.x,o.b.y,o.b.z,0},{o.c.x,o.c.y,o.c.z,0}});}
            geometrySrv_.Reset();orderSrv_.Reset();
            if(!buffer(device,all.data(),all.size(),48,D3D11_BIND_SHADER_RESOURCE,geometry_)||
               !srv(device,geometry_.Get(),all.size(),geometrySrv_)||
               !buffer(device,scene.order.data(),scene.order.size(),4,D3D11_BIND_SHADER_RESOURCE,order_)||
               !srv(device,order_.Get(),scene.order.size(),orderSrv_)){geometrySrv_.Reset();orderSrv_.Reset();error=L"Escargot original geometry allocation";return false;}
            source_=&scene;identity_=scene.gpuCacheIdentity;revision_=scene.gpuGeometryRevision;
        }
        if(!constants_){D3D11_BUFFER_DESC d{};d.ByteWidth=16;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            if(FAILED(device->CreateBuffer(&d,nullptr,&constants_))){error=L"Escargot trace constants";return false;}}
        if(capacity_<input.size()||!querySrv_||!hitUav_||!readback_){
            capacity_=0;querySrv_.Reset();hitUav_.Reset();readback_.Reset();const size_t n=input.size();
            if(!buffer(device,nullptr,n,32,D3D11_BIND_SHADER_RESOURCE,queries_)||!srv(device,queries_.Get(),n,querySrv_)||
               !buffer(device,nullptr,n,32,D3D11_BIND_UNORDERED_ACCESS,hits_)){error=L"Escargot trace query allocation";return false;}
            D3D11_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=DXGI_FORMAT_UNKNOWN;u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;u.Buffer.NumElements=UINT(n);
            D3D11_BUFFER_DESC s{};s.ByteWidth=UINT(n*32);s.Usage=D3D11_USAGE_STAGING;s.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            if(FAILED(device->CreateUnorderedAccessView(hits_.Get(),&u,&hitUav_))||FAILED(device->CreateBuffer(&s,nullptr,&readback_))){error=L"Escargot trace output allocation";return false;}capacity_=n;
        }
        prepareMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();start=Clock::now();
        D3D11_BOX region{0,0,0,UINT(input.size()*32),1,1};context->UpdateSubresource(queries_.Get(),0,&region,input.data(),0,0);
        const uint32_t params[]={uint32_t(input.size()),uint32_t(bvh.nodes.size()),uint32_t(scene.obj.size()),uint32_t(scene.order.size())};
        context->UpdateSubresource(constants_.Get(),0,nullptr,params,0,0);
        ID3D11ShaderResourceView* views[]={nodes,orderSrv_.Get(),geometrySrv_.Get(),querySrv_.Get()};
        ID3D11UnorderedAccessView* uav=hitUav_.Get();ID3D11Buffer* cb=constants_.Get();
        context->CSSetShader(shader_.Get(),nullptr,0);context->CSSetConstantBuffers(0,1,&cb);
        context->CSSetShaderResources(0,4,views);context->CSSetUnorderedAccessViews(0,1,&uav,nullptr);
        context->Dispatch(UINT((input.size()+63)/64),1,1);
        ID3D11ShaderResourceView* nullViews[4]{};ID3D11UnorderedAccessView* nullUav=nullptr;ID3D11Buffer* nullCb=nullptr;
        context->CSSetUnorderedAccessViews(0,1,&nullUav,nullptr);context->CSSetShaderResources(0,4,nullViews);context->CSSetConstantBuffers(0,1,&nullCb);context->CSSetShader(nullptr,nullptr,0);
        context->CopyResource(readback_.Get(),hits_.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(readback_.Get(),0,D3D11_MAP_READ,0,&mapped))){error=L"Escargot trace readback";return false;}
        output.resize(input.size());std::memcpy(output.data(),mapped.pData,output.size()*sizeof(EscargotRayHit));context->Unmap(readback_.Get(),0);
        queryMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();return true;
    }
};
}
