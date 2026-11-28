# macOS feasibility — historical compute probe

This paragraph describes the original standalone compute probe, not the subsequent engine port. macOS 26 ARM64 standard runner (no paid XL runner requested). Native Metal and each Vulkan ICD dispatch the same integer compute shader to a 256x256 image, read back every pixel, compare against a CPU reference, save PPM and record cold/warm wall time. Native Metal also records GPU command-buffer duration. Explicitly select each SDK ICD; reject CPU devices/non-Apple devices and Vulkan below the engine minimum 1.3.204. A green workflow only means the diagnostics ran; inspect per-backend exit codes. This is not yet raster, WSI, full engine validation or a representative gaming benchmark. Missing Metal/GPU access is a valid feasibility limitation. SDK 1.4.363.1 from LunarG official CDN. No merge to master.

## Subsequent engine port

Build 4 passed native ARM64 compilation, actual Vulkan raster scene tests and Qt offscreen navigation. The original compute probe is retained independently. See [macOS Vulkan port and validation](../../v17/docs/MACOS_VULKAN.md) for versions, artifacts, limitations and the four-build history.
