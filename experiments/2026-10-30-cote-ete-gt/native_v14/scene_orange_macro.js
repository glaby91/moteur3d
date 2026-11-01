// Standalone hero scene: macro study of one orange and its peel.
// Uses the V13.14 scene helpers from source_v1314.html and can be exported
// to a regular M3D1314 .bin file for the native Scene > Open menu.
const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({ orange: 4, orangeStage: 0, leaves: 0, wood: 0 });

function makeScene() {
  objects.length = 0;

  // Matte warm studio surface; it gives the fruit a clean contact shadow.
  box(-24, -.34, -28, 24, -.18, 12, 'orangeStage');

  const cx = 0, cy = 1.0, cz = -8.0;
  const rx = 1.28, ry = 1.25, rz = 1.24;
  const segments = 192, rings = 128;
  const grid = [];

  // A slightly flattened, irregular citrus profile, with shallow stem and
  // blossom dimples. The broad shape is geometric; pores are shading detail.
  for (let j = 0; j <= rings; j++) {
    const row = [];
    const theta = Math.PI * j / rings;
    const st = Math.sin(theta), ct = Math.cos(theta);
    for (let i = 0; i <= segments; i++) {
      const phi = Math.PI * 2 * i / segments;
      const lobe = 1 + .012 * Math.sin(5 * phi + .4) * st * st
        + .006 * Math.sin(3 * phi - 1.1) * Math.sin(2 * theta)
        + .003 * Math.sin(11 * phi + 2 * theta) * st;
      const dimple = .075 * Math.exp(-Math.pow(st / .13, 2));
      const x = cx + rx * st * Math.cos(phi) * lobe;
      const z = cz + rz * st * Math.sin(phi) * lobe;
      const y = cy + ry * ct - Math.sign(ct || 1) * dimple;
      const nx = st * Math.cos(phi) / rx;
      const ny = ct / ry;
      const nz = st * Math.sin(phi) / rz;
      row.push({ p: [x, y, z], n: norm(nx, ny, nz), uv: [phi / (2 * Math.PI) * 2.5, theta / Math.PI * 2.5] });
    }
    grid.push(row);
  }
  for (let j = 0; j < rings; j++) {
    for (let i = 0; i < segments; i++) {
      const a = grid[j][i], b = grid[j + 1][i], c = grid[j + 1][i + 1], d = grid[j][i + 1];
      meshTriangle(a, b, d, 'orange');
      meshTriangle(d, b, c, 'orange');
    }
  }

  // Short brown pedicel in the top dimple and five small green sepals.
  cone([cx, 2.13, cz], [cx + .025, 2.26, cz - .015], .047, .032, 'wood');
  for (let k = 0; k < 5; k++) {
    const a = k * Math.PI * 2 / 5 + .22;
    const start = [cx + Math.cos(a) * .045, 2.105, cz + Math.sin(a) * .045];
    const tip = [cx + Math.cos(a) * .20, 2.075, cz + Math.sin(a) * .20];
    cone(start, tip, .028, .004, 'leaves');
  }

  // A small, flat lance-shaped leaf laid toward the camera so its silhouette
  // reads clearly in the macro framing instead of becoming a thin spike.
  const leafBase = [cx - .015, 2.14, cz + .015];
  const leafMid = [cx - .18, 2.30, cz + .12];
  const leafTip = [cx - .38, 2.43, cz + .25];
  const side = [-.075, .020, -.075];
  const left = [leafMid[0] + side[0], leafMid[1] + side[1], leafMid[2] + side[2]];
  const right = [leafMid[0] - side[0], leafMid[1] - side[1], leafMid[2] - side[2]];
  const leafNormal = norm(-.10, .90, .42);
  const vertex = p => ({ p, n: leafNormal, uv: [0, 0] });
  const a = vertex(leafBase), m = vertex(leafMid), t = vertex(leafTip), l = vertex(left), r = vertex(right);
  meshTriangle(a, l, m, 'leaves');
  meshTriangle(a, m, r, 'leaves');
  meshTriangle(m, l, t, 'leaves');
  meshTriangle(m, t, r, 'leaves');
  capsule(leafBase, leafTip, .008, 'wood');
}
