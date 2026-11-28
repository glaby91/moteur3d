#pragma once
#include "m3d_directx11_raster_shader.h"
namespace m3d::escargot_gpu {
inline constexpr char kProjectedPass[]=R"(
struct ProjIn {float4 p:POSITION;float4 data:TEXCOORD0;float2 extra:TEXCOORD1;};
struct ProjOut {float4 p:SV_POSITION;float2 uv:TEXCOORD0;float illum:TEXCOORD1;nointerpolation uint mat:MATERIAL;nointerpolation float alpha:TEXCOORD2;float clip:SV_ClipDistance0;};
ProjOut VSProjected(ProjIn i){ProjOut o;o.p=i.p;o.uv=i.data.xy;o.illum=i.data.z;o.mat=uint(i.data.w);o.alpha=i.extra.x;o.clip=i.extra.y;return o;}
float4 PSProjected(ProjOut i):SV_TARGET{
    float layer=materialTextureDetail[i.mat].x;
    float3 base=layer>=0?preservedAlbedo.Sample(probeSampler,float3(saturate(i.uv),layer)).rgb:albedo.Sample(wrapSampler,float3(i.uv,i.mat)).rgb;
    float3 color=base*max(i.illum,0)+materialEmission[i.mat].rgb*max(materialEmission[i.mat].a,0);
    return float4(color,saturate(i.alpha));
}
)";
class ProjectedOptics {
    ID3D11Device* device_=nullptr;
    ComPtr<ID3D11VertexShader> vs_;ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11InputLayout> layout_;ComPtr<ID3D11Buffer> vertices_;
    ComPtr<ID3D11Texture2D> color_,depth_;
    ComPtr<ID3D11ShaderResourceView> srv_;ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11RenderTargetView> rtv_[4];
    UINT capacity_=0;int w_=0,h_=0;uint32_t layers_=0;
    bool shaders(ID3D11Device* d,std::wstring& error){
        if(vs_&&ps_&&layout_)return true;
        std::string source=std::string(dx11::shaders::raster::kShader)+kProjectedPass;
        ComPtr<ID3DBlob> v,p,e;
        auto compile=[&](const char* entry,const char* profile,ComPtr<ID3DBlob>& out){
            e.Reset();if(SUCCEEDED(D3DCompile(source.data(),source.size(),"EscargotProjected005",nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,out.ReleaseAndGetAddressOf(),e.ReleaseAndGetAddressOf())))return true;
            error=L"Object005 projection shader compilation";
            if(e){const char* a=static_cast<const char*>(e->GetBufferPointer());while(*a)error+=wchar_t(static_cast<unsigned char>(*a++));}return false;};
        if(!compile("VSProjected","vs_4_0",v)||!compile("PSProjected","ps_4_0",p))return false;
        D3D11_INPUT_ELEMENT_DESC elems[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,vs_.ReleaseAndGetAddressOf()))||
           FAILED(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,ps_.ReleaseAndGetAddressOf()))||
           FAILED(d->CreateInputLayout(elems,3,v->GetBufferPointer(),v->GetBufferSize(),layout_.ReleaseAndGetAddressOf()))){error=L"Object005 projection pipeline creation";return false;}return true;
    }
    bool targets(ID3D11Device* d,int w,int h,uint32_t layers,std::wstring& error){
        if(srv_&&dsv_&&w_==w&&h_==h&&layers_==layers)return true;
        color_.Reset();depth_.Reset();srv_.Reset();dsv_.Reset();for(auto& r:rtv_)r.Reset();w_=h_=0;layers_=0;
        D3D11_TEXTURE2D_DESC t{};t.Width=w;t.Height=h;t.MipLevels=1;t.ArraySize=layers;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(d->CreateTexture2D(&t,nullptr,color_.ReleaseAndGetAddressOf()))){error=L"Object005 color allocation";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=t.Format;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;sv.Texture2DArray.MipLevels=1;sv.Texture2DArray.ArraySize=layers;
        if(FAILED(d->CreateShaderResourceView(color_.Get(),&sv,srv_.ReleaseAndGetAddressOf()))){error=L"Object005 color view";return false;}
        for(UINT i=0;i<layers;++i){D3D11_RENDER_TARGET_VIEW_DESC rv{};rv.Format=t.Format;rv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;rv.Texture2DArray.ArraySize=1;rv.Texture2DArray.FirstArraySlice=i;
            if(FAILED(d->CreateRenderTargetView(color_.Get(),&rv,rtv_[i].ReleaseAndGetAddressOf()))){error=L"Object005 layer view";return false;}}
        t.ArraySize=1;t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(d->CreateTexture2D(&t,nullptr,depth_.ReleaseAndGetAddressOf()))||FAILED(d->CreateDepthStencilView(depth_.Get(),nullptr,dsv_.ReleaseAndGetAddressOf()))){error=L"Object005 depth allocation";return false;}
        w_=w;h_=h;layers_=layers;return true;
    }
public:
    ID3D11ShaderResourceView* view()const{return srv_.Get();}
    bool render(ID3D11Device* d,ID3D11DeviceContext* c,const EscargotPacketBuild& plan,int w,int h,const float clear[4],
        ID3D11Buffer* constants,ID3D11ShaderResourceView* albedo,ID3D11ShaderResourceView* preserved,
        ID3D11SamplerState* wrap,ID3D11SamplerState* clamp,ID3D11RasterizerState* raster,
        ID3D11DepthStencilState* opaque,ID3D11DepthStencilState* transparent,ID3D11BlendState* blend,std::wstring& error){
        if(device_!=d){*this=ProjectedOptics{};device_=d;}
        const auto count=plan.projectedVertices.size()/10;
        if(plan.projectedRanges.size()>4||plan.projectedVertices.size()%10||count>360000){error=L"Object005 projection budget exceeded";return false;}
        ID3D11ShaderResourceView* none=nullptr;c->PSSetShaderResources(8,1,&none);
        if(plan.projectedRanges.empty())return true;
        if(!shaders(d,error)||!targets(d,w,h,uint32_t(plan.projectedRanges.size()),error))return false;
        if(count){
            if(!vertices_||count>capacity_){capacity_=1;while(capacity_<count)capacity_*=2;
                D3D11_BUFFER_DESC b{};b.ByteWidth=capacity_*40;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
                if(FAILED(d->CreateBuffer(&b,nullptr,vertices_.ReleaseAndGetAddressOf()))){capacity_=0;error=L"Object005 vertices allocation";return false;}}
            D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(c->Map(vertices_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m))){error=L"Object005 vertices upload";return false;}
            std::memcpy(m.pData,plan.projectedVertices.data(),plan.projectedVertices.size()*sizeof(float));c->Unmap(vertices_.Get(),0);
        }
        D3D11_VIEWPORT vp{};vp.Width=float(w);vp.Height=float(h);vp.MaxDepth=1;c->RSSetViewports(1,&vp);
        c->RSSetState(raster);c->IASetInputLayout(layout_.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ID3D11Buffer* vb=vertices_.Get();UINT stride=40,offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        c->VSSetShader(vs_.Get(),nullptr,0);c->PSSetShader(ps_.Get(),nullptr,0);c->PSSetConstantBuffers(0,1,&constants);
        c->PSSetShaderResources(0,1,&albedo);c->PSSetShaderResources(6,1,&preserved);
        ID3D11SamplerState* samplers[]={wrap,clamp};c->PSSetSamplers(0,2,samplers);
        for(UINT i=0;i<plan.projectedRanges.size();++i){const auto& r=plan.projectedRanges[i];
            if(uint64_t(r.first)+r.opaque+r.transparent>count||r.opaque%3||r.transparent%3){error=L"Object005 invalid draw range";return false;}
            ID3D11RenderTargetView* target=rtv_[i].Get();c->OMSetRenderTargets(1,&target,dsv_.Get());c->ClearRenderTargetView(target,clear);c->ClearDepthStencilView(dsv_.Get(),D3D11_CLEAR_DEPTH,1,0);
            D3D11_RECT rect{r.x,r.y,r.x+r.w,r.y+r.h};c->RSSetScissorRects(1,&rect);
            c->OMSetDepthStencilState(opaque,0);c->OMSetBlendState(nullptr,nullptr,0xffffffffu);if(r.opaque)c->Draw(r.opaque,r.first);
            c->OMSetDepthStencilState(transparent,0);c->OMSetBlendState(blend,nullptr,0xffffffffu);if(r.transparent)c->Draw(r.transparent,r.first+r.opaque);
        }
        c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,0xffffffffu);return true;
    }
};
}
