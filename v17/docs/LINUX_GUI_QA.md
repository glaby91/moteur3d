# Linux GUI QA — V17.031 Build 8

Date: 2026-11-28. Ubuntu 24.04, Xvfb/X11, Mesa Lavapipe, virtual PulseAudio. Scene V17.634; executable from build 8, source `049fdae9f8eaa7824ae8cdd450841a0baeeb4bc6`. Only the diagnostic driver was compiled separately. [Audit run 46088634614](https://github.com/glaby91/moteur3d/actions/runs/46088634614). Five jobs, 26 isolated processes, all exit code 0. The overall green workflow means the audit finished, NOT that every functional check passed. Per-case reports contain failures and skips.

## Camera and renderer results

[Camera evidence](https://github.com/glaby91/moteur3d/actions/runs/46088634614/artifacts/15239724092). Each camera selected in a fresh GUI instance, accepted Vulkan frame at 1000×680. Camera 10 is the startup view; its time includes cold initialization.

| Camera | Frame time (ms) | Result |
|---|---:|---|
| 01 Place et monument | 441.4 | PASS |
| 02 Vue générale du site | 578.6 | PASS |
| 03 Monument vers le palais | 272.6 | PASS |
| 04 Piscine et fresque | 201.5 | PASS |
| 05 Hall intérieur | 182.3 | PASS |
| 06 Coupole extérieur | 246.6 | PASS |
| 07 Coupole intérieur | 127.9 | PASS |
| 08 Coupole arrière | 534.2 | PASS |
| 09 Jardin, butte et façade | 487.9 | PASS |
| Piscine référence V17.031 | 6350.5 (cold) | PASS |

CPU PBR direct 1 spp: 3942 ms; GI 2 bounces 4 spp: 32391.5 ms; GI 4 bounces 4 spp: 33562.4 ms. Each actual frame at 1000×680. Renderer menu synchronization passed. GUI Full HD test labeled PASS but its status still reads 1000×680: **inconclusive**, recheck actual rendered dimensions. Prior build 6 offscreen Full HD remains valid. No sustained performance or physical GPU claim.

## Navigation and audio

- [Navigation evidence](https://github.com/glaby91/moteur3d/actions/runs/46088634614/artifacts/15240391740): crouch, stand, jump and land, Pip-Boy open/close, retain walking preference after anomaly selection/return, RMB zoom and FOV restoration, free mouse look, fullscreen and editor return: PASS.
- [Audio evidence](https://github.com/glaby91/moteur3d/actions/runs/46088634614/artifacts/15240775436): forced station outage -> local WAV automatically, GUI stop PASS. Virtual output capture 1617752 stereo frames (includes startup silence), peak 151 / signed 16-bit, 297772 nonzero samples. Build 8 separate audio smoke validated stereo transitions and restart; physical listening and live stream not tested.

## Graphics and menus

[Graphics evidence](https://github.com/glaby91/moteur3d/actions/runs/46088634614/artifacts/15239892890): manual day/night frames and automatic solar timer PASS. Cloud, exterior lights, hybrid lighting, color balance, filmic, edge AA, exposure and azimuth controls changed UI state; rendered effect not independently established. Distance detail and frustum controls skipped (missing/disabled). 

[Menus evidence](https://github.com/glaby91/moteur3d/actions/runs/46088634614/artifacts/15239719127): both HUD controls, F11, About, camera JSON clipboard, PNG export, anomaly PNG+JSON capture, resource tree, material edit UI and catalogue save PASS. Benchmark menu hidden; not tested.

## Open issues (do not mark fixed)

| ID | Observation | Next verification / work |
|---|---|---|
| LNX-GUI-01 | Raster preset, MSAA, texture anisotropy control and adaptive LOD each fail with `DirectX 11 indisponible sur cet OS.` | Decouple quality settings from DX11; ensure each Vulkan setting is applied or explicitly disabled. Verify actual output / device values. |
| LNX-GUI-02 | GI bake button fails, expects `moteur3d_v17_gibake.exe` | Build/package Linux utility, platform-native path and functional bake smoke. |
| LNX-GUI-03 | Restore anomaly camera test failed; no dialog error logged | Recheck selection/acceptance, camera pose JSON round trip and fresh-frame detection independently. Root cause not proven. |
| LNX-GUI-04 | Open secondary scene test failed; original scene title remained | Check dialog acceptance, exact manifest and scene load completion; compare scene identity and fresh frame. Root cause not proven. |
| LNX-QA-01 | GUI Full HD reported PASS while logged frame remains 1000×680 | Assert exported image dimensions after new render, not just status. |
| LNX-QA-02 | Raster distance detail/frustum not exercised; hidden benchmark not exercised | Enable only supported options and test separately; benchmark native launcher requires review. |

The previous monolithic run timed out (124); camera 2 did not fail again in isolated case. Seven explicit FAIL records in the new functional checks (four Raster, GI launcher, restore anomaly, open secondary scene). No engine changes made in this audit. Preserve distinction between UI state, frame generation and visual equivalence. Vulkan visual differences and physical GPU validation remain open.
