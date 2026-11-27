#pragma once
#include "m3d_dx11_atmosphere_shader.h"
#include "../../../../v17/atmosphere/atmosphere.h"
#include <wrl/client.h>
namespace m3d::dx11 {
class AtmospherePass {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    struct alignas(16) Constants {
        float eye[4]{},forward[4]{},right[4]{},up[4]{},sun[4]{},shadowBase[4]{},shadowRight[4]{},shadowUp[4]{};
        float dimensions[4]{},settings[4]{};
        atmosphere::Lights lights{};
    };
    Ptr<ID3D11VertexShader> vs_;
    Ptr<ID3D11PixelShader> integrate_,composite_;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11Texture2D> low_;
    Ptr<ID3D11RenderTargetView> lowRtv_;
    Ptr<ID3D11ShaderResourceView> lowSrv_;
    Ptr<ID3D11BlendState> blend_;
    unsigned samples_=0,w_=0,h_=0;
    bool prepare(ID3D11Device* d,unsigned samples,unsigned w,unsigned h,std::wstring& error){
        if(!vs_||samples_!=samples){
            Ptr<ID3DBlob> v,p,c,log;
            const D3D_SHADER_MACRO macros[]={{"ZATM_MSAA","1"},{nullptr,nullptr}};
            auto compile=[&](const char* entry,const char* profile,Ptr<ID3DBlob>& out){
                log.Reset();
                return SUCCEEDED(D3DCompile(shaders::atmosphere::code,sizeof(shaders::atmosphere::code)-1,
                    "ZAtmosphere",samples>1?macros:nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&out,&log));
            };
            if(!compile("VS","vs_5_0",v)||!compile("IntegratePS","ps_5_0",p)||!compile("CompositePS","ps_5_0",c)){
                error=L"Z Atmosphere: compilation HLSL impossible.";
                if(log){auto msg=static_cast<const char*>(log->GetBufferPointer());for(size_t i=0;i<log->GetBufferSize();++i)error+=wchar_t(msg[i]);}
                return false;
            }
            vs_.Reset();integrate_.Reset();composite_.Reset();
            if(FAILED(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_))||
               FAILED(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&integrate_))||
               FAILED(d->CreatePixelShader(c->GetBufferPointer(),c->GetBufferSize(),nullptr,&composite_))){
                error=L"Z Atmosphere: création des shaders impossible.";return false;}
            samples_=samples;
        }
        if(!constants_){
            D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Constants);b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(d->CreateBuffer(&b,nullptr,&constants_))){error=L"Z Atmosphere: constantes indisponibles.";return false;}
        }
        if(!blend_){
            D3D11_BLEND_DESC b{};auto& a=b.RenderTarget[0];a.BlendEnable=TRUE;a.SrcBlend=D3D11_BLEND_ONE;a.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
            a.BlendOp=D3D11_BLEND_OP_ADD;a.SrcBlendAlpha=D3D11_BLEND_ZERO;a.DestBlendAlpha=D3D11_BLEND_ONE;a.BlendOpAlpha=D3D11_BLEND_OP_ADD;
            a.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
            if(FAILED(d->CreateBlendState(&b,&blend_))){error=L"Z Atmosphere: blend indisponible.";return false;}
        }
        if(!low_||w_!=w||h_!=h){
            low_.Reset();lowRtv_.Reset();lowSrv_.Reset();
            D3D11_TEXTURE2D_DESC td{};td.Width=(w+3)/4;td.Height=(h+3)/4;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
            td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
            if(FAILED(d->CreateTexture2D(&td,nullptr,&low_))||FAILED(d->CreateRenderTargetView(low_.Get(),nullptr,&lowRtv_))||
               FAILED(d->CreateShaderResourceView(low_.Get(),nullptr,&lowSrv_))){error=L"Z Atmosphere: cible basse résolution indisponible.";return false;}
            w_=w;h_=h;
        }
        return true;
    }
public:
    bool draw(ID3D11Device* d,ID3D11DeviceContext* ctx,ID3D11ShaderResourceView* depth,ID3D11ShaderResourceView* shadow,
              ID3D11RenderTargetView* output,ID3D11RasterizerState* raster,const Scene& scene,const Camera& cam,
              const ShadowMap& sm,unsigned samples,unsigned w,unsigned h,float density,std::wstring& error){
        if(!prepare(d,samples,w,h,error))return false;
        Constants cb{};
        auto put=[](float* a,V v,float q){a[0]=v.x;a[1]=v.y;a[2]=v.z;a[3]=q;};
        put(cb.eye,cam.eye,0);put(cb.forward,cam.f,cam.aspect);put(cb.right,cam.r,cam.tanHalf);put(cb.up,cam.u,0);
        put(cb.sun,sun,0);put(cb.shadowBase,sm.base,sm.span);put(cb.shadowRight,sm.right,sm.dist);put(cb.shadowUp,sm.up,0);
        cb.dimensions[0]=float(w);cb.dimensions[1]=float(h);cb.dimensions[2]=float((w+3)/4);cb.dimensions[3]=float((h+3)/4);
        cb.settings[0]=density;cb.settings[1]=daylight;cb.settings[2]=atmosphere::maxDistance;cb.settings[3]=float(sm.size);
        cb.lights=atmosphere::selectLights(scene,cam,daylight,lampFactor);
        D3D11_MAPPED_SUBRESOURCE mp{};
        if(FAILED(ctx->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mp))){error=L"Z Atmosphere: upload impossible.";return false;}
        std::memcpy(mp.pData,&cb,sizeof(cb));ctx->Unmap(constants_.Get(),0);
        ctx->OMSetRenderTargets(0,nullptr,nullptr);
        ctx->RSSetState(raster);ctx->OMSetDepthStencilState(nullptr,0);ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs_.Get(),nullptr,0);ctx->PSSetConstantBuffers(2,1,constants_.GetAddressOf());
        ID3D11ShaderResourceView* srvs[]={depth,shadow,nullptr};ctx->PSSetShaderResources(0,3,srvs);
        auto viewport=[&](float x,float y){D3D11_VIEWPORT v{};v.Width=x;v.Height=y;v.MaxDepth=1;ctx->RSSetViewports(1,&v);};
        viewport(cb.dimensions[2],cb.dimensions[3]);ctx->OMSetRenderTargets(1,lowRtv_.GetAddressOf(),nullptr);
        ctx->OMSetBlendState(nullptr,nullptr,0xffffffffu);ctx->PSSetShader(integrate_.Get(),nullptr,0);ctx->Draw(3,0);
        ctx->OMSetRenderTargets(0,nullptr,nullptr);viewport(float(w),float(h));ctx->OMSetRenderTargets(1,&output,nullptr);
        srvs[2]=lowSrv_.Get();ctx->PSSetShaderResources(0,3,srvs);ctx->OMSetBlendState(blend_.Get(),nullptr,0xffffffffu);
        ctx->PSSetShader(composite_.Get(),nullptr,0);ctx->Draw(3,0);
        ctx->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* none[3]{};ctx->PSSetShaderResources(0,3,none);
        ctx->OMSetBlendState(nullptr,nullptr,0xffffffffu);return true;
    }
};
}
