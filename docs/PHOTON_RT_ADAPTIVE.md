# PHOTON-RT adaptive sampling — offline prototype

This experiment is integrated into the CSE WARP repository **without changing the native renderer**.

It compares uniform 25% sampling with adaptive 25% sampling of **existing real CSE WARP rendered PNGs**. The adaptive budget includes a 6.25% pilot pass, followed by 18.75% importance-weighted samples. Reconstruction uses nearest sampled pixel. It writes comparison PNGs, metrics.csv and report.json.

Install: `python -m pip install numpy scipy pillow scikit-image`

Run: `python tools/photon_rt_adaptive.py path/to/render1.png path/to/render2.png --output photon-rt-results`

This is **not** an implementation of adaptive ray tracing in the V16/V17 native engine. The source PNGs already contain the full rendered image. FPS, wattage, GPU/CPU ray counts, and temporal reconstruction cannot be inferred from this offline test. Native integration requires changes to the ray scheduling path, and actual depth/motion/material buffers for temporal validation.

Research provenance: V14 image-domain experiment against V17.343 GitHub Actions WARP render outputs, 12 frames at 480x270. The initial local V14 results used a smoothed Sobel importance guide; this repository version uses gradient importance and is a related, not pixel-identical, reproduction.

## Native primary rays: opt-in 25% mode

Set `M3D_PRIMARY_RAYS_25=1` in the CSE WARP process environment. The CPU renderer (single sample/pixel only; offline path tracing and DOF stay full-resolution) and the D3D11 Compute primary shader now execute a **real BVH trace only at even-even pixels**. Odd rows and columns are reconstructed after rendering from their even-even donor in the same 2x2 quad. The exact primary count is `ceil(width/2)*ceil(height/2)`, reported by `Stats.primarySamples`; this is exactly 25% for even dimensions. It does **not** mean 75% less total GPU work or 4x FPS because the D3D11 GI prepass, output dispatch, memory transfers and reconstruction still cost time. Reference rendering is unchanged when the variable is absent.

**Important:** this is the native *uniform quarter-primary-ray baseline*, **not yet an image-importance-adaptive ray distribution**. The V14 offline image-domain experiment remains the evidence for adaptive allocations, but must not be misrepresented as a native adaptive implementation. This conservative first phase allows real ray-budget/FPS A/B tests on Windows and comparison to a future genuinely adaptive scheduler. The native output uses simple 2x2 nearest donor (block artifacts); quality improvement is a separate task.

Hardware-independent CTest: `v17_primary_quarter_exact_budget` validates primary ray counts and reconstruction across odd/even image sizes.
