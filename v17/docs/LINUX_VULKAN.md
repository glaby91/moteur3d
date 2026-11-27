# CSE WARP V17 — Linux native port (functionally validated, software Vulkan)
Target Ubuntu 24.04 x86_64; Vulkan loader and device API >= 1.3.204. Same Vulkan raster/GLSL as Windows, not DirectX11 or LumaTrace. Linux uses GPU offscreen raster and QImage readback; native Linux WSI (X11/Wayland) not ported yet. Z Reflect currently belongs to LumaTrace DX11 and is not available in Vulkan. The Linux package bundles the latest official Reflect scene selected at build time, verified against its published SHA-256 and recorded in SCENE_VERSION.json and BUILD_INFO.json. The updater still offers newer scenes when online. To open the bundled scene directly without a network check, run ./lancer.sh benchmark-civic/scene.json. 
Install runtime: sudo apt install libvulkan1 mesa-vulkan-drivers libgl1 libegl1 libxkbcommon-x11-0 libxcb-cursor0 libpulse0 unzip
Extract tar.gz, then ./lancer.sh [path/to/scene.json]. Vulkan is selected by default if compiled. CPU raster is not substituted automatically. Use Vulkan driver appropriate for GPU (Intel/AMD Mesa or NVIDIA). Software Vulkan is rejected by default; CSE_WARP_VK_ALLOW_SOFTWARE_TEST=1 only allows explicit CI lavapipe testing, not evidence of GPU performance. GitHub compiles with Qt 6.8.3, checks SPIR-V and Vulkan smoke; hardware, native WSI and Linux audio require independent validation. Windows backend defaults are preserved. The Linux package is built from the same engine revision as master.

## Validation record — 2026-11-28

Reference engine: V17.031, Linux build 6, source commit `ad188c9018e17c49f352f0c9370ea5f89b9902ff`, bundled Reflect scene V17.634. [Build and package checks](https://github.com/glaby91/moteur3d/actions/runs/46081559773) · [Package](https://github.com/glaby91/moteur3d/actions/runs/46081559773/artifacts/15236329498).

- Debian 13: Qt/X11 startup, bundled scene loading, CPU direct PBR and CPU GI (4 bounces, completed 16 spp) exercised.
- Ubuntu 24.04: full-scene Vulkan raster, Mesa 25.2.8 llvmpipe (LLVM 20.1.2, device API 1.4.318), software driver explicitly enabled for testing.
- [Successful full-scene validation](https://github.com/glaby91/moteur3d/actions/runs/46083487533) reuses the packaged engine library, compiling only the diagnostic caller. [Reference PNGs and JSON reports](https://github.com/glaby91/moteur3d/actions/runs/46083487533/artifacts/15237877283).

| Case | Wall time |
|---|---:|
| CPU direct PBR, 1 spp | 9714.26 ms |
| Vulkan, first frame | 4901.56 ms |
| Vulkan, second frame | 457.683 ms |
| Vulkan, third frame | 467.494 ms |
| Vulkan, mean of subsequent frames | 462.5885 ms |

Matched scene V17.634, camera `v7_pool_fresco`, 1920×1080, 2048 texture limit and daylight hour 14. Vulkan MSAA 1; anisotropy device default. Timings exclude scene preparation; first frame includes cold renderer initialization. Two warm frames are not a sustained performance benchmark. Software Vulkan is about 21× faster for this particular comparison; CPU ray tracing and raster are not quality-equivalent.

Known visual differences: brighter Vulkan output, weaker projected shadows, different water reflections. Physical-GPU execution, native Linux swapchain presentation, audio and visual parity remain unvalidated. Do not interpret Lavapipe success as hardware validation. No DX11 or LumaTrace GPU support on Linux.
