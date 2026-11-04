// Three-seat Energetik cinema row for inspecting the wood, upholstery and shared mount.
const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, seatPreviewFloor: 2, seatFabricClean: 16, seatTrim: 3, metal: 18, seatSteel: 30, wood: 4, seatWood: 8
});
const M3D_SCENE_LIGHTS = [];

function makeScene() {
  objects.length = 0;
  M3D_SCENE_LIGHTS.length = 0;
  box(-3, -0.14, -11, 3, -0.02, -5, 'seatPreviewFloor');

  // Three adjacent chairs, centered on the default Qt camera.
  const seatCenters = [-0.82, 0, 0.82];
  for (const centerX of seatCenters) {
    buildEnergetikCinemaSeat(
      { objects, box, capsule, ellipse, roundBox },
      centerX, 0, -8,
      { folded: true, fabric: 'seatFabricClean' }
    );
  }

  // Shared tubular mounting rail and the visible wooden uprights of the row.
  capsule([-1.25, 0.13, -7.73], [1.25, 0.13, -7.73], 0.038, 'seatSteel');
  for (const postX of [-1.23, -0.41, 0.41, 1.23]) {
    roundBox(postX - 0.045, 0.08, -7.81, postX + 0.045, 0.52, -7.72, 0.025, 'seatWood');
  }

  M3D_SCENE_LIGHTS.push([-1.1, 2.8, -9.2, 6.5, 0.8, 255, 235, 215]);
  M3D_SCENE_LIGHTS.push([1.6, 2.1, -6.5, 3.5, 0.55, 205, 216, 230]);
}
