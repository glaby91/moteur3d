#pragma once
// CSE WARP V17.312 — offline, scene-dependent, multi-hour irradiance table.
// No ray tracing during navigation. Written once, loaded/reused by D3D11.
#include "m3d_engine.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
namespace m3d::gibake {
struct Options {
    unsigned nx=12,ny=6,nz=12,rays=16,bounces=2;
    unsigned frames=25;
    float firstHour=7.f,stepHours=.5f;
};
struct Table {
    unsigned nx=0,ny=0,nz=0,frames=0;
    float firstHour=7.f,stepHours=.5f;
    V origin{},span{};
    uint64_t sceneHash=0;
    // Frames in increasing local solar hour, then z/y/x, 4 floats per voxel:
    // RGB = bounced light and visible sky irradiance in linear space;
    // A = fraction of sky directions visible at that probe.
    std::vector<float> rgba;
    size_t voxelCount()const{return size_t(nx)*ny*nz;}
    bool valid()const{return nx>0&&ny>0&&nz>0&&frames>0 &&
        rgba.size()==voxelCount()*size_t(frames)*4u;}
};
uint64_t signature(const Scene& scene);
bool bake(const Scene& scene,const Options& opt,Table& out,std::string& error);
bool writeFile(const std::filesystem::path& path,const Table& table,std::string& error);
bool readFile(const std::filesystem::path& path,Table& table,std::string& error);
void hourIndices(const Table& table,float hour,unsigned& lo,unsigned& hi,float& weight);
V sample(const Table& table,V point,float hour);
} // namespace m3d::gibake
