#include "m3d_engine.h"
#include <immintrin.h>
namespace m3d {
V sampleRgb565Avx2(uint16_t a,uint16_t b,uint16_t c,uint16_t d,float w0,float w1,float w2,float w3) noexcept{
 const __m256 weights=_mm256_setr_ps(w0,w0,w1,w1,w2,w2,w3,w3);
 const __m256 rg=_mm256_setr_ps(float(a>>11),float((a>>5)&63),float(b>>11),float((b>>5)&63),float(c>>11),float((c>>5)&63),float(d>>11),float((d>>5)&63));
 alignas(32) float blended[8];_mm256_store_ps(blended,_mm256_mul_ps(rg,weights));
 const __m256 blue=_mm256_setr_ps(float(a&31),float(b&31),float(c&31),float(d&31),0,0,0,0);
 const __m256 blueWeights=_mm256_setr_ps(w0,w1,w2,w3,0,0,0,0);alignas(32)float bsum[8];_mm256_store_ps(bsum,_mm256_mul_ps(blue,blueWeights));
 const float r=(blended[0]+blended[2]+blended[4]+blended[6])*255.f/31.f;
 const float g=(blended[1]+blended[3]+blended[5]+blended[7])*255.f/63.f;
 const float bl=(bsum[0]+bsum[1]+bsum[2]+bsum[3])*255.f/31.f;
 return{std::floor(r+.5f),std::floor(g+.5f),std::floor(bl+.5f)};
}
}
