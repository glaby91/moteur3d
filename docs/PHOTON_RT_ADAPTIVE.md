# PHOTON-RT adaptive sampling — offline prototype

This experiment is integrated into the CSE WARP repository **without changing the native renderer**.

It compares uniform 25% sampling with adaptive 25% sampling of **existing real CSE WARP rendered PNGs**. The adaptive budget includes a 6.25% pilot pass, followed by 18.75% importance-weighted samples. Reconstruction uses nearest sampled pixel. It writes comparison PNGs, metrics.csv and report.json.

Install: `python -m pip install numpy scipy pillow scikit-image`

Run: `python tools/photon_rt_adaptive.py path/to/render1.png path/to/render2.png --output photon-rt-results`

This is **not** an implementation of adaptive ray tracing in the V16/V17 native engine. The source PNGs already contain the full rendered image. FPS, wattage, GPU/CPU ray counts, and temporal reconstruction cannot be inferred from this offline test. Native integration requires changes to the ray scheduling path, and actual depth/motion/material buffers for temporal validation.

Research provenance: V14 image-domain experiment against V17.343 GitHub Actions WARP render outputs, 12 frames at 480x270. The initial local V14 results used a smoothed Sobel importance guide; this repository version uses gradient importance and is a related, not pixel-identical, reproduction.
