#pragma once
#include "m3d_directx11_raster_shader.h"
#include <array>
#include <chrono>
namespace m3d::escargot_gpu {
inline constexpr char kProjectedPass[]=R"(
struct ProjIn {float4 p:POSITION;float4 data:TEXCOORD0;float2 extra:TEXCOORD1;};
struct ProjOut {float4 p:SV_POSITION;float2 uv:TEXCOORD0;float illum:TEXCOORD1;nointerpolation uint mat:MATERIAL;nointerpolation float alpha:TEXCOORD2;float planeDistance:SV_ClipDistance0;};
ProjOut VSProjected(ProjIn i){ProjOut o;float3 q=i.p.xyz-eye;float z=dot(q,forward);o.p=float4(dot(q,right)/tanHalf,dot(q,up)*aspect/tanHalf,(farPlane/(farPlane-nearPlane))*z-(farPlane*nearPlane)/(farPlane-nearPlane),z);o.uv=i.data.xy;o.illum=i.data.z;o.mat=uint(i.data.w);o.alpha=i.extra.x;o.planeDistance=i.extra.y;return o;}
float4 PSProjected(ProjOut i):SV_TARGET{
    float layer=materialTextureDetail[i.mat].x;
    float3 base=layer>=0?preservedAlbedo.Sample(probeSampler,float3(saturate(i.uv),layer)).rgb:albedo.Sample(wrapSampler,float3(i.uv,i.mat)).rgb;
    float3 color=base*max(i.illum,0)+materialEmission[i.mat].rgb*max(materialEmission[i.mat].a,0);
    return float4(color,saturate(i.alpha));
}
)";
inline constexpr char kResidentPass[]=R"(
ByteAddressBuffer opticalVertices : register(t14);
Buffer<uint2> opticalBlocks : register(t15);
cbuffer Optical007 : register(b1) { float4 opticalPlane; float opticalSign; uint opticalBase; float2 opticalPad; };
ProjOut VSResident(uint vertex:SV_VertexID,uint instance:SV_InstanceID){
    ProjOut o=(ProjOut)0;
    uint2 b=opticalBlocks[opticalBase+instance];
    uint tri=vertex/3u;
    if(tri>=b.y){o.p=float4(0,0,0,1);o.planeDistance=-1;return o;}
    uint at=((b.x+tri)*3u+vertex%3u)*40u;
    float3 pos=asfloat(opticalVertices.Load3(at));
    float3 n=asfloat(opticalVertices.Load3(at+12u));
    o.uv=asfloat(opticalVertices.Load2(at+24u));
    o.mat=opticalVertices.Load(at+32u);
    o.alpha=asfloat(opticalVertices.Load(at+36u));
    float distance=dot(pos,opticalPlane.xyz)-opticalPlane.w;
    float3 q=pos-2*distance*opticalPlane.xyz-eye;
    float z=dot(q,forward);
    o.p=float4(dot(q,right)/tanHalf,dot(q,up)*aspect/tanHalf,(farPlane/(farPlane-nearPlane))*z-(farPlane*nearPlane)/(farPlane-nearPlane),z);
    o.planeDistance=opticalSign*distance-.0001;
    float shadow=1;
    if(shadowSize>0&&shadowSpan>0){
        float3 delta=pos-shadowBase;float scale=shadowSize/(2*shadowSpan);
        int2 coord=int2(floor(float2(dot(delta,shadowRight),dot(delta,shadowUp))*scale+shadowSize*.5));
        if(all(coord>=0)&&all(coord<int2(shadowSize,shadowSize)))
            shadow=shadowDist-dot(delta,lightDir)<=shadowDepth.Load(int3(coord,0))+.045?1:.22;
    }
    float day=materialDiffuse[o.mat].w;
    o.illum=.16+.34*day+.50*day*saturate(dot(normalize(n),lightDir))*shadow;
    return o;
}
)";
class ProjectedOptics {
    ID3D11Device* device_=nullptr;
    ComPtr<ID3D11VertexShader> vs_;ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11InputLayout> layout_;
    struct ResidentLayer {ComPtr<ID3D11Buffer> vertices;UINT capacity=0;std::vector<float> data;};
    std::array<ResidentLayer,4> resident_;
    ComPtr<ID3D11VertexShader> residentVs_;
    ComPtr<ID3D11Buffer> sourceBuffer_,blockBuffer_,opticalConstants_;
    ComPtr<ID3D11ShaderResourceView> sourceView_,blockView_;
    UINT blockCapacity_=0;
    std::vector<EscargotResidentBlock> uploadedBlocks_;
    uint64_t residentShadowRevision_=0;
    std::array<float,12> residentShadowKey_{};
    struct OpticalCB {float plane[4],sign;uint32_t base;float pad[2];};
    static_assert(sizeof(OpticalCB)==32,"007 constant layout");
    static_assert(sizeof(EscargotResidentBlock)==8,"007 typed buffer layout");
    bool residentShaders(ID3D11Device* d,std::wstring& error){
        if(residentVs_&&opticalConstants_)return true;
        const std::string src=std::string(dx11::shaders::raster::kShader)+kProjectedPass+kResidentPass;
        ComPtr<ID3DBlob> code,diagnostics;
        if(FAILED(D3DCompile(src.data(),src.size(),"EscargotResident007",nullptr,nullptr,"VSResident","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&diagnostics))){
            error=L"Object007 vertex shader compilation";
            if(diagnostics){const char* a=static_cast<const char*>(diagnostics->GetBufferPointer());while(*a)error+=wchar_t(static_cast<unsigned char>(*a++));}return false;
        }
        if(FAILED(d->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,residentVs_.ReleaseAndGetAddressOf()))){error=L"Object007 vertex shader creation";return false;}
        D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(OpticalCB);b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(d->CreateBuffer(&b,nullptr,opticalConstants_.ReleaseAndGetAddressOf()))){error=L"Object007 plane constants";return false;}return true;
    }
    bool source(ID3D11Device* d,const Scene& scene,EscargotProfile& profile,std::wstring& error){
        if(sourceView_)return true;
        struct Vertex {float p[3],n[3],uv[2];uint32_t mat;float alpha;};
        static_assert(sizeof(Vertex)==40,"007 raw vertex layout");
        if(scene.obj.empty()||scene.obj.size()>UINT_MAX/(3*sizeof(Vertex))){error=L"Object007 invalid resident geometry size";return false;}
        std::vector<Vertex> data;data.reserve(scene.obj.size()*3);
        for(const auto& t:scene.obj){
            if(t.kind!=Kind::Triangle||t.mat>=scene.mat.size()){error=L"Object007 invalid scene triangle";return false;}
            const auto& m=scene.mat[t.mat];
            const bool water=m.surfaceAuthored?m.waterSurface:(m.waterSurface||m.name=="cse_pool_water");
            const float alpha=water?0.f:1.f-std::clamp(float(m.transmissionPercent)/100.f,0.f,1.f);
            const V pos[]={t.a,t.b,t.c},normal[]={t.n0,t.n1,t.n2},uv[]={t.uv0,t.uv1,t.uv2};
            for(int k=0;k<3;++k)data.push_back({{pos[k].x,pos[k].y,pos[k].z},{normal[k].x,normal[k].y,normal[k].z},{uv[k].x,uv[k].y},uint32_t(t.mat),alpha});
        }
        D3D11_BUFFER_DESC b{};b.ByteWidth=UINT(data.size()*sizeof(Vertex));b.Usage=D3D11_USAGE_IMMUTABLE;b.BindFlags=D3D11_BIND_SHADER_RESOURCE;b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        D3D11_SUBRESOURCE_DATA init{};init.pSysMem=data.data();
        if(FAILED(d->CreateBuffer(&b,&init,sourceBuffer_.ReleaseAndGetAddressOf()))){error=L"Object007 resident geometry allocation";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=DXGI_FORMAT_R32_TYPELESS;v.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;v.BufferEx.NumElements=b.ByteWidth/4;v.BufferEx.Flags=D3D11_BUFFEREX_SRV_FLAG_RAW;
        if(FAILED(d->CreateShaderResourceView(sourceBuffer_.Get(),&v,sourceView_.ReleaseAndGetAddressOf()))){sourceBuffer_.Reset();error=L"Object007 resident geometry view";return false;}
        profile.residentGeometryBytes=b.ByteWidth;return true;
    }
    bool uploadBlocks(ID3D11Device* d,ID3D11DeviceContext* c,const std::vector<EscargotResidentBlock>& blocks,EscargotProfile& profile,std::wstring& error){
        if(blocks.empty())return true;
        const UINT count=UINT(blocks.size()),bytes=count*UINT(sizeof(EscargotResidentBlock));
        if(blockView_&&uploadedBlocks_.size()==count&&std::memcmp(uploadedBlocks_.data(),blocks.data(),bytes)==0)return true;
        if(!blockView_||count>blockCapacity_){
            blockCapacity_=1;while(blockCapacity_<count)blockCapacity_*=2;
            D3D11_BUFFER_DESC b{};b.ByteWidth=blockCapacity_*8;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_SHADER_RESOURCE;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            blockView_.Reset();uploadedBlocks_.clear();
            if(FAILED(d->CreateBuffer(&b,nullptr,blockBuffer_.ReleaseAndGetAddressOf()))){error=L"Object007 block allocation";return false;}
            D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=DXGI_FORMAT_R32G32_UINT;v.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;v.Buffer.NumElements=blockCapacity_;
            if(FAILED(d->CreateShaderResourceView(blockBuffer_.Get(),&v,blockView_.ReleaseAndGetAddressOf()))){error=L"Object007 block view";return false;}
        }
        D3D11_MAPPED_SUBRESOURCE map{};
        if(FAILED(c->Map(blockBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map))){uploadedBlocks_.clear();error=L"Object007 block upload";return false;}
        std::memcpy(map.pData,blocks.data(),bytes);c->Unmap(blockBuffer_.Get(),0);
        uploadedBlocks_=blocks;profile.uploadBytes+=bytes;return true;
    }
    bool renderResident(ID3D11Device* d,ID3D11DeviceContext* c,const Scene& scene,const Camera& cam,const ShadowMap& sh,uint64_t shadowRevision,
        const EscargotPacketBuild& plan,int w,int h,const float clear[4],ID3D11Buffer* constants,
        ID3D11ShaderResourceView* albedo,ID3D11ShaderResourceView* preserved,ID3D11ShaderResourceView* shadow,
        ID3D11SamplerState* wrap,ID3D11SamplerState* clamp,ID3D11RasterizerState* raster,
        ID3D11DepthStencilState* opaque,ID3D11DepthStencilState* transparent,ID3D11BlendState* blend,EscargotProfile& profile,std::wstring& error){
        using Clock=std::chrono::steady_clock;auto start=Clock::now();
        const auto& blocks=plan.resident.blocks;
        if(w<=0||h<=0||plan.projectedRanges.size()>4||!plan.projectedVertices.empty()||blocks.size()>120000||plan.data.size()<8||plan.data.size()%4){error=L"Object007 invalid packet";return false;}
        uint64_t total=0,work=0,next=0;const size_t objects=size_t(plan.data[0]);
        if(objects+2>plan.data.size()/4||2*objects+2>plan.data.size()/4){error=L"Object007 invalid receiver table";return false;}
        for(const auto& r:plan.projectedRanges){
            if(!r.receiver||r.receiver>objects||r.blockFirst!=next||uint64_t(r.blockFirst)+r.blockOpaque+r.blockTransparent>blocks.size()||
               r.x<0||r.y<0||r.w<=0||r.h<=0||int64_t(r.x)+r.w>w||int64_t(r.y)+r.h>h){error=L"Object007 invalid draw range";return false;}
            uint64_t solid=0,alpha=0;uint32_t priorWidth=0;
            for(uint32_t k=0;k<r.blockOpaque+r.blockTransparent;++k){
                const auto& b=blocks[r.blockFirst+k];const bool isAlpha=k>=r.blockOpaque;
                if(!b.count||b.count>(isAlpha?1u:32u)||uint64_t(b.first)+b.count>scene.obj.size()){error=L"Object007 invalid source block";return false;}
                const uint32_t width=escargotResidentWidth(b.count);
                if(!isAlpha&&width<priorWidth){error=L"Object007 unsorted opaque blocks";return false;}
                if(!isAlpha)priorWidth=width;
                (isAlpha?alpha:solid)+=b.count*3u;work+=width*3u;
            }
            if(solid!=r.opaque||alpha!=r.transparent){error=L"Object007 block count mismatch";return false;}
            total+=solid+alpha;next+=r.blockOpaque+r.blockTransparent;
        }
        if(next!=blocks.size()||total>360000||work>360000){error=L"Object007 geometry budget exceeded";return false;}
        ID3D11ShaderResourceView* none=nullptr;c->PSSetShaderResources(8,1,&none);
        if(plan.projectedRanges.empty())return true;
        const int tw=std::max(1,w*3/4),th=std::max(1,h*3/4);
        if(!shaders(d,error)||!residentShaders(d,error)||!targets(d,tw,th,uint32_t(plan.projectedRanges.size()),error))return false;
        float day=0;V sun{};escargotLightingSnapshot(day,sun);
        const std::array<float,12> shadowKey={sh.base.x,sh.base.y,sh.base.z,sh.right.x,sh.right.y,sh.right.z,sh.up.x,sh.up.y,sh.up.z,sh.span,sh.dist,float(sh.size)};
        const std::array<float,4> lightKey={day,sun.x,sun.y,sun.z};
        profile.layerPixels=uint64_t(tw)*th*plan.projectedRanges.size();
        profile.allocationMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        if(plan.projectedKey&&renderedKey_==plan.projectedKey&&residentShadowRevision_==shadowRevision&&residentShadowKey_==shadowKey&&residentLightKey_==lightKey&&std::equal(renderedClear_.begin(),renderedClear_.end(),clear)){profile.cacheHit=true;return true;}
        renderedKey_=0;start=Clock::now();
        if(!blocks.empty()&&(!source(d,scene,profile,error)||!uploadBlocks(d,c,blocks,profile,error)))return false;
        profile.uploadMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        D3D11_VIEWPORT vp{};vp.Width=float(tw);vp.Height=float(th);vp.MaxDepth=1;c->RSSetViewports(1,&vp);
        c->RSSetState(raster);c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->VSSetShader(residentVs_.Get(),nullptr,0);c->VSSetConstantBuffers(0,1,&constants);
        ID3D11Buffer* optical=opticalConstants_.Get();c->VSSetConstantBuffers(1,1,&optical);
        c->PSSetShader(ps_.Get(),nullptr,0);c->PSSetConstantBuffers(0,1,&constants);
        c->VSSetShaderResources(1,1,&shadow);ID3D11ShaderResourceView* views[]={sourceView_.Get(),blockView_.Get()};c->VSSetShaderResources(14,2,views);
        c->PSSetShaderResources(0,1,&albedo);c->PSSetShaderResources(6,1,&preserved);ID3D11SamplerState* samplers[]={wrap,clamp};c->PSSetSamplers(0,2,samplers);
        auto finish=[&](){ID3D11ShaderResourceView* nulls[2]{};c->VSSetShaderResources(14,2,nulls);c->VSSetShaderResources(1,1,nulls);c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,0xffffffffu);};
        for(UINT i=0;i<plan.projectedRanges.size();++i){
            const auto& r=plan.projectedRanges[i];ID3D11RenderTargetView* target=rtv_[i].Get();
            c->OMSetRenderTargets(1,&target,dsv_.Get());c->ClearRenderTargetView(target,clear);c->ClearDepthStencilView(dsv_.Get(),D3D11_CLEAR_DEPTH,1,0);
            const LONG x0=std::max(0,int(int64_t(r.x)*tw/w)-1),y0=std::max(0,int(int64_t(r.y)*th/h)-1);
            const LONG x1=std::min(tw,int((int64_t(r.x+r.w)*tw+w-1)/w)+1),y1=std::min(th,int((int64_t(r.y+r.h)*th+h-1)/h)+1);
            D3D11_RECT rect{x0,y0,x1,y1};c->RSSetScissorRects(1,&rect);profile.scissorPixels+=uint64_t(x1-x0)*(y1-y0);
            const size_t at=4*(size_t(r.receiver)+1);OpticalCB cb{};
            for(int j=0;j<3;++j)cb.plane[j]=plan.data[at+1+j];
            cb.plane[3]=plan.data[4*(objects+1+r.receiver)];
            cb.sign=dot(cam.eye,V{cb.plane[0],cb.plane[1],cb.plane[2]})>=cb.plane[3]?1.f:-1.f;
            auto draw=[&](UINT n,UINT base,UINT verts)->bool{
                if(!n)return true;cb.base=base;D3D11_MAPPED_SUBRESOURCE map{};
                if(FAILED(c->Map(optical,0,D3D11_MAP_WRITE_DISCARD,0,&map))){error=L"Object007 plane upload";return false;}
                std::memcpy(map.pData,&cb,sizeof(cb));c->Unmap(optical,0);c->DrawInstanced(verts,n,0,0);++profile.draws;profile.residentSubmittedVertices+=uint64_t(verts)*n;return true;};
            c->OMSetDepthStencilState(opaque,0);c->OMSetBlendState(nullptr,nullptr,0xffffffffu);
            for(UINT start=0;start<r.blockOpaque;){
                const UINT width=escargotResidentWidth(blocks[r.blockFirst+start].count);UINT end=start+1;
                while(end<r.blockOpaque&&escargotResidentWidth(blocks[r.blockFirst+end].count)==width)++end;
                if(!draw(end-start,r.blockFirst+start,width*3)){finish();return false;}start=end;
            }
            c->OMSetDepthStencilState(transparent,0);c->OMSetBlendState(blend,nullptr,0xffffffffu);
            if(!draw(r.blockTransparent,r.blockFirst+r.blockOpaque,3)){finish();return false;}
        }
        finish();renderedKey_=plan.projectedKey;residentShadowRevision_=shadowRevision;residentShadowKey_=shadowKey;residentLightKey_=lightKey;std::copy(clear,clear+4,renderedClear_.begin());return true;
    }
    std::array<float,4> residentLightKey_{};

    ComPtr<ID3D11Texture2D> color_,depth_;
    ComPtr<ID3D11ShaderResourceView> srv_;ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11RenderTargetView> rtv_[4];
    int w_=0,h_=0;uint32_t layers_=0;uint64_t renderedKey_=0;std::array<float,4> renderedClear_{};
    bool shaders(ID3D11Device* d,std::wstring& error){
        if(vs_&&ps_&&layout_)return true;
        std::string source=std::string(dx11::shaders::raster::kShader)+kProjectedPass;
        ComPtr<ID3DBlob> v,p,e;
        auto compile=[&](const char* entry,const char* profile,ComPtr<ID3DBlob>& out){
            e.Reset();if(SUCCEEDED(D3DCompile(source.data(),source.size(),"EscargotProjected006",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,out.ReleaseAndGetAddressOf(),e.ReleaseAndGetAddressOf())))return true;
            error=L"Object006 projection shader compilation";
            if(e){const char* a=static_cast<const char*>(e->GetBufferPointer());while(*a)error+=wchar_t(static_cast<unsigned char>(*a++));}return false;};
        if(!compile("VSProjected","vs_4_0",v)||!compile("PSProjected","ps_4_0",p))return false;
        D3D11_INPUT_ELEMENT_DESC elems[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,vs_.ReleaseAndGetAddressOf()))||
           FAILED(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,ps_.ReleaseAndGetAddressOf()))||
           FAILED(d->CreateInputLayout(elems,3,v->GetBufferPointer(),v->GetBufferSize(),layout_.ReleaseAndGetAddressOf()))){error=L"Object006 projection pipeline creation";return false;}return true;
    }
    bool targets(ID3D11Device* d,int w,int h,uint32_t layers,std::wstring& error){
        if(srv_&&dsv_&&w_==w&&h_==h&&layers_==layers)return true;
        renderedKey_=0;color_.Reset();depth_.Reset();srv_.Reset();dsv_.Reset();for(auto& r:rtv_)r.Reset();w_=h_=0;layers_=0;
        D3D11_TEXTURE2D_DESC t{};t.Width=w;t.Height=h;t.MipLevels=1;t.ArraySize=layers;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(d->CreateTexture2D(&t,nullptr,color_.ReleaseAndGetAddressOf()))){error=L"Object006 color allocation";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=t.Format;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;sv.Texture2DArray.MipLevels=1;sv.Texture2DArray.ArraySize=layers;
        if(FAILED(d->CreateShaderResourceView(color_.Get(),&sv,srv_.ReleaseAndGetAddressOf()))){error=L"Object006 color view";return false;}
        for(UINT i=0;i<layers;++i){D3D11_RENDER_TARGET_VIEW_DESC rv{};rv.Format=t.Format;rv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;rv.Texture2DArray.ArraySize=1;rv.Texture2DArray.FirstArraySlice=i;
            if(FAILED(d->CreateRenderTargetView(color_.Get(),&rv,rtv_[i].ReleaseAndGetAddressOf()))){error=L"Object006 layer view";return false;}}
        t.ArraySize=1;t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(d->CreateTexture2D(&t,nullptr,depth_.ReleaseAndGetAddressOf()))||FAILED(d->CreateDepthStencilView(depth_.Get(),nullptr,dsv_.ReleaseAndGetAddressOf()))){error=L"Object006 depth allocation";return false;}
        w_=w;h_=h;layers_=layers;return true;
    }
public:
    ID3D11ShaderResourceView* view()const{return srv_.Get();}
    void invalidate(){renderedKey_=0;sourceView_.Reset();sourceBuffer_.Reset();uploadedBlocks_.clear();for(auto& layer:resident_)layer.data.clear();}
    bool render(ID3D11Device* d,ID3D11DeviceContext* c,const Scene& scene,const Camera& cam,const ShadowMap& sh,uint64_t shadowRevision,ID3D11ShaderResourceView* shadow,const EscargotPacketBuild& plan,int w,int h,const float clear[4],
        ID3D11Buffer* constants,ID3D11ShaderResourceView* albedo,ID3D11ShaderResourceView* preserved,
        ID3D11SamplerState* wrap,ID3D11SamplerState* clamp,ID3D11RasterizerState* raster,
        ID3D11DepthStencilState* opaque,ID3D11DepthStencilState* transparent,ID3D11BlendState* blend,EscargotProfile& profile,std::wstring& error){
        using Clock=std::chrono::steady_clock;auto started=Clock::now();
        if(device_!=d){*this=ProjectedOptics{};device_=d;}
        if(plan.resident.enabled)return renderResident(d,c,scene,cam,sh,shadowRevision,plan,w,h,clear,constants,albedo,preserved,shadow,wrap,clamp,raster,opaque,transparent,blend,profile,error);
        const auto count=plan.projectedVertices.size()/10;
        if(w<=0||h<=0||plan.projectedRanges.size()>4||plan.projectedVertices.size()%10||count>360000){error=L"Object006 projection budget exceeded";return false;}
        if(plan.data.size()<8||plan.data.size()%4){error=L"Object006 invalid receiver map";return false;}
        for(const auto& r:plan.projectedRanges){
            if(!r.receiver||uint64_t(r.receiver)+1>=plan.data.size()/4||uint64_t(r.first)+r.opaque+r.transparent>count||
               r.first%3||r.opaque%3||r.transparent%3||r.x<0||r.y<0||r.w<=0||r.h<=0||
               int64_t(r.x)+r.w>w||int64_t(r.y)+r.h>h){error=L"Object006 invalid draw range";return false;}
        }
        ID3D11ShaderResourceView* none=nullptr;c->PSSetShaderResources(8,1,&none);
        if(plan.projectedRanges.empty())return true;
        if(!shaders(d,error)||!targets(d,w,h,uint32_t(plan.projectedRanges.size()),error))return false;
        profile.allocationMs=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
        profile.layerPixels=uint64_t(w)*h*plan.projectedRanges.size();
        for(const auto& r:plan.projectedRanges)profile.scissorPixels+=uint64_t(r.w)*r.h;
        if(plan.projectedKey&&renderedKey_==plan.projectedKey&&std::equal(renderedClear_.begin(),renderedClear_.end(),clear)){profile.cacheHit=true;return true;}
        started=Clock::now();
        renderedKey_=0;
        for(size_t i=0;i<plan.projectedRanges.size();++i){
            const auto& r=plan.projectedRanges[i];auto& layer=resident_[i];const UINT n=r.opaque+r.transparent;
            if(!n)continue;
            const float* src=plan.projectedVertices.data()+size_t(r.first)*10;const size_t floats=size_t(n)*10,bytes=floats*sizeof(float);
            if(layer.vertices&&layer.data.size()==floats&&std::memcmp(layer.data.data(),src,bytes)==0)continue;
            if(!layer.vertices||n>layer.capacity){
                layer.capacity=1;while(layer.capacity<n)layer.capacity*=2;
                D3D11_BUFFER_DESC b{};b.ByteWidth=layer.capacity*40;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
                layer.data.clear();
                if(FAILED(d->CreateBuffer(&b,nullptr,layer.vertices.ReleaseAndGetAddressOf()))){layer.capacity=0;error=L"Object006 vertices allocation";return false;}
            }
            D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(c->Map(layer.vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){layer.data.clear();error=L"Object006 vertices upload";return false;}
            std::memcpy(mapped.pData,src,bytes);c->Unmap(layer.vertices.Get(),0);
            layer.data.assign(src,src+floats);profile.uploadBytes+=bytes;
        }
        profile.uploadMs=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
        D3D11_VIEWPORT vp{};vp.Width=float(w);vp.Height=float(h);vp.MaxDepth=1;c->RSSetViewports(1,&vp);
        c->RSSetState(raster);c->IASetInputLayout(layout_.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->VSSetShader(vs_.Get(),nullptr,0);c->VSSetConstantBuffers(0,1,&constants);c->PSSetShader(ps_.Get(),nullptr,0);c->PSSetConstantBuffers(0,1,&constants);
        c->PSSetShaderResources(0,1,&albedo);c->PSSetShaderResources(6,1,&preserved);
        ID3D11SamplerState* samplers[]={wrap,clamp};c->PSSetSamplers(0,2,samplers);
        for(UINT i=0;i<plan.projectedRanges.size();++i){const auto& r=plan.projectedRanges[i];
            ID3D11RenderTargetView* target=rtv_[i].Get();c->OMSetRenderTargets(1,&target,dsv_.Get());c->ClearRenderTargetView(target,clear);c->ClearDepthStencilView(dsv_.Get(),D3D11_CLEAR_DEPTH,1,0);
            D3D11_RECT rect{r.x,r.y,r.x+r.w,r.y+r.h};c->RSSetScissorRects(1,&rect);
            ID3D11Buffer* vb=resident_[i].vertices.Get();UINT stride=40,offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);
            c->OMSetDepthStencilState(opaque,0);c->OMSetBlendState(nullptr,nullptr,0xffffffffu);if(r.opaque){c->Draw(r.opaque,0);++profile.draws;}
            c->OMSetDepthStencilState(transparent,0);c->OMSetBlendState(blend,nullptr,0xffffffffu);if(r.transparent){c->Draw(r.transparent,r.opaque);++profile.draws;}
        }
        c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,0xffffffffu);
        renderedKey_=plan.projectedKey;std::copy(clear,clear+4,renderedClear_.begin());return true;
    }
};
}
