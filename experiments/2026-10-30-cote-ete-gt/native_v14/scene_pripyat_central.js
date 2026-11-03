// Pripyat city-centre scene — native Moteur 3D V14 scene source.
// Geometry is authored in metres and exported through the existing V4 pipeline.
// Documentary-photo references and map sources are listed in docs/pripyat/README.md.

const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, brick: 2, glass: 4, windowdark: 1, windowlit: 3,
  asphalt: 3, stone: 5, metal: 18, rust: 7, leaves: 2, grass: 1,
  bark: 1, wood: 4, sign: 3, debris: 2, puddle: 28
});
const M3D_MATERIAL_TRANSMISSION_PERCENT = Object.freeze({ glass: 62 });
const M3D_MATERIAL_IOR = Object.freeze({ glass: 1.48 });
const M3D_SCENE_LIGHTS = [];

function makeScene() {
  objects.length = 0;

  // Coordinate convention: Y is vertical. The camera approaches from +Z.
  // Zone 01 covers the central plaza, Energetik and Hotel Polissya frontages.
  const groundY = -0.08;
  box(-64, -0.32, -70, 64, groundY, 88, 'asphalt');
  box(-58, -0.10, -62, 58, -0.045, 70, 'concrete');

  // Main paved square: broad worn paving, joint bands, patched slabs and weeds.
  box(-53, -0.045, -15, 53, 0.015, 61, 'stone');
  for (let x = -52; x <= 52; x += 3.2) {
    box(x, 0.016, -14, x + 0.055, 0.021, 60, 'concrete');
  }
  for (let z = -14; z <= 60; z += 3.2) {
    box(-52, 0.016, z, 52, 0.021, z + 0.055, 'concrete');
  }
  // Replacement slabs and darker repaired patches break the regular grid.
  for (const p of [
    [-42, 9, -7, 6, 3], [-29, 19, 10, 4, 5], [-10, 30, -4, 7, 3],
    [12, 45, 5, 4, 4], [34, 18, -8, 6, 4], [41, 49, 9, 3, 5],
    [-5, 4, 29, 5, 4], [20, 33, 38, 7, 3], [-35, 2, 45, 4, 6]
  ]) box(p[0], 0.018, p[1], p[0] + p[3], 0.023, p[1] + p[4], 'concrete');

  // Roads and sidewalks frame the plaza and connect to the surrounding blocks.
  box(-64, -0.015, 65, 64, 0.025, 88, 'asphalt');
  box(-64, 0.025, 61, 64, 0.13, 65, 'concrete');
  box(-64, 0.025, -19, 64, 0.13, -15, 'concrete');
  box(-60, 0.02, -15, -55, 0.14, 65, 'concrete');
  box(55, 0.02, -15, 60, 0.14, 65, 'concrete');
  for (let z = 67; z < 87; z += 4.5) {
    box(-62, 0.028, z, 62, 0.035, z + 0.12, 'stone');
  }
  // Crosswalk and worn lane markings.
  for (let x = -13; x < 14; x += 2.4) {
    box(x, 0.036, 72, x + 1.25, 0.043, 77, 'concrete');
  }

  // Long, low connecting gallery behind the open square.
  box(-9, 0, -24, 12, 6.2, -18.2, 'concrete');
  box(-9.4, 6.1, -24.4, 12.4, 6.55, -17.8, 'stone');
  for (let x = -7.5; x < 11; x += 2.3) {
    box(x, 0.5, -18.12, x + 1.25, 3.2, -17.92, 'windowdark');
    box(x - 0.12, 3.25, -18.25, x + 1.38, 3.42, -17.9, 'concrete');
  }
  box(-1.2, 0.05, -17.9, 4.2, 2.8, -17.45, 'glass');
  box(-1.5, 2.8, -18.0, 4.5, 3.05, -17.4, 'sign');

  // Palace of Culture Energetik — broad low mass, recessed glass entrance,
  // vertical concrete fins and a stepped roofline.
  box(-43, 0, -47, -7, 15.6, -19.5, 'concrete');
  box(-40.5, 0.02, -19.6, -8.8, 8.8, -17.3, 'concrete');
  box(-41.5, 8.8, -19.2, -8.0, 9.45, -17.0, 'stone');
  box(-39, 9.45, -44.5, -11, 16.2, -19.5, 'concrete');
  box(-36, 16.2, -41, -14, 17.1, -21, 'stone');
  // Deep entrance glazing and rhythm of monumental columns.
  box(-39.8, 0.65, -17.31, -9.4, 7.9, -17.05, 'windowdark');
  for (let x = -38.5; x <= -10.8; x += 3.95) {
    box(x, 0.1, -17.0, x + 0.75, 8.55, -16.28, 'concrete');
    box(x - 0.18, 0.08, -16.94, x + 0.91, 0.38, -16.12, 'stone');
    box(x - 0.14, 8.38, -16.96, x + 0.89, 8.72, -16.1, 'stone');
  }
  box(-41, 8.55, -17.1, -8, 9.15, -16.25, 'concrete');
  // Side wings and long rows of dark window openings.
  for (let x = -41; x <= -11; x += 4.25) {
    box(x, 10.05, -19.45, x + 2.2, 13.0, -19.18, 'windowdark');
    box(x - 0.16, 9.82, -19.58, x + 2.36, 10.02, -19.1, 'stone');
    box(x - 0.16, 13.02, -19.58, x + 2.36, 13.2, -19.1, 'stone');
  }
  // Entrance steps and broad forecourt.
  for (let step = 0; step < 6; step++) {
    const y = 0.05 + step * 0.18;
    box(-34.5 + step * 0.4, y, -16.4 + step * 0.58,
        -14.5 - step * 0.4, y + 0.18, -15.95 + step * 0.58, 'stone');
  }
  box(-34.8, 1.2, -12.8, -14.2, 1.45, -11.9, 'concrete');

  // Hotel Polissya — taller central slab with two stepped wings and a rooftop sign.
  box(13.2, 0, -43, 42.2, 18.4, -18.7, 'concrete');
  box(19.0, 18.4, -41.4, 36.4, 23.2, -21.0, 'concrete');
  box(20.2, 23.2, -38.8, 35.4, 24.0, -23.2, 'stone');
  box(12.7, 0.12, -18.75, 42.6, 0.52, -18.15, 'stone');
  for (let floor = 0; floor < 6; floor++) {
    const y = 2.1 + floor * 2.72;
    const x0 = floor < 2 ? 15.2 : 19.4;
    const x1 = floor < 2 ? 40.2 : 36.1;
    for (let x = x0; x < x1; x += 2.45) {
      box(x, y, -18.62, x + 1.28, y + 1.62, -18.35, floor === 3 && x % 2 < 1 ? 'windowlit' : 'windowdark');
      box(x - 0.12, y - 0.13, -18.72, x + 1.4, y + 0.04, -18.27, 'stone');
      box(x - 0.12, y + 1.68, -18.72, x + 1.4, y + 1.82, -18.27, 'concrete');
    }
  }
  // Recessed hotel entrance, canopy and roof lettering plaque (geometry only).
  box(24.0, 0.4, -18.65, 31.2, 4.3, -18.3, 'windowdark');
  box(22.8, 4.0, -20.0, 32.4, 4.35, -17.1, 'stone');
  box(21.0, 24.0, -24.0, 34.8, 24.7, -22.9, 'sign');

  // Repeated small windows add depth to the visible west side of Energetik.
  for (let z = -43; z <= -22; z += 4.2) {
    for (let y = 2.2; y < 13; y += 3.0) {
      box(-43.08, y, z, -42.78, y + 1.5, z + 1.7, 'windowdark');
      box(-43.16, y - 0.12, z - 0.12, -42.72, y + 1.62, z + 1.82, 'stone');
    }
  }
  // Two surrounding apartment slabs establish the next urban blocks without
  // pretending that unseen districts have already been modelled.
  function apartment(x0, z0, x1, z1, floors, frontZ) {
    const height = floors * 2.75;
    box(x0, 0, z0, x1, height, z1, 'concrete');
    for (let f = 0; f < floors; f++) {
      const y = 1.1 + f * 2.75;
      for (let x = x0 + 1.4; x < x1 - 1.1; x += 2.7) {
        box(x, y, frontZ - 0.1, x + 1.25, y + 1.25, frontZ + 0.06, 'windowdark');
        box(x - 0.12, y - 0.12, frontZ - 0.16, x + 1.37, y + 1.38, frontZ - 0.06, 'stone');
      }
      box(x0, y + 1.45, frontZ - 0.05, x1, y + 1.55, frontZ + 0.07, 'stone');
    }
    // Roof parapet and occasional broken bays.
    box(x0 - 0.2, height, z0 - 0.15, x1 + 0.2, height + 0.75, z1 + 0.15, 'concrete');
    for (let x = x0 + 2; x < x1; x += 8.1) {
      box(x, height * 0.56, frontZ - 0.2, x + 1.3, height * 0.56 + 1.25, frontZ + 0.08, 'windowlit');
    }
  }
  apartment(-60, -34, -47, -10, 6, -10.0);
  apartment(46, -37, 60, -12, 7, -12.0);
  apartment(-60, 20, -47, 52, 5, 20.0);
  apartment(47, 18, 60, 50, 6, 18.0);

  // Broken low walls, concrete planters and street furniture around the square.
  function planter(x, z, w, d, height) {
    box(x, 0.02, z, x + w, height, z + d, 'concrete');
    box(x + 0.22, height, z + 0.22, x + w - 0.22, height + 0.18, z + d - 0.22, 'stone');
    box(x + 0.36, height + 0.18, z + 0.36, x + w - 0.36, height + 0.25, z + d - 0.36, 'grass');
  }
  for (const p of [[-48,5,7,2],[-31,47,8,2],[31,42,9,2],[45,6,7,2],[-5,53,8,2],[7,-6,7,2]]) {
    planter(p[0],p[1],p[2],p[3],0.72);
  }
  function bench(x, z, angle) {
    const c=Math.cos(angle), s=Math.sin(angle);
    const plank=(u0,u1,y0,y1,mat)=>{
      const a=[x+c*u0, z+s*u0], b=[x+c*u1,z+s*u1];
      box(Math.min(a[0],b[0])-.10,y0,Math.min(a[1],b[1])-.10,
          Math.max(a[0],b[0])+.10,y1,Math.max(a[1],b[1])+.10,mat);
    };
    plank(-1.15,1.15,0.68,0.82,'wood');
    plank(-1.15,1.15,0.88,0.99,'wood');
    for (const u of [-0.86,0.86]) {
      capsule([x+c*u,0.08,z+s*u],[x+c*u,0.68,z+s*u],0.07,'rust');
      capsule([x+c*u,0.58,z+s*u],[x+c*u,1.35,z+s*u],0.06,'rust');
    }
    plank(-1.05,1.05,1.27,1.36,'wood');
  }
  for (const b of [
    [-45,19,0],[-30,35,Math.PI/2],[-14,42,0],[4,40,Math.PI/2],
    [25,33,0],[43,22,Math.PI/2],[-40,-5,Math.PI/2],[38,-5,Math.PI/2],
    [-24,52,0],[18,52,0],[0,22,Math.PI/2],[0,-3,0]
  ]) bench(b[0],b[1],b[2]);

  // Poplars and younger trees: slender trunks with narrow crowns, progressively
  // varied so the square does not read as a repeated asset grid.
  function poplar(x, z, h, crownScale=1) {
    capsule([x,0.05,z],[x,h*0.78,z],0.20,'bark');
    for (let k=0;k<4;k++) {
      const y=h*(0.46+k*0.125);
      ellipsoidMesh(x,y,z,1.05*crownScale*(1-k*0.14),h*0.22,0.78*crownScale*(1-k*0.12),'leaves',12,10);
    }
    for (const side of [-1,1]) {
      capsule([x,0.12,z],[x+side*0.42,0.04,z+0.16],0.08,'bark');
    }
  }
  const matureTrees=[
    [-52,-11,12],[-48,29,14],[-39,58,13],[-27,58,15],[-13,59,12],
    [5,59,15],[20,59,13],[38,57,14],[51,49,13],[53,28,12],
    [52,7,14],[48,-11,13],[-52,49,12],[-48,2,11],[-44,-12,12],
    [-34,-10,13],[34,-11,12],[43,-11,13],[-56,68,11],[53,67,12]
  ];
  for (let i=0;i<matureTrees.length;i++) poplar(...matureTrees[i],0.76+(i%4)*0.11);
  // Young volunteer trees and shrubs growing through failed paving.
  const saplings=[[-37,13],[-24,7],[-19,29],[-7,48],[2,12],[8,31],[15,46],[28,11],
    [37,36],[-46,40],[47,42],[-4,56],[30,-4],[-31,-2],[4,2],[18,6]];
  for (let i=0;i<saplings.length;i++) {
    const [x,z]=saplings[i],h=3.3+(i%4)*0.75;
    capsule([x,0.05,z],[x,h*.72,z],0.09,'bark');
    ellipsoidMesh(x,h*.78,z,0.72,h*.34,0.62,'leaves',10,8);
    ellipsoidMesh(x-.42,h*.70,z+.10,0.45,h*.25,0.42,'leaves',9,7);
  }
  // Weed tufts along joints, curbs and beneath trees.
  for (let x=-49;x<=49;x+=7) for (let z=-12;z<=59;z+=9) {
    const dx=((x*13+z*7)%5)*0.12, dz=((x*3-z*11)%5)*0.12;
    ellipsoidMesh(x+dx,0.22,z+dz,0.38,0.28,0.34,'grass',8,6);
    for (let k=0;k<3;k++) capsule([x+dx,0.08,z+dz],[x+dx+(k-1)*0.15,0.52+(k%2)*0.18,z+dz+0.04],0.035,'leaves');
  }

  // Soviet-era street lamps with intact and broken fittings.
  function lamp(x,z,broken=false) {
    capsule([x,0.05,z],[x,5.25,z],0.095,'metal');
    capsule([x,4.9,z],[x+0.95,5.15,z],0.075,'metal');
    ellipsoidMesh(x+0.97,5.05,z,0.34,0.16,0.22,broken?'rust':'lamp',10,8);
    if (!broken) M3D_SCENE_LIGHTS.push([x+0.95,4.9,z,7.2,0.38,255,198,133]);
  }
  const lamps=[[-50,-8],[-43,56],[-25,-10],[-12,54],[8,55],[28,54],[48,50],[52,23],
    [47,-8],[24,-10],[0,57],[0,67],[-52,25],[52,2]];
  for (let i=0;i<lamps.length;i++) lamp(lamps[i][0],lamps[i][1],i%5===0);

  // Damaged paving, fragments and shallow puddles near low points.
  for (const d of [
    [-47,4,0.9],[-35,27,1.1],[-20,54,0.7],[-9,8,0.8],[6,48,1.2],
    [21,16,0.9],[33,48,0.7],[44,14,1.0],[-3,35,0.8],[12,1,1.1]
  ]) {
    ellipsoidMesh(d[0],0.028,d[1],d[2],0.012,d[2]*0.62,'puddle',12,8);
    for (let k=0;k<4;k++) {
      const x=d[0]+((k*7)%5-2)*0.24, z=d[1]+((k*3)%5-2)*0.2;
      roundBox(x,0.06,z,x+0.32,0.16,z+0.24,0.035,'debris');
    }
  }

  // Facade fragments and modest local lighting keep the central landmarks readable.
  for (const x of [-38,-31,-24,-17,-10,17,24,31,38]) {
    M3D_SCENE_LIGHTS.push([x,5,-15.8,12,0.20,236,204,157]);
  }
}

makeScene();
