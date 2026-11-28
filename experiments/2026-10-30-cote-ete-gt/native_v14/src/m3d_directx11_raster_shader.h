#pragma once

// Versioned DX11 shader feature module.
namespace m3d::dx11::shaders::raster {

inline constexpr char kShader[] = R"(
cbuffer Frame : register(b0) {
    float3 eye; float tanHalf;
    float3 forward; float aspect;
    float3 right; float nearPlane;
    float3 up; float farPlane;
    float3 lightDir; float ambient;
    float3 skyColor; uint materialCount;
    float3 shadowBase; float shadowSpan;
    float3 shadowRight; float shadowDist;
    float3 shadowUp; float shadowSize;
    float4 materialGlow[256];
    float4 materialSpecular[256];
    float4 materialOpacity[256];
    float4 materialDiffuse[256];
    float4 materialEmission[256];
    float4 materialTextureDetail[256];
    float4 lightPositionRadius[256];
    float4 lightColorIntensity[256];
    uint lightCount; float3 padding;
    float4 indirectSky;
    float4 indirectBounce;
    float4 indirectSun;
    float4 probeOrigin;
    float4 probeInvSpan;
    float4 hybridControls;
    float4 colorControls;
    float4 peripheralControls;
    float4 reflectionControls;
    uint4 adaptiveZero[128];
    float4 zClip;
    float4 zPlaneIds;
    float4 zEye[2],zForward[2],zRight[2],zUp[2];
};
Texture2DArray<float4> albedo : register(t0);
Texture2D<float> shadowDepth : register(t1);
Texture2DArray<float> roughnessMap : register(t2);
Texture2DArray<float4> normalMap : register(t3);
Texture3D<float4> indirectProbes : register(t4);
Texture3D<float4> indirectProbesNext : register(t5);
Texture2DArray<float4> preservedAlbedo : register(t6);
StructuredBuffer<float4> lumaTriangleRadiance : register(t7);
Texture2D<float4> zReflection0 : register(t9);
Texture2D<float4> zReflection1 : register(t10);
Texture2D<float4> escColor : register(t11);
Texture2D<float4> escMeta : register(t12);
StructuredBuffer<float4> escPacket : register(t13);
SamplerState wrapSampler : register(s0);
SamplerState probeSampler : register(s1);
struct VSIn { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint mat:MATERIAL; uint primitiveId:TRIID; uint receiverId:RECEIVERID; };
struct VSOut { float4 p:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; float3 world:TEXCOORD1; nointerpolation uint mat:MATERIAL; nointerpolation uint primitiveId:TEXCOORD2; nointerpolation uint opticalId:TEXCOORD3; nointerpolation uint receiverId:TEXCOORD4; };
VSOut VSMain(VSIn i) {
    VSOut o;
    float3 q = i.p - eye;
    float z = dot(q, forward);
    float safeZ = max(z, nearPlane);
    float nx = dot(q, right) / (safeZ * tanHalf);
    float ny = dot(q, up) * aspect / (safeZ * tanHalf);
    float depth = (farPlane / (farPlane - nearPlane)) * z - (farPlane * nearPlane) / (farPlane - nearPlane);
    // Keep the real camera-space z in w and clip space. Using safeZ here would
    // pull geometry behind the near plane onto the near plane instead of clipping it.
    o.p = float4(nx * safeZ, ny * safeZ, depth, z);
    o.n = i.n; o.uv = i.uv; o.world=i.p; o.mat = min(i.mat, materialCount - 1);o.primitiveId=i.primitiveId;
    o.opticalId=0u;o.receiverId=i.receiverId;
    if(i.primitiveId!=0xffffffffu && (i.primitiveId&0x80000000u)!=0u){o.opticalId=(i.primitiveId&0x7fffffffu)+1u;o.primitiveId=0xffffffffu;}
    // The pixel shader already skips local radiance for the sentinel.
    // Preserve all other LumaTrace lighting controls and all draw calls.
    [branch] if(padding.x>.5 && o.primitiveId!=0xffffffffu){
        uint group=o.primitiveId>>8;uint word=group>>5;
        if((adaptiveZero[word>>2][word&3u]&(1u<<(group&31u)))!=0u)o.primitiveId=0xffffffffu;
    }
    return o;
}
struct VSBoxIn {
    float3 p:POSITION; float3 n:NORMAL; float2 unusedUv:TEXCOORD0;
    uint unusedMaterial:MATERIAL;uint unusedPrimitiveId:TRIID;
    float3 boxLo:BOXMIN; float uvScale:BOXSCALE;
    float3 boxHi:BOXMAX; uint boxMat:BOXMAT;
    float3 boxSize:BOXSIZE;uint uvMode:BOXUVMODE;
};
VSOut VSBoxMain(VSBoxIn i) {
    // Select the original box corners directly rather than recomputing a
    // matrix: world coordinates and texture mapping match the legacy mesh.
    float3 p=float3(i.p.x<0.5?i.boxLo.x:i.boxHi.x,
                    i.p.y<0.5?i.boxLo.y:i.boxHi.y,
                    i.p.z<0.5?i.boxLo.z:i.boxHi.z);
    float3 n=i.n;
    float2 uv;
    if(i.uvMode==1 || i.uvMode==2){
        // uvMode=1: normal scene PBR, repeat per metre.
        // uvMode=2: image artwork, cover the entire box face once.
        // The mirroring matches the original 12-triangle V16 source boxes.
        float2 localUV=i.unusedUv;
        if(n.z < -0.5 || n.x > 0.5)localUV.x=1.0-localUV.x;
        if(n.y > 0.5)localUV.y=1.0-localUV.y;
        float uLength=(abs(n.x)>0.5?i.boxSize.z:i.boxSize.x)*0.5;
        float vLength=(abs(n.y)>0.5?i.boxSize.z:i.boxSize.y)*0.5;
        uv=i.uvMode==2?localUV:localUV*float2(uLength,vLength);
    }else{
        uv=(abs(n.y)>=abs(n.x)&&abs(n.y)>=abs(n.z))
            ?p.xz*i.uvScale
            :(abs(n.x)>=abs(n.z)?p.zy*i.uvScale:p.xy*i.uvScale);
    }
    VSIn raw;
    raw.p=p;raw.n=n;raw.uv=uv;raw.mat=i.boxMat;raw.primitiveId=i.unusedPrimitiveId;raw.receiverId=0u;
    return VSMain(raw);
}
struct VSEllipseIn {
    float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0;
    uint unusedMaterial:MATERIAL;uint unusedPrimitiveId:TRIID;
    float3 center:ELLIPSECENTER;uint ellipseMat:ELLIPSEMAT;
    float3 radii:ELLIPSERADII;float pad:ELLIPSEPAD;
};
VSOut VSEllipseMain(VSEllipseIn i) {
    VSIn raw;
    raw.p=i.center+i.p*i.radii;
    raw.n=normalize(i.n/max(i.radii,float3(.00001,.00001,.00001)));
    raw.uv=i.uv;
    raw.mat=i.ellipseMat;raw.primitiveId=i.unusedPrimitiveId;raw.receiverId=0u;
    return VSMain(raw);
}
// Common optical slot and projection; material supplies only distortion/composition.
bool sampleOptical(VSOut i,float2 distortion,out float3 reflected){
    reflected=0;
    if(i.opticalId==0u||zPlaneIds.w>.5||zPlaneIds.z<.5)return false;
    int slot=-1;
    if(i.opticalId==(uint)zPlaneIds.x)slot=0;
    else if(zPlaneIds.z>1.5&&i.opticalId==(uint)zPlaneIds.y)slot=1;
    if(slot<0)return false;
    float3 q=i.world-zEye[slot].xyz;
    float z=dot(q,zForward[slot].xyz);
    if(z<=.08)return false;
    float2 ndc=float2(dot(q,zRight[slot].xyz),dot(q,zUp[slot].xyz)*zForward[slot].w)/(z*zEye[slot].w);
    float2 uv=float2(.5+.5*ndc.x,.5-.5*ndc.y)+distortion;
    if(any(uv<0.0)||any(uv>1.0))return false;
    reflected=slot==0?zReflection0.SampleLevel(probeSampler,uv,0).rgb:zReflection1.SampleLevel(probeSampler,uv,0).rgb;
    return true;
}
)";
inline constexpr char kEscargotGpuShader[] = R"(// Compact exact top-four search. Integer pixel positions and stable sample
// IDs preserve measured zeros and deterministic equidistant ties.
bool escLess(uint d,uint id,uint refD,uint refId){return d<refD||(d==refD&&id<refId);}
void escNearestScalar(uint start,uint count,int2 pixel,out uint4 addr,out uint4 dist,out uint n){
    addr=0;dist=0xffffffffu;uint4 ids=0xffffffffu;
    for(uint j=0;j<count;++j){
        uint a=start+3*j;float4 v=escPacket[a];int2 d=int2(v.xy)-pixel;
        uint dd=uint(d.x*d.x+d.y*d.y),id=uint(v.z);
        if(!escLess(dd,id,dist.w,ids.w))continue;
        if(escLess(dd,id,dist.x,ids.x)){dist=uint4(dd,dist.xyz);ids=uint4(id,ids.xyz);addr=uint4(a,addr.xyz);}
        else if(escLess(dd,id,dist.y,ids.y)){dist=uint4(dist.x,dd,dist.yz);ids=uint4(ids.x,id,ids.yz);addr=uint4(addr.x,a,addr.yz);}
        else if(escLess(dd,id,dist.z,ids.z)){dist=uint4(dist.xy,dd,dist.z);ids=uint4(ids.xy,id,ids.z);addr=uint4(addr.xy,a,addr.z);}
        else{dist.w=dd;ids.w=id;addr.w=a;}
    }
    n=min(count,4u);if(n>0&&dist.x==0)n=1;
}
void escNearest(uint start,uint count,int2 pixel,out uint4 addr,out uint4 dist,out uint n){
    if(escPacket[1].w>.5){escNearestScalar(start,count,pixel,addr,dist,n);return;}
    addr=0;dist=0xffffffffu;n=0;uint4 ids=0xffffffffu;
    [loop] for(uint j=0;j<count;++j){
        uint a=start+3*j;float4 v=escPacket[a];int2 d=int2(v.xy)-pixel;
        uint dd=uint(d.x*d.x+d.y*d.y),id=uint(v.z);
        [unroll] for(uint k=0;k<4;++k){
            if(dd<dist[k]||(dd==dist[k]&&id<ids[k])){
                [unroll] for(int m=3;m>0;--m){if(uint(m)>k){addr[m]=addr[m-1];dist[m]=dist[m-1];ids[m]=ids[m-1];}}
                addr[k]=a;dist[k]=dd;ids[k]=id;break;
            }
        }
    }
    n=min(count,4u);if(n>0&&dist.x==0)n=1;
}
float4 escCoverageWeights(uint4 d,uint n){
    float4 w=0;
    if(n==0u)return w;
    if(n==1u||d.x==0u)return float4(1,0,0,0);
    // Compact continuous-ish taper at stencil transitions. The floor
    // prevents the symmetric four-way tie from becoming singular.
    float farthest=float(d[n-1]);
    float radius=farthest+max(1.0,.125*farthest);
    [unroll]for(uint j=0;j<4;++j)if(j<n)w[j]=max(0.0,radius-float(d[j]))/(1.0+float(d[j]));
    return w/max(dot(w,float4(1,1,1,1)),1e-6);
}
// Shared reconstruction/shading preserves the original arithmetic and lane masks.
bool escLayer(VSOut i,int2 pixel,float3 ray,float4 td,bool common,uint4 ra,uint4 rd,uint rn,uint base,
    inout uint lanes,out float4 val,out float4 nrOut,out uint maskOut){
    val=0;nrOut=0;maskOut=0;
        float4 info=escPacket[base],bounds=escPacket[base+1];
        int2 q=pixel-int2(bounds.xy);if(any(q<0)||any(q>=int2(bounds.zw)))return false;
        uint4 a,d;uint n;
        if(common){
            n=rn;d=rd;lanes=rn;
            a=uint(info.x)+2*((ra-uint(td.x))/3u);
        }else escNearest(uint(info.x),uint(info.y),pixel,a,d,n);
        if(!n)return false;
        float4 total=0;float3 normalSum=0;float2 uvSum=0,uvMin=float2(1e20,1e20),uvMax=-uvMin,seed=0;
        uint count=0,uvCount=0,mask=0;float primitive=-1;
        [loop] for(uint j=0;j<n;++j){
            uint pos=a[j]+(common?0u:1u);
            float4 value=escPacket[pos],nr=escPacket[pos+1];
            if(nr.w<.5)continue; // primitive+1, zero = absent
            if(count==0){seed=value.xy;primitive=nr.w;}
            ++count;mask|=1u<<j;total.zw+=value.zw;normalSum+=nr.xyz;
            if(nr.w==primitive||all(abs(value.xy-seed)<.5)){uvSum+=value.xy;++uvCount;}
            uvMin=min(uvMin,value.xy);uvMax=max(uvMax,value.xy);
        }
        if(!count)return false;
        float2 uv=uvSum/max(1u,uvCount);float depth=total.z/count,alpha=saturate(total.w/count);
        // A coherent planar UV chart has a continuous geometric transform.
        // Four measured positives are required, no edge extrapolation beyond
        // their local UV envelope. Exact samples retain their original UV.
        if(info.w>.5&&n==4&&count==4&&uvCount==4){
            float4 origin=escPacket[base+2],plane=escPacket[base+3];
            float den=dot(ray,plane.xyz);
            if(abs(den)>.00001){
                float t=dot(origin.xyz-i.world,plane.xyz)/den;
                if(t>0){
                    float3 delta=i.world+ray*t-origin.xyz;
                    float2 projected=float2(origin.w+dot(escPacket[base+4].xyz,delta),plane.w+dot(escPacket[base+5].xyz,delta));
                    float2 margin=max((uvMax-uvMin)*.10,float2(.002,.002));
                    if(all(projected>=uvMin-margin)&&all(projected<=uvMax+margin))uv=projected;
                }
            }
        }
        if(!common){
            float4 cw=escCoverageWeights(d,n);float support=0;
            [unroll]for(uint j=0;j<4;++j)if(mask&(1u<<j))support+=cw[j];
            alpha*=support;mask=1u;
        }
        if(alpha<=0)return false;

    val=float4(uv,depth,alpha);
    nrOut=float4(normalSum*rsqrt(max(dot(normalSum,normalSum),1e-12)),info.z);
    maskOut=mask;return true;
}
float3 escLayerColor(VSOut i,float3 ray,bool glass,float4 lighting,uint base,float4 val,float4 nr){
        float4 emission=escPacket[base+6];
        uint mat=uint(nr.w);float2 uv=val.xy;
        if(emission.w>.5)uv=saturate(uv);
        float layer=materialTextureDetail[mat].x;
        float3 color;
        if(layer>=0)color=preservedAlbedo.SampleLevel(probeSampler,float3(uv,layer),0).rgb;
        else if(emission.w>.5)color=albedo.SampleLevel(probeSampler,float3(uv,mat),0).rgb;
        else color=albedo.SampleLevel(wrapSampler,float3(uv,mat),0).rgb;
        float3 sourceBase=color;
        color=sourceBase*(.12+.35*lighting.w+.48*lighting.w*saturate(dot(nr.xyz,lighting.xyz)))+emission.xyz;
        // Glass: use the existing LT diffuse bounce at the reconstructed
        // source position, not an arbitrary boost to Fresnel or opacity.
        // No extra light loop or optical ray. Depth is an approximation.
        if(glass&&hybridControls.x>.5&&hybridControls.w>1.5){
            float3 sourcePos=i.world+ray*val.z;
            float3 uvw=saturate((sourcePos-probeOrigin.xyz)*probeInvSpan.xyz);
            float3 bounce=max(indirectProbes.SampleLevel(probeSampler,uvw,0).rgb,0.0);
            color+=sourceBase*materialTextureDetail[mat].z*bounce;
        }

    return color;
}
bool escGpuSample(VSOut i,out float3 reflected,out float coverage,out float weight){
    reflected=0;coverage=0;weight=0;
    if(i.receiverId==0u||i.receiverId>uint(escPacket[1].x))return false;
    uint rec=2+2*i.receiverId;float4 rect=escPacket[rec],desc=escPacket[rec+1];
    int2 pixel=int2(i.p.xy),localPixel=pixel-int2(rect.xy);
    if(desc.z!=float(i.mat+1u)||any(localPixel<0)||any(localPixel>=int2(rect.zw)))return false;
    uint tileSize=uint(escPacket[1].y);if(tileSize==0u)return false;
    uint2 tile=uint2(localPixel)/tileSize;
    float4 td=escPacket[uint(desc.x)+tile.y*uint(desc.y)+tile.x];
    if(td.y<=0||td.w<=0)return false;
    uint4 ra,rd;uint rn;escNearest(uint(td.x),uint(td.y),pixel,ra,rd,rn);
    if(rn==1&&rd.x==0&&escPacket[ra.x].w<.5)return false;
    float3 geom=normalize(i.n),view=normalize(i.world-eye);
    if(dot(geom,view)>0)geom=-geom;
    float3 ray=reflect(view,geom);float wr=0;uint positive=0;bool receiverMatch=false;
    // Geometry is not reconstructed by averaging ray depths. Exact raster
    // receiver ID and a local tangent-plane check reject unrelated faces.
    [loop] for(uint j=0;j<rn;++j){
        if(escPacket[ra[j]].w<.5)continue;
        float4 pos=escPacket[ra[j]+1],normal=escPacket[ra[j]+2];
        wr+=pos.w;++positive;
        float3 delta=i.world-pos.xyz;
        if(abs(dot(geom,normal.xyz))>.80&&abs(dot(delta,normal.xyz))<max(.025,.025*length(delta)))receiverMatch=true;
    }
    if(!positive||!receiverMatch)return false;weight=saturate(wr/positive);
    // Each contributing source layer has its own top-four observations.
    // Sort locally from far to near; 64 is a packet validation limit per
    // tile, not a global optical surface cap.
    bool common=desc.w>.5;uint lanes=1;
    float4 laneWeights=common?escCoverageWeights(rd,rn):float4(1,0,0,0);
    const bool glass=materialOpacity[i.mat].x<.999&&materialEmission[i.mat].a>=-.5;
    if(uint(td.w)==1u){
        uint base=uint(td.z),mask;float4 val,nr;
        if(!escLayer(i,pixel,ray,td,common,ra,rd,rn,base,lanes,val,nr,mask))return false;
        float3 color=escLayerColor(i,ray,glass,escPacket[0],base,val,nr);
        // Same four-lane accumulation order as the general path.
        float4 v=float4(color*val.w,val.w);
        float4 a0=(lanes>0u&&(mask&1u))?v:0,a1=(lanes>1u&&(mask&2u))?v:0;
        float4 a2=(lanes>2u&&(mask&4u))?v:0,a3=(lanes>3u&&(mask&8u))?v:0;
        float4 result=a0*laneWeights.x+a1*laneWeights.y+a2*laneWeights.z+a3*laneWeights.w;
        coverage=saturate(result.a);if(coverage<=.00001)return false;reflected=max(result.rgb/coverage,0);return true;
    }
    float4 uvDepthAlpha[64];float4 normalMat[64];uint masks[64];uint layerRefs[64];
    uint used=0;
    [loop] for(uint l=0;l<uint(td.w);++l){
        uint base=uint(td.z)+7*l;float4 val,nr;uint mask;
        if(!escLayer(i,pixel,ray,td,common,ra,rd,rn,base,lanes,val,nr,mask))continue;
        float depth=val.z;
        if(used>=64u)return false; // defensive: CPU packet validated
        uint k=used;
        [loop] while(k>0&&uvDepthAlpha[k-1].z<depth){
            uvDepthAlpha[k]=uvDepthAlpha[k-1];normalMat[k]=normalMat[k-1];masks[k]=masks[k-1];layerRefs[k]=layerRefs[k-1];--k;
        }
        uvDepthAlpha[k]=val;
        normalMat[k]=nr;
        masks[k]=mask;layerRefs[k]=base;++used;
    }
    uint blocked=0;
    [loop] for(int k=int(used)-1;k>=0;--k){masks[k]&=~blocked;if(uvDepthAlpha[k].w>=1.0)blocked|=masks[k];}
    float4 acc[4];[unroll]for(uint j=0;j<4;++j)acc[j]=0;
    float4 lighting=escPacket[0];
    [loop] for(uint k=0;k<used;++k){
        if(!masks[k])continue;
        float4 val=uvDepthAlpha[k];float3 color=escLayerColor(i,ray,glass,lighting,layerRefs[k],val,normalMat[k]);
        [unroll]for(uint j=0;j<4;++j){if(j<lanes&&(masks[k]&(1u<<j)))acc[j]=float4(color*val.w,val.w)+acc[j]*(1-val.w);}
    }
    float4 result=acc[0]*laneWeights.x+acc[1]*laneWeights.y+acc[2]*laneWeights.z+acc[3]*laneWeights.w;
    coverage=saturate(result.a);if(coverage<=.00001)return false;reflected=max(result.rgb/coverage,0);return true;
}
)";
inline constexpr char kShaderAfterEscargot[] = R"(bool sampleEscargot(VSOut i,out float3 reflected,out float coverage,out float weight){
    reflected=0;coverage=0;weight=0;
    if(zPlaneIds.z>=0||zPlaneIds.w>.5)return false;
#ifdef CSE_ESCARGOT_RASTER_DIRECT
    if(zPlaneIds.z< -2.5)return escRasterOptical(i,reflected,coverage,weight);
#endif
    if(zPlaneIds.z< -1.5)return escGpuSample(i,reflected,coverage,weight);
    int2 p=int2(i.p.xy);float4 m=escMeta.Load(int3(p,0));
    if(i.receiverId==0u||m.w!=float(i.receiverId)||m.y!=float(i.mat+1u)||m.x<=0)return false;
    float d=length(i.world-eye);
    if(abs(m.x-d)>max(.03,.025*d))return false;
    float4 c=escColor.Load(int3(p,0));coverage=saturate(c.a);weight=saturate(m.z);
    if(coverage<=.00001)return false;
    reflected=max(c.rgb/coverage,0);return true;
}
// Same rejection for the reflected color and depth passes.
void clipReflection(VSOut i){
    if(zPlaneIds.w>.5 && dot(float4(i.world,1.0),zClip)<0.0) discard;
    if(zPlaneIds.w>.5 && reflectionControls.x>.5 &&
       (materialEmission[i.mat].a<-.5 || materialOpacity[i.mat].x<.45)) discard;
}
void PSReflectionDepth(VSOut i){clipReflection(i);}
float4 PSMain(VSOut i):SV_TARGET {
    clipReflection(i);
    // Smooth focus-lens / lighthouse geometry. The sharp center follows the
    // camera's optical axis, with no rectangular hard cut or temporal reuse.
    // Mode 0: original full-frame, 1: low-res surround, 2: full-res focus.
    if(peripheralControls.x>.5) {
        float2 uvScreen=i.p.xy*peripheralControls.yz;
        float2 focus=(uvScreen-.5)/float2(.47,.48);
        float radius=length(focus);
        if(peripheralControls.x<1.5 && radius<.82) discard;
        if(peripheralControls.x>1.5 && radius>1.0) discard;
    }
    // WRAP sampler already handles tiling. frac(uv) here broke automatic
    // derivatives at tile boundaries and forced inappropriate mip levels.
    float2 artworkDdx=ddx(i.uv),artworkDdy=ddy(i.uv);
    float3 base;
    float detailLayer=materialTextureDetail[i.mat].x;
    if(detailLayer>=0.0) {
        // Scene-authored preservation keeps paintings at their source tier,
        // independent of the shared array budget used by ordinary materials.
        base=preservedAlbedo.SampleGrad(wrapSampler,float3(i.uv,detailLayer),
                                       artworkDdx,artworkDdy).rgb;
    } else {
        base=albedo.Sample(wrapSampler,float3(i.uv,i.mat)).rgb;
    }
    float3 normal = normalize(i.n);
    const bool poolWater=materialEmission[i.mat].a < -0.5;
    // Dedicated animated-looking standing waves (static in world space) only on
    // the new translucent pool water. Avoid extra trig on every other material.
    if(poolWater && normal.y>0.75){
        float waveA=sin(i.world.x*2.1+i.world.z*.75);
        float waveB=cos(i.world.z*2.8-i.world.x*.45);
        normal=normalize(normal+float3(.028*waveA,0,.025*waveB));
        base=lerp(base,float3(.055,.38,.55),.62);
    }
#if !defined(M3D_V17_OPTIMIZE_PIXELS)
    float3 dpdx = ddx(i.world), dpdy = ddy(i.world);
    float2 duvdx = ddx(i.uv), duvdy = ddy(i.uv);
    float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
#endif
    if (materialOpacity[i.mat].y > 0.5) {
#if defined(M3D_V17_OPTIMIZE_PIXELS)
        // The nointerpolation material index is uniform per primitive.
        // Do not evaluate derivatives when normal mapping is disabled.
        float3 dpdx = ddx(i.world), dpdy = ddy(i.world);
        float2 duvdx = ddx(i.uv), duvdy = ddy(i.uv);
        float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
#endif
        float3 detailNormal = normalize(normalMap.Sample(wrapSampler, float3(i.uv, i.mat)).xyz * 2.0 - 1.0);
        if (abs(determinant) > 0.000001) {
            float3 tangent = (dpdx * duvdy.y - dpdy * duvdx.y) / determinant;
            float3 rawBitangent = (-dpdx * duvdy.x + dpdy * duvdx.x) / determinant;
            tangent = normalize(tangent - normal * dot(normal, tangent));
            float3 bitangent = normalize(cross(normal, tangent)) * (dot(cross(normal, tangent), rawBitangent) < 0.0 ? -1.0 : 1.0);
            // V17.600: a modest tangent-space gain restores visible material relief
            // without tessellation or changing silhouette geometry.
            normal = normalize(tangent * (detailNormal.x * 1.25) + bitangent * (detailNormal.y * 1.25) + normal * detailNormal.z);
        }
    }
    float diffuse = saturate(dot(normal, normalize(lightDir)));
    float shadowFactor = 1.0;
    // With zero solar energy, every use of shadowFactor is multiplied by
    // daylight. Keep the legacy branch for LumaTrace and R001 A/B.
    // Derivatives before R001's potentially divergent solar-energy branch.
    float3 receiverN = cross(ddx(i.world), ddy(i.world));
    [branch] if (!(reflectionControls.w > .5 && materialDiffuse[i.mat].w == 0.0)) {
    float3 shadowOffset = i.world - shadowBase;
    float shadowScale = shadowSize / (2.0 * shadowSpan);
    float shadowCenter = shadowSize * 0.5 - 0.5;
    float2 shadowCoord = float2(dot(shadowOffset, shadowRight), dot(shadowOffset, shadowUp)) * shadowScale + shadowCenter;
    int2 shadowPixel = int2(floor(shadowCoord + 0.5));
    float shadowDistance = shadowDist - dot(shadowOffset, lightDir);
    // L01: compare every PCF tap at its own receiver-plane depth.
    // Geometric normal excludes the material normal map and water waves.
    float receiverDot = dot(receiverN, lightDir);
    float2 receiverSlope = float2(0.0, 0.0);
    float receiverBias = 0.095;
    // Preserve the legacy bias at grazing/degenerate triangles.
    if (abs(receiverDot) > 0.05 * length(receiverN)) {
        receiverSlope = float2(dot(receiverN, shadowRight), dot(receiverN, shadowUp))
                      / (receiverDot * shadowScale);
        receiverBias = 0.005 + 0.000002 * abs(shadowDistance);
    }
    if (shadowPixel.x >= 2 && shadowPixel.y >= 2 && shadowPixel.x < int(shadowSize) - 2 && shadowPixel.y < int(shadowSize) - 2) {
        float visible = 0.0;
        float weightSum = 0.0;
        [unroll] for (int sy = -1; sy <= 1; ++sy) [unroll] for (int sx = -1; sx <= 1; ++sx) { )";
inline constexpr char kShaderPart2[] = R"(
            int2 tap = shadowPixel + int2(sx, sy);
            float2 delta = float2(tap) - shadowCoord;
            float2 tent = max(float2(0.0, 0.0), 1.5 - abs(delta));
            float weight = tent.x * tent.y;
            float storedDepth = shadowDepth.Load(int3(tap, 0));
            float receiverDepth = shadowDistance + dot(receiverSlope, delta);
            visible += weight * (receiverDepth <= storedDepth + receiverBias ? 1.0 : 0.0);
            weightSum += weight;
        }
        shadowFactor = 0.16 + 0.84 * visible / max(weightSum, 0.0001);
    }
    }
    float4 diffuseTerms = materialDiffuse[i.mat];
    // Cheap hemispherical sky fill: upward surfaces receive slightly more
    // ambient daylight than vertical walls and undersides. Direct lighting
    // and the existing shadow map remain separate for stronger depth.
    float hemisphere=saturate(normal.y*0.5+0.5);
    float skyFill=lerp(0.86,1.10,hemisphere);
    float ambientEnergy=(diffuseTerms.x+diffuseTerms.y*diffuseTerms.w)*skyFill;
    float sunEnergy=diffuseTerms.z*diffuse*shadowFactor*diffuseTerms.w;
    if(colorControls.x>0.5){
        // Neutral concrete previously received ~.74 ambient + .56 sun:
        // correct the LIGHT budget, not the pixels after they have clipped.
        float sunlightVisibility=saturate((shadowFactor-.16)/.84);
        ambientEnergy=lerp(.055,.335,diffuseTerms.w)*lerp(.77,1.03,hemisphere);
        sunEnergy=.88*diffuse*sunlightVisibility*diffuseTerms.w;
    }
    // Z Material: preserve the legacy shading of nonmetallic and optical surfaces.
    const float zmMetal=materialTextureDetail[i.mat].y;
    const float zmDiffuse=materialTextureDetail[i.mat].z;
    const float3 diffuseBase=base*zmDiffuse;
    float environmentEnergy=ambientEnergy;
    float3 color = diffuseBase*(ambientEnergy+sunEnergy)
                 + base*materialGlow[i.mat].rgb*materialGlow[i.mat].a;
    [branch] if(peripheralControls.w>.5 && i.primitiveId!=0xffffffffu)
        color+=diffuseBase*lumaTriangleRadiance[i.primitiveId].rgb;
    color += materialEmission[i.mat].rgb * max(materialEmission[i.mat].a,0.0);
    // The old warm (28,13,4) constant tinted *every* night material brown.
    // Keep a tiny near-neutral floor instead; actual lamps add local warmth.
    color += float3(4.0, 5.0, 7.0) / 255.0 * (1.0 - diffuseTerms.w);
    float3 viewDir = normalize(eye - i.world);
    float3 reflectedLight = reflect(-normalize(lightDir), normal);
    float roughness = roughnessMap.Sample(wrapSampler, float3(i.uv, i.mat));
    if(zmMetal>0.0) roughness=clamp(roughness*materialTextureDetail[i.mat].w,.04,1.0);
    float specularPower = materialSpecular[i.mat].y;
    if (materialSpecular[i.mat].z > 0.5) {
        specularPower = clamp(2.0 / max(roughness * roughness, 0.0001) - 2.0, 8.0, 80.0);
    }
    float specular = pow(saturate(dot(viewDir, reflectedLight)), specularPower)
                   * materialSpecular[i.mat].x * shadowFactor * diffuseTerms.w;
    // Use PBR metalness for the highlight tint and a cheap 5th-power
    // Schlick grazing response. This avoids another expensive pow() call.
    float metallic=saturate(materialSpecular[i.mat].w);
    float grazing=1.0-saturate(dot(viewDir,normal));
    float f2=grazing*grazing;
    float fresnel=f2*f2*grazing;
    float3 specTint=lerp(float3(178.0,202.0,214.0)/255.0,
                         saturate(base*1.2),metallic);
    if(colorControls.x>0.5){
        // Dielectrics use neutral Fresnel F0 ~4%, not metallic steel blue.
        // Keep glass specular response using transmission; maintain metals.
        float glazing=1.0-saturate(materialOpacity[i.mat].x);
        specTint=lerp(float3(.04,.04,.04),saturate(base),metallic);
        specTint=lerp(specTint,float3(.24,.26,.28),saturate(glazing*1.8));
    }
    color += specTint * specular * (0.88+0.22*fresnel);
    // Keep the raster fallback's diffuse point lights. In LumaTrace mode,
    // visit local lights only for surfaces that can show a specular reflection.
    const bool localSpecEligible=poolWater || materialOpacity[i.mat].x<.65 ||
        roughness<.14 || (materialSpecular[i.mat].w>.08 && roughness<.50);
    if(peripheralControls.w<.5 || localSpecEligible) {
    [loop] for (uint l = 0; l < lightCount; ++l) {
        float3 delta = lightPositionRadius[l].xyz - i.world;
        float radius = lightPositionRadius[l].w;
#if defined(M3D_V17_LIGHT_AABB_CULL)
        // Conservative light-volume rejection. A light outside ANY axis
        // cannot contribute. Expand radius to preserve the legacy squared
        // distance epsilon and avoid losing boundary pixels.
        [branch] if (radius > 0.0 &&
            all(abs(delta) <= radius * 1.000001)) {
#endif
#if defined(M3D_V17_OPTIMIZE_PIXELS)
        // Reject far lights before computing length()/sqrt. Allow a
        // conservative epsilon to keep the legacy threshold identical.
        float radius2 = radius * radius;
        float distance2 = dot(delta, delta);
        [branch] if (radius > 0.0 && distance2 <= radius2 * 1.000001) {
#endif
            float distanceToLight = length(delta);
            if (distanceToLight < radius && distanceToLight > 0.0001) {
                float edge = 1.0 - distanceToLight / radius;
                float lambert = saturate(dot(normal, delta / distanceToLight));
                float3 localIrradiance=lightColorIntensity[l].rgb
                    *(edge*edge*lightColorIntensity[l].a*lambert*.27);
                if(peripheralControls.w<.5)
                    color += (colorControls.x>0.5 ? base*localIrradiance : localIrradiance)*zmDiffuse;
                if(localSpecEligible){
                    float3 toLight=delta/distanceToLight;
                    float3 reflectedPoint=reflect(-toLight,normal);
                    float pointPower=clamp(specularPower,8.0,120.0);
                    float pointSpec=pow(saturate(dot(viewDir,reflectedPoint)),pointPower)
                        *max(materialSpecular[i.mat].x,.035)
                        *edge*edge*lightColorIntensity[l].a*.27;
                    color += specTint*lightColorIntensity[l].rgb*pointSpec
                        *(0.88+0.22*fresnel);
                }
            }
#if defined(M3D_V17_OPTIMIZE_PIXELS)
        }
#endif
#if defined(M3D_V17_LIGHT_AABB_CULL)
        }
#endif
    }
    }
    if(hybridControls.x>0.5){
        // Indirect probes modify AMBIENT energy instead of adding an extra
        // full-strength light pass. This prevents washed-out concrete.
        float3 position=saturate((i.world-probeOrigin.xyz)*probeInvSpan.xyz);
        float4 probe=indirectProbes.SampleLevel(probeSampler,position,0);
        if(hybridControls.w>.5){
            const float4 nextProbe=indirectProbesNext.SampleLevel(probeSampler,position,0);
            probe=lerp(probe,nextProbe,hybridControls.y);
        }
        if(hybridControls.w>1.5){
            // LumaTrace's lightweight GPU cache contains one diffuse bounce
            // from directly lit surfaces, bilinearly interpolated in space.
            const float3 bounce=max(probe.rgb,0.0);
            color+=diffuseBase*bounce;
            if(zmMetal>0.0)environmentEnergy+=dot(bounce,float3(.2126,.7152,.0722));
        }else{
            float skyVisibility=saturate(probe.a);
            float upward=saturate(normal.y*.5+.5);
            float existingAmbient=ambientEnergy;
            // Normalize the time-dependent sky color so daylight changes hue,
            // but doesn't create energy from a second hemispherical fill.
            float skyMean=max(dot(indirectSky.rgb,float3(.333333,.333333,.333333)),.001);
            float3 skyTint=lerp(float3(1.0,1.0,1.0),
                                indirectSky.rgb/skyMean,.22);
            // Retain up to 92% of the legacy ambient in open air, but only
            // 48% in occluded interiors. Static probe volume interpolates in XYZ.
            float3 accessibleSky=existingAmbient
                                 *lerp(.48,.92,skyVisibility)*skyTint;
            // Low-energy, material-colored bounce gives warmth to nearby walls
            // without washing out roofs. Decays in exposed outdoor probes.
            float3 reflected=indirectBounce.rgb*probe.rgb
                             *(.12+.18*(1.0-upward))
                             *(.80-.30*skyVisibility);
            float3 indirect=accessibleSky+reflected;
            if(hybridControls.w>.5){
                // The offline cache stores real surface-to-probe illumination.
                // Do not re-color it using the old empirical sky/bounce terms.
                indirect=max(probe.rgb,0.0);
            }
            color+=diffuseBase*(indirect-existingAmbient);
            if(zmMetal>0.0)environmentEnergy=dot(indirect,float3(.2126,.7152,.0722));
            // The existing shadow map and dynamic direct sun remain unchanged;
            // gently tint only the direct contribution at warm solar hours.
            float sunContribution=diffuseTerms.z*diffuse*shadowFactor*diffuseTerms.w;
            color+=diffuseBase*sunContribution*(indirectSun.rgb-1.0)*.40;
        }
    }
    [branch] if(zmMetal>0.0){
        const float reflectedY=2.0*saturate(dot(normal,viewDir))*normal.y-viewDir.y;
        const float directional=1.0+0.55*(1.0-roughness*roughness)*clamp(reflectedY,-1.0,1.0);
        color+=base*(zmMetal*max(environmentEnergy,0.0)*directional);
    }
    if(hybridControls.z>0.5){
        // Conservative display-referred shoulder, independently toggled.
        // IMPORTANT: no sRGB gamma here; the legacy back buffer is UNORM
        // and the V17.309 pow(1/2.2) incorrectly lifted ALL midtones.
        float3 linearPositive=max(color,0.0);
        color=linearPositive/(1.0+.70*max(linearPositive-.64,0.0));
    }
    if(colorControls.x>0.5){
        // Color is LINEAR; SRGB RTV encodes exactly once. A shared
        // luminance shoulder preserves chroma ratios while avoiding clips.
        float3 positive=max(color,0.0);
        float luminance=dot(positive,float3(.2126,.7152,.0722));
        float adjusted=luminance*colorControls.y;
        float compressed=adjusted/(1.0+.70*max(adjusted-.63,0.0));
        color=positive*(compressed/max(luminance,0.000001));
    }
    if(poolWater){
        // Grazing-angle Fresnel, soft cool highlights and bottom visibility
        // through the transparent polygon. No DXR/SSR dependency.
        const float incidence=1.0-saturate(dot(normal,viewDir));
        const float fresnelWater=.02+.98*pow(incidence,5.0);
        float stripe=pow(saturate(.5+.5*sin(i.world.x*1.4+i.world.z*.37)),14.0);
        color=lerp(color,float3(.24,.57,.69),.07+fresnelWater*.20);
        color+=float3(.19,.32,.37)*stripe*(.025+.13*fresnelWater);
        // Underwater ladders must remain visible through the surface from the deck.
        // Avoid the former 45% high-angle opaque cyan veil.
        float opacity=lerp(.08,.24,saturate(fresnelWater*1.75));
        float3 reflectedScene;
        float coverage=1.0,escWeight=0.0;
        bool optical=sampleOptical(i,clamp(normal.xz*.35,-.04,.04),reflectedScene);
        if(!optical)optical=sampleEscargot(i,reflectedScene,coverage,escWeight);
        if(optical){
            const float reflectionWeight=saturate(.22+.62*fresnelWater)*coverage;
            color=lerp(color,reflectedScene,reflectionWeight);
            opacity=lerp(opacity,.62,saturate(fresnelWater*1.45)*coverage);
        }
        // Transmission controls the water's opacity continuously; its Fresnel
        // opacity remains the authored 100%-transmission appearance.
        const float authoredTransmission=1.0-saturate(materialOpacity[i.mat].x);
        opacity=1.0-authoredTransmission*(1.0-opacity);
        return float4(color,opacity);
    }
    // Fresnel-composite reflected opaque surroundings into original glass PBR.
    // The ordinary alpha blend still supplies transmission; this is NOT refraction.
    float3 reflected;
    if(sampleOptical(i,float2(0,0),reflected)){
        float f0=saturate(materialOpacity[i.mat].z);
        float f=f0+(1.0-f0)*pow(1.0-saturate(abs(dot(normal,viewDir))),5.0);
        float alpha=saturate(materialOpacity[i.mat].x);
        float combined=f+(1.0-f)*alpha;
        return float4((reflected*f+color*alpha*(1.0-f))/max(combined,.00001),combined);
    }
    float escCoverage,escWeight;
    if(sampleEscargot(i,reflected,escCoverage,escWeight)){
        const float alpha=saturate(materialOpacity[i.mat].x);
        if(alpha<.999){
            const float f0=saturate(materialOpacity[i.mat].z);
            const float f=(f0+(1.0-f0)*pow(1.0-saturate(abs(dot(normal,viewDir))),5.0))*escCoverage;
            const float combined=f+(1.0-f)*alpha;
            return float4((reflected*f+color*alpha*(1.0-f))/max(combined,.00001),combined);
        }
        color=lerp(color,reflected,saturate(escWeight*escCoverage));
    }
    return float4(color, materialOpacity[i.mat].x);
}
)";

} // namespace m3d::dx11::shaders::raster
