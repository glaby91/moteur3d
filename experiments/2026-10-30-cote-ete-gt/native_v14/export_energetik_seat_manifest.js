// Generate object/component and future-physics metadata for the Energetik row preview.
// The metadata is not consumed by today's renderer.
const fs = require('fs');
const path = require('path');
const buildSeat = require('./assets/pripyat/energetik_cinema_seat.js');
const physicsProfiles = require('./energetik_seat_physics_profiles.json');
const output = process.argv[2] || path.join(__dirname, 'scene_energetik_seat_preview.assets.json');

// Match scene_energetik_seat_preview.js exactly. Object 0 is the neutral floor.
const objects = [{ kind: 'box', args: ['preview floor'] }];
const record = kind => (...args) => objects.push({ kind, args });
const api = {
  objects,
  box: record('box'), roundBox: record('roundbox'),
  capsule: record('capsule'), ellipse: record('ellipse')
};
const seatCenters = [-0.82, 0, 0.82];
const poses = [
  { id: 'left_missing_pan', folded: false, seatPanMissing: true },
  { id: 'center_raised_pan', folded: true },
  { id: 'right_lowered_pan', folded: false }
];
const seats = seatCenters.map((centerX, index) => ({
  index: index + 1,
  centerX,
  pose: poses[index],
  instance: buildSeat(api, centerX, 0, -8, {
    variantId: poses[index].id, folded: poses[index].folded,
    seatPanMissing: !!poses[index].seatPanMissing,
    omitSharedFrame: true, sideStyle: 'padded'
  })
}));
const supportStart = objects.length;
api.capsule([-1.235, 0.405, -7.975], [1.235, 0.405, -7.975], 0.027, 'seatSupportPaint');
for (const postX of [-1.235, -0.41, 0.41, 1.235])
  api.capsule([postX, 0.405, -7.975], [postX, 0.485, -7.975], 0.019, 'seatSteel');
api.capsule([0, 0.405, -7.975], [0, 0.025, -7.975], 0.031, 'seatSupportPaint');
const components = seats.flatMap(({ index, instance }) =>
  instance.components.map(component => ({ ...component, id: `seat_${index}_${component.id}` }))
);
components.push({
  id: 'shared_mounting_rail',
  role: 'rigid',
  firstObject: supportStart,
  objectCount: objects.length - supportStart,
  materialProfileIds: ['seat_steel', 'seat_shell_wood']
});
const valid = value => Number.isFinite(value) && value > 0;
const withCalculatedProperties = profile => {
  const m = profile.measurements;
  const strain = valid(m.displacement_m) && valid(m.initial_length_m) ? m.displacement_m / m.initial_length_m : null;
  const stress = valid(m.force_n) && valid(m.loaded_area_m2) ? m.force_n / m.loaded_area_m2 : null;
  return {
    ...profile,
    calculated: {
      density_kg_m3: valid(m.mass_kg) && valid(m.volume_m3) ? m.mass_kg / m.volume_m3 : null,
      engineering_strain: strain,
      engineering_stress_pa: stress,
      secant_modulus_pa: strain !== null && stress !== null ? stress / strain : null,
      compression_curve_secant_moduli_pa: profile.compression_curve.map(point =>
        valid(point.strain) && valid(point.stress_pa) ? point.stress_pa / point.strain : null)
    }
  };
};
const joints = seats.flatMap(({ index, instance }) =>
  instance.joints.map(joint => ({ ...joint, id: `seat_${index}_${joint.id}`, part: `seat_${index}_${joint.part}` }))
);
const deformableParts = seats.flatMap(({ index, instance }) =>
  instance.deformableParts.map(part => ({ ...part, id: `seat_${index}_${part.id}`, component: `seat_${index}_${part.component}` }))
);
const manifest = {
  schema: 'm3d-asset-manifest-v1',
  asset: 'energetik-folding-cinema-seat-row',
  units: 'metres',
  pose: { seatCount: seats.length, variants: poses.map(pose => pose.id), sharedRail: true },
  geometry: {
    objectCount: objects.length - 1,
    sceneObjectRange: [1, objects.length],
    primitiveCounts: objects.slice(1).reduce((acc, item) => { acc[item.kind] = (acc[item.kind] || 0) + 1; return acc; }, {}),
    components
  },
  materialProperties: {
    ...physicsProfiles,
    profiles: physicsProfiles.profiles.map(withCalculatedProperties)
  },
  joints,
  deformableParts,
  rendererStatus: {
    current: 'The .bin contains three static seat states (missing pan, raised pan, lowered pan) and one continuous shared mounting rail.',
    future: 'The manifest identifies each rigid seat, seat-pan hinge and foam-backed pad. Runtime articulation and deformation require engine support.'
  }
};
fs.writeFileSync(output, JSON.stringify(manifest, null, 2));
console.log(JSON.stringify({
  output, seatCount: seats.length, components: components.length,
  primitiveCounts: manifest.geometry.primitiveCounts, objectCount: manifest.geometry.objectCount
}, null, 2));
