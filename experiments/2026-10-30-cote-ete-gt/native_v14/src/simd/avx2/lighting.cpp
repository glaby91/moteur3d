#include "m3d_engine.h"
#include <immintrin.h>
namespace m3d {
namespace { inline __m256 g(const float*p,__m256i i){return _mm256_i32gather_ps(p,i,4);} }
V accumulatePointLightsAvx2(const Light*lights,size_t count,V p,V normal,float scale) noexcept{
 V sum{0,0,0};const float*base=count?reinterpret_cast<const float*>(lights):nullptr;
 size_t i=0;const int stride=int(sizeof(Light)/sizeof(float));
 for(;i+8<=count;i+=8){int o[8];for(int k=0;k<8;k++)o[k]=int((i+size_t(k))*size_t(stride));__m256i idx=_mm256_setr_epi32(o[0],o[1],o[2],o[3],o[4],o[5],o[6],o[7]);
  auto f=[&](int offset){return g(base,_mm256_add_epi32(idx,_mm256_set1_epi32(offset)));};
  __m256 qx=_mm256_sub_ps(f(0),_mm256_set1_ps(p.x)),qy=_mm256_sub_ps(f(1),_mm256_set1_ps(p.y)),qz=_mm256_sub_ps(f(2),_mm256_set1_ps(p.z));
  __m256 d2=_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(qx,qx),_mm256_mul_ps(qy,qy)),_mm256_mul_ps(qz,qz));
  __m256 radius=f(3),valid=_mm256_and_ps(_mm256_cmp_ps(d2,_mm256_mul_ps(radius,radius),_CMP_LT_OQ),_mm256_cmp_ps(d2,_mm256_set1_ps(1e-5f),_CMP_GT_OQ));
  __m256 d=_mm256_sqrt_ps(d2),edge=_mm256_sub_ps(_mm256_set1_ps(1),_mm256_div_ps(d,radius));
  __m256 energy=_mm256_mul_ps(_mm256_mul_ps(_mm256_mul_ps(edge,edge),f(4)),_mm256_set1_ps(scale));
  __m256 invD=_mm256_div_ps(_mm256_set1_ps(1),d);
  __m256 lam=_mm256_max_ps(_mm256_setzero_ps(),_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(qx,_mm256_mul_ps(invD,_mm256_set1_ps(normal.x))),_mm256_mul_ps(qy,_mm256_mul_ps(invD,_mm256_set1_ps(normal.y)))),_mm256_mul_ps(qz,_mm256_mul_ps(invD,_mm256_set1_ps(normal.z)))));
  __m256 power=_mm256_mul_ps(_mm256_mul_ps(energy,lam),_mm256_set1_ps(.27f));power=_mm256_and_ps(power,valid);
  alignas(32)float k[8],cr[8],cg[8],cb[8];_mm256_store_ps(k,power);_mm256_store_ps(cr,_mm256_mul_ps(f(5),power));_mm256_store_ps(cg,_mm256_mul_ps(f(6),power));_mm256_store_ps(cb,_mm256_mul_ps(f(7),power));
  for(int lane=0;lane<8;lane++){sum.x+=cr[lane];sum.y+=cg[lane];sum.z+=cb[lane];}
 }
 for(;i<count;i++){const Light&l=lights[i];V q=l.p-p;float d2=dot(q,q);if(d2>=l.radius*l.radius||d2<1e-5f)continue;float d=std::sqrt(d2),edge=1-d/l.radius,energy=edge*edge*l.intensity*scale,lam=std::max(0.f,dot(normal,q/d)),diff=energy*lam*.27f;sum=sum+l.color*diff;}
 return sum;
}
}
