// Moteur3D - D3D11 compute ray tracer, phase 1: primary triangle visibility.
// Shader Model 5.0; no DXR dependency. This is a GPU kernel, not yet wired to Qt.
// Coordinates and BVH layout MUST be converted by the C++ upload layer.
// Pixel output: surface normal visualization for CPU/GPU parity testing.

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
cbuffer Frame : register(b0) {
    float3 eye; float tanHalfFov;
    float3 forward; float aspect;
    float3 right; float nearPlane;
    float3 up; float farPlane;
    uint width; uint height; uint nodeCount; uint triangleCount;
    uint rootIndex; uint maxTraversal; uint reserved0; uint reserved1;
};
StructuredBuffer<GpuBvhNode> Nodes : register(t0);
StructuredBuffer<GpuTriangle> Triangles : register(t1);
StructuredBuffer<uint> PrimitiveOrder : register(t2);
RWTexture2D<float4> Output : register(u0);

bool intersectBounds(Ray ray, float3 bmin, float3 bmax, float tLimit) {
    float tmin = max(nearPlane, 0.0001f);
    float tmax = tLimit;
    [unroll] for (uint axis = 0; axis < 3; ++axis) {
        float o = ray.origin[axis], d = ray.direction[axis];
        if (abs(d) < 1e-8f) {
            if (o < bmin[axis] || o > bmax[axis]) return false;
        } else {
            float a = (bmin[axis] - o) / d;
            float b = (bmax[axis] - o) / d;
            tmin = max(tmin, min(a,b));
            tmax = min(tmax, max(a,b));
            if (tmax < tmin) return false;
        }
    }
    return true;
}
bool intersectTriangle(Ray ray, GpuTriangle tri, out float t, out float2 uv) {
    float3 e1 = tri.p1 - tri.p0;
    float3 e2 = tri.p2 - tri.p0;
    float3 p = cross(ray.direction, e2);
    float det = dot(e1,p);
    t = 0; uv = 0;
    if (abs(det) < 1e-8f) return false;
    float inv = rcp(det);
    float3 s = ray.origin - tri.p0;
    float u = dot(s,p) * inv;
    if (u < 0 || u > 1) return false;
    float3 q = cross(s,e1);
    float v = dot(ray.direction,q) * inv;
    if (v < 0 || u+v > 1) return false;
    t = dot(e2,q) * inv;
    uv = float2(u,v);
    return t >= max(nearPlane, 0.0001f) && t <= farPlane;
}
// Fixed stack deliberately fails visibly (magenta) on overflow;
// no silent missed triangles. BVH leaf count is independently bounded.
float4 tracePrimary(Ray ray) {
    if (nodeCount == 0 || rootIndex >= nodeCount) return float4(0.02,0.03,0.06,1);
    uint stack[64];
    uint sp = 0;
    stack[sp++] = rootIndex;
    float closest = farPlane;
    float3 normal = 0;
    bool hit = false;
    bool overflow = false;
    uint visits = 0;
    [loop] while (sp > 0) {
        if (++visits > maxTraversal) { overflow = true; break; }
        uint nodeIndex = stack[--sp];
        if (nodeIndex >= nodeCount) { overflow = true; break; }
        GpuBvhNode node = Nodes[nodeIndex];
        if (!intersectBounds(ray,node.boundsMin,node.boundsMax,closest)) continue;
        if (node.count > 0) {
            [loop] for (uint i=0; i<node.count; ++i) {
                uint primIndex = PrimitiveOrder[node.first+i];
                if (primIndex >= triangleCount) { overflow=true; break; }
                GpuTriangle tri = Triangles[primIndex];
                float t; float2 uv;
                if (intersectTriangle(ray,tri,t,uv) && t < closest) {
                    closest = t;
                    normal = normalize(tri.n0*(1-uv.x-uv.y)+tri.n1*uv.x+tri.n2*uv.y);
                    hit = true;
                }
            }
        } else {
            if (sp+2 > 64) { overflow=true; break; }
            stack[sp++] = node.left;
            stack[sp++] = node.right;
        }
        if (overflow) break;
    }
    if (overflow) return float4(1,0,1,1);
    if (!hit) return float4(0.02,0.03,0.06,1);
    return float4(normal*0.5f+0.5f,1);
}
[numthreads(8,8,1)]
void CSMain(uint3 tid : SV_DispatchThreadID) {
    if (tid.x >= width || tid.y >= height) return;
    float2 uv = ((float2(tid.xy)+0.5f)/float2(width,height))*2.0f-1.0f;
    Ray ray;
    ray.origin = eye;
    ray.direction = normalize(forward + right*uv.x*tanHalfFov*aspect - up*uv.y*tanHalfFov);
    Output[tid.xy] = tracePrimary(ray);
}
