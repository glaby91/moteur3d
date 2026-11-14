// Three-state Energetik auditorium row: missing pan, raised pan, lowered pan.
// Geometry is shared across the native scene and the Unreal GLB exporter.
const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, seatPreviewFloor: 2, seatVinylBrown: 7, seatSupportPaint: 4,
  seatTrim: 3, metal: 18, seatSteel: 18, wood: 4, seatWood: 5
});
const M3D_SCENE_LIGHTS = [];

function makeScene() {
  objects.length = 0;
  M3D_SCENE_LIGHTS.length = 0;
  box(-2.8, -0.14, -11, 2.8, -0.02, -5, 'seatPreviewFloor');

  const seatCenters = [-0.82, 0, 0.82];
  const poses = [
    { id: 'left_missing_pan', folded: false, seatPanMissing: true },
    { id: 'center_raised_pan', folded: true },
    { id: 'right_lowered_pan', folded: false }
  ];
  poses.forEach((pose, index) => {
    buildEnergetikCinemaSeat(
      { objects, box, capsule, ellipse, roundBox },
      seatCenters[index], 0, -8,
      { variantId: pose.id, folded: pose.folded, seatPanMissing: !!pose.seatPanMissing,
        omitSharedFrame: true, sideStyle: 'padded', fabric: 'seatVinylBrown',
        trim: 'seatVinylBrown', support: 'seatSupportPaint' }
    );
  });

  // One continuous painted-steel rail, three mount brackets and one centre
  // floor pedestal reproduce the connected row visible in the supplied image.
  capsule([-1.235, 0.405, -7.975], [1.235, 0.405, -7.975], 0.027, 'seatSupportPaint');
  for (const x of [-1.235, -0.41, 0.41, 1.235])
    capsule([x, 0.405, -7.975], [x, 0.485, -7.975], 0.019, 'seatSteel');
  capsule([0, 0.405, -7.975], [0, 0.025, -7.975], 0.031, 'seatSupportPaint');

  M3D_SCENE_LIGHTS.push([-1.1, 2.8, -9.2, 6.5, 0.8, 255, 235, 215]);
  M3D_SCENE_LIGHTS.push([1.6, 2.1, -6.5, 3.5, 0.55, 205, 216, 230]);
}
