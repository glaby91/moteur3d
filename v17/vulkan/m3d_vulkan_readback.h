#pragma once
// GPU readback formats into Qt QImage::Format_ARGB32 on little-endian Windows.
#include <cstddef>
#include <cstdint>
namespace m3d { namespace vkreadback {
enum class PixelBytes : std::uint8_t { Rgba8, Bgra8 };
inline void toArgb32(const std::uint32_t* source, std::uint32_t* dest,
                     std::size_t count, PixelBytes format) noexcept {
    if (format == PixelBytes::Bgra8) {
        // Native BGRA byte order: only enforce opaque alpha, no channel swaps.
        for (std::size_t i=0; i<count; ++i) dest[i]=source[i] | 0xff000000u;
    } else {
        for (std::size_t i=0; i<count; ++i) {
            const std::uint32_t c=source[i];
            dest[i]=0xff000000u | ((c & 255u) << 16) |
                    (c & 0x0000ff00u) | ((c >> 16) & 255u);
        }
    }
}
}}
