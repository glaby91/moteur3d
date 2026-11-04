// Generate the object/component and future-physics description beside the
// standalone seat .bin. This metadata is not consumed by today's renderer.
const fs = require('fs');
const path = require('path');
const buildSeat = require('./assets/pripyat/energetik_cinema_seat.js');
const physicsProfiles = require('./energetik_seat_physics_profiles.json');
const output = process.argv[2] || path.join(__dirname, 'scene_energetik_seat_preview.assets.json');
// Preview scene object 0 is the neutral floor; mirror it so component ranges
// in this manifest line up with the static .bin's object table.
const objects = [{ kind: 'box', args: ['preview floor'] }];
const record = kind => (...args) => objects.push({ kind, args });
const seat = buildSeat({
  objects,
  box: record('box'), roundBox: record('roundbox'),
  capsule: record('capsule'), ellipse: record('ellipse')
}, 0, 0, 0, { folded: true });
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

const manifest = {
  schema: 'm3d-asset-manifest-v1',
  asset: seat.asset,
  units: 'metres',
  pose: { folded: seat.folded, seatPanAngleDegrees: seat.joints[0].currentAngleDegrees },
  geometry: {
    objectCount: seat.objectRange[1] - seat.objectRange[0],
    sceneObjectRange: seat.objectRange,
    primitiveCounts: objects.reduce((acc, item) => { acc[item.kind] = (acc[item.kind] || 0) + 1; return acc; }, {}),
    components: seat.components
  },
  materialProperties: {
    ...physicsProfiles,
    profiles: physicsProfiles.profiles.map(withCalculatedProperties)
  },
  joints: seat.joints,
  deformableParts: seat.deformableParts,
  rendererStatus: {
    current: 'The .bin contains static geometry in the selected pose.',
    future: 'The manifest identifies rigid parts, the seat-pan revolute joint, and foam-backed pads. Runtime articulation and deformation require engine support.'
  }
};
fs.writeFileSync(output, JSON.stringify(manifest, null, 2));
console.log(JSON.stringify({ output, components: seat.components, primitiveCounts: manifest.geometry.primitiveCounts, objectCount: manifest.geometry.objectCount }, null, 2));
