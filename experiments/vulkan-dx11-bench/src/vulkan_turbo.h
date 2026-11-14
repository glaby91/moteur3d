// CSE WARP Vulkan turbo A/B research, Vulkan 1.0 + Windows. Included after VkBench.
// All modes are experimental; NONE is a V17 raster backend.
class VkTurboBench {
 struct Buf {
  VkBuffer buf=VK_NULL_HANDLE;
  VkDeviceMemory memory=VK_NULL_HANDLE;
  VkMemoryPropertyFlags flags=0;
  void* mapped=nullptr;
  VkDeviceSize allocationSize=0;
  uint32_t type=0;
 };
 struct Slot {
  Buf output,readback;
  VkCommandBuffer cmd=VK_NULL_HANDLE;
  VkFence fence=VK_NULL_HANDLE;
  bool pending=false;
  int index=-1;
  Clock::time_point begin{};
  double recordMs=0,submitMs=0;
 };
 VkInstance instance=VK_NULL_HANDLE;
 VkPhysicalDevice physical=VK_NULL_HANDLE;
 VkDevice device=VK_NULL_HANDLE;
 VkQueue queue=VK_NULL_HANDLE;
 uint32_t family=0,validBits=0;
 float timestampPeriod=0;
 VkPhysicalDeviceMemoryProperties memoryProperties{};
 VkPhysicalDeviceProperties physicalProperties{};
 VkDescriptorSetLayout descriptorLayout=VK_NULL_HANDLE;
 VkDescriptorPool descriptorPool=VK_NULL_HANDLE;
 VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;
 VkPipeline pipeline=VK_NULL_HANDLE;
 VkCommandPool commandPool=VK_NULL_HANDLE;
 VkQueryPool queryPool=VK_NULL_HANDLE;
 std::vector<VkDescriptorSet> descriptors;
 std::vector<Slot> slots;
 std::vector<uint32_t> pixels;
 Params params{};
 std::string mode;
 bool reuse=true, softwareAllowed=false;
 double batchMs=0;
 uint64_t lastHash=0;
 int lastSlot=0;
 uint32_t bufferBytes=0;

 static double elapsed(Clock::time_point a,Clock::time_point b){
  return std::chrono::duration<double,std::milli>(b-a).count();
 }
 uint32_t chooseMemory(uint32_t bits,VkMemoryPropertyFlags required,bool preferCache,bool preferLocal) {
  int score=-1;uint32_t chosen=UINT32_MAX;
  for(uint32_t i=0;i<memoryProperties.memoryTypeCount;i++) {
   if((bits&(1u<<i))==0)continue;
   VkMemoryPropertyFlags f=memoryProperties.memoryTypes[i].propertyFlags;
   if((f&required)!=required)continue;
   // Cached host reads take priority over coherent write-combined mappings.
   int s=((preferCache&&(f&VK_MEMORY_PROPERTY_HOST_CACHED_BIT))?128:0)
       +((preferLocal&&(f&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))?64:0)
       +((f&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)?8:0);
   if(s>score){score=s;chosen=i;}
  }
  must(chosen!=UINT32_MAX,"No Vulkan memory type compatible with required flags");
  return chosen;
 }
 Buf alloc(VkDeviceSize bytes,VkBufferUsageFlags usage,VkMemoryPropertyFlags required,bool cache,bool local){
  Buf b;
  VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bc.size=bytes;bc.usage=usage;bc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  vkc(vkCreateBuffer(device,&bc,nullptr,&b.buf),"Turbo vkCreateBuffer");
  VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device,b.buf,&req);
  b.type=chooseMemory(req.memoryTypeBits,required,cache,local);
  b.flags=memoryProperties.memoryTypes[b.type].propertyFlags;
  b.allocationSize=req.size;
  VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  a.allocationSize=req.size;a.memoryTypeIndex=b.type;
  vkc(vkAllocateMemory(device,&a,nullptr,&b.memory),"Turbo vkAllocateMemory");
  vkc(vkBindBufferMemory(device,b.buf,b.memory,0),"Turbo vkBindBufferMemory");
  if(b.flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
   vkc(vkMapMemory(device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped),"Turbo persistent vkMapMemory");
  return b;
 }
 void release(Buf& b){
  if(b.mapped)vkUnmapMemory(device,b.memory);
  if(b.buf)vkDestroyBuffer(device,b.buf,nullptr);
  if(b.memory)vkFreeMemory(device,b.memory,nullptr);
  b={};
 }
 void invalidate(Buf& b){
  if(b.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)return;
  VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
  r.memory=b.memory;r.offset=0;r.size=VK_WHOLE_SIZE;
  vkc(vkInvalidateMappedMemoryRanges(device,1,&r),"Turbo vkInvalidateMappedMemoryRanges");
 }
 void record(uint32_t index){
  Slot& s=slots[index];
  vkc(vkResetCommandBuffer(s.cmd,0),"Turbo vkResetCommandBuffer");
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkc(vkBeginCommandBuffer(s.cmd,&begin),"Turbo vkBeginCommandBuffer");
  vkCmdResetQueryPool(s.cmd,queryPool,2*index,2);
  vkCmdBindPipeline(s.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);
  vkCmdBindDescriptorSets(s.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout,0,1,&descriptors[index],0,nullptr);
  vkCmdPushConstants(s.cmd,pipelineLayout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params),&params);
  if(validBits)vkCmdWriteTimestamp(s.cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,queryPool,2*index);
  vkCmdDispatch(s.cmd,(params.w+15)/16,(params.h+15)/16,1);
  if(validBits)vkCmdWriteTimestamp(s.cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,queryPool,2*index+1);
  if(mode!="compute"){
   VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
   b.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
   b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   b.size=VK_WHOLE_SIZE;
   if(mode=="direct"){
    b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;b.buffer=s.output.buf;
    vkCmdPipelineBarrier(s.cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);
   }else{
    b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;b.buffer=s.output.buf;
    vkCmdPipelineBarrier(s.cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&b,0,nullptr);
    VkBufferCopy c{};c.size=bufferBytes;
    vkCmdCopyBuffer(s.cmd,s.output.buf,s.readback.buf,1,&c);
    b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    b.buffer=s.readback.buf;
    vkCmdPipelineBarrier(s.cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);
   }
  }
  vkc(vkEndCommandBuffer(s.cmd),"Turbo vkEndCommandBuffer");
 }
 void submit(uint32_t idx,int sample){
  Slot& s=slots[idx];
  must(!s.pending,"Turbo slot reused before fence");
  auto start=Clock::now();
  if(!reuse)record(idx);
  auto recorded=Clock::now();
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount=1;si.pCommandBuffers=&s.cmd;
  vkc(vkQueueSubmit(queue,1,&si,s.fence),"Turbo vkQueueSubmit");
  s.begin=start;
  s.recordMs=elapsed(start,recorded);
  s.submitMs=elapsed(recorded,Clock::now());
  s.index=sample;s.pending=true;
  lastSlot=int(idx);
 }
 Sample collect(uint32_t idx,bool hashOutput){
  Slot& s=slots[idx];
  must(s.pending,"Turbo empty fence slot");
  auto w0=Clock::now();
  vkc(vkWaitForFences(device,1,&s.fence,VK_TRUE,UINT64_MAX),"Turbo vkWaitForFences");
  auto w1=Clock::now();
  Sample out;out.api="vulkan";out.index=s.index;out.recordMs=s.recordMs;
  out.submitMs=s.submitMs;out.waitMs=elapsed(w0,w1);
  if(mode!="compute"){
   Buf& b=(mode=="direct")?s.output:s.readback;
   auto m0=Clock::now();
   invalidate(b);
   auto m1=Clock::now();
   memcpy(pixels.data(),b.mapped,bufferBytes);
   auto m2=Clock::now();
   out.invalidateMs=elapsed(m0,m1);out.copyCpuMs=elapsed(m1,m2);
   if(hashOutput){out.checksum=fnv(pixels);lastHash=out.checksum;}
  }
  auto done=Clock::now();
  out.wallMs=elapsed(s.begin,done);
  if(validBits){
   uint64_t ts[2]={};
   vkc(vkGetQueryPoolResults(device,queryPool,2*idx,2,sizeof(ts),ts,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT),"Turbo GPU timestamp read");
   uint64_t mask=validBits>=64?UINT64_MAX:((uint64_t(1)<<validBits)-1u);
   uint64_t delta=(ts[1]-ts[0])&mask;
   out.gpuMs=double(delta)*double(timestampPeriod)/1000000.0;
  }
  vkc(vkResetFences(device,1,&s.fence),"Turbo vkResetFences");
  s.pending=false;
  return out;
 }
 uint64_t verifyCompute(){
  // Verification is deliberately OUTSIDE timed compute-only samples.
  // All submissions must be completed before touching the source buffer.
  Slot& slot=slots[size_t(lastSlot)];
  must(!slot.pending,"verifyCompute must follow all fence waits");
  Buf stage=alloc(bufferBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,true,false);
  VkCommandBuffer cmd=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
  VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  a.commandPool=commandPool;a.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;a.commandBufferCount=1;
  vkc(vkAllocateCommandBuffers(device,&a,&cmd),"Turbo verify command buffer");
  VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  vkc(vkCreateFence(device,&fi,nullptr,&fence),"Turbo verify fence");
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  vkc(vkBeginCommandBuffer(cmd,&bi),"Turbo verify begin");
  VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  b.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
  b.buffer=slot.output.buf;b.size=VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&b,0,nullptr);
  VkBufferCopy c{};c.size=bufferBytes;vkCmdCopyBuffer(cmd,slot.output.buf,stage.buf,1,&c);
  b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;b.buffer=stage.buf;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);
  vkc(vkEndCommandBuffer(cmd),"Turbo verify end");
  VkSubmitInfo sub{VK_STRUCTURE_TYPE_SUBMIT_INFO};sub.commandBufferCount=1;sub.pCommandBuffers=&cmd;
  vkc(vkQueueSubmit(queue,1,&sub,fence),"Turbo verify submit");
  vkc(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX),"Turbo verify wait");
  invalidate(stage);memcpy(pixels.data(),stage.mapped,bufferBytes);
  uint64_t checksum=fnv(pixels);
  vkDestroyFence(device,fence,nullptr);vkFreeCommandBuffers(device,commandPool,1,&cmd);
  release(stage);lastHash=checksum;
  return checksum;
 }
public:
 std::string deviceName;uint32_t vendor=0,deviceId=0;bool software=false;
 VkTurboBench(const Params& p,const std::string& spirv,const std::string& selectedMode,int inflight,bool wantReuse,bool allowSoftware)
 :params(p),mode(selectedMode),reuse(wantReuse),softwareAllowed(allowSoftware),pixels(size_t(p.w)*p.h),bufferBytes(uint32_t(size_t(p.w)*p.h*4u)){
  must(mode=="cached"||mode=="direct"||mode=="compute","Invalid turbo memory mode");
  must(inflight>=1&&inflight<=3,"Turbo inflight must be 1..3");
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="CSE WARP Vulkan Turbo A-B";
  app.apiVersion=VK_API_VERSION_1_0;
  VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ii.pApplicationInfo=&app;
  vkc(vkCreateInstance(&ii,nullptr,&instance),"Turbo vkCreateInstance");
  uint32_t n=0;vkc(vkEnumeratePhysicalDevices(instance,&n,nullptr),"Turbo enumerate physical devices");
  must(n>0,"No Vulkan ICD / GPU available");
  std::vector<VkPhysicalDevice> all(n);
  vkc(vkEnumeratePhysicalDevices(instance,&n,all.data()),"Turbo list devices");
  int highest=-1;
  for(auto candidate:all){
   VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(candidate,&props);
   if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU&&!allowSoftware)continue;
   uint32_t count=0;vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,nullptr);
   std::vector<VkQueueFamilyProperties> q(count);
   vkGetPhysicalDeviceQueueFamilyProperties(candidate,&count,q.data());
   for(uint32_t k=0;k<count;k++)if(q[k].queueFlags&VK_QUEUE_COMPUTE_BIT){
    int score=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?100:props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?90:10;
    if(score>highest){highest=score;physical=candidate;family=k;validBits=q[k].timestampValidBits;}
    break;
   }
  }
  must(physical!=VK_NULL_HANDLE,"No compatible Vulkan compute GPU found");
  vkGetPhysicalDeviceProperties(physical,&physicalProperties);
  vkGetPhysicalDeviceMemoryProperties(physical,&memoryProperties);
  deviceName=physicalProperties.deviceName;vendor=physicalProperties.vendorID;
  deviceId=physicalProperties.deviceID;
  software=(physicalProperties.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU);
  timestampPeriod=physicalProperties.limits.timestampPeriod;
  float priority=1.f;VkDeviceQueueCreateInfo qc{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qc.queueFamilyIndex=family;qc.queueCount=1;qc.pQueuePriorities=&priority;
  VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qc;
  vkc(vkCreateDevice(physical,&dc,nullptr,&device),"Turbo vkCreateDevice");
  vkGetDeviceQueue(device,family,0,&queue);
  VkDescriptorSetLayoutBinding binding{};binding.binding=0;
  binding.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;binding.descriptorCount=1;
  binding.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
  VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  dl.bindingCount=1;dl.pBindings=&binding;
  vkc(vkCreateDescriptorSetLayout(device,&dl,nullptr,&descriptorLayout),"Turbo descriptor layout");
  VkDescriptorPoolSize ps{};ps.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;ps.descriptorCount=uint32_t(inflight);
  VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pi.maxSets=uint32_t(inflight);pi.poolSizeCount=1;pi.pPoolSizes=&ps;
  vkc(vkCreateDescriptorPool(device,&pi,nullptr,&descriptorPool),"Turbo descriptor pool");
  descriptors.resize(size_t(inflight),VK_NULL_HANDLE);
  std::vector<VkDescriptorSetLayout> layouts(size_t(inflight),descriptorLayout);
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  ai.descriptorPool=descriptorPool;ai.descriptorSetCount=uint32_t(inflight);ai.pSetLayouts=layouts.data();
  vkc(vkAllocateDescriptorSets(device,&ai,descriptors.data()),"Turbo descriptor sets");
  VkPushConstantRange pc{};pc.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;pc.size=sizeof(Params);
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pli.setLayoutCount=1;pli.pSetLayouts=&descriptorLayout;
  pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&pc;
  vkc(vkCreatePipelineLayout(device,&pli,nullptr,&pipelineLayout),"Turbo pipeline layout");
  auto code=readSpirv(spirv);
  VkShaderModule module=VK_NULL_HANDLE;
  VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  smi.codeSize=code.size()*4;smi.pCode=code.data();
  vkc(vkCreateShaderModule(device,&smi,nullptr,&module),"Turbo shader module");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=module;cp.stage.pName="main";
  cp.layout=pipelineLayout;
  VkResult cr=vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&cp,nullptr,&pipeline);
  vkDestroyShaderModule(device,module,nullptr);vkc(cr,"Turbo compute pipeline");
  VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  ci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;ci.queueFamilyIndex=family;
  vkc(vkCreateCommandPool(device,&ci,nullptr,&commandPool),"Turbo command pool");
  VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=2u*uint32_t(inflight);
  vkc(vkCreateQueryPool(device,&qi,nullptr,&queryPool),"Turbo query pool");
  slots.resize(size_t(inflight));
  for(int i=0;i<inflight;i++){
   Slot& s=slots[size_t(i)];
   if(mode=="direct")
    s.output=alloc(bufferBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,true,true);
   else
    s.output=alloc(bufferBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,false,true);
   if(mode=="cached")
    s.readback=alloc(bufferBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,true,false);
   VkDescriptorBufferInfo info{s.output.buf,0,bufferBytes};
   VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
   wr.dstSet=descriptors[size_t(i)];wr.dstBinding=0;wr.descriptorCount=1;
   wr.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;wr.pBufferInfo=&info;
   vkUpdateDescriptorSets(device,1,&wr,0,nullptr);
   VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
   ca.commandPool=commandPool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
   vkc(vkAllocateCommandBuffers(device,&ca,&s.cmd),"Turbo command buffer");
   VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
   vkc(vkCreateFence(device,&fi,nullptr,&s.fence),"Turbo fence");
   if(reuse)record(uint32_t(i));
  }
  std::cout<<"TURBO mode="<<mode<<" inflight="<<inflight<<" reuse="<<reuse
           <<" GPU="<<deviceName<<" vendor="<<vendor<<" device="<<deviceId<<"\n";
  for(size_t i=0;i<slots.size();i++){
   const Slot& s=slots[i];const Buf& b=(mode=="direct")?s.output:s.readback;
   if(mode!="compute")std::cout<<"TURBO read_slot="<<i<<" type="<<b.type
    <<" flags="<<b.flags
    <<" HOST_CACHED="<<bool(b.flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    <<" HOST_COHERENT="<<bool(b.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    <<" DEVICE_LOCAL="<<bool(b.flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)<<"\n";
  }
 }
 ~VkTurboBench(){
  if(!device){if(instance)vkDestroyInstance(instance,nullptr);return;}
  vkDeviceWaitIdle(device);
  for(Slot& s:slots){if(s.fence)vkDestroyFence(device,s.fence,nullptr);release(s.readback);release(s.output);}
  if(queryPool)vkDestroyQueryPool(device,queryPool,nullptr);
  if(commandPool)vkDestroyCommandPool(device,commandPool,nullptr);
  if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
  if(pipelineLayout)vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
  if(descriptorPool)vkDestroyDescriptorPool(device,descriptorPool,nullptr);
  if(descriptorLayout)vkDestroyDescriptorSetLayout(device,descriptorLayout,nullptr);
  vkDestroyDevice(device,nullptr);
  if(instance)vkDestroyInstance(instance,nullptr);
 }
 Sample run(int index,bool hashOutput){
  submit(0,index);
  Sample s=collect(0,hashOutput&&mode!="compute");
  if(hashOutput&&mode=="compute")s.checksum=verifyCompute();
  return s;
 }
 std::vector<Sample> runBatch(int warmup,int repetitions){
  must(repetitions>=1,"Turbo batch requires repetitions");
  for(int i=0;i<warmup;i++)run(-1,false);
  std::vector<Sample> values;values.reserve(size_t(repetitions));
  auto begin=Clock::now();
  for(int i=0;i<repetitions;i++){
   const uint32_t idx=uint32_t(i%int(slots.size()));
   if(slots[idx].pending)values.push_back(collect(idx,slots[idx].index==repetitions-1&&mode!="compute"));
   submit(idx,i);
  }
  for(uint32_t i=0;i<uint32_t(slots.size());i++)
   if(slots[i].pending)values.push_back(collect(i,slots[i].index==repetitions-1&&mode!="compute"));
  batchMs=elapsed(begin,Clock::now());
  std::sort(values.begin(),values.end(),[](const Sample&a,const Sample&b){return a.index<b.index;});
  if(mode=="compute")verifyCompute();
  return values;
 }
 double batchElapsedMs()const{return batchMs;}
 uint64_t checksum()const{return lastHash;}
};
