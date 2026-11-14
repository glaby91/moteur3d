#pragma once
#include <cstdint>
namespace m3d { namespace vkmips {
constexpr uint32_t levelCount(uint32_t side) noexcept {
    uint32_t n=0;while(side){++n;side>>=1;}return n;
}
constexpr uint32_t extent(uint32_t side,uint32_t level) noexcept {
    if(!side)return 0;while(level-- && side>1)side>>=1;return side;
}
constexpr uint64_t texelsPerLayer(uint32_t side) noexcept {
    uint64_t n=0;while(side){n+=uint64_t(side)*side;side>>=1;}return n;
}
}}
