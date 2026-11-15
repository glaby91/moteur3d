# CSE WARP V17.493 — Photographic celestial hemisphere

Expected optional image (user-approved 2026-11-13):

`assets/sky/CSE_WARP_ciel_nocturne_hemisphere_UV_4096x2048.png`

Source: the user's previously approved 4096x2048 night-sky panorama.
Do not silently replace it with a generated starfield or an unrelated photograph.

## Install

Copy the original PNG into this directory alongside this README, preserving
its filename, or extract the separately supplied `CSE_WARP_SKYDOME_ASSET_V493.zip`
at the same level as `moteur3d_v17.exe`.

The file is not automatically embedded in this source branch because the
original approved image is stored in the user's ChatGPT Library, not GitHub.
If missing, CSE WARP keeps the previous procedural-star fallback without
blocking Vulkan, DirectX 11, or the scene loader.

## Mapping

The image is a 2:1 **hemisphere** panorama, NOT a 360x180 full-sphere HDRI.
Zenith = top row (v=0); horizon = bottom row (v=1); longitude wraps around U.
Left/right edges of the approved texture match. The sky remains infinitely
distant and world-locked; camera translation creates no parallax.
UV longitude rotates with the simulation's 24-hour solar clock; this is an
artistic rotation, not a sidereal simulation calibrated to Pripyat latitude.
Photo fades in at night, fades out around the horizon, and stays masked by
animated cirrus.

Vulkan uploads one dedicated sRGB 2D image with mipmaps when supported.
On HD620 the default GPU texture is 2048x1024, generated once from the
approved 4096x2048 source (much lower shared-memory use). Set
`CSE_WARP_SKYDOME_4K=1` before launching to use the full 4096x2048 GPU image.

GPU sampling occurs in the fullscreen sky pass, without a CPU readback.
The DirectX 11 interactive GUI uses the independent Qt celestial compositor;
standalone DX11 benchmark shaders retain their procedural fallback.
Both preserve the same hemisphere UV mapping and night/cloud fades.

Vulkan resources are freed when the Vulkan context is released.
An image change reuploads once only and waits for in-flight frames.
