# Versioning CSE WARP V17

The engine release, renderer components, scene and CI run are separate identifiers.

- Engine release: `engine.major` and `engine.build` in `v17/version.json`. The first release under this scheme is `17.001`.
- Component revisions: PBR, CPU lighting, DirectX 11 and Vulkan are tracked independently in `components`. Their initial revision is `001`.
- Scene release: `sceneVersion` in each scene manifest. The current civic benchmark scene is `17.604` (`V17.604` in the UI).
- Official build: the main Windows shipping workflow records the GitHub Actions run number separately from the product version and includes the commit SHA in package metadata. A run number is a CI identifier; it does not replace the engine version.

Keep versions as strings in user-visible metadata so leading zeroes are preserved. Increment the engine release for a complete engine revision, the relevant component revision for a focused renderer change, and the scene release only when the scene changes.
