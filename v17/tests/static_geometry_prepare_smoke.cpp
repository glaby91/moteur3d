#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_static_geometry_prepare.h"

#include <cstdint>
#include <iostream>

namespace {
struct Vertex {
    float p[3];
    float n[3];
    float uv[2];
    std::uint32_t material;
};
static_assert(sizeof(Vertex)==36,"fixture layout must have no padding");

Vertex v(float x,float y,float z,float u,float vv,std::uint32_t material=2) {
    return {{x,y,z},{0,1,0},{u,vv},material};
}

bool check(bool condition,const char* message) {
    if(!condition)std::cerr<<message<<"\n";
    return condition;
}
}

int main() {
    const Vertex a=v(0,0,0,0,0),b=v(1,0,0,1,0),c=v(1,0,1,1,1),d=v(0,0,1,0,1);
    const std::vector<Vertex> source={a,b,c,a,c,d};
    const auto off=m3d::static_geometry::prepareStaticMesh(source,false);
    const auto on=m3d::static_geometry::prepareStaticMesh(source,true);
    if(!check(off.valid&&off.vertices.empty()&&off.indices.empty()
              &&source.size()==6,
              "disabled preparation must leave the original stream untouched"))return 1;
    if(!check(on.valid&&on.vertices.size()==4&&on.indices.size()==6,
              "adjacent triangles must reuse identical vertices"))return 2;
    if(!check(on.indices[0]==on.indices[3]&&on.indices[2]==on.indices[4],
              "shared source vertices must map to the same index"))return 3;
    if(!check(on.indices[0]!=on.indices[1]&&on.indices[1]!=on.indices[2],
              "triangle winding and vertex order must be preserved"))return 4;

    // Same position/normal with a different UV is a seam and must remain split.
    const std::vector<Vertex> seam={a,b,c,v(0,0,0,.25f,0),c,d};
    const auto seamMesh=m3d::static_geometry::prepareStaticMesh(seam,true);
    if(!check(seamMesh.valid&&seamMesh.vertices.size()==5,
              "UV seams must not be welded"))return 5;

    // Material identity is part of the full vertex record too.
    const std::vector<Vertex> materials={a,b,c,v(0,0,0,0,0,3),c,d};
    const auto materialMesh=m3d::static_geometry::prepareStaticMesh(materials,true);
    if(!check(materialMesh.valid&&materialMesh.vertices.size()==5,
              "vertices with different materials must not be welded"))return 6;

    if(!check(!m3d::static_geometry::prepareStaticMesh(
                  std::vector<Vertex>{a,b},true).valid,
              "incomplete triangle streams must be rejected"))return 7;
    std::cout<<"static geometry preparation smoke: OK\n";
    return 0;
}
