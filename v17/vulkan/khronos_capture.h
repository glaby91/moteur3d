#pragma once
// CSE WARP V17.32 Khronos experiment: low-overhead per-frame CSV telemetry.
// Used by the ORIGINAL VulkanRaster, not a substitute renderer.
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace m3d { namespace khronos {
struct Sample {
    double total=0, setup=0, swapchain=0, targets=0, rebuild=0;
    double shadow=0, gi=0, uniforms=0, acquire=0, record=0;
    double submit=0, fence=0, present=0, readback=0, presentInterval=0;
    double gpuDraw=0, gpuTransfer=0, gpuTotal=0;
    unsigned width=0,height=0,swapchainImages=0,msaa=0;
    unsigned objects=0,lights=0;
    uint64_t draws=0,boxInstances=0,sceneRevision=0;
    bool direct=false, rebuilt=false,gpuValid=false;
    const char* presentMode="OFFSCREEN";
};
class CsvWriter {
    std::ofstream out_;
    uint64_t frame_=0;
public:
    CsvWriter(){
        const char* path=std::getenv("CSE_WARP_VK_KHRONOS_CSV");
        if(!path||!*path)return;
        try {
            const std::filesystem::path file(path);
            if(file.has_parent_path())std::filesystem::create_directories(file.parent_path());
            out_.open(file,std::ios::binary|std::ios::trunc);
            if(out_) {
                out_<<"frame,width,height,direct,swapchain_images,present_mode,msaa,objects,lights,"
                    <<"scene_revision,scene_rebuilt,draw_calls,box_instances,"
                    <<"cpu_total_ms,cpu_setup_ms,cpu_swapchain_ms,cpu_targets_ms,"
                    <<"cpu_rebuild_ms,cpu_shadow_ms,cpu_gi_ms,cpu_uniform_ms,"
                    <<"cpu_acquire_ms,cpu_record_ms,cpu_submit_ms,cpu_fence_wait_ms,"
                    <<"cpu_present_call_ms,cpu_readback_ms,present_interval_ms,"
                    <<"gpu_draw_ms,gpu_transfer_ms,gpu_total_ms,gpu_valid\n";
                out_<<std::fixed<<std::setprecision(6);
            }
        }catch(const std::exception& e){
            std::clog<<"[CSE WARP Khronos] CSV unavailable: "<<e.what()<<"\n";
        }
    }
    void write(const Sample& s){
        if(!out_)return;
        out_<<frame_++<<','<<s.width<<','<<s.height<<','<<int(s.direct)<<','
            <<s.swapchainImages<<','<<s.presentMode<<','<<s.msaa<<','
            <<s.objects<<','<<s.lights<<','<<s.sceneRevision<<','
            <<int(s.rebuilt)<<','<<s.draws<<','<<s.boxInstances<<','
            <<s.total<<','<<s.setup<<','<<s.swapchain<<','<<s.targets<<','
            <<s.rebuild<<','<<s.shadow<<','<<s.gi<<','<<s.uniforms<<','
            <<s.acquire<<','<<s.record<<','<<s.submit<<','<<s.fence<<','
            <<s.present<<','<<s.readback<<','<<s.presentInterval<<','
            <<s.gpuDraw<<','<<s.gpuTransfer<<','<<s.gpuTotal<<','
            <<int(s.gpuValid)<<'\n';
        if(frame_%60==0)out_.flush();
    }
};
inline void capture(const Sample& sample){
    static thread_local CsvWriter csv;
    csv.write(sample);
}
}} // namespace m3d::khronos
