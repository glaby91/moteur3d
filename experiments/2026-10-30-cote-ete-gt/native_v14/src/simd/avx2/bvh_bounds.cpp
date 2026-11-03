#include "m3d_engine.h"
#include <immintrin.h>
namespace m3d {
void boundsHitPairAvx2(const Ray&r,const Node&a,const Node&b,float limit,float*outNear,bool*outHit) noexcept{
 const float org[8]={r.o.x,r.o.y,r.o.z,r.o.x,r.o.y,r.o.z,0,0};
 float inv[8],mn[8],mx[8],parallel[8];
 const float dir[8]={r.d.x,r.d.y,r.d.z,r.d.x,r.d.y,r.d.z,1,1};
 const float av[6]={a.mn.x,a.mn.y,a.mn.z,a.mx.x,a.mx.y,a.mx.z};
 const float bv[6]={b.mn.x,b.mn.y,b.mn.z,b.mx.x,b.mx.y,b.mx.z};
 bool outside[2]={false,false};
 for(int lane=0;lane<6;lane++){
  const int ax=lane%3,which=lane/3;
  const float origin=org[lane],d=dir[lane];const float lo=which?bv[ax]:av[ax],hi=which?bv[3+ax]:av[3+ax];
  inv[lane]=std::abs(d)<1e-12f?0.f:1.f/d;parallel[lane]=std::abs(d)<1e-12f?-1.f:0.f;mn[lane]=lo;mx[lane]=hi;
  if(parallel[lane]!=0.f&&(origin<lo||origin>hi))outside[which]=true;
 }
 inv[6]=inv[7]=0;parallel[6]=parallel[7]=-1;mn[6]=mn[7]=0;mx[6]=mx[7]=0;
 const __m256 originV=_mm256_loadu_ps(org),inverseV=_mm256_loadu_ps(inv),mnV=_mm256_loadu_ps(mn),mxV=_mm256_loadu_ps(mx);
 const __m256 t0=_mm256_mul_ps(_mm256_sub_ps(mnV,originV),inverseV),t1=_mm256_mul_ps(_mm256_sub_ps(mxV,originV),inverseV);
 __m256 nearV=_mm256_min_ps(t0,t1),farV=_mm256_max_ps(t0,t1);
 const __m256 pm=_mm256_loadu_ps(parallel);
 nearV=_mm256_blendv_ps(nearV,_mm256_set1_ps(-INF),pm);farV=_mm256_blendv_ps(farV,_mm256_set1_ps(INF),pm);
 alignas(32) float nearLane[8],farLane[8];_mm256_store_ps(nearLane,nearV);_mm256_store_ps(farLane,farV);
 for(int which=0;which<2;which++){
  const int k=which*3;float lo=std::max(0.f,std::max(nearLane[k],std::max(nearLane[k+1],nearLane[k+2])));
  float hi=std::min(limit,std::min(farLane[k],std::min(farLane[k+1],farLane[k+2])));
  outNear[which]=lo;outHit[which]=!outside[which]&&hi>=lo&&hi>=0.f;
 }
}
}
