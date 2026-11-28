# macOS Vulkan Command Optimization 002 — experimental campaign

Base: master 3161e852f9f0e44065d099a28b082ad62763bd90, validated Build 7 draw merge + BGRA enabled in all profiles. Actual ARM64 MoltenVK raster, pinned V17.634, Full HD, independent paired references and two repetitions. 25 sessions, 600 measured frames plus warmups, including separate MSAA4 transient tests and driver telemetry. All flags are opt-in; no shader or scene change. 

1. Metal prefill (0 vs 1), argument buffers (1 vs 0), timestamp instrumentation on/off.
2. Batch final mipmap transitions into a single vkCmdPipelineBarrier (keep per-level dependencies).
3. Pair independent material image uploads in one command buffer / vkQueueSubmit / fence, bounded staging memory 128 MiB; test MoltenVK asynchronous submission dispatch separately. Host fence and host-read barrier remain mandatory; no multi-frame offscreen asynchronous pipeline implemented.
4. Transient depth and MSAA color attachments; check exact format/usage/sample support, prefer compatible non-host-visible lazy device memory only for transient attachments; otherwise retain private allocation. Existing subpasses and BY_REGION dependencies retained. MSAA1 and MSAA4 compared separately; report actual samples and active transient flags.
5. Combine candidates only when exact image hashes and predefined performance criteria pass; fresh reference and combined test. No automatic promotion. Driver statistics collected separately with MVK_CONFIG_PERFORMANCE_TRACKING. Runtime options checked against official MoltenVK configuration docs; exact driver support and effect must be verified in logs. Timing on virtual GPU, not physical Apple GPU or presentation FPS.

References: https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Configuration_Parameters.md and https://docs.vulkan.org/samples/latest/samples/performance/render_passes/README.html. No unsafe removal of memory dependencies or changes to fast math. Existing Windows/Linux defaults retained.

## Stability follow-up (28 November 2026)

[Campaign 002 results](https://github.com/glaby91/moteur3d/actions/runs/46099606754/artifacts/15244489259): 23/25 sessions passed; 48/48 exact comparisons with asynchronous submission and grouped uploads. Argument buffers disabled: 46/48, rejected. Transient attachments failed both MSAA4 runs (exit 6). Combined candidate: no significant frame-rate gain. Guard: transient usage is now refused whenever samples >1; normal private attachments retained. This is a conservative fallback, not a root-cause fix. Defaults unchanged. Short 6-session stability test at FHD 1/2/4 compares reference and candidate (uploads + asynchronous driver submit + guarded transient). Manual dispatch retains full campaign. No further optimisation push or merge until validation and explicit prioritisation.
