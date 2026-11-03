#include "m3d_engine.h"
#include <immintrin.h>
namespace m3d {
void primaryRays8Avx2(const Camera&c,int y,int x,int w,int h,float ox,float oy,Ray*out) noexcept{
 const __m256i lanes=_mm256_setr_epi32(0,1,2,3,4,5,6,7);
 const __m256 px=_mm256_add_ps(_mm256_cvtepi32_ps(lanes),_mm256_set1_ps(float(x)+ox));
 const __m256 rx=_mm256_mul_ps(_mm256_sub_ps(_mm256_mul_ps(px,_mm256_set1_ps(2.f/w)),_mm256_set1_ps(1.f)),_mm256_set1_ps(c.tanHalf));
 const float ry=(1.f-2.f*(float(y)+oy)/h)*c.tanHalf/c.aspect;
 const __m256 dx=_mm256_add_ps(_mm256_add_ps(_mm256_set1_ps(c.f.x),_mm256_mul_ps(rx,_mm256_set1_ps(c.r.x))),_mm256_set1_ps(c.u.x*ry));
 const __m256 dy=_mm256_add_ps(_mm256_add_ps(_mm256_set1_ps(c.f.y),_mm256_mul_ps(rx,_mm256_set1_ps(c.r.y))),_mm256_set1_ps(c.u.y*ry));
 const __m256 dz=_mm256_add_ps(_mm256_add_ps(_mm256_set1_ps(c.f.z),_mm256_mul_ps(rx,_mm256_set1_ps(c.r.z))),_mm256_set1_ps(c.u.z*ry));
 const __m256 len=_mm256_sqrt_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx,dx),_mm256_mul_ps(dy,dy)),_mm256_mul_ps(dz,dz)));
 alignas(32) float a[8],b[8],d[8];_mm256_store_ps(a,_mm256_div_ps(dx,len));_mm256_store_ps(b,_mm256_div_ps(dy,len));_mm256_store_ps(d,_mm256_div_ps(dz,len));
 for(int k=0;k<8;k++)out[k]={c.eye,{a[k],b[k],d[k]}};
}
}
