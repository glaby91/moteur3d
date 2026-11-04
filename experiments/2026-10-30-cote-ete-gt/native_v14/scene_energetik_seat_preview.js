// Minimal scene for inspecting or benchmarking the reusable cinema seat.
const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, seatPreviewFloor: 2, seatFabricClean: 1, seatTrim: 2, metal: 18, seatSteel: 34, wood: 4, seatWood: 8
});
const M3D_SCENE_LIGHTS = [];

function makeScene() {
  objects.length = 0;
  M3D_SCENE_LIGHTS.length = 0;
  // The default Qt camera looks at (0, 1, -8), so center the seat there. This
  // makes the preview visible immediately without adding a camera feature.
  box(-3, -0.14, -11, 3, -0.02, -5, 'seatPreviewFloor');
  buildEnergetikCinemaSeat({ objects, box, capsule, ellipse, roundBox }, 0, 0, -8, { folded: true });
  M3D_SCENE_LIGHTS.push([-1.1, 2.8, -9.2, 5.5, 0.8, 255, 226, 198]);
  M3D_SCENE_LIGHTS.push([1.6, 2.1, -6.5, 3.0, 0.55, 176, 199, 255]);
}
