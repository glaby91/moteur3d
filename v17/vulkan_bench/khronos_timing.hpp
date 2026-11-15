#pragma once
// CSE WARP Vulkan timing probes. Include in the existing Vulkan render loop.
// No implicit vkDeviceWaitIdle. GPU timestamps are resolved after the fence
// of the corresponding frame has signaled (never on the submitting frame).
#include <vulkan/vulkan.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <stdexcept>
namespace warp_vk {
using Clock=std::chrono::steady_clock;
inline double ms(Clock::time_point a,Clock::time_point b) {
 return std::chrono::duration<double,std::milli>(b-a).count();
}
struct Frame {
 unsigned long long index=0;
 double acquire=0,fence=0,record=0,submit=0,present=0,cpuFrame=0,gpu=0;
 bool gpuValid=false;
 unsigned drawCalls=0,triangles=0,barriers=0,copies=0,descriptorUpdates=0;
};
class Csv {
 std::ofstream f;
public:
 explicit Csv(const std::string& path):f(path) {
  if(!f) throw std::runtime_error("Cannot open Vulkan telemetry CSV");
  f<<"frame,acquire_ms,fence_ms,record_ms,submit_ms,present_call_ms,cpu_frame_ms,gpu_ms,gpu_valid,draw_calls,triangles,barriers,copies,descriptor_updates\n";
 }
 void write(const Frame& x) {
  f<<x.index<<','<<x.acquire<<','<<x.fence<<','<<x.record<<','<<x.submit<<','<<x.present<<','<<x.cpuFrame<<','<<x.gpu<<','<<x.gpuValid<<','<<x.drawCalls<<','<<x.triangles<<','<<x.barriers<<','<<x.copies<<','<<x.descriptorUpdates<<'\n';
 }
};
class GpuTimestamps {
 VkDevice device=VK_NULL_HANDLE;
 VkQueryPool pool=VK_NULL_HANDLE;
 float period=0;
 uint32_t slots=0;
public:
 GpuTimestamps(VkDevice d,const VkPhysicalDeviceProperties& props,uint32_t frames)
 :device(d),period(props.limits.timestampPeriod),slots(frames) {
  if(!props.limits.timestampComputeAndGraphics) return;
  VkQueryPoolCreateInfo ci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  ci.queryType=VK_QUERY_TYPE_TIMESTAMP; ci.queryCount=2*slots;
  if(vkCreateQueryPool(device,&ci,nullptr,&pool)!=VK_SUCCESS) pool=VK_NULL_HANDLE;
 }
 ~GpuTimestamps(){if(pool) vkDestroyQueryPool(device,pool,nullptr);}
 bool available() const {return pool!=VK_NULL_HANDLE;}
 void begin(VkCommandBuffer cb,uint32_t slot) {
  if(!available()) return;
  vkCmdResetQueryPool(cb,pool,2*slot,2);
  vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,pool,2*slot);
 }
 void end(VkCommandBuffer cb,uint32_t slot) {
  if(available()) vkCmdWriteTimestamp(cb,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,pool,2*slot+1);
 }
 // Call only after that slot's submission fence is signaled.
 bool resolve(uint32_t slot,double& gpuMs) {
  if(!available()) return false;
  uint64_t ticks[2]={0,0};
  auto rc=vkGetQueryPoolResults(device,pool,2*slot,2,sizeof(ticks),ticks,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);
  if(rc!=VK_SUCCESS || ticks[1]<ticks[0]) return false;
  gpuMs=double(ticks[1]-ticks[0])*double(period)/1e6;
  return true;
 }
};
} // namespace warp_vk
