#include "m3d_engine.h"
#include <immintrin.h>
#include <climits>
namespace m3d {
namespace { inline __m256 g(const float*p,__m256i i){return _mm256_i32gather_ps(p,i,4);} }
bool intersectTriangles8Avx2(const Scene&s,const Ray&r,const uint32_t*ids,float limit,float*out) noexcept{
 static_assert(sizeof(Obj)%sizeof(float)==0,"Obj stride alignment");
 const int stride=int(sizeof(Obj)/sizeof(float)),a0=int(offsetof(Obj,a)/4),e10=int(offsetof(Obj,center)/4),e20=int(offsetof(Obj,core)/4);int o[8];
 for(int k=0;k<8;k++){uint64_t v=uint64_t(ids[k])*stride;if(v>uint64_t(INT_MAX-std::max({a0,e10,e20})-2))return false;o[k]=int(v);}
 __m256i idx=_mm256_setr_epi32(o[0],o[1],o[2],o[3],o[4],o[5],o[6],o[7]);const float*base=reinterpret_cast<const float*>(s.obj.data());
 auto f=[&](int b,int c){return g(base,_mm256_add_epi32(idx,_mm256_set1_epi32(b+c)));};
 __m256 ax=f(a0,0),ay=f(a0,1),az=f(a0,2),u1x=f(e10,0),u1y=f(e10,1),u1z=f(e10,2),u2x=f(e20,0),u2y=f(e20,1),u2z=f(e20,2);
 __m256 dx=_mm256_set1_ps(r.d.x),dy=_mm256_set1_ps(r.d.y),dz=_mm256_set1_ps(r.d.z);
 __m256 px=_mm256_sub_ps(_mm256_mul_ps(dy,u2z),_mm256_mul_ps(dz,u2y)),py=_mm256_sub_ps(_mm256_mul_ps(dz,u2x),_mm256_mul_ps(dx,u2z)),pz=_mm256_sub_ps(_mm256_mul_ps(dx,u2y),_mm256_mul_ps(dy,u2x));
 __m256 det=_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(u1x,px),_mm256_mul_ps(u1y,py)),_mm256_mul_ps(u1z,pz)),inv=_mm256_div_ps(_mm256_set1_ps(1.f),det);
 __m256 qx=_mm256_sub_ps(_mm256_set1_ps(r.o.x),ax),qy=_mm256_sub_ps(_mm256_set1_ps(r.o.y),ay),qz=_mm256_sub_ps(_mm256_set1_ps(r.o.z),az);
 __m256 u=_mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(qx,px),_mm256_mul_ps(qy,py)),_mm256_mul_ps(qz,pz)),inv);
 __m256 zx=_mm256_sub_ps(_mm256_mul_ps(qy,u1z),_mm256_mul_ps(qz,u1y)),zy=_mm256_sub_ps(_mm256_mul_ps(qz,u1x),_mm256_mul_ps(qx,u1z)),zz=_mm256_sub_ps(_mm256_mul_ps(qx,u1y),_mm256_mul_ps(qy,u1x));
 __m256 v=_mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx,zx),_mm256_mul_ps(dy,zy)),_mm256_mul_ps(dz,zz)),inv);
 __m256 t=_mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(u2x,zx),_mm256_mul_ps(u2y,zy)),_mm256_mul_ps(u2z,zz)),inv);
 __m256 ad=_mm256_andnot_ps(_mm256_set1_ps(-0.f),det),m=_mm256_cmp_ps(ad,_mm256_set1_ps(1e-10f),_CMP_GE_OQ);
 m=_mm256_and_ps(m,_mm256_cmp_ps(u,_mm256_setzero_ps(),_CMP_GE_OQ));m=_mm256_and_ps(m,_mm256_cmp_ps(u,_mm256_set1_ps(1),_CMP_LE_OQ));m=_mm256_and_ps(m,_mm256_cmp_ps(v,_mm256_setzero_ps(),_CMP_GE_OQ));m=_mm256_and_ps(m,_mm256_cmp_ps(_mm256_add_ps(u,v),_mm256_set1_ps(1),_CMP_LE_OQ));m=_mm256_and_ps(m,_mm256_cmp_ps(t,_mm256_set1_ps(.0001f),_CMP_GT_OQ));m=_mm256_and_ps(m,_mm256_cmp_ps(t,_mm256_set1_ps(limit),_CMP_LT_OQ));
 alignas(32)float ts[8],ms[8];_mm256_store_ps(ts,t);_mm256_store_ps(ms,m);for(int k=0;k<8;k++)out[k]=ms[k]!=0?ts[k]:INF;return true;
}
}
