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
    float3 reflectionEye; float reflectionTanHalf;
    float3 reflectionForward; float reflectionAspect;
    float3 reflectionRight; float reflectionPad0;
    float3 reflectionUp; float reflectionPad1;
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
Texture2D<float4> poolReflection : register(t8);
Texture2D<float4> zReflection0 : register(t9);
Texture2D<float4> zReflection1 : register(t10);
SamplerState wrapSampler : register(s0);
SamplerState probeSampler : register(s1);
struct VSIn { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint mat:MATERIAL; uint primitiveId:TRIID; };
struct VSOut { float4 p:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; float3 world:TEXCOORD1; nointerpolation uint mat:MATERIAL; nointerpolation uint primitiveId:TEXCOORD2; nointerpolation uint opticalId:TEXCOORD3; };
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
    o.opticalId=0u;
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
    raw.p=p;raw.n=n;raw.uv=uv;raw.mat=i.boxMat;raw.primitiveId=i.unusedPrimitiveId;
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
    raw.mat=i.ellipseMat;raw.primitiveId=i.unusedPrimitiveId;
    return VSMain(raw);
}
float4 PSMain(VSOut i):SV_TARGET {
    if(zPlaneIds.w>.5 && dot(float4(i.world,1.0),zClip)<0.0) discard;
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
    if(reflectionControls.x>.5 && reflectionControls.x<1.5 &&
       (i.world.y<reflectionControls.w || poolWater || materialOpacity[i.mat].x<.45)) discard;
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
    float3 shadowOffset = i.world - shadowBase;
    float shadowFactor = 1.0;
    float shadowScale = shadowSize / (2.0 * shadowSpan);
    float shadowCenter = shadowSize * 0.5 - 0.5;
    int2 shadowPixel = int2(round(float2(dot(shadowOffset, shadowRight), dot(shadowOffset, shadowUp)) * shadowScale + shadowCenter));
    float shadowDistance = shadowDist - dot(shadowOffset, lightDir);
    if (shadowPixel.x >= 2 && shadowPixel.y >= 2 && shadowPixel.x < int(shadowSize) - 2 && shadowPixel.y < int(shadowSize) - 2) {
        float visible = 0.0;
        [unroll] for (int sy = -1; sy <= 1; ++sy) [unroll] for (int sx = -1; sx <= 1; ++sx) { )";
inline constexpr char kShaderPart2[] = R"(
            float storedDepth = shadowDepth.Load(int3(shadowPixel + int2(sx, sy), 0));
            visible += shadowDistance <= storedDepth + 0.095 ? 1.0 : 0.0;
        }
        shadowFactor = 0.16 + 0.84 * visible / 9.0;
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
    float3 color = base*(ambientEnergy+sunEnergy)
                 + base*materialGlow[i.mat].rgb*materialGlow[i.mat].a;
    [branch] if(peripheralControls.w>.5 && i.primitiveId!=0xffffffffu)
        color+=base*lumaTriangleRadiance[i.primitiveId].rgb;
    color += materialEmission[i.mat].rgb * max(materialEmission[i.mat].a,0.0);
    // The old warm (28,13,4) constant tinted *every* night material brown.
    // Keep a tiny near-neutral floor instead; actual lamps add local warmth.
    color += float3(4.0, 5.0, 7.0) / 255.0 * (1.0 - diffuseTerms.w);
    float3 viewDir = normalize(eye - i.world);
    float3 reflectedLight = reflect(-normalize(lightDir), normal);
    float roughness = roughnessMap.Sample(wrapSampler, float3(i.uv, i.mat));
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
                    color += colorControls.x>0.5 ? base*localIrradiance : localIrradiance;
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
            color+=base*max(probe.rgb,0.0);
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
            color+=base*(indirect-existingAmbient);
            // The existing shadow map and dynamic direct sun remain unchanged;
            // gently tint only the direct contribution at warm solar hours.
            float sunContribution=diffuseTerms.z*diffuse*shadowFactor*diffuseTerms.w;
            color+=base*sunContribution*(indirectSun.rgb-1.0)*.40;
        }
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
        if(reflectionControls.x>1.5 && abs(i.world.y-reflectionControls.w)<=.002 ){
            float3 reflectedView=i.world-reflectionEye;
            float reflectedDepth=dot(reflectedView,reflectionForward);
            if(reflectedDepth>.08){
                float reflectedNdcX=dot(reflectedView,reflectionRight)/(reflectedDepth*reflectionTanHalf);
                float reflectedNdcY=dot(reflectedView,reflectionUp)*reflectionAspect/(reflectedDepth*reflectionTanHalf);
                // Keep normal distortion inside the 10% mirror guard band.
                float2 reflectionUv=float2(.5+.5*reflectedNdcX,.5-.5*reflectedNdcY)
                    + clamp(normal.xz*.35,-.04,.04);
                if(all(reflectionUv>=0.0)&&all(reflectionUv<=1.0)){
                    float3 reflectedScene=poolReflection.SampleLevel(probeSampler,reflectionUv,0).rgb;
                    const float reflectionWeight=saturate(.22+.62*fresnelWater);
                    color=lerp(color,reflectedScene,reflectionWeight);
                    opacity=lerp(opacity,.62,saturate(fresnelWater*1.45));
                }
            }
        }
        // Transmission controls the water's opacity continuously; its Fresnel
        // opacity remains the authored 100%-transmission appearance.
        const float authoredTransmission=1.0-saturate(materialOpacity[i.mat].x);
        opacity=1.0-authoredTransmission*(1.0-opacity);
        return float4(color,opacity);
    }
    // Fresnel-composite reflected opaque surroundings into original glass PBR.
    // The ordinary alpha blend still supplies transmission; this is NOT refraction.
    if(i.opticalId!=0u && zPlaneIds.w<.5 && zPlaneIds.z>.5 && reflectionControls.x!=1.0){
        int slot=-1;
        if(i.opticalId==(uint)zPlaneIds.x)slot=0;
        else if(zPlaneIds.z>1.5 && i.opticalId==(uint)zPlaneIds.y)slot=1;
        if(slot>=0){
            float3 q=i.world-zEye[slot].xyz;
            float z=dot(q,zForward[slot].xyz);
            if(z>.08){
                float2 ndc=float2(dot(q,zRight[slot].xyz),dot(q,zUp[slot].xyz)*zForward[slot].w)/(z*zEye[slot].w);
                float2 uv=float2(.5+.5*ndc.x,.5-.5*ndc.y);
                if(all(uv>=0.0)&&all(uv<=1.0)){
                    float3 reflected=slot==0?zReflection0.SampleLevel(probeSampler,uv,0).rgb:zReflection1.SampleLevel(probeSampler,uv,0).rgb;
                    float f0=saturate(materialOpacity[i.mat].z);
                    float f=f0+(1.0-f0)*pow(1.0-saturate(abs(dot(normal,viewDir))),5.0);
                    float alpha=saturate(materialOpacity[i.mat].x);
                    float combined=f+(1.0-f)*alpha;
                    return float4((reflected*f+color*alpha*(1.0-f))/max(combined,.00001),combined);
                }
            }
        }
    }
    return float4(color, materialOpacity[i.mat].x);
}
)";

} // namespace m3d::dx11::shaders::raster
