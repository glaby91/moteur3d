#include "m3d_engine.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace m3d {
bool avx2FmaAvailable() noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int regs[4]{};
    __cpuidex(regs, 1, 0);
    constexpr int osxsave = 1 << 27, avx = 1 << 28, fma = 1 << 12;
    if ((regs[2] & (osxsave | avx | fma)) != (osxsave | avx | fma)) return false;
    if ((_xgetbv(0) & 0x6) != 0x6) return false;
    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 5)) != 0;
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return false;
#endif
}
}
