// Moteur3D V16 - Windows D3D11 Compute shader, Shader Model 5.0.
// Same scene, projection and PBR mip chains as the CPU renderer.
// Preview shading: direct sun, BVH hard shadows, one straight-through glass path.
// No DXR or physical multi-bounce path tracing; do not call this GPU/CPU PBR parity.
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
    float metallic; float transmission; float ior; float pad;
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
};
StructuredBuffer<GpuBvhNode> Nodes : register(t0);
StructuredBuffer<GpuTriangle> Triangles : register(t1);
StructuredBuffer<uint> PrimitiveOrder : register(t2);
StructuredBuffer<GpuMaterial> Materials : register(t3);
// Each material owns three slices: color, tangent-space normal, roughness.
Texture2DArray<float4> PbrMaps : register(t4);
SamplerState RepeatTrilinear : register(s0);
RWTexture2D<float4> Output : register(u0);

float3 background(Ray ray) {
    float t=saturate(ray.direction.y*.90f+.25f);
    return saturate(lerp(skyHorizon,skyZenith,t)*skyStrength);
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
float3 shadeSurface(Ray ray,HitInfo hit,float3 n,float3 color,float roughness,float metallic) {
    GpuTriangle tri=Triangles[hit.primitive];
    float3 p=ray.origin+ray.direction*hit.t;
    if(dot(n,ray.direction)>0)n=-n;
    float3 L=normalize(sunDirection);
    float diffuse=saturate(dot(n,L));
    if(diffuse>.0001f) {
        // Use the unperturbed triangle normal for ray offsets. Bump-map normals
        // can point underneath a surface and cause false dark patches ("shadow acne").
        float3 geometryNormal=normalize(cross(tri.p1-tri.p0,tri.p2-tri.p0));
        if(dot(geometryNormal,ray.direction)>0.f)geometryNormal=-geometryNormal;
        Ray shadow;
        shadow.origin=p+geometryNormal*.035f+L*.025f;
        shadow.direction=L;
        HitInfo blocker;
        // Exclude the originating triangle and tiny adjacent coplanar hits.
        const uint state=traceClosest(shadow,1000.f,true,hit.primitive,blocker);
        if(state==1)diffuse*=.35f;
    }
    float3 lit=color*(ambientStrength+diffuse*sunStrength*(1.f-.55f*metallic));
    float3 viewDir=-ray.direction;
    float3 reflected=reflect(-L,n);
    float spec=pow(saturate(dot(reflected,viewDir)),lerp(90.f,8.f,roughness));
    float3 specular=lerp(float3(.18f,.18f,.18f),color,metallic);
    lit+=specular*spec*(.2f+.55f*(1.f-roughness))*sunStrength;
    return saturate(lit);
}
float4 tracePrimary(Ray ray) {
    float3 accumulated=float3(0,0,0);
    float throughput=1.f;
    [loop] for(uint layer=0;layer<6;++layer) {
        HitInfo hit;
        uint status=traceClosest(ray,farPlane,false,0xffffffffu,hit);
        if(status==2)return float4(1,0,1,1);
        if(status==0) {
            accumulated+=background(ray)*throughput;
            return float4(saturate(accumulated),1);
        }
        GpuTriangle tri=Triangles[hit.primitive];
        GpuMaterial material=Materials[tri.material];
        float3 n=normalize(tri.n0*(1-hit.bary.x-hit.bary.y)+
                           tri.n1*hit.bary.x+tri.n2*hit.bary.y);
        float2 uv=textureCoordinates(tri,hit.bary);
        float mip=estimateMip(tri,hit.t);
        float3 base=PbrMaps.SampleLevel(RepeatTrilinear,float3(uv,tri.material*3),mip).rgb;
        float3 mapped=PbrMaps.SampleLevel(RepeatTrilinear,float3(uv,tri.material*3+1),mip).xyz*2.f-1.f;
        if(dot(mapped,mapped)>.25f)n=tangentSpaceNormal(tri,n,normalize(mapped));
        float roughness=saturate(PbrMaps.SampleLevel(RepeatTrilinear,
                                float3(uv,tri.material*3+2),mip).r*material.roughnessScale);
        float3 local=shadeSurface(ray,hit,n,base,roughness,material.metallic);
        float t=saturate(material.transmission);
        if(t<=.05f) {
            accumulated+=throughput*local;
            return float4(saturate(accumulated),1);
        }
        // Straight-through glass: let the ray see interior faces and their lighting.
        // Fresnel-like edge tint. Refraction and multi-bounce GI are not simulated.
        float facing=saturate(dot(-ray.direction,normalize(n)));
        float edge=pow(1.f-facing,5.f);
        float opacity=saturate((1.f-t)*.68f+.06f+.20f*edge);
        accumulated+=throughput*local*opacity;
        throughput*=1.f-opacity;
        ray.origin=ray.origin+ray.direction*(hit.t+.03f);
        if(throughput<.025f)return float4(saturate(accumulated),1);
    }
    accumulated+=throughput*background(ray);
    return float4(saturate(accumulated),1);
}
[numthreads(8,8,1)]
void CSMain(uint3 tid:SV_DispatchThreadID) {
    if(tid.x>=width||tid.y>=height)return;
    float2 uv=((float2(tid.xy)+.5f)/float2(width,height))*2.f-1.f;
    Ray ray;ray.origin=eye;
    // Matches CPU: tanHalfFov is HORIZONTAL, vertical half-FOV = tanHalfFov/aspect.
    ray.direction=normalize(forward+right*uv.x*tanHalfFov-
                            up*uv.y*(tanHalfFov/aspect));
    Output[tid.xy]=tracePrimary(ray);
}
