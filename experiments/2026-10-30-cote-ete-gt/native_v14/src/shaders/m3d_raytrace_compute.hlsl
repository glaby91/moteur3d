// Moteur3D V16 - Windows D3D11 Compute shader, Shader Model 5.0.
// Same scene, projection and PBR mip chains as the CPU renderer.
// Color-managed primary lighting: CPU-GI GGX/Schlick + linear irradiance + ACES/sRGB.
// Single-bounce preview: not identical to CPU two-bounce global illumination.
struct Ray { float3 origin; float3 direction; };
struct GpuBvhNode {
    float3 boundsMin; uint left;
    float3 boundsMax; uint right;
    uint first; uint count; uint pad0; uint pad1;
};
struct GpuTriangle {
    float3 p0; uint material;
    float3 p1; uint pad0;
    float3 p2; uint pad1;
    float3 n0; uint pad2;
    float3 n1; uint pad3;
    float3 n2; uint pad4;
    float2 uv0; float2 uv1;
    float2 uv2; float2 uvPad;
};
struct GpuMaterial {
    float3 albedo; float roughnessScale;
    float metallic; float transmission; float ior; float dielectricF0;
};
struct HitInfo {
    float t;
    float2 bary;
    uint primitive;
};
cbuffer Frame : register(b0) {
    float3 eye; float tanHalfFov;
    float3 forward; float aspect;
    float3 right; float nearPlane;
    float3 up; float farPlane;
    uint width; uint height; uint nodeCount; uint triangleCount;
    uint rootIndex; uint maxTraversal; uint materialCount; uint textureBaseSize;
    float3 sunDirection; float ambientStrength;
    float3 skyZenith; float skyStrength;
    float3 skyHorizon; float sunStrength;
    float3 sunColor; float skyFillStrength;
    uint lightCount; float sceneLightScale; float indirectFill; uint gpuIndirectSamples;
    uint adaptiveGI; uint indirectWidth; uint indirectHeight; uint primaryQuarter;
};
struct GpuLight {
    float3 position; float radius;
    float3 color; float intensity;
};
StructuredBuffer<GpuBvhNode> Nodes : register(t0);
StructuredBuffer<GpuTriangle> Triangles : register(t1);
StructuredBuffer<uint> PrimitiveOrder : register(t2);
StructuredBuffer<GpuMaterial> Materials : register(t3);
// Each material owns three slices: color, tangent-space normal, roughness.
Texture2DArray<float4> PbrMaps : register(t4);
StructuredBuffer<GpuLight> Lights : register(t5);
// The low-res GI buffers hold untonemapped diffuse radiance and hit normal/depth.
// They are read only in CSMain and written only in CSIndirect; never bound
// concurrently as SRV+UAV in D3D11.
Texture2D<float4> IndirectRadiance : register(t6);
Texture2D<float4> IndirectGeometry : register(t7);
RWTexture2D<float4> IndirectOut : register(u1);
RWTexture2D<float4> IndirectGeoOut : register(u2);
SamplerState RepeatTrilinear : register(s0);
RWTexture2D<float4> Output : register(u0);

// CPU GI packHdr() uses ACES + sRGB, not raw saturate.
float3 srgbToLinear(float3 c) {
    c=max(c,float3(0,0,0));
    return lerp(c/12.92f,pow((c+.055f)/1.055f,2.4f),step(.04045f,c));
}
float3 linearToSrgb(float3 c) {
    c=max(c,float3(0,0,0));
    return lerp(c*12.92f,1.055f*pow(c,1.f/2.4f)-.055f,step(.0031308f,c));
}
float3 encodeHdr(float3 radiance) {
    float3 v=max(radiance,float3(0,0,0));
    float3 aces=saturate((v*(2.51f*v+.03f))/(v*(2.43f*v+.59f)+.14f));
    return saturate(linearToSrgb(aces));
}
float3 background(Ray ray) {
    float t=saturate((ray.direction.y+.10f)*1.10f);
    float3 sky=lerp(skyHorizon,skyZenith,t)*skyStrength;
    float alignment=saturate(dot(ray.direction,normalize(sunDirection)));
    float aureole=pow(alignment,64.f)*.065f*saturate(sunStrength/2.8f);
    return srgbToLinear(saturate(sky+sunColor*aureole));
}
bool intersectBounds(Ray ray,float3 bmin,float3 bmax,float limit) {
    float nearT=max(nearPlane,.0001f),farT=limit;
    [unroll] for(uint axis=0;axis<3;++axis) {
        float origin=ray.origin[axis],direction=ray.direction[axis];
        if(abs(direction)<1e-8f) {
            if(origin<bmin[axis]||origin>bmax[axis])return false;
        } else {
            float a=(bmin[axis]-origin)/direction;
            float b=(bmax[axis]-origin)/direction;
            nearT=max(nearT,min(a,b));
            farT=min(farT,max(a,b));
            if(farT<nearT)return false;
        }
    }
    return true;
}
bool intersectTriangle(Ray ray,GpuTriangle tri,out float t,out float2 uv) {
    float3 e1=tri.p1-tri.p0,e2=tri.p2-tri.p0;
    float3 p=cross(ray.direction,e2);
    float det=dot(e1,p);
    t=0;uv=float2(0,0);
    if(abs(det)<1e-9f)return false;
    float inv=rcp(det);
    float3 s=ray.origin-tri.p0;
    float u=dot(s,p)*inv;
    if(u<0||u>1)return false;
    float3 q=cross(s,e1);
    float v=dot(ray.direction,q)*inv;
    if(v<0||u+v>1)return false;
    t=dot(e2,q)*inv;
    uv=float2(u,v);
    return t>max(nearPlane,.0001f)&&t<farPlane;
}
// Returns 0 = miss, 1 = hit, 2 = invalid BVH / traversal overflow.
// Ignore mostly transparent triangles for opaque shadow occlusion.
uint traceClosest(Ray ray,float limit,bool shadowOnly,uint skipPrimitive,out HitInfo hit) {
    hit.t=limit;hit.bary=float2(0,0);hit.primitive=0;
    if(nodeCount==0||rootIndex>=nodeCount)return 0;
    uint stack[64],sp=1;
    stack[0]=rootIndex;
    bool found=false;
    uint visits=0;
    [loop] while(sp>0) {
        if(++visits>maxTraversal)return 2;
        uint index=stack[--sp];
        if(index>=nodeCount)return 2;
        GpuBvhNode node=Nodes[index];
        if(!intersectBounds(ray,node.boundsMin,node.boundsMax,hit.t))continue;
        if(node.count>0) {
            [loop] for(uint i=0;i<node.count;++i) {
                if(node.first+i>=triangleCount)return 2;
                uint primIndex=PrimitiveOrder[node.first+i];
                if(primIndex>=triangleCount)return 2;
                if(shadowOnly&&primIndex==skipPrimitive)continue;
                GpuTriangle tri=Triangles[primIndex];
                if(tri.material>=materialCount)return 2;
                if(shadowOnly&&Materials[tri.material].transmission>.35f)continue;
                float t;float2 uv;
                if(intersectTriangle(ray,tri,t,uv)&&t<hit.t&&(!shadowOnly||t>.065f)) {
                    hit.t=t;hit.bary=uv;hit.primitive=primIndex;found=true;
                    if(shadowOnly)return 1;
                }
            }
        } else {
            if(sp+2>64)return 2;
            stack[sp++]=node.left;
            stack[sp++]=node.right;
        }
    }
    return found?1:0;
}
float2 textureCoordinates(GpuTriangle tri,float2 bary) {
    return tri.uv0*(1-bary.x-bary.y)+tri.uv1*bary.x+tri.uv2*bary.y;
}
float estimateMip(GpuTriangle tri,float distance) {
    float3 e1=tri.p1-tri.p0,e2=tri.p2-tri.p0;
    float2 uv1=tri.uv1-tri.uv0,uv2=tri.uv2-tri.uv0;
    float s1=length(uv1)/max(.001f,length(e1));
    float s2=length(uv2)/max(.001f,length(e2));
    float uvPerMeter=max(s1,s2);
    float widthFootprint=max(.0001f,2.f*distance*tanHalfFov/max(1.f,(float)width));
    return clamp(log2(max(1.f,widthFootprint*textureBaseSize*uvPerMeter)),
                 0.f,log2((float)textureBaseSize));
}
float3 tangentSpaceNormal(GpuTriangle tri,float3 baseNormal,float3 mapped) {
    float3 e1=tri.p1-tri.p0,e2=tri.p2-tri.p0;
    float2 d1=tri.uv1-tri.uv0,d2=tri.uv2-tri.uv0;
    float det=d1.x*d2.y-d1.y*d2.x;
    if(abs(det)<1e-7f)return baseNormal;
    float3 tangent=normalize((e1*d2.y-e2*d1.y)/det);
    tangent=normalize(tangent-baseNormal*dot(baseNormal,tangent));
    float3 bitangent=cross(baseNormal,tangent)*((det<0.f)?-1.f:1.f);
    return normalize(tangent*mapped.x+bitangent*mapped.y+baseNormal*mapped.z);
}
// CPU offlinePbrShade's GGX/Smith/Schlick direct BRDF and hemispheric sky.
// No invented GI bounce; indirectFill is zero until a validated probe exists.
float ggxDistribution(float nh,float roughness) {
    float a=max(.045f,roughness);float a2=a*a*a*a;
    float d=nh*nh*(a2-1.f)+1.f;
    return a2/(3.141592654f*d*d+1e-7f);
}
float smithG1(float nd,float roughness) {
    float a=max(.045f,roughness);float k=(a+1.f)*(a+1.f)*.125f;
    return nd/(nd*(1.f-k)+k+1e-7f);
}
float3 brdf(float3 n,float3 view,float3 l,float3 base,float roughness,
            float metallic,float3 f0,float3 radiance,float visibilityFactor) {
    float nl=saturate(dot(n,l));
    if(nl<=0.f||visibilityFactor<=0.f)return float3(0,0,0);
    float nv=max(1e-4f,dot(n,view));
    float3 halfVector=normalize(view+l);
    float nh=saturate(dot(n,halfVector)),vh=saturate(dot(view,halfVector));
    float3 F=f0+(1.f-f0)*pow(1.f-vh,5.f);
    float D=ggxDistribution(nh,roughness);
    float G=smithG1(nv,roughness)*smithG1(nl,roughness);
    float3 spec=F*(D*G/(4.f*nv*nl+1e-6f));
    float3 diff=(1.f-F)*(1.f-metallic)*base*(1.f/3.141592654f);
    return (diff+spec)*radiance*(nl*visibilityFactor);
}
float3 shadeSurface(Ray ray,HitInfo hit,float3 n,float3 base,float roughness,
                    GpuMaterial material) {
    GpuTriangle tri=Triangles[hit.primitive];
    float3 p=ray.origin+ray.direction*hit.t;
    if(dot(n,ray.direction)>0.f)n=-n;
    float3 view=normalize(-ray.direction);
    float metallic=saturate(material.metallic);
    float3 f0=lerp(float3(material.dielectricF0,material.dielectricF0,material.dielectricF0),
                   base,metallic);
    float3 L=normalize(sunDirection);
    float visibilityFactor=1.f;
    if(dot(n,L)>.0001f) {
        float3 geom=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
        if(dot(geom,ray.direction)>0.f)geom=-geom;
        Ray shadow;shadow.origin=p+geom*.035f+L*.025f;shadow.direction=L;
        HitInfo blocker;
        uint state=traceClosest(shadow,1000.f,true,hit.primitive,blocker);
        visibilityFactor=(state==1)?0.f:1.f;
    }
    float3 sunRadiance=srgbToLinear(sunColor)*sunStrength;
    float3 lit=brdf(n,view,L,base,roughness,metallic,f0,sunRadiance,visibilityFactor);
    float skyFacing=saturate(.5f+.5f*n.y);
    float3 skyIrradiance=srgbToLinear(skyZenith)*(.25f+.55f*skyFacing)+
                         srgbToLinear(skyHorizon)*(.50f-.20f*skyFacing);
    lit+=base*skyIrradiance*.22f;
    // Respect point lights authored in scenes/v16/pavillon01/lighting/day.json.
    [loop] for(uint i=0;i<min(lightCount,64u);++i) {
        GpuLight light=Lights[i];
        float3 delta=light.position-p;
        float d2=dot(delta,delta),r2=light.radius*light.radius;
        if(d2<1.e-5f||d2>=r2)continue;
        float distance=sqrt(d2),edge=1.f-distance/light.radius;
        float3 radiance=srgbToLinear(light.color)*
                        (light.intensity*sceneLightScale*edge*edge*.45f);
        lit+=brdf(n,view,delta/distance,base,roughness,metallic,f0,radiance,1.f);
    }
    lit+=base*indirectFill;
    return max(lit,float3(0,0,0));
}
// One physically motivated diffuse secondary ray per sample. This term is
// averaged over independent cosine-weighted hemisphere directions. We deliberately
// DO NOT invent hidden walls and DO NOT add arbitrary exposure to match CPU.
// Secondary hits are shaded by the CPU-aligned GGX direct+hemisphere model;
// no tertiary recursion. This is an approximate 1-bounce indirect estimate.
uint mix32(uint x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
    return x;
}
float rand01(uint x) {return float(mix32(x)&0x00ffffffu)*(1.f/16777216.f);}
// One radiance query for a secondary/reflective ray. Respect transparent
// panes instead of treating every glass hit as an opaque blue diffuse wall.
float3 secondaryRadiance(Ray ray) {
    float3 result=float3(0,0,0);
    float throughput=1.f;
    [loop] for(uint layer=0;layer<4u;++layer) {
        HitInfo h;
        uint state=traceClosest(ray,farPlane,false,0xffffffffu,h);
        if(state==0u)return result+background(ray)*throughput;
        if(state!=1u)return result;
        GpuTriangle tri=Triangles[h.primitive];
        GpuMaterial material=Materials[tri.material];
        float k=1.f-h.bary.x-h.bary.y;
        float3 n=normalize(tri.n0*k+tri.n1*h.bary.x+tri.n2*h.bary.y);
        float2 uv=textureCoordinates(tri,h.bary);
        float mip=estimateMip(tri,h.t);
        float3 base=srgbToLinear(PbrMaps.SampleLevel(RepeatTrilinear,
                                                    float3(uv,tri.material*3),mip).rgb);
        float3 mapped=PbrMaps.SampleLevel(RepeatTrilinear,
                                         float3(uv,tri.material*3+1),mip).xyz*2.f-1.f;
        if(dot(mapped,mapped)>.25f)n=tangentSpaceNormal(tri,n,normalize(mapped));
        float roughness=clamp(PbrMaps.SampleLevel(RepeatTrilinear,
                                float3(uv,tri.material*3+2),mip).r*material.roughnessScale,
                                .04f,.98f);
        float3 lit=shadeSurface(ray,h,n,base,roughness,material);
        float transmission=saturate(material.transmission);
        if(transmission<=.05f)return result+throughput*lit;
        // Preserve the direct material response for a pane but let GI/sky
        // actually travel through the glass. Not a full refractive ray path.
        float3 geomN=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
        float facing=abs(dot(geomN,ray.direction));
        float f0=saturate(material.dielectricF0);
        float fresnel=f0+(1.f-f0)*pow(1.f-facing,5.f);
        float transmittedWeight=saturate(transmission*(1.f-fresnel));
        result+=throughput*(1.f-transmittedWeight)*lit;
        throughput*=transmittedWeight;
        if(throughput<.02f)return result;
        ray.origin+=ray.direction*(h.t+.065f);
    }
    return result+throughput*background(ray);
}
float3 diffuseBounce(Ray primary,HitInfo hit,float3 n,float3 albedo,
                     float metallic) {
    uint samples=min(gpuIndirectSamples,4u);
    if(samples==0u || metallic>=.99f)return float3(0,0,0);
    GpuTriangle tri=Triangles[hit.primitive];
    float3 p=primary.origin+primary.direction*hit.t;
    float3 geom=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
    if(dot(geom,primary.direction)>0.f)geom=-geom;
    if(dot(n,primary.direction)>0.f)n=-n;
    // The normal map is useful for BRDF shading but can turn a secondary
    // hemisphere into geometry, producing dark salt-and-pepper speckles.
    // Use the geometric hemisphere for visibility, with modest normal detail.
    float3 transportNormal=normalize(geom*.85f+n*.15f);
    float3 axis=abs(transportNormal.y)<.9f?float3(0.f,1.f,0.f):float3(1.f,0.f,0.f);
    float3 tangent=normalize(cross(axis,transportNormal));
    float3 bitangent=normalize(cross(transportNormal,tangent));
    float3 bounced=float3(0,0,0);
    // Fixed, stratified cosine quadrature: coherent on coplanar surfaces.
    // Independent pixel random phases previously gave 2-spp GI severe
    // salt/pepper flicker around the facade base and transparent interiors.
    // This trades a little stochastic unbiasedness for stable real-time GI.
    [loop] for(uint i=0;i<samples;++i) {
        float u=(float(i)+.5f)/float(samples);
        float v=frac((float(i)+.5f)*.618033989f);
        float r=sqrt(u),phi=6.28318530718f*v;
        float3 dir=normalize(tangent*(r*cos(phi))+bitangent*(r*sin(phi))+
                             transportNormal*sqrt(max(0.f,1.f-u)));
        // Never launch from a bump-normal-under-surface position.
        Ray secondary;secondary.origin=p+geom*.06f;
        secondary.direction=dir;
        bounced+=secondaryRadiance(secondary);
    }
    return bounced*(albedo*(1.f-saturate(metallic))/float(samples));
}
// Half-resolution indirect lighting (2x2 full-res pixels / sample).
// Primary lighting, transparent windows and specular pool reflections
// remain native Full HD. Only the expensive diffuse secondary BVH rays
// are shared. Unsupported surfaces fall back to real full-res GI.
float3 reusableBounce(Ray primary,HitInfo hit,float3 n,float3 albedo,
                       float metallic,uint2 pixelCoord) {
    if(gpuIndirectSamples==0u)return float3(0,0,0);
    if(adaptiveGI==0u)return diffuseBounce(primary,hit,n,albedo,metallic);
    GpuTriangle tri=Triangles[hit.primitive];
    float3 geom=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
    if(dot(geom,primary.direction)>0.f)geom=-geom;
    float depth=length(primary.origin+primary.direction*hit.t-eye);
    float2 uv=(float2(pixelCoord)+.5f)*.5f-.5f;
    int2 cell=int2(floor(uv));
    float3 result=float3(0,0,0);
    float weightSum=0.f;
    [unroll] for(int iy=0;iy<2;++iy) {
        [unroll] for(int ix=0;ix<2;++ix) {
            int2 p=cell+int2(ix,iy);
            if(p.x<0||p.y<0||p.x>=int(indirectWidth)||p.y>=int(indirectHeight))
                continue;
            float4 geometry=IndirectGeometry.Load(int3(p,0));
            if(geometry.w<=0.f || abs(geometry.w-depth)>max(.09f,depth*.015f))
                continue;
            float normalFit=dot(geom,geometry.xyz);
            if(normalFit<.88f)continue;
            float2 weights=saturate(1.f-abs(float2(p)-uv));
            float weight=weights.x*weights.y*
                saturate((normalFit-.88f)/.12f);
            if(weight<=1e-5f)continue;
            result+=IndirectRadiance.Load(int3(p,0)).rgb*weight;
            weightSum+=weight;
        }
    }
    // Disocclusions, glass borders, geometry edges: never blur GI over
    // unrelated surfaces. Expensive fallback is local to these pixels.
    if(weightSum<.02f)
        return diffuseBounce(primary,hit,n,albedo,metallic);
    // Store incident irradiance in the atlas, not the producer texel color.
    // Albedo/metallicity are applied at each final Full HD shading point.
    return (result/weightSum)*albedo*(1.f-saturate(metallic));
}

// This pass runs at 1/2 resolution in each axis, tracing 1/4 as many
// expensive secondary GI samples. G-buffer filtering is deferred to CSMain.
[numthreads(8,8,1)]
void CSIndirect(uint3 tid:SV_DispatchThreadID) {
    if(tid.x>=indirectWidth||tid.y>=indirectHeight)return;
    float2 xy=float2(tid.xy)*2.f+1.f;
    float2 uv=xy/float2(width,height)*2.f-1.f;
    Ray ray;ray.origin=eye;
    ray.direction=normalize(forward+right*uv.x*tanHalfFov-
                            up*uv.y*(tanHalfFov/aspect));
    [loop] for(uint layer=0;layer<6u;++layer) {
        HitInfo hit;
        uint status=traceClosest(ray,farPlane,false,0xffffffffu,hit);
        if(status!=1u)break;
        GpuTriangle tri=Triangles[hit.primitive];
        GpuMaterial mat=Materials[tri.material];
        if(mat.transmission>.05f) {
            // The actual visible opaque receiver may be behind glazing.
            ray.origin+=ray.direction*(hit.t+.065f);
            continue;
        }
        float3 n=normalize(tri.n0*(1.f-hit.bary.x-hit.bary.y)+
                           tri.n1*hit.bary.x+tri.n2*hit.bary.y);
        float2 tex=textureCoordinates(tri,hit.bary);
        float mip=estimateMip(tri,hit.t);
        float3 albedo=srgbToLinear(PbrMaps.SampleLevel(
                RepeatTrilinear,float3(tex,tri.material*3),mip).rgb);
        float3 mapped=PbrMaps.SampleLevel(RepeatTrilinear,
                                    float3(tex,tri.material*3+1),mip).xyz*2.f-1.f;
        if(dot(mapped,mapped)>.25f)n=tangentSpaceNormal(tri,n,normalize(mapped));
        float3 geom=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
        if(dot(geom,ray.direction)>0.f)geom=-geom;
        float depth=length(ray.origin+ray.direction*hit.t-eye);
        // GI cache stores the incoming radiance before material response.
        // This avoids 2x2 color blocks on textured grass, plaster and wood.
        float3 incoming=mat.metallic>=.99f?float3(0.f,0.f,0.f):
                         diffuseBounce(ray,hit,n,float3(1.f,1.f,1.f),0.f);
        IndirectOut[tid.xy]=float4(incoming,1.f);
        IndirectGeoOut[tid.xy]=float4(geom,depth);
        return;
    }
    IndirectOut[tid.xy]=float4(0.f,0.f,0.f,0.f);
    IndirectGeoOut[tid.xy]=float4(0.f,0.f,0.f,-1.f);
}
float4 tracePrimary(Ray ray,uint2 pixelCoord) {
    float3 accumulated=float3(0,0,0);
    float throughput=1.f;
    [loop] for(uint layer=0;layer<6;++layer) {
        HitInfo hit;
        uint status=traceClosest(ray,farPlane,false,0xffffffffu,hit);
        if(status==2)return float4(1,0,1,1);
        if(status==0) {
            accumulated+=background(ray)*throughput;
            return float4(accumulated,1);
        }
        GpuTriangle tri=Triangles[hit.primitive];
        GpuMaterial material=Materials[tri.material];
        float3 n=normalize(tri.n0*(1-hit.bary.x-hit.bary.y)+
                           tri.n1*hit.bary.x+tri.n2*hit.bary.y);
        float2 uv=textureCoordinates(tri,hit.bary);
        float mip=estimateMip(tri,hit.t);
        float3 base=srgbToLinear(PbrMaps.SampleLevel(RepeatTrilinear,
                                                   float3(uv,tri.material*3),mip).rgb);
        float3 mapped=PbrMaps.SampleLevel(RepeatTrilinear,float3(uv,tri.material*3+1),mip).xyz*2.f-1.f;
        if(dot(mapped,mapped)>.25f)n=tangentSpaceNormal(tri,n,normalize(mapped));
        float roughness=clamp(PbrMaps.SampleLevel(RepeatTrilinear,
                                float3(uv,tri.material*3+2),mip).r*material.roughnessScale,
                                .04f,.98f);
        float3 local=shadeSurface(ray,hit,n,base,roughness,material);
        // Bounded, shallow architectural pool (IOR approximately 1.333):
        // a subtle sky reflection is much more convincing than showing the stone
        // basin through a noisy opaque-looking top. Still a single-bounce preview.
        const bool pool=(material.ior>1.28f&&material.ior<1.38f&&
                         material.transmission>.05f);
        const float facing=saturate(dot(-ray.direction,normalize(n)));
        const float edge=pow(1.f-facing,3.f);
        if(pool) {
            Ray reflected;
            // Real reflected scene geometry is now queried, not sky-only.
            float3 g=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
            if(dot(g,ray.direction)>0.f)g=-g;
            reflected.origin=ray.origin+ray.direction*hit.t+g*.06f;
            reflected.direction=reflect(ray.direction,n);
            float3 reflectedLight=secondaryRadiance(reflected);
            local=lerp(local,reflectedLight,.27f+.33f*edge);
        }
        float t=saturate(material.transmission);
        if(t<=.05f) {
            local+=reusableBounce(ray,hit,n,base,material.metallic,pixelCoord);
            accumulated+=throughput*local;
            return float4(accumulated,1);
        }
        // Straight-through glass: let the ray see interior faces and their lighting.
        // Fresnel-like edge tint. Refraction and multi-bounce GI are not simulated.
        float glassEdge=pow(1.f-facing,5.f);
        float opacity=pool?saturate(.88f+.08f*edge):
                            saturate((1.f-t)*.68f+.06f+.20f*glassEdge);
        accumulated+=throughput*local*opacity;
        throughput*=1.f-opacity;
        ray.origin=ray.origin+ray.direction*(hit.t+.03f);
        if(throughput<.025f)return float4(accumulated,1);
    }
    accumulated+=throughput*background(ray);
    return float4(accumulated,1);
}
[numthreads(8,8,1)]
void CSMain(uint3 tid:SV_DispatchThreadID) {
    if(tid.x>=width||tid.y>=height)return;
    // Skip BVH traversal and all PBR/GI work for 3/4 of primary pixels.
    // Alpha 0 marks missing pixels for post-readback reconstruction.
    if(primaryQuarter!=0u && ((tid.x&1u)!=0u||(tid.y&1u)!=0u)) {
        Output[tid.xy]=float4(0.f,0.f,0.f,0.f);
        return;
    }
    float2 uv=((float2(tid.xy)+.5f)/float2(width,height))*2.f-1.f;
    Ray ray;ray.origin=eye;
    // Matches CPU: tanHalfFov is HORIZONTAL, vertical half-FOV = tanHalfFov/aspect.
    ray.direction=normalize(forward+right*uv.x*tanHalfFov-
                            up*uv.y*(tanHalfFov/aspect));
    Output[tid.xy]=float4(encodeHdr(tracePrimary(ray,tid.xy).rgb),1.f);
}
