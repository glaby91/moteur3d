// Reusable, clean Soviet-era folding cinema chair for the Energetik Palace.
// All dimensions are in metres. It faces -Z. No damage or grime is modeled.
// Geometry stays deliberately moderate: shaped shells, pads and hardware,
// rather than a dense scanned mesh or millions of triangles.
module.exports = function buildEnergetikCinemaSeat(api, x, y, z, options = {}) {
  const { box, capsule, ellipse, roundBox } = api;
  const scale = options.scale || 1;
  const fabric = options.fabric || 'seatVinylBrown';
  const piping = options.trim || fabric;
  const metal = options.metal || 'seatSteel';
  const wood = options.wood || 'seatWood';
  const support = options.support || 'seatSupportPaint';
  const sideStyle = options.sideStyle || 'padded';
  const assetVariant = options.variantId || `side-${sideStyle}`;
  const X = v => x + v * scale, Y = v => y + v * scale, Z = v => z + v * scale;
  const B = (x0,y0,z0,x1,y1,z1,r,mat) => roundBox(X(x0),Y(y0),Z(z0),X(x1),Y(y1),Z(z1),r*scale,mat);
  const C = (a,b,r,mat) => capsule([X(a[0]),Y(a[1]),Z(a[2])],[X(b[0]),Y(b[1]),Z(b[2])],r*scale,mat);
  const E = (cx,cy,cz,rx,ry,rz,mat) => ellipse(X(cx),Y(cy),Z(cz),rx*scale,ry*scale,rz*scale,mat);
  const Q = (x0,y0,z0,x1,y1,z1,mat) => box(Math.min(X(x0),X(x1)),Math.min(Y(y0),Y(y1)),Math.min(Z(z0),Z(z1)),Math.max(X(x0),X(x1)),Math.max(Y(y0),Y(y1)),Math.max(Z(z0),Z(z1)),mat);
  const padFrontZ = (px,py,cx,cy,cz,rx,ry,rz) => cz-rz*Math.sqrt(Math.max(.035,1-((px-cx)/rx)**2-((py-cy)/ry)**2))-.002;
  const padTopY = (px,pz,cx,cy,cz,rx,rz,ry) => cy+ry*Math.sqrt(Math.max(.035,1-((px-cx)/rx)**2-((pz-cz)/rz)**2));
  const folded = options.folded === true;
  const objectStart = Array.isArray(api.objects) ? api.objects.length : null;
  const components = [];
  let componentStart = objectStart;
  const componentMaterials = {
    frame: ['seat_steel', 'seat_support_paint'],
    backrest: ['seat_shell_wood', 'seat_upholstery_cover', 'seat_stitched_trim', 'seat_foam_core'],
    seat_pan: ['seat_steel', 'seat_shell_wood', 'seat_upholstery_cover', 'seat_stitched_trim', 'seat_foam_core'],
    armrests: ['seat_upholstery_cover', 'seat_steel']
  };
  const finishComponent = (id, role) => {
    if (componentStart === null) return;
    const end = api.objects.length;
    components.push({ id, role, firstObject: componentStart, objectCount: end - componentStart, materialProfileIds: componentMaterials[id] });
    componentStart = end;
  };

  // The padded side panels carry the seat; no separate tall wooden stiles or foot bar.
  for (const side of [-1,1]) {
    // Exposed lateral pivot hardware sits at the rear of the open seat pan.
    Q(side*.190,.455,-.145,side*.224,.515,-.085,metal);
    E(side*.378,.485,-.115,.012,.031,.031,metal);
    E(side*.390,.485,-.115,.006,.016,.016,piping);
  }
  // The row builder may supply one shared continuous rail and its floor supports.
  if (!options.omitSharedFrame) {
    C([-.205,.405,.025],[.205,.405,.025],.022,support);
    C([0,.405,.025],[0,.025,.025],.026,support);
  }
  finishComponent('frame', 'rigid');

  // Thick back shell. Four narrow mouldings frame the upholstery instead of
  // leaving one featureless cuboid; the slightly stepped layers stay smooth.
  B(-.282,.49,.205,.282,1.17,.365,.040,wood);
  B(-.258,.515,.178,.258,1.145,.215,.032,wood);
  B(-.235,.545,.145,.235,1.105,.238,.046,fabric);
  E(0,.825,.166,.222,.270,.050,fabric);
  B(-.270,.515,.207,-.245,1.14,.231,.009,piping);
  B(.245,.515,.207,.270,1.14,.231,.009,piping);
  B(-.235,.515,.214,.235,.548,.237,.008,piping);
  B(-.235,1.105,.214,.235,1.14,.237,.008,piping);
  // Tailored vertical channels and two cross seams on the front face.
  for (const dx of [-.135,-.068,0,.068,.135])
    C([dx,.625,padFrontZ(dx,.625,0,.825,.166,.222,.270,.050)],[dx,1.015,padFrontZ(dx,1.015,0,.825,.166,.222,.270,.050)],.0045,piping);
  for (const yy of [.72,.91]) C([-.19,yy,padFrontZ(-.19,yy,0,.825,.166,.222,.270,.050)],[.19,yy,padFrontZ(.19,yy,0,.825,.166,.222,.270,.050)],.0045,piping);
  // Small upholstery buttons: subdued, evenly aligned, and not decorative.
  for (const dx of [-.145,-.072,.072,.145]) for (const yy of [.675,.965])
    E(dx,yy,padFrontZ(dx,yy,0,.825,.166,.222,.270,.050)-.002,.009,.009,.004,piping);
  finishComponent('backrest', 'rigid');

  // Seat board and padded face. The folded version stands upright in front of
  // the back, with a separate hinge axle, end caps and return-spring shapes.
  if (!options.seatPanMissing && folded) {
    B(-.255,.485,-.214,.255,.925,-.095,.035,wood);
    B(-.232,.515,-.238,.232,.895,-.161,.042,fabric);
    E(0,.705,-.215,.216,.174,.046,fabric);
    for (const dx of [-.145,-.072,.072,.145])
      C([dx,.590,padFrontZ(dx,.590,0,.705,-.215,.216,.174,.046)],[dx,.820,padFrontZ(dx,.820,0,.705,-.215,.216,.174,.046)],.0045,piping);
    C([-.155,.715,padFrontZ(-.155,.715,0,.705,-.215,.216,.174,.046)],[.155,.715,padFrontZ(.155,.715,0,.705,-.215,.216,.174,.046)],.004,piping);
    C([-.245,.49,-.115],[.245,.49,-.115],.022,metal);
    for (const side of [-1,1]) {
      E(side*.245,.49,-.115,.024,.024,.024,metal);
      E(side*.262,.49,-.115,.010,.010,.010,piping);
      C([side*.19,.45,-.09],[side*.19,.55,.015],.012,metal);
      C([side*.19,.55,.015],[side*.19,.64,-.09],.010,metal);
    }
  } else if (!options.seatPanMissing) {
    B(-.215,.435,-.245,.215,.565,.205,.045,wood);
    B(-.195,.48,-.254,.195,.605,.17,.050,fabric);
    E(0,.560,-.045,.185,.052,.180,fabric);
    for (const dx of [-.15,-.075,0,.075,.15]) {
      const z0=-.20,z1=.11;
      C([dx,padTopY(dx,z0,0,.560,-.045,.215,.180,.052),z0],[dx,padTopY(dx,z1,0,.560,-.045,.215,.180,.052),z1],.0045,piping);
    }
    C([-.205,.455,.165],[.205,.455,.165],.020,metal);
  }
  finishComponent('seat_pan', options.seatPanMissing ? 'broken_missing' : 'hinged_rigid');

  // The catalogue has more than one auditorium-seat construction. Keep the
  // broad upholstered side as the reference variant, and expose two plausible
  // alternatives without changing the shared seat/back geometry.
  for (const side of [-1,1]) {
    if (sideStyle === 'wood_stiles') {
      B(side*.280-.045,.035,-.255,side*.280+.045,.735,.245,.035,wood);
      B(side*.280-.105,.690,-.275,side*.280+.105,.765,.265,.050,fabric);
      B(side*.280-.052,.065,-.270,side*.280+.052,.105,.260,.014,metal);
    } else if (sideStyle === 'tubular') {
      C([side*.280,.055,-.245],[side*.280,.730,-.245],.024,metal);
      C([side*.280,.055,.205],[side*.280,.730,.205],.024,metal);
      C([side*.280,.730,-.245],[side*.280,.730,.205],.024,metal);
      B(side*.280-.085,.690,-.255,side*.280+.085,.755,.225,.032,fabric);
      C([side*.280,.055,-.245],[side*.280,.055,.205],.017,support);
    } else {
      B(side*.280-.095,.035,-.285,side*.280+.095,.755,.285,.085,fabric);
    }
  }

  finishComponent('armrests', sideStyle === 'padded' ? 'padded_structural_side_panel' : sideStyle);
  const objectEnd = Array.isArray(api.objects) ? api.objects.length : null;
  const open = !folded && !options.seatPanMissing;
  return {
    schema: 'm3d-asset-instance-v1',
    asset: 'energetik-folding-cinema-seat',
    variantId: assetVariant,
    sideStyle,
    origin: [x, y, z],
    scale,
    folded,
    seatPanMissing: !!options.seatPanMissing,
    armrestIsSideMontant: sideStyle === 'padded',
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
