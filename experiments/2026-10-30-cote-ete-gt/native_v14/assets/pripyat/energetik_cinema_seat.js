// Reusable, pristine Soviet-era folding cinema seat inspired by the supplied
// Energetik auditorium photograph. Coordinates are metres; front is -Z.
// The builder is shared by the full Pripyat scene and the single-seat preview.
module.exports = function buildEnergetikCinemaSeat(api, x, y, z, options = {}) {
  const { box, capsule, roundBox } = api;
  const scale = options.scale || 1;
  const fabric = options.fabric || 'seatFabricClean';
  const trim = options.trim || 'seatTrim';
  const metal = options.metal || 'metal';
  const wood = options.wood || 'wood';
  const s = value => value * scale;
  const e = (dx, dy, dz, rx, ry, rz, material) =>
    ellipse(x + s(dx), y + s(dy), z + s(dz), s(rx), s(ry), s(rz), material);
  const c = (a, b, radius, material) => capsule(
    [x + s(a[0]), y + s(a[1]), z + s(a[2])],
    [x + s(b[0]), y + s(b[1]), z + s(b[2])], s(radius), material);
  const r = (x0, y0, z0, x1, y1, z1, radius, material) => roundBox(
    x + s(x0), y + s(y0), z + s(z0), x + s(x1), y + s(y1), z + s(z1), s(radius), material);

  const folded = options.folded === true;
  // Heavy side frames, legs, and an exposed cross brace support the hinge.
  for (const side of [-1, 1]) {
    c([side * .205, .045, -.015], [side * .205, .47, .025], .026, metal);
    c([side * .205, .045, .23], [side * .205, .46, .18], .026, metal);
    r(side * .25 - .046, .018, -.105, side * .25 + .046, .078, .30, .022, metal);
    // Solid angular arm supports echo the original dark wood / painted frame.
    r(side * .30 - .055, .31, -.035, side * .30 + .055, .70, .205, .026, wood);
    c([side * .205, .45, .10], [side * .205, .50, .10], .022, metal);
  }
  c([-.21, .15, .10], [.21, .15, .10], .026, metal);

  // A compact, almost upright back with a thick wood shell and padded face.
  r(-.275, .50, .205, .275, 1.16, .355, .045, wood);
  r(-.252, .535, .165, .252, 1.125, .24, .052, fabric);
  // Seat pan pivots at its rear edge. Closed is useful for the abandoned hall;
  // open shows the cushion in the isolated asset preview.
  if (folded) {
    r(-.245, .50, -.205, .245, .91, -.095, .045, wood);
    r(-.220, .525, -.225, .220, .875, -.155, .048, fabric);
  } else {
    r(-.255, .435, -.245, .255, .555, .20, .052, wood);
    r(-.235, .49, -.23, .235, .595, .16, .058, fabric);
  }

  // Fine piping and pressed seams add scale without making the chair ornate.
  c([-.238, .61, .255], [-.238, 1.055, .255], .006, trim);
  c([ .238, .61, .255], [ .238, 1.055, .255], .006, trim);
  c([-.18, .91, .258], [.18, .91, .258], .005, trim);
  if (!folded) c([-.19, .55, -.224], [.19, .55, -.224], .005, trim);

  // Broad, slightly softened wooden arm caps and visible hinge pins.
  for (const side of [-1, 1]) {
    r(side * .30 - .085, .675, -.12, side * .30 + .085, .755, .245, .032, wood);
    box(x + s(side * .205 - .018), y + s(.455), z + s(.095),
      x + s(side * .205 + .018), y + s(.495), z + s(.135), metal);
  }
};
