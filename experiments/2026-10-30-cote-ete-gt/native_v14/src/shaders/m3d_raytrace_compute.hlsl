// Moteur3D V16: Direct3D 11 Compute ray-tracing preview.
// Phase 2: visible material colors, basic lighting and sky.
// This is NOT full PBR (no texture sampling, secondary reflections, refraction or shadows).
// GPU layout must match the C++ upload structures in m3d_compute11.cpp.
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
};
struct GpuMaterial {
    float3 albedo; float roughness;
    float metallic; float transmission; float ior; float pad;
};
cbuffer Frame : register(b0) {
    float3 eye; float tanHalfFov;
    float3 forward; float aspect;
    float3 right; float nearPlane;
    float3 up; float farPlane;
    uint width; uint height; uint nodeCount; uint triangleCount;
    uint rootIndex; uint maxTraversal; uint materialCount; uint reserved1;
    float3 sunDirection; float ambientStrength;
    float3 skyZenith; float skyStrength;
    float3 skyHorizon; float sunStrength;
};
StructuredBuffer<GpuBvhNode> Nodes : register(t0);
StructuredBuffer<GpuTriangle> Triangles : register(t1);
StructuredBuffer<uint> PrimitiveOrder : register(t2);
StructuredBuffer<GpuMaterial> Materials : register(t3);
RWTexture2D<float4> Output : register(u0);

float3 background(Ray ray) {
    float t = saturate(ray.direction.y * 0.75f + 0.45f);
    return saturate(lerp(skyHorizon, skyZenith, t) * skyStrength);
}
bool intersectBounds(Ray ray,float3 bmin,float3 bmax,float tLimit) {
    float tmin = max(nearPlane, 0.0001f);
    float tmax = tLimit;
    [unroll] for (uint axis = 0;axis < 3;++axis) {
        float o = ray.origin[axis], d = ray.direction[axis];
        if (abs(d) < 1e-8f) {
            if (o < bmin[axis] || o > bmax[axis]) return false;
        } else {
            float a=(bmin[axis]-o)/d;
            float b=(bmax[axis]-o)/d;
            tmin=max(tmin,min(a,b));
            tmax=min(tmax,max(a,b));
            if(tmax<tmin) return false;
        }
    }
    return true;
}
bool intersectTriangle(Ray ray,GpuTriangle tri,out float t,out float2 uv) {
    float3 e1=tri.p1-tri.p0;
    float3 e2=tri.p2-tri.p0;
    float3 p=cross(ray.direction,e2);
    float det=dot(e1,p);
    t=0;uv=float2(0,0);
    if(abs(det)<1e-8f) return false;
    float inv=rcp(det);
    float3 s=ray.origin-tri.p0;
    float u=dot(s,p)*inv;
    if(u<0||u>1) return false;
    float3 q=cross(s,e1);
    float v=dot(ray.direction,q)*inv;
    if(v<0||u+v>1) return false;
    t=dot(e2,q)*inv;
    uv=float2(u,v);
    return t>=max(nearPlane,0.0001f)&&t<=farPlane;
}
float4 tracePrimary(Ray ray) {
    if(nodeCount==0||rootIndex>=nodeCount) return float4(background(ray),1);
    uint stack[64];
    uint sp=0;
    stack[sp++]=rootIndex;
    float closest=farPlane;
    float3 normal=float3(0,1,0);
    uint materialIndex=0;
    bool hit=false,overflow=false;
    uint visits=0;
    [loop] while(sp>0) {
        if(++visits>maxTraversal){overflow=true;break;}
        uint nodeIndex=stack[--sp];
        if(nodeIndex>=nodeCount){overflow=true;break;}
        GpuBvhNode node=Nodes[nodeIndex];
        if(!intersectBounds(ray,node.boundsMin,node.boundsMax,closest))continue;
        if(node.count>0) {
            [loop] for(uint i=0;i<node.count;++i) {
                uint primIndex=PrimitiveOrder[node.first+i];
                if(primIndex>=triangleCount){overflow=true;break;}
                GpuTriangle tri=Triangles[primIndex];
                float t;float2 uv;
                if(intersectTriangle(ray,tri,t,uv)&&t<closest) {
                    closest=t;
                    normal=normalize(tri.n0*(1-uv.x-uv.y)+tri.n1*uv.x+tri.n2*uv.y);
                    materialIndex=tri.material;
                    hit=true;
                }
            }
        } else {
            if(sp+2>64){overflow=true;break;}
            stack[sp++]=node.left;
            stack[sp++]=node.right;
        }
        if(overflow)break;
    }
    // Magenta is reserved for actual invalid geometry/traversal (not normal shading).
    if(overflow||materialIndex>=materialCount&&hit)return float4(1,0,1,1);
    if(!hit)return float4(background(ray),1);
    GpuMaterial material=Materials[materialIndex];
    float3 n=normalize(normal);
    if(dot(n,ray.direction)>0)n=-n;
    float3 L=normalize(sunDirection);
    float diffuse=saturate(dot(n,L));
    float roughness=saturate(material.roughness);
    float metallic=saturate(material.metallic);
    float3 base=saturate(material.albedo);
    // Simple illuminated albedo preview; the engine's complete PBR is still CPU only.
    float3 color=base*(ambientStrength+diffuse*sunStrength*(1.0f-0.55f*metallic));
    float3 viewDir=-ray.direction;
    float3 reflected=reflect(-L,n);
    float specPower=lerp(90.f,8.f,roughness);
    float spec=pow(saturate(dot(reflected,viewDir)),specPower);
    float3 specColor=lerp(float3(.20f,.20f,.20f),base,metallic);
    color+=specColor*spec*(0.2f+0.55f*(1.f-roughness))*sunStrength;
    if(material.transmission>0.05f) {
        // Tinted glass/water preview, not physical transparency.
        float facing=pow(1.f-saturate(dot(n,viewDir)),5.f);
        float3 reflectedSky=lerp(skyHorizon,skyZenith,saturate(reflect(ray.direction,n).y*.5f+.5f));
        color=lerp(color,base*.40f+reflectedSky*.60f,
                   saturate(material.transmission)*(.32f+.35f*facing));
    }
    return float4(saturate(color),1);
}
[numthreads(8,8,1)]
void CSMain(uint3 tid:SV_DispatchThreadID){
    if(tid.x>=width||tid.y>=height)return;
    float2 uv=((float2(tid.xy)+.5f)/float2(width,height))*2.f-1.f;
    Ray ray;
    ray.origin=eye;
    ray.direction=normalize(forward+right*uv.x*tanHalfFov*aspect-up*uv.y*tanHalfFov);
    Output[tid.xy]=tracePrimary(ray);
}
