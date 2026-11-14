// Native 25% primary-ray scheduler regression, hardware-independent.
#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/m3d_primary_quarter.h"
#include <cassert>
#include <cstdint>
#include <vector>
int main() {
 using namespace m3d;
 assert(primaryQuarterSampleCount(4,4)==4);
 assert(primaryQuarterSampleCount(5,3)==6);
 assert(primaryQuarterSampleCount(1,1)==1);
 assert(primaryQuarterSampleCount(0,8)==0);
 for(int h=1;h<=11;++h)for(int w=1;w<=13;++w) {
  std::vector<uint32_t> img(size_t(w)*h,0u);
  uint64_t traced=0;
  for(int y=0;y<h;y+=2)for(int x=0;x<w;x+=2) {
   img[size_t(y)*w+x]=uint32_t((y+1)*10000+(x+1));
   ++traced;
  }
  assert(traced==primaryQuarterSampleCount(w,h));
  reconstructQuarterPixels(img,w,h);
  for(int y=0;y<h;++y)for(int x=0;x<w;++x)
   assert(img[size_t(y)*w+x]==uint32_t(((y&~1)+1)*10000+((x&~1)+1)));
 }
}
