// Vulkan geometry parity check: GPU-independent CPU regression.
// All failures must be actionable in GitHub Actions logs (including the return code).
#include "m3d_engine.h"
#include "m3d_vulkan_mesh.h"
#include <cmath>
#include <exception>
#include <iostream>
int main() {
    try {
        std::cout << "Starting GPU-independent Vulkan mesh test" << std::endl;
        m3d::Scene scene;
        scene.mat.resize(2);
        scene.mat[1].name = "glass";
        scene.mat[1].transmissionPercent = 65;
        m3d::Obj t{};
        t.kind = m3d::Kind::Triangle;
        t.a = {-1,0,-2}; t.b = {0,1,-2}; t.c = {1,0,-2};
        t.n0 = t.n1 = t.n2 = {0,0,1};
        t.uv0 = {0,0,0}; t.uv1 = {.5f,1,0}; t.uv2 = {1,0,0};
        t.mn = {-1,0,-2}; t.mx = {1,1,-2};
        scene.obj.push_back(t);
        auto glass = t; glass.mat = 1; scene.obj.push_back(glass);
        m3d::Obj bx{}; bx.kind = m3d::Kind::Box;
        bx.mn = {1,0,-3}; bx.mx = {2,2,-2}; scene.obj.push_back(bx);
        m3d::Obj ell{}; ell.kind = m3d::Kind::Ellipse;
        ell.a = {-2,1,-2}; ell.b = {1,1,1};
        ell.mn = {-3,0,-3}; ell.mx = {-1,2,-1}; scene.obj.push_back(ell);
        std::cout << "Input: " << scene.obj.size() << " objects, "
                  << scene.mat.size() << " materials" << std::endl;
        const auto mesh = m3d::vkmesh::build(scene);
        std::cout << "Output: " << mesh.vertices.size() << " opaque vertices, "
                  << mesh.transparent.size() << " transparent triangles" << std::endl;
        if (mesh.vertices.size() < 39 || (mesh.vertices.size() % 3) ||
            mesh.transparent.size() != 1 || mesh.vertices.size() > 12000000u) {
            std::cerr << "FAIL [1] unexpected mesh counts" << std::endl;
            return 1;
        }
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            const auto& v = mesh.vertices[i];
            for (float x : v.p) {
                if (!std::isfinite(x)) {
                    std::cerr << "FAIL [2] non-finite vertex position at index " << i << std::endl;
                    return 2;
                }
            }
            if (v.material >= scene.mat.size()) {
                std::cerr << "FAIL [3] invalid material " << v.material
                          << " at index " << i << std::endl;
                return 3;
            }
        }
        if (mesh.transparent.front().v[0].material != 1) {
            std::cerr << "FAIL [4] incorrect transparent material: "
                      << mesh.transparent.front().v[0].material << std::endl;
            return 4;
        }
        std::cout << "PASS Vulkan shared mesh tessellation" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL [90] mesh test threw exception: " << e.what() << std::endl;
        return 90;
    } catch (...) {
        std::cerr << "FAIL [91] mesh test threw unknown exception" << std::endl;
        return 91;
    }
}
