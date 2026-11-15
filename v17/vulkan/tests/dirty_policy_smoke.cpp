#include "../vulkan_dirty_policy.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace m3d::vkdirty;
 int scene=1;
 Key baseline{&scene,42,0,0,0,0,256,8};
 assert(classify({},baseline)==Update::Full);
 assert(classify(baseline,baseline)==Update::None);
 Key cam=baseline; // camera moves do not invalidate GPU resources
 assert(classify(baseline,cam)==Update::None);
 Key geom=baseline;geom.revision++;geom.geometryRevision++;
 assert(classify(baseline,geom)==Update::Geometry);
 Key legacy=baseline;legacy.revision++;
 assert(classify(baseline,legacy)==Update::Full);
 Key mats=baseline;mats.revision++;mats.materialsRevision++;
 assert(classify(baseline,mats)==Update::Full);
 Key param=baseline;param.revision++;param.parametersRevision++;
 assert(classify(baseline,param)==Update::None);
 Key paramThenGeometry=param;paramThenGeometry.revision++;paramThenGeometry.geometryRevision++;
 assert(classify(baseline,paramThenGeometry)==Update::Geometry);
 Key count=baseline;count.objects++;
 assert(classify(baseline,count)==Update::Geometry);
 Key identity=baseline;identity.identity++;
 assert(classify(baseline,identity)==Update::Full);
 Key untracked=baseline;untracked.identity=0;
 assert(classify(baseline,untracked)==Update::Full);
 Key changedMatCount=baseline;changedMatCount.materials++;
 assert(classify(baseline,changedMatCount)==Update::Full);
 Key previousGeom=geom;
 Key nextGeom=previousGeom;nextGeom.revision++;nextGeom.geometryRevision++;
 assert(classify(previousGeom,nextGeom)==Update::Geometry);
 Key mixed=previousGeom;mixed.revision+=2;mixed.geometryRevision++;mixed.materialsRevision++;
 assert(classify(previousGeom,mixed)==Update::Full);
 std::cout<<"Khronos incremental invalidation checks passed\n";
}
