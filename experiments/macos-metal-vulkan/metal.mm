#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "common.h"
int main(int argc,char** argv){
 @autoreleasepool {
  if(argc<2)return 2;
  id<MTLDevice> d=MTLCreateSystemDefaultDevice();
  if(!d){puts("NO_METAL_DEVICE");return 10;}
  printf("Metal device=%s registry=%llu Metal4=%d\n",d.name.UTF8String,(unsigned long long)d.registryID,[d supportsFamily:MTLGPUFamilyMetal4]);
  NSError* err=nil;
  NSString* src=@"#include <metal_stdlib>\nusing namespace metal;\nkernel void test(device uint* rgba [[buffer(0)]],uint2 p [[thread_position_in_grid]]){if(p.x>=256||p.y>=256)return;uint b=((p.x/16+p.y/16)&1)*255;rgba[p.y*256+p.x]=p.x|(p.y<<8)|(b<<16)|0xff000000u;}";
  auto start=Clock::now();
  id<MTLLibrary> lib=[d newLibraryWithSource:src options:nil error:&err];
  if(!lib){NSLog(@"Metal library error: %@",err);return 11;}
  id<MTLComputePipelineState> pipeline=[d newComputePipelineStateWithFunction:[lib newFunctionWithName:@"test"] error:&err];
  if(!pipeline){NSLog(@"Metal pipeline error: %@",err);return 12;}
  id<MTLBuffer> b=[d newBufferWithLength:N*4 options:MTLResourceStorageModeShared];
  id<MTLCommandQueue> q=[d newCommandQueue];
  if(!q||!b)return 13;
  printf("pipeline_setup_ms=%.3f\n",ms(start,Clock::now()));
  for(int i=0;i<6;i++){
   auto t=Clock::now();
   id<MTLCommandBuffer> cb=[q commandBuffer];
   id<MTLComputeCommandEncoder> e=[cb computeCommandEncoder];
   [e setComputePipelineState:pipeline];[e setBuffer:b offset:0 atIndex:0];
   [e dispatchThreadgroups:MTLSizeMake(16,16,1) threadsPerThreadgroup:MTLSizeMake(16,16,1)];
   [e endEncoding];[cb commit];[cb waitUntilCompleted];
   if(cb.status!=MTLCommandBufferStatusCompleted){NSLog(@"Metal command error: %@",cb.error);return 14;}
   printf("dispatch=%d wall_ms=%.3f gpu_ms=%.3f\n",i,ms(t,Clock::now()),(cb.GPUEndTime-cb.GPUStartTime)*1000.);
  }
  return verify((const uint32_t*)b.contents,argv[1]);
 }
}
