#include "m3d_engine.h"
#include <immintrin.h>

namespace m3d {
namespace {
inline __m256 channel(__m256i p, int shift) {
    const __m256i mask = _mm256_set1_epi32(255);
    return _mm256_cvtepi32_ps(_mm256_and_si256(_mm256_srli_epi32(p, shift), mask));
}
inline __m256 luminance(__m256i p) {
    return _mm256_fmadd_ps(channel(p, 0), _mm256_set1_ps(.114f),
                         _mm256_fmadd_ps(channel(p, 8), _mm256_set1_ps(.587f),
                                         _mm256_mul_ps(channel(p, 16), _mm256_set1_ps(.299f))));
}
inline __m256i roundedChannel(__m256 center, __m256 neighbours) {
    __m256 value = _mm256_fmadd_ps(neighbours, _mm256_set1_ps(.45f),
                                   _mm256_mul_ps(center, _mm256_set1_ps(.55f)));
    value = _mm256_min_ps(_mm256_set1_ps(255.f), _mm256_max_ps(_mm256_setzero_ps(), value));
    return _mm256_cvttps_epi32(_mm256_add_ps(value, _mm256_set1_ps(.5f)));
}
inline uint32_t scalarEdge(uint32_t c, uint32_t n, uint32_t s, uint32_t e, uint32_t west) {
    auto lum=[](uint32_t p){return .299f*((p>>16)&255)+.587f*((p>>8)&255)+.114f*(p&255);};
    const float lc=lum(c),ln=lum(n),ls=lum(s),le=lum(e),lw=lum(west);
    const float lo=std::min({lc,ln,ls,le,lw}),hi=std::max({lc,ln,ls,le,lw});
    if(hi-lo<=std::max(20.f,hi*.15f)) return c;
    uint32_t result=0xff000000u;
    for(int shift:{0,8,16}) {
        const float center=float((c>>shift)&255);
        const float mean=(((n>>shift)&255)+((s>>shift)&255)+((e>>shift)&255)+((west>>shift)&255))*.25f;
        result|=uint32_t(std::lround(center*.55f+mean*.45f))<<shift;
    }
    return result;
}
}

std::vector<uint32_t> antialiasEdges4Avx2(const std::vector<uint32_t>&src,int w,int h,unsigned threads) {
    if(w<3||h<3||src.size()<size_t(w)*h) return src;
    std::vector<uint32_t> out=src;
    std::atomic<int> next{1};
    const int lanes[8]={0,1,2,3,4,5,6,7};
    const __m256i laneOffsets=_mm256_loadu_si256(reinterpret_cast<const __m256i*>(lanes));
    const __m256 quarter=_mm256_set1_ps(.25f), thresholdFloor=_mm256_set1_ps(20.f);
    const __m256 mixCenter=_mm256_set1_ps(.55f), mixNeighbours=_mm256_set1_ps(.45f);
    auto work=[&]{
        for(;;){
            const int y=next.fetch_add(1); if(y>=h-1) break;
            int x=1;
            for(;x+7<w-1;x+=8){
                const int base=y*w+x;
                const __m256i idx=_mm256_add_epi32(_mm256_set1_epi32(base),laneOffsets);
                auto gather=[&](int offset){return _mm256_i32gather_epi32(reinterpret_cast<const int*>(src.data()),_mm256_add_epi32(idx,_mm256_set1_epi32(offset)),4);};
                const __m256i c=gather(0), n=gather(-w), s=gather(w), e=gather(1), west=gather(-1);
                const __m256 lc=luminance(c),ln=luminance(n),ls=luminance(s),le=luminance(e),lw=luminance(west);
                const __m256 lo=_mm256_min_ps(lc,_mm256_min_ps(ln,_mm256_min_ps(ls,_mm256_min_ps(le,lw))));
                const __m256 hi=_mm256_max_ps(lc,_mm256_max_ps(ln,_mm256_max_ps(ls,_mm256_max_ps(le,lw))));
                const __m256 limit=_mm256_max_ps(thresholdFloor,_mm256_mul_ps(hi,_mm256_set1_ps(.15f)));
                const __m256i edge=_mm256_castps_si256(_mm256_cmp_ps(_mm256_sub_ps(hi,lo),limit,_CMP_GT_OQ));
                auto meanChannel=[&](int shift){return _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(channel(n,shift),channel(s,shift)),_mm256_add_ps(channel(e,shift),channel(west,shift))),quarter);};
                __m256i result=_mm256_set1_epi32(int(0xff000000u));
                result=_mm256_or_si256(result,_mm256_slli_epi32(roundedChannel(channel(c,16),meanChannel(16)),16));
                result=_mm256_or_si256(result,_mm256_slli_epi32(roundedChannel(channel(c,8),meanChannel(8)),8));
                result=_mm256_or_si256(result,roundedChannel(channel(c,0),meanChannel(0)));
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(out.data()+base),_mm256_blendv_epi8(c,result,edge));
            }
            for(;x<w-1;x++){const size_t i=size_t(y)*w+x;out[i]=scalarEdge(src[i],src[i-w],src[i+w],src[i+1],src[i-1]);}
        }
    };
    threads=std::max(1u,std::min(threads,unsigned(h-2)));
    if(threads==1) work(); else {std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}
    return out;
}
}
