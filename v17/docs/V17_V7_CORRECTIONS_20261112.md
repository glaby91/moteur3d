# CSE WARP V17.422 — V7 corrective campaign (12 Nov 2026)

## Implemented in independent V7 scene
- V5/V6 preserved; V7 uses new scene JSON, EXE default alias points to V7.
- 22 primitive shrub/canopy instances removed, all 26 detailed GLB trees retained unchanged.
- 26 real tree-pit openings sized from individual rotated trunk meshes, ~0.95–2.05 m inner radii, complete soil rings and recentered trunks.
- 82 affected paving sections retessellated with constrained triangulation, matching red-brick annular border.
- Eight bench access openings, with associated seat, backrest and supports removed (80 remaining seats).
- Exterior entrance staircase widened from 61 to 68.2 m to meet the raised 68 m parvis; eight lateral step transitions.
- Raised mound from 1.2 to 2.7 m and 5 m sculpture scaled to approximately 7.1 m; planted elements adapted to mound relief.
- Closer starting pedestrian camera and mural inspection camera.
- Narrow pool-side stairs (2.4 m) with true continuous handrail meshes; second flight reaches level 2 with cut floor opening and landing.
- All three central rear gallery slabs (including 4.5 m mezzanine) and unsupported transverse guardrail removed to expose the full-height mural; side circulation retained.
- Enlarged fresco uses native 2048x930 pixels, eight 512x465 PNG tiles with exact source pixel reconstruction and custom UV-mapped triangle meshes.
- Windows package keeps memory-corrected V17.421 EXE/DLL and defaults experimental Z-prepass off.

## Automated Windows validation
- V7 generator completed; references, unique object IDs, tree count, eight bench gaps, 26 ring sizes, stair rails, artwork image tiles asserted.
- Source JPG and combined PNG tiles compared exact RGB bytes.
- Actual Windows D3D11 WARP frames at 1366x768 for exterior and 640x360 for new fresco produced.
- UV bug discovered in first screenshot and corrected in a distinct commit.
- Portable ZIP contains all original scenes, V7 manifest, models, scripts, Windows executable and image resources.

## Not yet validated on the user hardware
- Full live walkthrough in every room and around each exterior pillar; Windows engine renders and static-structure tests do not guarantee no navigation trap.
- Real Intel HD620 FPS at MSAA 8x; CI uses WARP software raster rather than hardware HD620.
- Fine architectural placement of every interior gallery and staircase railing.
- Exact appearance of paving detail and all tree roots at every camera angle.
- Higher artwork detail than the provided 2048x930 original; tiling preserves detail but does not invent missing pixels.

## Acceptance and rollback
Launch DEMARRER_CSE_WARP_V7.bat from the complete extracted ZIP. If the walker becomes stuck, choose a saved camera preset. Compare with the preserved V6/V5 scenes rather than deleting old assets. Do not label these final on-device checks as passed without a real interactive navigation campaign.

## Commits
Initial independent V7 scene; validated Windows WARP CI; mural explicit-UV fix; exterior staircase side transition; QA report.


## Final validated delivery (GitHub Actions)
- Run: https://github.com/glaby91/moteur3d/actions/runs/43315114865 — SUCCESS.
- Scene-generator verification and actual D3D11 WARP exterior/mural frames passed.
- Portable Windows artifact id 13988725568 (243 ZIP entries, testzip PASS).
- Final ZIP SHA-256: fb6fda6c1b9c00964b466bb2782b9b6dbe7f3e18eff5b601acd1c2811230ef37
- Final run report: 26 custom pits, 82 paving sections retessellated, eight access gaps, eight lateral parvis transitions, 68.2 m front stairs, zero central rear gallery slabs.
- Mural render checked: original full-width artwork now unobstructed by the deleted mezzanine and cross-gallery rail; safety railing at pool remains.
- No claim of an interactive on-device walkthrough or HD620 benchmark; those remain explicit acceptance tests.


## Fresco sightline repair — final validated revision
- The original V7.422 mural obstruction was two different mechanisms: a **35-part transverse far-end pool barrier** and the **first-level side gallery** at x=28.5m. The latter could still overlay the mosaic from camera 07 even after removing the central rear floor.
- Fixed with a constrained V7 scene upgrade: delete 35 rear pool-end barrier pieces, six inboard right-side gallery slabs, and the obsolete x=28.5m plus redundant old guardrail pieces; retain an outer 11.6m-wide level 3 strip and the stairwell at x=43m.
- Rebuild the right gallery's outboard protective rail as five separated supported segments; the left side gallery and far pool-end opposite guardrails remain.
- Remove the final 22 hanging pool pennants (two wires and 20 fabrics), which still occluded the mosaic after the gallery was opened. The pool water-lane dividers remain.
- Camera 07 now starts from a grounded exterior pool deck position, not the removed upper mezzanine.
- Latest validated WARP test run: https://github.com/glaby91/moteur3d/actions/runs/43321072039 — SUCCESS. Actual images from both camera 07 and frontal mural generated; visual inspection shows the mosaic unobstructed by either gallery or pennants.
- Full Windows artifact id: 13990661584, ZIP SHA-256: e1158477ade6d5cb0fe6ed841bfb4d7e1bd7dd3c72a8c46eb5d686d78165041e; 243 ZIP entries, CRC test PASS.
- Original V5/V6 files remain preserved; V17.421 low-RAM binary remains unchanged.
- Important limitation: the 640x360 and 1366x768 WARP static renders do not constitute a full interactive, on-device collision or navigation test. This milestone solves the visible mural obstruction, not the remaining lighting/water/geometry backlog.
