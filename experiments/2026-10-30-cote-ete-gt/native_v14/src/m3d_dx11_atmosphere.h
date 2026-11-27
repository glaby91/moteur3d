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
    Ptr<ID3D11PixelShader> composite_;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11BlendState> blend_;
    unsigned samples_=0;
    bool prepare(ID3D11Device* d,unsigned samples,std::wstring& error){
        if(!vs_||samples_!=samples){
            Ptr<ID3DBlob> v,c,log;
            const D3D_SHADER_MACRO macros[]={{"ZATM_MSAA","1"},{nullptr,nullptr}};
            auto compile=[&](const char* entry,const char* profile,Ptr<ID3DBlob>& out){
                log.Reset();
                return SUCCEEDED(D3DCompile(shaders::atmosphere::code,sizeof(shaders::atmosphere::code)-1,
                    "ZAtmosphere",samples>1?macros:nullptr,nullptr,entry,profile,D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&out,&log));
            };
            if(!compile("VS","vs_5_0",v)||!compile("CompositePS","ps_5_0",c)){
                error=L"Z Atmosphere: compilation HLSL impossible.";
                if(log){auto msg=static_cast<const char*>(log->GetBufferPointer());for(size_t i=0;i<log->GetBufferSize();++i)error+=wchar_t(msg[i]);}
                return false;
            }
            vs_.Reset();composite_.Reset();
            if(FAILED(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_))||
               FAILED(d->CreatePixelShader(c->GetBufferPointer(),c->GetBufferSize(),nullptr,&composite_))){
                error=L"Z Atmosphere: création des shaders impossible.";return false;}
            samples_=samples;
        }
        if(!constants_){
            D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Constants);b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(d->CreateBuffer(&b,nullptr,&constants_))){error=L"Z Atmosphere: constantes indisponibles.";return false;}
        }
        if(!blend_){
            D3D11_BLEND_DESC b{};auto& a=b.RenderTarget[0];a.BlendEnable=TRUE;a.SrcBlend=D3D11_BLEND_ONE;a.DestBlend=D3D11_BLEND_ONE;
            a.BlendOp=D3D11_BLEND_OP_ADD;a.SrcBlendAlpha=D3D11_BLEND_ZERO;a.DestBlendAlpha=D3D11_BLEND_ONE;a.BlendOpAlpha=D3D11_BLEND_OP_ADD;
            a.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
            if(FAILED(d->CreateBlendState(&b,&blend_))){error=L"Z Atmosphere: blend indisponible.";return false;}
        }
        return true;
    }
public:
    bool draw(ID3D11Device* d,ID3D11DeviceContext* ctx,ID3D11ShaderResourceView* depth,ID3D11ShaderResourceView* shadow,
              ID3D11RenderTargetView* output,ID3D11RasterizerState* raster,const Scene& scene,const Camera& cam,
              const ShadowMap& sm,unsigned samples,unsigned w,unsigned h,float density,std::wstring& error){
        if(!prepare(d,samples,error))return false;
        Constants cb{};
        auto put=[](float* a,V v,float q){a[0]=v.x;a[1]=v.y;a[2]=v.z;a[3]=q;};
        put(cb.eye,cam.eye,0);put(cb.forward,cam.f,cam.aspect);put(cb.right,cam.r,cam.tanHalf);put(cb.up,cam.u,0);
        put(cb.sun,sun,0);put(cb.shadowBase,sm.base,sm.span);put(cb.shadowRight,sm.right,sm.dist);put(cb.shadowUp,sm.up,0);
        cb.dimensions[0]=float(w);cb.dimensions[1]=float(h);cb.dimensions[2]=0;cb.dimensions[3]=0;
        cb.settings[0]=density;cb.settings[1]=daylight;cb.settings[2]=atmosphere::maxDistance;cb.settings[3]=float(sm.size);
        cb.lights=atmosphere::selectLights(scene,cam,daylight,lampFactor);
        D3D11_MAPPED_SUBRESOURCE mp{};
        if(FAILED(ctx->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mp))){error=L"Z Atmosphere: upload impossible.";return false;}
        std::memcpy(mp.pData,&cb,sizeof(cb));ctx->Unmap(constants_.Get(),0);
        ctx->OMSetRenderTargets(0,nullptr,nullptr);
        ctx->RSSetState(raster);ctx->OMSetDepthStencilState(nullptr,0);ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs_.Get(),nullptr,0);ctx->PSSetConstantBuffers(2,1,constants_.GetAddressOf());
        ID3D11ShaderResourceView* srvs[]={depth,shadow};ctx->PSSetShaderResources(0,2,srvs);
        D3D11_VIEWPORT viewport{};viewport.Width=float(w);viewport.Height=float(h);viewport.MaxDepth=1;
        ctx->RSSetViewports(1,&viewport);ctx->OMSetRenderTargets(1,&output,nullptr);
        ctx->OMSetBlendState(blend_.Get(),nullptr,0xffffffffu);
        ctx->PSSetShader(composite_.Get(),nullptr,0);ctx->Draw(3,0);
        ctx->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* none[2]{};ctx->PSSetShaderResources(0,2,none);
        ctx->OMSetBlendState(nullptr,nullptr,0xffffffffu);return true;
    }
};
}
