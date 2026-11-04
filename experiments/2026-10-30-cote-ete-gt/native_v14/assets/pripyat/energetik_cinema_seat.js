// Reusable, clean Soviet-era folding cinema chair for the Energetik Palace.
// All dimensions are in metres. It faces -Z. No damage or grime is modeled.
// Geometry stays deliberately moderate: shaped shells, pads and hardware,
// rather than a dense scanned mesh or millions of triangles.
module.exports = function buildEnergetikCinemaSeat(api, x, y, z, options = {}) {
  const { box, capsule, ellipse, roundBox } = api;
  const scale = options.scale || 1;
  const fabric = options.fabric || 'seatFabricClean';
  const piping = options.trim || 'seatTrim';
  const metal = options.metal || 'metal';
  const wood = options.wood || 'wood';
  const X = v => x + v * scale, Y = v => y + v * scale, Z = v => z + v * scale;
  const B = (x0,y0,z0,x1,y1,z1,r,mat) => roundBox(X(x0),Y(y0),Z(z0),X(x1),Y(y1),Z(z1),r*scale,mat);
  const C = (a,b,r,mat) => capsule([X(a[0]),Y(a[1]),Z(a[2])],[X(b[0]),Y(b[1]),Z(b[2])],r*scale,mat);
  const E = (cx,cy,cz,rx,ry,rz,mat) => ellipse(X(cx),Y(cy),Z(cz),rx*scale,ry*scale,rz*scale,mat);
  const Q = (x0,y0,z0,x1,y1,z1,mat) => box(X(x0),Y(y0),Z(z0),X(x1),Y(y1),Z(z1),mat);
  const folded = options.folded === true;
  const objectStart = Array.isArray(api.objects) ? api.objects.length : null;
  const components = [];
  let componentStart = objectStart;
  const componentMaterials = {
    frame: ['seat_steel', 'seat_shell_wood'],
    backrest: ['seat_shell_wood', 'seat_upholstery_cover', 'seat_stitched_trim', 'seat_foam_core'],
    seat_pan: ['seat_steel', 'seat_shell_wood', 'seat_upholstery_cover', 'seat_stitched_trim', 'seat_foam_core'],
    armrests: ['seat_steel', 'seat_shell_wood']
  };
  const finishComponent = (id, role) => {
    if (componentStart === null) return;
    const end = api.objects.length;
    components.push({ id, role, firstObject: componentStart, objectCount: end - componentStart, materialProfileIds: componentMaterials[id] });
    componentStart = end;
  };

  // Floor shoes, four anchor bolts, and the two substantial side frames.
  for (const side of [-1,1]) {
    B(side*.215-.075,.018,-.11,side*.215+.075,.085,.32,.025,metal);
    for (const dx of [-.043,.043]) for (const dz of [-.065,.265])
      E(side*.215+dx,.089,dz,.018,.009,.018,metal);
    C([side*.215,.055,-.035],[side*.215,.47,.045],.027,metal);
    C([side*.215,.055,.27],[side*.215,.46,.19],.027,metal);
    C([side*.215,.13,-.02],[side*.215,.37,.25],.018,metal);
    B(side*.255-.048,.30,.005,side*.255+.048,.67,.20,.028,wood);
    B(side*.255-.030,.34,.030,side*.255+.030,.62,.16,.018,metal);
    // Short tabs connect the seat and back to the frame; circular hinge pins
    // make the folding mechanism readable from a close camera.
    Q(side*.197,.44,.055,side*.233,.51,.15,metal);
    E(side*.236,.475,.115,.018,.043,.043,metal);
    E(side*.252,.475,.115,.008,.022,.022,piping);
  }
  C([-.215,.13,.10],[.215,.13,.10],.026,metal);
  C([-.215,.39,.14],[.215,.39,.14],.022,metal);
  C([-.215,.455,.095],[.215,.455,.095],.020,metal);
  finishComponent('frame', 'rigid');

  // Thick back shell. Four narrow mouldings frame the upholstery instead of
  // leaving one featureless cuboid; the slightly stepped layers stay smooth.
  B(-.282,.49,.205,.282,1.17,.365,.040,wood);
  B(-.258,.515,.178,.258,1.145,.215,.032,wood);
  B(-.235,.545,.145,.235,1.105,.238,.046,fabric);
  B(-.205,.575,.129,.205,1.075,.153,.034,fabric);
  B(-.270,.515,.207,-.245,1.14,.231,.009,piping);
  B(.245,.515,.207,.270,1.14,.231,.009,piping);
  B(-.235,.515,.214,.235,.548,.237,.008,piping);
  B(-.235,1.105,.214,.235,1.14,.237,.008,piping);
  // Tailored vertical channels and two cross seams on the front face.
  for (const dx of [-.135,-.068,0,.068,.135])
    C([dx,.625,.126],[dx,1.015,.126],.0045,piping);
  for (const yy of [.72,.91]) C([-.19,yy,.125],[.19,yy,.125],.0045,piping);
  // Small upholstery buttons: subdued, evenly aligned, and not decorative.
  for (const dx of [-.145,-.072,.072,.145]) for (const yy of [.675,.965])
    E(dx,yy,.119,.009,.009,.004,wood);
  finishComponent('backrest', 'rigid');

  // Seat board and padded face. The folded version stands upright in front of
  // the back, with a separate hinge axle, end caps and return-spring shapes.
  if (folded) {
    B(-.255,.485,-.214,.255,.925,-.095,.035,wood);
    B(-.232,.515,-.238,.232,.895,-.161,.042,fabric);
    B(-.210,.545,-.245,.210,.865,-.226,.020,fabric);
    for (const dx of [-.145,-.072,.072,.145])
      C([dx,.565,-.244],[dx,.845,-.244],.0045,piping);
    C([-.245,.49,-.115],[.245,.49,-.115],.022,metal);
    for (const side of [-1,1]) {
      E(side*.245,.49,-.115,.024,.024,.024,metal);
      E(side*.262,.49,-.115,.010,.010,.010,piping);
      C([side*.19,.45,-.09],[side*.19,.55,.015],.012,metal);
      C([side*.19,.55,.015],[side*.19,.64,-.09],.010,metal);
    }
  } else {
    B(-.265,.435,-.245,.265,.565,.205,.045,wood);
    B(-.242,.48,-.254,.242,.605,.17,.050,fabric);
    B(-.210,.505,-.266,.210,.575,-.230,.020,fabric);
    for (const dx of [-.15,-.075,0,.075,.15])
      C([dx,.515,-.26],[dx,.565,.13],.0045,piping);
    C([-.245,.455,.165],[.245,.455,.165],.020,metal);
  }
  finishComponent('seat_pan', 'hinged_rigid');

  // Two broad arm caps with a slight crown, underside trim, and fixing pins.
  for (const side of [-1,1]) {
    B(side*.315-.095,.665,-.12,side*.315+.095,.755,.245,.030,wood);
    B(side*.315-.077,.735,-.095,side*.315+.077,.772,.215,.018,wood);
    C([side*.315,.64,-.075],[side*.315,.68,-.075],.018,metal);
    C([side*.315,.64,.205],[side*.315,.68,.205],.018,metal);
    E(side*.315,.765,.05,.018,.008,.018,metal);
    E(side*.315,.765,.18,.018,.008,.018,metal);
  }

  finishComponent('armrests', 'rigid');
  const objectEnd = Array.isArray(api.objects) ? api.objects.length : null;
  const open = !folded;
  return {
    schema: 'm3d-asset-instance-v1',
    asset: 'energetik-folding-cinema-seat',
    origin: [x, y, z],
    scale,
    folded,
    objectRange: objectStart === null ? null : [objectStart, objectEnd],
    components,
    joints: [{
      id: 'seat-pan-hinge',
      part: 'seat_pan',
      type: 'revolute',
      pivot: [x, y + .49 * scale, z - .115 * scale],
      axis: [1, 0, 0],
      foldedAngleDegrees: 0,
      openAngleDegrees: -90,
      currentAngleDegrees: open ? -90 : 0
    }],
    deformableParts: [
      { id: 'seat-pad', component: 'seat_pan', coreProfile: 'seat_foam_core', coverProfile: 'seat_upholstery_cover', runtimeDeformation: 'not-supported-by-current-scene-format' },
      { id: 'back-pad', component: 'backrest', coreProfile: 'seat_foam_core', coverProfile: 'seat_upholstery_cover', runtimeDeformation: 'not-supported-by-current-scene-format' }
    ]
  };
};
