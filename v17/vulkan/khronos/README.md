# CSE WARP Vulkan V17.32 — Khronos frame pipeline diagnosis

The source of this experiment is the actual engine commit `d7c2118906214e989dd88a6077b575fa38b1f0ea`, GitHub Actions run [43726838117](https://github.com/glaby91/moteur3d/actions/runs/43726838117). Everything happens on a separate experiment branch, without changing master or DX11.

## What is real
- `v17_vulkan_khronos_bench.exe` creates a **real Win32 HWND** and invokes the unchanged public engine entry `m3d::renderVulkanDirectPresent()` in the instrumented VulkanRaster.
- The original depth prepass, sky, opaque pipeline, sRGB, GPU-to-GPU swapchain blit, semaphore and fence logic remain in use.
- The synthetic scene has 1–256 cubes, 1–64 **real active shader point lights** (experimental uniform array and all shader stages enlarged to 64), and fixed/orbit/translation camera paths.
- Special `spin` scenario rotates twelve triangle faces and **intentionally changes scene revision every frame**, separating expensive scene/texture rebuild from pure camera movement.
- Existing renderer timestamp pool expanded to measure draw pass versus transfer/blit separately, after its existing blocking submission fence. CPU timers separately measure scene rebuild, shadows, GI, uniforms, acquire, command recording, submit, fence wait, CPU pixel conversion, and vkQueuePresentKHR call.
- Present interval measured between returned frame submissions. **It is not the actual display scanout interval**. PresentMon can complement the analysis.
- Swapchain image count 2/3 and present FIFO/MAILBOX are requested only; actual values are recorded in CSV. Vulkan may cap image count or lack MAILBOX.
- This benchmark deliberately preserves the original single command buffer, single fence and synchronous GPU wait. It is **a baseline to measure and isolate the bottleneck**, not a claim that Khronos-recommended multi-frame overlap is already implemented.

## Run on actual HD 620
1. Download and fully extract GitHub Actions `CSE-WARP-V17.32-KHRONOS-INSTRUMENTED-HD620` ZIP.
2. Keep the laptop plugged in and avoid resize/movement of the Windows benchmark window.
3. Double-click `LANCER_KHRONOS_HD620.bat`. It runs 12 scenarios x 3 repeats x 20 seconds (~12 minutes plus initialization).
4. Send `KHRONOS_V17_32_HD620_YYYYMMDD_HHMMSS.zip`; it contains a CSV **for every frame**, `manifest.csv`, `summary.csv`, GPU/driver metadata and per-case stdout/stderr logs.
5. Quick check: `powershell -ExecutionPolicy Bypass -File RUN_KHRONOS_HD620.ps1 -Seconds 5 -Repetitions 1 -WarmupSeconds 1`.

## Interpreting the CSV
- `cpu_fence_wait_ms` large with `gpu_total_ms` small: likely blocking CPU/GPU synchronization.
- `cpu_acquire_ms` or `cpu_present_call_ms` large: WSI/refresh pacing suspected; compare FIFO/MAILBOX and 2/3 images.
- `gpu_draw_ms` grows with lights/cubes: shader/geometry bottleneck.
- `gpu_transfer_ms` large: GPU-to-swapchain blit/transfer may warrant optimization.
- `cpu_rebuild_ms` high in spin but low in orbit: resource recreation not camera raster itself.
- `cpu_total_ms` includes initialization or scene construction in first frames; automated summary discards a **warmup window**.
- PresentMode/active swapchainImages and MSAA are logged per frame; always compare actual state rather than just requested state.

## Scope / important limitations
The 64-light pipeline is **experimental**; it must not be merged into master without performance and visual validation. The benchmark runs on the actual GPU, not Github-hosted Microsoft Basic Render Driver. CI validates compilation/shader compilation, not Intel hardware performance. CPU/GPU timestamp support is reported, not fabricated. No existing Vulkan runtime feature uses NV/AMD-specific extensions. Do not interpret GPU start/end timestamps as total panel latency.

Upstream guidance: https://docs.vulkan.org/samples/latest/samples/performance/wait_idle/README.html and https://docs.vulkan.org/samples/latest/samples/performance/swapchain_images/README.html
