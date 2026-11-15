# Khronos Vulkan diagnostic — CSE WARP V17

Branch: `experiment/vulkan-khronos-instrumented-261115`. This is an **integration branch**, not a validated Windows executable.

## Intended test scene
Standalone rotating cube (12 triangles), then 64 and 256 instanced cubes; 1, 8, 32, 64 animated point lights. Resolution 1366x768, MSAA 1x baseline. The scene must use the **same Vulkan backend and swapchain** as WARP exploration, not a separate renderer, to make timings representative.

## Instrumentation
`khronos_timing.hpp` provides:
- steady-clock CPU timings: acquire, fence wait, command recording, submit, present call, total frame.
- Vulkan GPU timestamps, allocated per in-flight frame; results only after that frame's fence is signaled.
- CSV schema with draw calls, triangles, barriers, copies, descriptor updates.
- No GPU timestamp is invented when unsupported. GPU elapsed is a command-range measurement, not necessarily full presentation latency.
- Measure present-to-present frame pacing separately (e.g. PresentMon), since vkQueuePresentKHR call duration alone is not display latency.

## Experimental matrix
1. Static camera / cube rotating, 1 light.
2. Camera rotating / cube rotating, 1 light.
3. Camera moving / cube rotating, 1 light.
4. 8 / 32 / 64 lights (keep cube count fixed).
5. 1 / 64 / 256 cubes (keep lights fixed).
6. 2 versus 3 swapchain images, FIFO and MAILBOX only if supported.
7. 1 versus 2 versus 3 frames in flight (fence per slot).
8. Command buffer reuse versus re-record; descriptor updates only when needed.
9. Direct swapchain render versus offscreen + GPU copy, where supported.
10. Validation layers on/off as diagnostic only; report performance with validation off.

Each configuration: 10 s warmup + 60 s measurement; 3 repetitions; randomized or alternating order. Capture median, p95, p99 frame ms, stutter count (>50 ms), CPU/GPU and present cadence. Record GPU, driver, Vulkan API, present mode, image count, build SHA, resolution, VSync, power plan and temperature if available.

## Integration checklist (not yet completed)
- Identify actual WARP Vulkan render loop and add the header.
- Attach begin/end GPU queries to its command buffers; resolve on signaled frame fences.
- Instrument wait/acquire/record/submit/present, and per-frame counters.
- Add synthetic scene through existing scene loader, with controllable camera and light counts.
- Add CLI `--khronos-bench` and automated test runner, CSV/JSON/ZIP collection.
- Compile Windows x64 in GitHub Actions and validate on Intel HD 620.
- Compare against WARP DX11 baseline, same resolution and scene.

**Important:** Current commit provides only the reusable timing primitives and the measurement specification. It does not yet hook into the render loop, create the synthetic scene or produce a runnable benchmark. No performance results are claimed.
