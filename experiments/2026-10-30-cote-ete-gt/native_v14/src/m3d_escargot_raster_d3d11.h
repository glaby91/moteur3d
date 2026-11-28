#pragma once
#include "m3d_escargot_trace_d3d11.h"
#include <stdexcept>
namespace m3d::escargot_gpu {
// Original mesh UV/normal data and authored receiver permission. One upload
// per immutable scene/permission revision, no full-frame CPU transfer.
class RasterOptics {
    struct Triangle {float a[4],b[4],c[4];};
    struct Attributes {float n0[3];uint32_t material;float n1[3],opacity;float n2[3],pad;float uv01[4],uv2[4];};
    static_assert(sizeof(Triangle)==48&&sizeof(Attributes)==80,"Raster optical ABI");
    ComPtr<ID3D11Buffer> geom_,order_,attr_,eligible_,params_;
    ComPtr<ID3D11ShaderResourceView> geomView_,orderView_,attrView_,eligibleView_;
    ID3D11Device* device_=nullptr;
    const Scene* source_=nullptr;uint64_t identity_=0,revision_=0;
    std::vector<uint32_t> receiverKey_;
    static bool make(ID3D11Device* d,const void* data,size_t count,UINT stride,ComPtr<ID3D11Buffer>& b,ComPtr<ID3D11ShaderResourceView>& v){
        if(!count||count>std::numeric_limits<UINT>::max()/stride)return false;
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(count*stride);desc.StructureByteStride=stride;
        desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        D3D11_SUBRESOURCE_DATA init{};init.pSysMem=data;
        if(FAILED(d->CreateBuffer(&desc,&init,b.ReleaseAndGetAddressOf())))return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_UNKNOWN;view.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;view.Buffer.NumElements=UINT(count);
        return SUCCEEDED(d->CreateShaderResourceView(b.Get(),&view,v.ReleaseAndGetAddressOf()));
    }
public:
    bool prepare(ID3D11Device* d,ID3D11DeviceContext* ctx,const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& receivers,std::wstring& error){
        if(device_!=d){*this=RasterOptics{};device_=d;}
        if(s.obj.empty()||s.order.empty()||b.nodes.empty()||b.primitiveCount!=s.order.size()||s.obj.size()>2000000||receivers.empty()||receivers.size()>2000000){error=L"Invalid Object GPU geometry/catalogue";return false;}
        if(!geomView_||!attrView_||!orderView_||!eligibleView_||source_!=&s||identity_!=s.gpuCacheIdentity||revision_!=s.gpuCacheRevision||receiverKey_!=receivers){
            std::vector<Triangle> geom;std::vector<Attributes> attr;geom.reserve(s.obj.size());attr.reserve(s.obj.size());
            for(const auto& o:s.obj){
                if(o.kind!=Kind::Triangle||o.mat>=s.mat.size()||o.mat>=256){error=L"Object GPU requires triangles and valid raster materials";return false;}
                const auto& m=s.mat[o.mat];bool water=m.surfaceAuthored?m.waterSurface:(m.waterSurface||m.name=="cse_pool_water");
                float opacity=water?0.f:1.f-clampf(float(m.transmissionPercent)/100.f,0.f,1.f);
                geom.push_back({{o.a.x,o.a.y,o.a.z,0},{o.b.x,o.b.y,o.b.z,0},{o.c.x,o.c.y,o.c.z,0}});
                attr.push_back({{o.n0.x,o.n0.y,o.n0.z},o.mat,{o.n1.x,o.n1.y,o.n1.z},opacity,{o.n2.x,o.n2.y,o.n2.z},0,{o.uv0.x,o.uv0.y,o.uv1.x,o.uv1.y},{o.uv2.x,o.uv2.y,0,0}});
            }
            geomView_.Reset();attrView_.Reset();orderView_.Reset();eligibleView_.Reset();
            if(!make(d,geom.data(),geom.size(),48,geom_,geomView_)||!make(d,s.order.data(),s.order.size(),4,order_,orderView_)||
               !make(d,attr.data(),attr.size(),80,attr_,attrView_)||!make(d,receivers.data(),receivers.size(),4,eligible_,eligibleView_)){
                error=L"Object GPU immutable buffers allocation";return false;}
            source_=&s;identity_=s.gpuCacheIdentity;revision_=s.gpuCacheRevision;receiverKey_=receivers;
        }
        if(!params_){D3D11_BUFFER_DESC desc{};desc.ByteWidth=16;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            if(FAILED(d->CreateBuffer(&desc,nullptr,&params_))){error=L"Object GPU parameters allocation";return false;}}
        uint32_t data[]={uint32_t(receivers.size()),uint32_t(b.nodes.size()),uint32_t(s.obj.size()),uint32_t(s.order.size())};
        ctx->UpdateSubresource(params_.Get(),0,nullptr,data,0,0);return true;
    }
    void bind(ID3D11DeviceContext* c,ID3D11ShaderResourceView* nodes){
        ID3D11ShaderResourceView* views[]={nodes,orderView_.Get(),geomView_.Get(),attrView_.Get(),eligibleView_.Get()};
        ID3D11Buffer* cb=params_.Get();c->PSSetShaderResources(14,5,views);c->PSSetConstantBuffers(1,1,&cb);
    }
    static void unbind(ID3D11DeviceContext* c){ID3D11ShaderResourceView* views[5]{};ID3D11Buffer* cb=nullptr;c->PSSetShaderResources(14,5,views);c->PSSetConstantBuffers(1,1,&cb);}
};
// Share the certified BVH4 traversal source with the compute diagnostic.
// Only resource bindings and the compute entry point are different.
inline std::string rasterTraceSource(){
    std::string s(kTrace);auto cut=s.find("[numthreads(64,1,1)]");s.resize(cut);
    auto replace=[&](const std::string& a,const std::string& b){auto at=s.find(a);if(at==std::string::npos)throw std::runtime_error("Missing shared trace binding");s.replace(at,a.size(),b);};
    replace("nodes:register(t0)","nodes:register(t14)");replace("order:register(t1)","order:register(t15)");replace("triangles:register(t2)","triangles:register(t16)");
    replace("StructuredBuffer<Query> queries:register(t3);","");replace("RWStructuredBuffer<Hit> hits:register(u0);","");
    replace("cbuffer Params:register(b0)","cbuffer OpticalParams:register(b1)");
    // Exact geometry and tie-break are preserved. Native GPU division avoids
    // the CPU bit-parity emulation overhead for each shaded fragment. The
    // resulting render requires visual validation on actual hardware.
    auto first=s.find("float cpuDivide(float a,float b){"),last=s.find("bool hitTriangle(",first);
    if(first==std::string::npos||last==std::string::npos)throw std::runtime_error("Missing trace division function");
    s.replace(first,last-first,"float cpuDivide(float a,float b){return a/b;}\n");
    return s;
}
inline constexpr char kRasterOptics[] = R"(
struct ObjectAttributes {float3 n0;uint mat;float3 n1;float opacity;float3 n2;float pad;float4 uv01;float4 uv2;};
StructuredBuffer<ObjectAttributes> opticalAttr:register(t17);
StructuredBuffer<uint> opticalEligible:register(t18);
float3 opticalTexture(uint mat,float2 uv){
    float layer=materialTextureDetail[mat].x;
    if(layer>=0)return preservedAlbedo.SampleLevel(probeSampler,float3(saturate(uv),layer),0).rgb;
    return albedo.SampleLevel(wrapSampler,float3(uv,mat),0).rgb;
}
float3 opticalShade(uint mat,float2 uv,float3 pos,float3 n){
    float3 base=opticalTexture(mat,uv);
    // Reuse the scene's solar direction and existing diffuse GI volume.
    // Single shadow-map lookup at the actual reflected world-space hit.
    float3 sd=pos-shadowBase;
    float2 su=float2(dot(sd,shadowRight),dot(sd,shadowUp))/max(shadowSpan,.001)*.5+.5;
    float visibility=1;
    if(all(su>=0)&&all(su<=1)){
        float dist=shadowDist-dot(sd,lightDir);
        float stored=shadowDepth.SampleLevel(probeSampler,su,0);
        visibility=dist<=stored+.025?1:.16;
    }
    float day=saturate(lightDir.y*4.0+.2);
    float3 light=float3(.12+.35*day,.12+.35*day,.12+.35*day)+.48*day*saturate(dot(n,normalize(lightDir)))*visibility;
    if(hybridControls.x>.5&&hybridControls.w>1.5){
        float3 uvw=saturate((pos-probeOrigin.xyz)*probeInvSpan.xyz);
        light+=max(indirectProbes.SampleLevel(probeSampler,uvw,0).rgb,0)*materialTextureDetail[mat].z;
    }
    return base*max(light,0)+materialEmission[mat].rgb*max(materialEmission[mat].a,0);
}
bool escRasterOptical(VSOut i,out float3 reflected,out float coverage,out float weight){
    reflected=0;coverage=0;weight=0;
    if(i.receiverId==0u||i.receiverId>=queryCount||opticalEligible[i.receiverId]==0u)return false;
    float3 view=normalize(i.world-eye),n=normalize(i.n);
    if(dot(n,view)>0)n=-n;
    float3 direction=normalize(reflect(view,n));
    float f0=max(saturate(materialOpacity[i.mat].z),saturate(materialSpecular[i.mat].w)*.85);
    weight=saturate(f0+(1-f0)*pow(1-saturate(-dot(n,view)),5));
    if(weight<.001)return false;
    Query q;q.o=i.world;q.d=direction;q.low=max(1e-5,9.5367431640625e-7*max(1.0,length(i.world)));q.previousPrimitive=0xffffffffu;
    float3 inv=traceInverse(q.d),rgb=0;float remain=1;
    bool complete=false;
    [loop] for(uint crossing=0;crossing<16u;++crossing){
        Hit hit=traceOnePrepared(q,inv);
        if(hit.status==2u)return false;
        if(hit.status==0u){complete=true;break;}
        ObjectAttributes a=opticalAttr[hit.primitive];
        if(a.mat>=materialCount)return false;
        float w=1-hit.u-hit.v;
        float2 uv=a.uv01.xy*w+a.uv01.zw*hit.u+a.uv2.xy*hit.v;
        float3 normal=normalize(a.n0*w+a.n1*hit.u+a.n2*hit.v);
        if(dot(normal,q.d)>0)normal=-normal;
        float alpha=saturate(a.opacity);
        if(alpha>.00001)rgb+=remain*alpha*opticalShade(a.mat,uv,q.o+q.d*hit.depth,normal);
        remain*=1-alpha;
        if(alpha>=.999||remain<=.001){complete=true;break;}
        q.low=hit.depth;q.previousPrimitive=hit.primitive;
    }
    // Never treat an exhausted/invalid path as a resolved reflection.
    if(!complete)return false;
    coverage=saturate(1-remain);
    if(coverage<=.00001)return false;
    reflected=max(rgb/coverage,0);return true;
}
)";
}
