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
bool slab(float o,float d,float lo,float hi,inout float low,inout float high){
    if(abs(d)<1e-12)return o>=lo&&o<=hi;
    precise float inverse=1.0/d;
    precise float x=(lo-o)*inverse;
    precise float y=(hi-o)*inverse;
    float l=min(x,y),h=max(x,y);
    l-=4.76837158203125e-7*max(1.0,abs(l));h+=4.76837158203125e-7*max(1.0,abs(h));
    low=max(low,l);high=min(high,h);return high>=low;
}
bool box(Node n,float3 o,float3 d,float low,float high,out float nearT){
    nearT=low;
    if(!slab(o.x,d.x,n.lo.x,n.hi.x,nearT,high))return false;
    if(!slab(o.y,d.y,n.lo.y,n.hi.y,nearT,high))return false;
    return slab(o.z,d.z,n.lo.z,n.hi.z,nearT,high);
}
// Match CPU m3d::dot and m3d::cross, including evaluation order.
float cpuDot(float3 a,float3 b){
    precise float x=a.x*b.x,y=a.y*b.y,z=a.z*b.z;
    precise float xy=x+y;
    precise float result=xy+z;return result;
}
float3 cpuCross(float3 a,float3 b){
    precise float x=a.y*b.z-a.z*b.y;
    precise float y=a.z*b.x-a.x*b.z;
    precise float z=a.x*b.y-a.y*b.x;
    return float3(x,y,z);
}
bool hitTriangle(Triangle t,Query q,float high,out float depth,out float u,out float v){
    depth=0;u=0;v=0;
    precise float3 e=t.b.xyz-t.a.xyz,f=t.c.xyz-t.a.xyz;
    precise float3 p=cpuCross(q.d,f);
    precise float det=cpuDot(e,p);if(abs(det)<1e-10)return false;
    precise float3 a=q.o-t.a.xyz;
    precise float uu=cpuDot(a,p)/det;u=uu;if(uu<0||uu>1)return false;
    precise float3 z=cpuCross(a,e);
    precise float vv=cpuDot(q.d,z)/det;v=vv;
    precise float uv=uu+vv;if(vv<0||uv>1)return false;
    precise float tt=cpuDot(f,z)/det;depth=tt;return isfinite(tt)&&tt>q.low&&tt<=high;
}
// Four-way sorting network: fixed five comparisons, no nested dynamic loop.
// Push farthest first so the nearest node is popped first.
void compareFarFirst(inout uint a,inout float da,inout uint b,inout float db){
    if(da<db){float t=da;da=db;db=t;uint k=a;a=b;b=k;}
}
bool childDistance(uint k,uint parent,Query q,float high,out float distance){
    distance=-3.402823466e38;
    if(k>=nodeCount||k==parent)return false;
    float t;
    if(box(nodes[k],q.o,q.d,q.low,high,t))distance=t;
    return true;
}
[numthreads(64,1,1)]
void CSTrace(uint3 tid:SV_DispatchThreadID){
    uint i=tid.x;if(i>=queryCount)return;
    Query q=queries[i];Hit h;h.primitive=0xffffffffu;h.status=0;h.depth=3.402823466e38;h.u=0;h.v=0;h.pad=0;
    uint stack[128];float stackNear[128];uint top=0,visited=0;float n;
    if(nodeCount>0&&box(nodes[0],q.o,q.d,q.low,h.depth,n)){stack[top]=0;stackNear[top++]=n;}
    while(top>0){
        --top;uint id=stack[top];float nearT=stackNear[top];
        if(id>=nodeCount||++visited>nodeCount){h.status=2;break;}
        if(nearT>h.depth)continue;
        Node node=nodes[id];
        if(node.count){
            if(node.first>orderCount||node.count>orderCount-node.first){h.status=2;break;}
            for(uint j=0;j<node.count;++j){
                uint p=order[node.first+j];if(p>=triangleCount){h.status=2;break;}
                float t,u,v;if(hitTriangle(triangles[p],q,h.depth,t,u,v)&&(t<h.depth||(t==h.depth&&p<h.primitive))){
                    h.primitive=p;h.status=1;h.depth=t;h.u=u;h.v=v;
                }
            }
            if(h.status==2)break;
        }else{
            if(node.children<2||node.children>4){h.status=2;break;}
            uint k0=node.c0,k1=node.c1,k2=node.c2,k3=node.c3;
            float d0=-3.402823466e38,d1=d0,d2=d0,d3=d0;
            bool ok0=childDistance(k0,id,q,h.depth,d0);
            bool ok1=childDistance(k1,id,q,h.depth,d1);
            if(!ok0||!ok1){h.status=2;break;}
            if(node.children>=3){if(!childDistance(k2,id,q,h.depth,d2)){h.status=2;break;}}
            if(node.children==4){if(!childDistance(k3,id,q,h.depth,d3)){h.status=2;break;}}
            compareFarFirst(k0,d0,k1,d1);compareFarFirst(k2,d2,k3,d3);
            compareFarFirst(k0,d0,k2,d2);compareFarFirst(k1,d1,k3,d3);
            compareFarFirst(k1,d1,k2,d2);
            uint count=uint(d0>=q.low)+uint(d1>=q.low)+uint(d2>=q.low)+uint(d3>=q.low);
            if(top+count>128){h.status=2;break;}
            if(count>=1){stack[top]=k0;stackNear[top]=d0;++top;}
            if(count>=2){stack[top]=k1;stackNear[top]=d1;++top;}
            if(count>=3){stack[top]=k2;stackNear[top]=d2;++top;}
            if(count>=4){stack[top]=k3;stackNear[top]=d3;++top;}
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
            HRESULT hr=D3DCompile(kTrace,sizeof(kTrace)-1,"EscargotTrace",nullptr,nullptr,"CSTrace","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&code,&log);
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
