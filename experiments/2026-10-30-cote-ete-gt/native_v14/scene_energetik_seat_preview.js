// Three-seat Energetik cinema row for inspecting worn vinyl and the central mount.
const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, seatPreviewFloor: 2, seatVinylBrown: 28, seatSupportPaint: 12,
  seatTrim: 3, metal: 18, seatSteel: 30, wood: 4, seatWood: 8
});
const M3D_SCENE_LIGHTS = [];

function makeScene() {
  objects.length = 0;
  M3D_SCENE_LIGHTS.length = 0;
  box(-3, -0.14, -11, 3, -0.02, -5, 'seatPreviewFloor');

  // Three chairs match the approved reference; the first has its broken pan missing.
  const seatCenters = [-0.82, 0, 0.82];
  seatCenters.forEach((centerX, index) => {
    buildEnergetikCinemaSeat(
      { objects, box, capsule, ellipse, roundBox },
      centerX, 0, -8,
      { folded: false, fabric: 'seatVinylBrown', trim: 'seatVinylBrown',
        support: 'seatSupportPaint', seatPanMissing: index === 0 }
    );
  });

  // Retain the approved neutral-warm fill; the material fix does not rely on extra lights.
  M3D_SCENE_LIGHTS.push([-1.1, 2.8, -9.2, 6.5, 0.8, 255, 235, 215]);
  M3D_SCENE_LIGHTS.push([1.6, 2.1, -6.5, 3.5, 0.55, 205, 216, 230]);
}
