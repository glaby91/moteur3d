#include <vulkan/vulkan.h>
#include "common.h"
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdlib>
#define VK(expr) do {VkResult r=(expr);if(r!=VK_SUCCESS){fprintf(stderr,"%s failed: %d\n",#expr,int(r));return 20;}}while(0)
int main(int argc,char** argv){
 if(argc!=3)return 2;
 auto setup=Clock::now();
 uint32_t ec=0;VK(vkEnumerateInstanceExtensionProperties(nullptr,&ec,nullptr));
 std::vector<VkExtensionProperties> ie(ec);VK(vkEnumerateInstanceExtensionProperties(nullptr,&ec,ie.data()));
 std::vector<const char*> enabled;VkInstanceCreateFlags flags=0;
 for(auto& e:ie)if(!strcmp(e.extensionName,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)){
  enabled.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
 }
 VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};ai.pApplicationName="CSE WARP Apple feasibility";ai.apiVersion=VK_API_VERSION_1_3;
 VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ii.pApplicationInfo=&ai;ii.flags=flags;ii.enabledExtensionCount=uint32_t(enabled.size());ii.ppEnabledExtensionNames=enabled.data();
 VkInstance inst;VK(vkCreateInstance(&ii,nullptr,&inst));
 uint32_t count=0;VK(vkEnumeratePhysicalDevices(inst,&count,nullptr));
 if(!count){puts("NO_VULKAN_DEVICE");return 21;}
 std::vector<VkPhysicalDevice> devices(count);VK(vkEnumeratePhysicalDevices(inst,&count,devices.data()));
 VkPhysicalDevice p=VK_NULL_HANDLE;uint32_t family=0;
 for(auto candidate:devices){
  VkPhysicalDeviceProperties props;vkGetPhysicalDeviceProperties(candidate,&props);
  printf("device=%s vendor=0x%x type=%d api=%u.%u.%u\n",props.deviceName,props.vendorID,int(props.deviceType),VK_API_VERSION_MAJOR(props.apiVersion),VK_API_VERSION_MINOR(props.apiVersion),VK_API_VERSION_PATCH(props.apiVersion));
  if(props.apiVersion<VK_API_VERSION_1_3||props.vendorID!=0x106b||props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
  uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,nullptr);
  std::vector<VkQueueFamilyProperties> q(n);vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,q.data());
  for(uint32_t j=0;j<n;j++)if(q[j].queueCount&&(q[j].queueFlags&VK_QUEUE_COMPUTE_BIT)){p=candidate;family=j;break;}
  if(p)break;
 }
 if(!p){puts("NO_APPLE_VULKAN_1_3_COMPUTE_DEVICE");return 22;}
 VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
 VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props2.pNext=&driver;vkGetPhysicalDeviceProperties2(p,&props2);
 printf("driver=%s info=%s driverID=%d\n",driver.driverName,driver.driverInfo,int(driver.driverID));
 if(props2.properties.apiVersion<VK_MAKE_API_VERSION(0,1,3,204)){puts("ENGINE_API_MINIMUM_NOT_MET");return 23;}
 uint32_t de=0;VK(vkEnumerateDeviceExtensionProperties(p,nullptr,&de,nullptr));
 std::vector<VkExtensionProperties> ex(de);VK(vkEnumerateDeviceExtensionProperties(p,nullptr,&de,ex.data()));
 std::vector<const char*> dex;for(auto& e:ex)if(!strcmp(e.extensionName,"VK_KHR_portability_subset"))dex.push_back("VK_KHR_portability_subset");
 float priority=1;
 VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
 VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;di.enabledExtensionCount=uint32_t(dex.size());di.ppEnabledExtensionNames=dex.data();
 VkDevice d;VK(vkCreateDevice(p,&di,nullptr,&d));VkQueue q;vkGetDeviceQueue(d,family,0,&q);
 VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=N*4;bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
 VkBuffer b;VK(vkCreateBuffer(d,&bi,nullptr,&b));VkMemoryRequirements req;vkGetBufferMemoryRequirements(d,b,&req);
 VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(p,&mp);uint32_t mem=UINT32_MAX;
 for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((req.memoryTypeBits&(1u<<i))&&(mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){mem=i;break;}
 if(mem==UINT32_MAX){puts("NO_COHERENT_HOST_MEMORY");return 24;}
 VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mi.allocationSize=req.size;mi.memoryTypeIndex=mem;
 VkDeviceMemory memory;VK(vkAllocateMemory(d,&mi,nullptr,&memory));VK(vkBindBufferMemory(d,b,memory,0));
 VkDescriptorSetLayoutBinding binding{};binding.binding=0;binding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;binding.descriptorCount=1;binding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
 VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=1;li.pBindings=&binding;
 VkDescriptorSetLayout layout;VK(vkCreateDescriptorSetLayout(d,&li,nullptr,&layout));
 VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&layout;
 VkPipelineLayout pl;VK(vkCreatePipelineLayout(d,&pli,nullptr,&pl));
 std::ifstream file(argv[1],std::ios::binary|std::ios::ate);if(!file)return 25;
 auto bytes=file.tellg();if(bytes<=0||bytes%4!=0)return 25;
 std::vector<uint32_t> code(size_t(bytes)/4);file.seekg(0);if(!file.read((char*)code.data(),bytes))return 25;
 VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.codeSize=code.size()*4;si.pCode=code.data();
 VkShaderModule shader;VK(vkCreateShaderModule(d,&si,nullptr,&shader));
 VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};ci.layout=pl;ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;ci.stage.module=shader;ci.stage.pName="main";
 VkPipeline pipeline;VK(vkCreateComputePipelines(d,VK_NULL_HANDLE,1,&ci,nullptr,&pipeline));
 VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=1;pi.poolSizeCount=1;pi.pPoolSizes=&ps;
 VkDescriptorPool pool;VK(vkCreateDescriptorPool(d,&pi,nullptr,&pool));
 VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=pool;da.descriptorSetCount=1;da.pSetLayouts=&layout;
 VkDescriptorSet ds;VK(vkAllocateDescriptorSets(d,&da,&ds));
 VkDescriptorBufferInfo db{b,0,N*4};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=ds;write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&db;vkUpdateDescriptorSets(d,1,&write,0,nullptr);
 VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpi.queueFamilyIndex=family;VkCommandPool cp;VK(vkCreateCommandPool(d,&cpi,nullptr,&cp));
 VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=cp;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
 VkCommandBuffer cb;VK(vkAllocateCommandBuffers(d,&ca,&cb));
 VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};VK(vkBeginCommandBuffer(cb,&begin));
 vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(cb,VK_PIPELINE_BIND_POINT_COMPUTE,pl,0,1,&ds,0,nullptr);
 vkCmdDispatch(cb,16,16,1);
 VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
 vkCmdPipelineBarrier(cb,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
 VK(vkEndCommandBuffer(cb));VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cb;
 VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence;VK(vkCreateFence(d,&fi,nullptr,&fence));
 printf("pipeline_setup_ms=%.3f\n",ms(setup,Clock::now()));
 for(int i=0;i<6;i++){auto t=Clock::now();VK(vkQueueSubmit(q,1,&submit,fence));VK(vkWaitForFences(d,1,&fence,VK_TRUE,30000000000ull));printf("dispatch=%d wall_ms=%.3f\n",i,ms(t,Clock::now()));VK(vkResetFences(d,1,&fence));}
 void* mapped;VK(vkMapMemory(d,memory,0,N*4,0,&mapped));int result=verify((const uint32_t*)mapped,argv[2]);vkUnmapMemory(d,memory);
 vkDestroyFence(d,fence,nullptr);vkDestroyCommandPool(d,cp,nullptr);vkDestroyDescriptorPool(d,pool,nullptr);vkDestroyPipeline(d,pipeline,nullptr);vkDestroyShaderModule(d,shader,nullptr);vkDestroyPipelineLayout(d,pl,nullptr);vkDestroyDescriptorSetLayout(d,layout,nullptr);vkDestroyBuffer(d,b,nullptr);vkFreeMemory(d,memory,nullptr);vkDestroyDevice(d,nullptr);vkDestroyInstance(inst,nullptr);return result;
}
