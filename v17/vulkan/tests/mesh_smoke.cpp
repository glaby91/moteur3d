// Vulkan geometry parity check: GPU-independent CPU regression.
#include "m3d_engine.h"
#include "m3d_vulkan_mesh.h"
#include <cmath>
#include <iostream>
int main(){
    m3d::Scene scene;scene.mat.resize(2);
    scene.mat[1].name="glass";scene.mat[1].transmissionPercent=65;
    m3d::Obj t{};t.kind=m3d::Kind::Triangle;
    t.a={-1,0,-2};t.b={0,1,-2};t.c={1,0,-2};
    t.n0=t.n1=t.n2={0,0,1};
    t.uv0={0,0,0};t.uv1={.5f,1,0};t.uv2={1,0,0};
    t.mn={-1,0,-2};t.mx={1,1,-2};
    scene.obj.push_back(t);
    auto glass=t;glass.mat=1;scene.obj.push_back(glass);
    m3d::Obj bx{};bx.kind=m3d::Kind::Box;bx.mn={1,0,-3};bx.mx={2,2,-2};scene.obj.push_back(bx);
    m3d::Obj ell{};ell.kind=m3d::Kind::Ellipse;ell.a={-2,1,-2};ell.b={1,1,1};
    ell.mn={-3,0,-3};ell.mx={-1,2,-1};scene.obj.push_back(ell);
    auto mesh=m3d::vkmesh::build(scene);
    if(mesh.vertices.size()<3+36 || mesh.vertices.size()%3 ||
       mesh.transparent.size()!=1 || mesh.vertices.size()>12000000u){
        std::cerr<<"Invalid Vulkan geometry output\n";return 1;
    }
    for(const auto& v:mesh.vertices){
        for(float x:v.p)if(!std::isfinite(x))return 2;
        if(v.material>=scene.mat.size())return 3;
    }
    if(mesh.transparent.front().v[0].material!=1)return 4;
    std::cout<<"PASS Vulkan shared mesh tessellation. Opaque vertices="
             <<mesh.vertices.size()<<" transparent triangles="
             <<mesh.transparent.size()<<"\n";
    return 0;
}
