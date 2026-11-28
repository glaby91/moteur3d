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
struct Query {float3 o;float low;float3 d;uint previousPrimitive;};
struct Hit {uint primitive,status;float depth,u,v;float3 pad;};
StructuredBuffer<Node> nodes:register(t0);
StructuredBuffer<uint> order:register(t1);
StructuredBuffer<Triangle> triangles:register(t2);
StructuredBuffer<Query> queries:register(t3);
RWStructuredBuffer<Hit> hits:register(u0);
cbuffer Params:register(b0){uint queryCount,nodeCount,triangleCount,orderCount;}
float cpuDivide(float a,float b);
bool slab(float o,float d,float inverse,float lo,float hi,inout float low,inout float high){
    if(abs(d)<1e-12)return o>=lo&&o<=hi;
    precise float x=(lo-o)*inverse;
    precise float y=(hi-o)*inverse;
    float l=min(x,y),h=max(x,y);
    l-=4.76837158203125e-7*max(1.0,abs(l));h+=4.76837158203125e-7*max(1.0,abs(h));
    low=max(low,l);high=min(high,h);return high>=low;
}
bool box(Node n,float3 o,float3 d,float3 inverse,float low,float high,out float nearT){
    nearT=low;
    if(!slab(o.x,d.x,inverse.x,n.lo.x,n.hi.x,nearT,high))return false;
    if(!slab(o.y,d.y,inverse.y,n.lo.y,n.hi.y,nearT,high))return false;
    return slab(o.z,d.z,inverse.z,n.lo.z,n.hi.z,nearT,high);
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
// DX11 float division may differ from CPU IEEE rounding by one ULP.
// Correct the approximate quotient against exact integer midpoint bounds.
// Normal finite operands/results only; exceptional inputs retain native
// division. Products fit 49 bits, stored as low/high uint pairs.
uint2 wideProduct(uint a,uint b){
    uint a0=a&65535u,a1=a>>16,b0=b&65535u,b1=b>>16;
    uint low0=a0*b0,mid=a0*b1+a1*b0;
    uint lo=low0+(mid<<16);
    return uint2(lo,a1*b1+(mid>>16)+uint(lo<low0));
}
int wideCompare(uint2 a,uint2 b){
    if(a.y!=b.y)return a.y<b.y?-1:1;
    if(a.x!=b.x)return a.x<b.x?-1:1;
    return 0;
}
float packQuotient(uint signBits,int exponent,uint mantissa){
    if(mantissa>=0x1000000u){mantissa>>=1;++exponent;}
    return asfloat(signBits|(uint(exponent)<<23)|(mantissa&0x7fffffu));
}
float cpuDivide(float a,float b){
    precise float approx=a/b;
    uint aa=asuint(a),bb=asuint(b),ea=(aa>>23)&255u,eb=(bb>>23)&255u;
    uint ma=(aa&0x7fffffu)|0x800000u,mb=(bb&0x7fffffu)|0x800000u;
    uint sh=uint(ma<mb);
    int exponent=int(ea)-int(eb)+127-int(sh);
    if(ea==0u||eb==0u||ea==255u||eb==255u||exponent<=0||exponent>=254)return approx;
    uint signBits=(aa^bb)&0x80000000u;
    uint qb=asuint(approx)&0x7fffffffu;int qe=int(qb>>23);
    uint qm=(qb&0x7fffffu)|0x800000u;
    uint mantissa=qe==exponent?qm:(qe==exponent+1?0x1000000u:(qe==exponent-1?qm>>1:0u));
    uint shiftCount=24u+sh;
    uint2 numerator=uint2(ma<<shiftCount,ma>>(32u-shiftCount));
    // Usually zero or one correction; exact midpoint and round-to-even.
    if(mantissa>=0x7ffffeu&&mantissa<=0x1000002u){
        for(uint attempt=0;attempt<3;++attempt){
            int lower=wideCompare(numerator,wideProduct(2u*mantissa-1u,mb));
            if(lower<0||(lower==0&&(mantissa&1u)!=0u)){--mantissa;continue;}
            int upper=wideCompare(numerator,wideProduct(2u*mantissa+1u,mb));
            if(upper>0||(upper==0&&(mantissa&1u)!=0u)){++mantissa;continue;}
            return packQuotient(signBits,exponent,mantissa);
        }
    }
    // Exact restoring division if the hardware estimate is unexpectedly far.
    uint remainder=ma<<sh;mantissa=0;
    for(uint bitIndex=0;bitIndex<24;++bitIndex){
        mantissa<<=1;
        if(remainder>=mb){mantissa|=1u;remainder-=mb;}
        if(bitIndex!=23u)remainder<<=1;
    }
    if(2u*remainder>mb||(2u*remainder==mb&&(mantissa&1u)!=0u))++mantissa;
    return packQuotient(signBits,exponent,mantissa);
}
bool hitTriangle(Triangle t,Query q,float high,out float depth,out float u,out float v){
    depth=0;u=0;v=0;
    precise float3 e=t.b.xyz-t.a.xyz,f=t.c.xyz-t.a.xyz;
    precise float3 p=cpuCross(q.d,f);
    precise float det=cpuDot(e,p);if(abs(det)<1e-10)return false;
    precise float3 a=q.o-t.a.xyz;
    precise float uu=cpuDivide(cpuDot(a,p),det);u=uu;if(uu<0||uu>1)return false;
    precise float3 z=cpuCross(a,e);
    precise float vv=cpuDivide(cpuDot(q.d,z),det);v=vv;
    precise float uv=uu+vv;if(vv<0||uv>1)return false;
    precise float tt=cpuDivide(cpuDot(f,z),det);depth=tt;return isfinite(tt)&&tt>q.low&&tt<=high;
}
// Four-way sorting network: fixed five comparisons, no nested dynamic loop.
// Push farthest first so the nearest node is popped first.
void compareFarFirst(inout uint a,inout float da,inout uint b,inout float db){
    if(da<db){float t=da;da=db;db=t;uint k=a;a=b;b=k;}
}
bool childDistance(uint k,uint parent,Query q,float3 inverse,float high,out float distance){
    distance=-3.402823466e38;
    if(k>=nodeCount||k==parent)return false;
    float t;
    if(box(nodes[k],q.o,q.d,inverse,q.low,high,t))distance=t;
    return true;
}
Hit traceOne(Query q){
    Hit h;h.primitive=0xffffffffu;h.status=0;h.depth=3.402823466e38;h.u=0;h.v=0;h.pad=0;
    float3 inverse=float3(abs(q.d.x)<1e-12?0.0:cpuDivide(1.0,q.d.x),abs(q.d.y)<1e-12?0.0:cpuDivide(1.0,q.d.y),abs(q.d.z)<1e-12?0.0:cpuDivide(1.0,q.d.z));
    uint stack[128];float stackNear[128];uint top=0,visited=0;float n;
    if(nodeCount>0&&box(nodes[0],q.o,q.d,inverse,q.low,h.depth,n)){stack[top]=0;stackNear[top++]=n;}
    while(top>0){
        --top;uint id=stack[top];float nearT=stackNear[top];
        if(id>=nodeCount||++visited>nodeCount){h.status=2;break;}
        if(nearT>h.depth)continue;
        Node node=nodes[id];
        if(node.count){
            if(node.first>orderCount||node.count>orderCount-node.first){h.status=2;break;}
            for(uint j=0;j<node.count;++j){
                uint p=order[node.first+j];if(p>=triangleCount){h.status=2;break;}
                if(p==q.previousPrimitive)continue;
                float t,u,v;if(hitTriangle(triangles[p],q,h.depth,t,u,v)&&(t<h.depth||(t==h.depth&&p<h.primitive))){
                    h.primitive=p;h.status=1;h.depth=t;h.u=u;h.v=v;
                }
            }
            if(h.status==2)break;
        }else{
            if(node.children<2||node.children>4){h.status=2;break;}
            uint k0=node.c0,k1=node.c1,k2=node.c2,k3=node.c3;
            float d0=-3.402823466e38,d1=d0,d2=d0,d3=d0;
            bool ok0=childDistance(k0,id,q,inverse,h.depth,d0);
            bool ok1=childDistance(k1,id,q,inverse,h.depth,d1);
            if(!ok0||!ok1){h.status=2;break;}
            if(node.children>=3){if(!childDistance(k2,id,q,inverse,h.depth,d2)){h.status=2;break;}}
            if(node.children==4){if(!childDistance(k3,id,q,inverse,h.depth,d3)){h.status=2;break;}}
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
    return h;
}
[numthreads(64,1,1)]
void CSTrace(uint3 tid:SV_DispatchThreadID){
    uint i=tid.x;if(i<queryCount)hits[i]=traceOne(queries[i]);
}
)";
// Kept separate from the validated trace literal to respect MSVC's limit.
inline constexpr char kPaths[] = R"(
struct PathQuery { Query initial;uint receiver,quota,offset,maxCrossings; };
struct PathEvent { Query q;Hit hit; };
struct OpticalAttr {float3 n0;uint owner;float3 n1;float transmission;float3 n2;float pad0;float3 geometric;float pad1;};
StructuredBuffer<OpticalAttr> optical:register(t4);
StructuredBuffer<PathQuery> paths:register(t5);
RWStructuredBuffer<PathEvent> events:register(u1);
// Correct native sqrt to round-to-nearest-even for normal finite inputs.
// Compare the squared midpoint with the exact integer radicand.
float cpuSqrt(float x){
    precise float approx=sqrt(x);
    uint bits=asuint(x),ea=(bits>>23)&255u;
    if((bits&0x80000000u)||ea==0u||ea==255u)return approx;
    int e=int(ea)-127;
    uint parity=uint(e&1);
    int exponent=(e-int(parity))/2+127;
    uint ma=(bits&0x7fffffu)|0x800000u,shiftCount=25u+parity;
    uint2 radicand=uint2(ma<<shiftCount,ma>>(32u-shiftCount));
    uint qb=asuint(approx),qm=(qb&0x7fffffu)|0x800000u;int qe=int((qb>>23)&255u);
    uint m=qe==exponent?qm:(qe==exponent+1?0x1000000u:(qe==exponent-1?qm>>1:0u));
    if(m>=0x7ffffeu&&m<=0x1000002u){
        for(uint attempt=0;attempt<3;++attempt){
            int lo=wideCompare(radicand,wideProduct(2u*m-1u,2u*m-1u));
            int hi=wideCompare(radicand,wideProduct(2u*m+1u,2u*m+1u));
            if(lo<0||(lo==0&&(m&1u))){--m;continue;}
            if(hi>0||(hi==0&&(m&1u))){++m;continue;}
            return packQuotient(0u,exponent,m);
        }
    }
    // Bounded exact fallback: largest mantissa not exceeding sqrt.
    uint lower=0x800000u,upper=0x1000000u;
    for(uint bitIndex=0;bitIndex<25;++bitIndex){
        if(lower>=upper)break;
        uint mid=lower+(upper-lower+1u)/2u;
        if(wideCompare(wideProduct(2u*mid,2u*mid),radicand)<=0)lower=mid;else upper=mid-1u;
    }
    m=lower;
    int cmp=wideCompare(radicand,wideProduct(2u*m+1u,2u*m+1u));
    if(cmp>0||(cmp==0&&(m&1u)))++m;
    return packQuotient(0u,exponent,m);
}
float cpuLength(float3 v){return cpuSqrt(cpuDot(v,v));}
float3 cpuNorm(float3 v){
    precise float l=cpuLength(v);
    return l>1e-20?float3(cpuDivide(v.x,l),cpuDivide(v.y,l),cpuDivide(v.z,l)):float3(0,1,0);
}
float3 cpuReflect(float3 d,float3 n){
    precise float k=2.0*cpuDot(d,n);
    precise float3 v=d-n*k;
    return cpuNorm(v);
}
[numthreads(64,1,1)]
void CSPaths(uint3 tid:SV_DispatchThreadID){
    uint i=tid.x;if(i>=queryCount)return;
    PathQuery path=paths[i];Query q=path.initial;
    uint used=0,crossings=0,error=0;bool secondary=false;
    PathEvent first;first.q=q;first.hit=(Hit)0;
    precise float primaryTransmission=1.0;
    while(used<path.quota&&crossings<path.maxCrossings){
        Hit hit=traceOne(q);PathEvent ev;ev.q=q;ev.hit=hit;
        if(used==0u)first=ev;else events[path.offset+used]=ev;++used;++crossings;
        if(hit.status>1u){error=1;break;}
        if(hit.status==0u)break;
        OpticalAttr a=optical[hit.primitive];
        if(a.owner==0u){error=1;break;}
        if(!secondary){
            if(a.owner!=path.receiver){
                primaryTransmission*=a.transmission;
                if(primaryTransmission<=.001)break;
                q.low=hit.depth;q.previousPrimitive=hit.primitive;continue;
            }
            precise float w=1.0-hit.u-hit.v;
            precise float3 nn=a.n0*w+a.n1*hit.u+a.n2*hit.v;
            float3 geometric=a.geometric;
            precise float3 normal=cpuLength(nn)>1e-8?cpuNorm(nn):geometric;
            if(cpuDot(geometric,q.d)>=0.0)geometric=-geometric;
            if(cpuDot(normal,geometric)<0.0)normal=-normal;
            if(cpuDot(normal,q.d)>0.0)normal=geometric;
            precise float3 direction=cpuReflect(q.d,normal);
            if(cpuDot(direction,geometric)<=0.0)direction=cpuReflect(q.d,geometric);
            precise float3 pos=q.o+q.d*hit.depth;
            q.o=pos;q.d=direction;q.previousPrimitive=0xffffffffu;
            q.low=max(1e-6,4.76837158203125e-7*max(1.0,cpuLength(pos)));
            secondary=true;crossings=0;
        }else{
            if(1.0-a.transmission>=.999)break;
            q.low=hit.depth;q.previousPrimitive=hit.primitive;
        }
    }
    // Every path has at least two reserved entries and at least one event.
    // Write count/error only after the actual path has terminated.
    first.hit.pad.x=float(used);first.hit.pad.y=float(error);events[path.offset]=first;
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
    struct OpticalAttr {float n0[3];uint32_t owner;float n1[3],transmission,n2[3],pad0,geometric[3],pad1;};
    static_assert(sizeof(OpticalAttr)==64&&sizeof(EscargotPathQuery)==48&&sizeof(EscargotPathEvent)==64,"Escargot resident path ABI");
    ComPtr<ID3D11Buffer> optical_,pathInput_,pathEvents_,pathReadback_;
    ComPtr<ID3D11ShaderResourceView> opticalSrv_,pathInputSrv_;
    ComPtr<ID3D11UnorderedAccessView> pathEventUav_;
    ComPtr<ID3D11ComputeShader> pathShader_;
    ComPtr<ID3D11Query> pathDisjoint_,pathStart_,pathEnd_;
    const Scene* opticalSource_=nullptr;uint64_t opticalRevision_=0,opticalIdentity_=0;
    size_t pathCapacity_=0,eventCapacity_=0;
    bool ensureGeometry(ID3D11Device* device,const Scene& scene,std::wstring& error){
        if(device_!=device){*this=TraceBatch{};device_=device;}
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
        return true;
    }
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
        if(!ensureGeometry(device,scene,error))return false;
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
    bool runPaths(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11ShaderResourceView* nodes,const Scene& scene,const LumaTraceBvh4& bvh,
        const std::vector<EscargotPathQuery>& input,std::vector<EscargotPathEvent>& output,EscargotPathTiming& timing,std::wstring& error){
        using Clock=std::chrono::steady_clock;auto start=Clock::now();timing={};output.clear();
        if(!device||!context||!nodes||input.empty()||input.size()>500000||scene.obj.empty()||scene.obj.size()>2000000||
           bvh.primitiveCount!=scene.order.size()||scene.escargotReceiverIds.size()!=scene.obj.size()){
            error=L"Escargot paths: incompatible scene or memory limit";return false;}
        size_t count=0;
        for(const auto& q:input){
            if(q.offset!=count||!q.receiver||q.quota<2||q.quota>64||q.maxCrossings<1||q.maxCrossings>32||q.quota>1000000-count){
                error=L"Escargot paths: invalid quota/range";return false;}
            count+=q.quota;
        }
        if(!ensureGeometry(device,scene,error))return false;
        if(!pathShader_){
            std::string source=std::string(kTrace)+kPaths;
            ComPtr<ID3DBlob> code,log;
            auto hr=D3DCompile(source.data(),source.size(),"EscargotPaths",nullptr,nullptr,"CSPaths","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_IEEE_STRICTNESS,0,&code,&log);
            if(FAILED(hr)){error=L"Escargot paths shader: ";if(log){const char* p=static_cast<const char*>(log->GetBufferPointer());for(size_t j=0;j<std::min<size_t>(log->GetBufferSize(),4096);++j)error.push_back(wchar_t(static_cast<unsigned char>(p[j])));}return false;}
            if(FAILED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&pathShader_))){error=L"Escargot paths shader creation";return false;}
        }
        if(!opticalSrv_||opticalSource_!=&scene||opticalIdentity_!=scene.gpuCacheIdentity||opticalRevision_!=scene.gpuCacheRevision){
            std::vector<OpticalAttr> all;all.reserve(scene.obj.size());
            for(size_t i=0;i<scene.obj.size();++i){const auto& o=scene.obj[i];if(o.mat>=scene.mat.size()||!scene.escargotReceiverIds[i]){error=L"Escargot optical attributes";return false;}
                const auto& m=scene.mat[o.mat];const bool water=m.surfaceAuthored?m.waterSurface:(m.waterSurface||m.name=="cse_pool_water");
                const float trans=water?1.f:clampf(float(m.transmissionPercent)/100.f,0.f,1.f);
                const V g=norm(cross(o.b-o.a,o.c-o.a));
                all.push_back({{o.n0.x,o.n0.y,o.n0.z},scene.escargotReceiverIds[i],{o.n1.x,o.n1.y,o.n1.z},trans,{o.n2.x,o.n2.y,o.n2.z},0,{g.x,g.y,g.z},0});
            }
            opticalSrv_.Reset();
            if(!buffer(device,all.data(),all.size(),64,D3D11_BIND_SHADER_RESOURCE,optical_)||!srv(device,optical_.Get(),all.size(),opticalSrv_)){error=L"Escargot optical attributes allocation";return false;}
            opticalSource_=&scene;opticalRevision_=scene.gpuCacheRevision;opticalIdentity_=scene.gpuCacheIdentity;
        }
        if(pathCapacity_<input.size()||!pathInputSrv_){
            pathCapacity_=0;pathInputSrv_.Reset();
            if(!buffer(device,nullptr,input.size(),48,D3D11_BIND_SHADER_RESOURCE,pathInput_)||!srv(device,pathInput_.Get(),input.size(),pathInputSrv_)){error=L"Escargot path input allocation";return false;}
            pathCapacity_=input.size();
        }
        if(eventCapacity_<count||!pathEventUav_||!pathReadback_){
            eventCapacity_=0;pathEventUav_.Reset();pathReadback_.Reset();
            if(!buffer(device,nullptr,count,64,D3D11_BIND_UNORDERED_ACCESS,pathEvents_)){error=L"Escargot path event allocation";return false;}
            D3D11_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=DXGI_FORMAT_UNKNOWN;u.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;u.Buffer.NumElements=UINT(count);
            D3D11_BUFFER_DESC s{};s.ByteWidth=UINT(count*64);s.Usage=D3D11_USAGE_STAGING;s.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            if(FAILED(device->CreateUnorderedAccessView(pathEvents_.Get(),&u,&pathEventUav_))||FAILED(device->CreateBuffer(&s,nullptr,&pathReadback_))){error=L"Escargot path output allocation";return false;}
            eventCapacity_=count;
        }
        if(!pathDisjoint_||!pathStart_||!pathEnd_){
            D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
            if(FAILED(device->CreateQuery(&desc,pathDisjoint_.ReleaseAndGetAddressOf())))pathDisjoint_.Reset();
            desc.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(device->CreateQuery(&desc,pathStart_.ReleaseAndGetAddressOf())))pathStart_.Reset();
            if(FAILED(device->CreateQuery(&desc,pathEnd_.ReleaseAndGetAddressOf())))pathEnd_.Reset();
        }
        const bool timestamp=pathDisjoint_&&pathStart_&&pathEnd_;
        timing.prepareMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();start=Clock::now();
        D3D11_BOX region{0,0,0,UINT(input.size()*48),1,1};context->UpdateSubresource(pathInput_.Get(),0,&region,input.data(),0,0);
        const uint32_t params[]={uint32_t(input.size()),uint32_t(bvh.nodes.size()),uint32_t(scene.obj.size()),uint32_t(scene.order.size())};
        context->UpdateSubresource(constants_.Get(),0,nullptr,params,0,0);
        ID3D11ShaderResourceView* views[]={nodes,orderSrv_.Get(),geometrySrv_.Get(),nullptr,opticalSrv_.Get(),pathInputSrv_.Get()};
        ID3D11UnorderedAccessView* uav=pathEventUav_.Get();ID3D11Buffer* cb=constants_.Get();
        context->CSSetShader(pathShader_.Get(),nullptr,0);context->CSSetConstantBuffers(0,1,&cb);
        context->CSSetShaderResources(0,6,views);context->CSSetUnorderedAccessViews(1,1,&uav,nullptr);
        if(timestamp){context->Begin(pathDisjoint_.Get());context->End(pathStart_.Get());}
        context->Dispatch(UINT((input.size()+63)/64),1,1);
        if(timestamp){context->End(pathEnd_.Get());context->End(pathDisjoint_.Get());}
        ID3D11ShaderResourceView* nullViews[6]{};ID3D11UnorderedAccessView* nullUav=nullptr;ID3D11Buffer* nullCb=nullptr;
        context->CSSetUnorderedAccessViews(1,1,&nullUav,nullptr);context->CSSetShaderResources(0,6,nullViews);context->CSSetConstantBuffers(0,1,&nullCb);context->CSSetShader(nullptr,nullptr,0);
        // One copy and one blocking readback at the end of ALL optical paths.
        context->CopyResource(pathReadback_.Get(),pathEvents_.Get());
        timing.submitMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();start=Clock::now();
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(pathReadback_.Get(),0,D3D11_MAP_READ,0,&mapped))){error=L"Escargot path readback";return false;}
        output.resize(count);std::memcpy(output.data(),mapped.pData,count*sizeof(EscargotPathEvent));context->Unmap(pathReadback_.Get(),0);
        timing.readbackMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        if(timestamp){
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info{};UINT64 a=0,b=0;
            if(context->GetData(pathDisjoint_.Get(),&info,sizeof(info),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK&&
               context->GetData(pathStart_.Get(),&a,sizeof(a),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK&&
               context->GetData(pathEnd_.Get(),&b,sizeof(b),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK&&!info.Disjoint&&info.Frequency&&b>=a){
                timing.gpuMs=1000.0*double(b-a)/double(info.Frequency);timing.gpuTimingValid=true;
            }
        }
        return true;
    }
};
}
