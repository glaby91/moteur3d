// Pripyat city-centre scene — native Moteur 3D V14 scene source.
// Geometry is authored in metres and exported through the existing V4 pipeline.
// Documentary-photo references and map sources are listed in docs/pripyat/README.md.

const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, plaster: 1, mural: 1, energetikFresco: 2, volcanicTuff: 3, brick: 2, glass: 4, windowdark: 1, windowlit: 3,
  asphalt: 3, stone: 5, metal: 18, rust: 7, leaves: 2, grass: 1,
  bark: 1, wood: 4, sign: 3, neon: 2, debris: 2, puddle: 28, seatFabric: 1, seatFabricClean: 1, seatVinylBrown: 28, seatSupportPaint: 12, seatTrim: 2, curtain: 1, cinemaScreen: 0
});
const M3D_MATERIAL_TRANSMISSION_PERCENT = Object.freeze({ glass: 62 });
const M3D_MATERIAL_IOR = Object.freeze({ glass: 1.48 });
const M3D_SCENE_LIGHTS = [];

// Modular building geometry: independent object generator, compatible with V14 box API.
// Coordinates are legacy placeholders pending verified cadastral/photogrammetric survey.

/**
 * Scene assembly, one component per urban element. Coordinates in metres.
 * Geometry is a photo-guided BLOCKOUT, not a surveyed cadastral model.
 * Each building owns its components; changes do not alter unrelated assets.
 */
const M3D_PRIPYAT_BLOCKOUT = Object.freeze({
  plaza:{x0:-53,x1:53,z0:-15,z1:61},
  energetik:{x0:-50.5,x1:30.5,z0:-43.4,z1:-18.8,mainHeight:11.4,towerHeight:14},
  polissya:{x0:40,x1:63,z0:-42,z1:-20},
  housing:[
    {id:'west-south',x0:-64,z0:-34,x1:-57,z1:-10,floors:6,frontZ:-10},
    {id:'east-south',x0:46,z0:-37,x1:60,z1:-12,floors:7,frontZ:-12},
    {id:'west-north',x0:-64,z0:20,x1:-57,z1:52,floors:5,frontZ:20},
    {id:'east-north',x0:47,z0:18,x1:60,z1:50,floors:6,frontZ:18}
  ]
});
// Single architectural datum for Energetik. Values are provisional until
// measured plans and matched photo viewpoints establish historical dimensions.
// Coordinates are world metres; Y=0 is the ground-floor reference.
const M3D_ENERGETIK_ARCH = Object.freeze({
  origin: Object.freeze({x:0,y:0,z:0}),
  envelope: M3D_PRIPYAT_BLOCKOUT.energetik,
  levels: Object.freeze({
    ground:0.38, firstSlab:3.25, secondSlab:6.35,
    slabThickness:0.24, roof:9.4
  }),
  foyer: Object.freeze({
    mosaicWallX:8.15, mosaicSurfaceX:8.12,
    mosaicBottomY:2.0, mosaicTopY:6.30,
    mosaicStartZ:-41.55, mosaicEndZ:-32.05,
    galleryEastEdgeX:8.65, rearGalleryEndZ:-40.9,
    firstLandingY:3.49,
    muralBackingBaseY:1.95,
    arcadePierZ:Object.freeze([-41.5,-38.55,-35.7,-32.65]),
    arcadePierLength:0.60,
    arcadePierTopY:2.10,
    arcadeDepth:0.18
  }),
  rearWindowXs:Object.freeze([-14.5,-8.3,-2.1,4.1,10.3,16.5]),
  rearWindowWidth:3.5,
  tower: Object.freeze({roofY:14.0})
});
function m3dCheckEnergetikDatum(a=M3D_ENERGETIK_ARCH){
  const {levels:l,foyer:f,envelope:e}=a;
  if(!(l.ground<l.firstSlab && l.firstSlab<l.secondSlab && l.secondSlab<l.roof))
    throw new Error('Energetik: inconsistent floor elevations');
  if(!(f.mosaicBottomY<l.firstSlab && f.mosaicTopY<l.secondSlab &&
       f.mosaicTopY<=l.secondSlab && f.mosaicSurfaceX<f.mosaicWallX &&
       f.galleryEastEdgeX>f.mosaicWallX))
    throw new Error('Energetik: mosaic intersects a structural datum');
  if(!(f.mosaicStartZ<f.mosaicEndZ && e.z0<f.mosaicStartZ && f.mosaicEndZ<e.z1))
    throw new Error('Energetik: mosaic outside building envelope');
  if(!(l.ground<f.muralBackingBaseY && f.muralBackingBaseY<=f.mosaicBottomY &&
       f.arcadePierTopY>f.mosaicBottomY && f.arcadePierTopY<l.firstSlab &&
       f.arcadePierZ.every(z=>z>=f.mosaicStartZ && z+f.arcadePierLength<=f.mosaicEndZ)))
    throw new Error('Energetik: mural arcade dimensions inconsistent');
  if(Math.abs(f.firstLandingY-(l.firstSlab+l.slabThickness))>0.001)
    throw new Error('Energetik: stair landing and first floor disconnected');
  return true;
}
m3dCheckEnergetikDatum();

/**
 * Photo-guided wall of the Energetik foyer.
 * Historical photos show a continuous painted upper wall suspended over
 * open ground-floor bays and masonry piers, not an opaque three-storey wall.
 * The mural itself remains an independent textured mesh in makeScene().
 */
// East stair volume with three geometrically connected flights.
// The shared datum drives risers, upper landings and return gallery.
// Main double-flight foyer stair: every tread is derived from the actual
// finished upper floor (slab top), not the underside of the slab.
// The real foyer photograph shows one broad staircase to the side of the
// mural, not two stair flights cutting across the mural viewing corridor.
function m3dBuildFoyerStairFlights({box,capsule},a=M3D_ENERGETIK_ARCH){
  const l=a.levels,f=a.foyer;
  const count=17,base=l.ground+0.10;
  const rise=(f.firstLandingY-base-0.17)/(count-1);
  const x0=-11.0,x1=-2.0;
  for(let i=0;i<count;i++){
    const y=base+i*rise,z=-25.0-i*0.50;
    box(x0,y,z,x1,y+0.17,z+0.50,'stone');
  }
  // Landing stays entirely west of the viewing corridor and still meets the
  // upper walking surface. This removes a 9-metre slab in front of the mural.
  box(-12.4,l.firstSlab,-34.1,-1.4,l.firstSlab+l.slabThickness,-32.8,'stone');
  for(const railX of [x0+0.25,x1-0.25]){
    capsule([railX,0.80,-24.8],[railX,f.firstLandingY,-33.0],0.10,'rust');
    for(let i=0;i<10;i++){
      const z=-25.1-i*0.73,y=1.0+i*0.22;
      capsule([railX,y,z],[railX,y+0.43,z],0.035,'metal');
    }
  }
}
function m3dBuildEastStairCore({box,capsule}, a=M3D_ENERGETIK_ARCH){
  const l=a.levels;
  const stairDatum=[l.ground,l.firstSlab+l.slabThickness,
                    l.secondSlab+l.slabThickness,l.roof];
  for(let flight=0;flight<3;flight++){
    const start=stairDatum[flight], top=stairDatum[flight+1];
    const steps=17, rise=(top-start)/steps, tread=0.625, startZ=-40.50;
    for(let i=0;i<steps;i++){
      const z=startZ+i*tread,topY=start+(i+1)*rise;
      box(21.0,topY-0.15,z,26.05,topY,z+tread+0.015,'stone');
    }
    box(20.0,top-0.16,-29.90,27.0,top+0.02,-27.40,'concrete');
    box(26.0,top-0.16,-40.55,27.0,top+0.02,-23.05,'concrete');
    capsule([25.93,top+0.13,-40.2],[25.93,top+0.93,-29.7],0.035,'rust');
  }
}
// Independently testable floor slab generator: the foyer atrium must stay open.
function m3dBuildEnergetikSlabs({box},a=M3D_ENERGETIK_ARCH){
  const {levels:archLevels,foyer:archFoyer}=a;
  function floorPlate(y, openAuditorium=false){
    const t=y,b=y+archLevels.slabThickness;
    if(openAuditorium){
      // Keep the west-side audience hall open through the first upper slab.
      // Narrow side strips and a rear cross-corridor still connect the upper floor.
      box(-49.2,t,-42.2,-47.5,b,-25.2,'concrete');
      box(-17.3,t,-42.2,-15,b,-25.2,'concrete');
      box(-49.2,t,-25.2,-15,b,-20.1,'concrete');
    } else box(-49.2,t,-42.2,-15,b,-20.1,'concrete');
    // Double-height mosaic hall: remove the upper slab in front of the mural.
    // Preserve a narrow structural strip and the rear cross-corridor.
    box(-15,t,-42.2,-12.8,b,-35.8,'concrete');
    box(-12.8,t,-42.2,8,b,archFoyer.rearGalleryEndZ,'concrete');
    box(-15,t,-26.5,8,b,-20.1,'concrete');
    // First landing/gallery above the foyer staircase; leave the mural atrium open.
    box(-15,t,-35.8,-7,b,-26.5,'concrete');
    // Removed: this narrow slab ran directly across the mosaic sightline.
    // The hall's east gallery is supported beyond the wall at X=8.65.
    // Start the east upper floor behind the mural support wall (X from datum).
    box(archFoyer.galleryEastEdgeX,t,-42.2,18,b,-20.1,'concrete');
    box(18,t,-42.2,20,b,-20.1,'concrete');
    box(27,t,-42.2,29.2,b,-20.1,'concrete');
    box(20,t,-42.2,27,b,-38.0,'concrete');
    box(20,t,-23.0,27,b,-20.1,'concrete');
  }
  floorPlate(archLevels.firstSlab,true); floorPlate(archLevels.secondSlab);
}
function m3dBuildFoyerMuralArcade({box}, a=M3D_ENERGETIK_ARCH) {
  const f=a.foyer,l=a.levels, x0=f.mosaicWallX, x1=x0+f.arcadeDepth;
  // Full mural backing; side facing the foyer is on the -X side.
  box(x0,f.muralBackingBaseY,f.mosaicStartZ,x1,l.secondSlab,f.mosaicEndZ,'plaster');
  // Small solid concrete soffit directly behind the artwork's lower edge.
  box(x0-0.025,f.muralBackingBaseY,f.mosaicStartZ,
      x1+0.035,f.arcadePierTopY,f.mosaicEndZ,'concrete');
  // Open bays with individual structural piers; avoid the old opaque wall.
  for(const z of f.arcadePierZ) {
    box(x0-0.14,l.ground,z,x1+0.14,f.arcadePierTopY,
        z+f.arcadePierLength,'stone');
    // Eroded column foot and staining use existing scene materials.
    box(x0-0.18,l.ground,z-0.04,x1+0.18,l.ground+0.13,
        z+f.arcadePierLength+0.04,'concrete');
    box(x0-0.145,l.ground+0.63,z+0.03,x0-0.135,
        l.ground+0.95,z+f.arcadePierLength-0.08,'rust');
  }
}
function m3dBuildPlaza({box}, p=M3D_PRIPYAT_BLOCKOUT.plaza){
  // Separate physical layers: foundation, pedestrian paving, expansion joints,
  // patch repairs and flush perimeter drainage.
  box(p.x0,-0.045,p.z0,p.x1,0.015,p.z1,'stone');
  for(let x=p.x0+1;x<p.x1-0.5;x+=3.2)
    box(x,0.016,p.z0+1,x+0.055,0.021,p.z1-1,'concrete');
  for(let z=p.z0+1;z<p.z1-0.5;z+=3.2)
    box(p.x0+1,0.016,z,p.x1-1,0.021,z+0.055,'concrete');
  for(const [x,z,w,d] of [[-42,-7,6,3],[-29,10,4,5],[-10,-4,7,3],
    [12,5,4,4],[34,-8,6,4],[41,9,3,5],[-5,29,5,4],[20,38,7,3],[-35,45,4,6]])
    box(x,0.018,z,x+w,0.023,z+d,'concrete');
  for(const x of [p.x0+0.3,p.x1-0.5])
    box(x,0.017,p.z0+1,x+0.22,0.026,p.z1-1,'rust');
}
function m3dBuildHousing({box},b){
  const {x0,x1,z0,z1,floors,frontZ}=b,storey=2.75,height=floors*storey;
  // Discrete structural parts, windows, balcony ledges, entrance and parapet.
  box(x0,0,z0,x1,0.45,z1,'stone');
  box(x0,0.45,z0,x1,height,z1,'concrete');
  for(let f=0;f<floors;f++){
    const y=1.1+f*storey;
    for(let x=x0+1.4;x<x1-1.1;x+=2.7){
      box(x,y,frontZ-0.1,x+1.25,y+1.25,frontZ+0.06,'windowdark');
      box(x-0.12,y-0.12,frontZ-0.16,x+1.37,y+1.38,frontZ-0.06,'stone');
    }
    box(x0,y+1.45,frontZ-0.05,x1,y+1.55,frontZ+0.07,'stone');
  }
  box(x0-0.2,height,z0-0.15,x1+0.2,height+0.75,z1+0.15,'concrete');
  for(let x=x0+2;x<x1;x+=8.1)
    box(x,height*0.56,frontZ-0.2,x+1.3,height*0.56+1.25,frontZ+0.08,'windowlit');
}
// Photo-informed connector between Energetik and Polissya: the central
// pedestrian opening follows a shallow segmented arch, not a solid prism.
// The footprint is provisional until the survey fixes both anchor points.
function m3dBuildConnector({box},a=M3D_ENERGETIK_ARCH){
  const left=a.envelope.x1-0.3, right=M3D_PRIPYAT_BLOCKOUT.polissya.x0+0.2;
  const z0=-24.0,z1=-18.6, roof=5.82,steps=18,dx=(right-left)/steps;
  // Side abutments and two horizontal cornice bands use existing materials.
  box(left,0,z0,left+0.72,roof,z1,'concrete');
  box(right-0.72,0,z0,right,roof,z1,'concrete');
  for(let i=0;i<steps;i++){
    const x=left+i*dx, normalized=(i+0.5)/steps*2-1;
    const intrados=3.48+1.12*Math.sqrt(Math.max(0,1-normalized*normalized));
    // Masonry ABOVE the pedestrian arch, leaving the underpass open.
    box(x,intrados,z0,x+dx+0.012,roof,z1,'concrete');
    box(x,intrados-0.08,z1-0.06,x+dx+0.012,intrados+0.025,z1+0.05,'stone');
  }
  box(left-0.15,roof,z0-0.2,right+0.15,roof+0.25,z1+0.2,'stone');
  // Corroded front balustrade, visibly separate from the load-bearing arch.
  for(let x=left+0.9;x<right-0.6;x+=0.95)
    box(x,roof+0.26,z1+0.035,x+0.055,roof+1.00,z1+0.09,'rust');
  box(left+0.6,roof+0.97,z1+0.025,right-0.6,roof+1.045,z1+0.13,'metal');
}

// Shared rear-wall bay grid: openings are actual holes in the masonry,
// aligned with the existing translucent/dark window planes.
function m3dBuildEnergetikRearWall({box}, a=M3D_ENERGETIK_ARCH){
  const e=a.envelope,l=a.levels, wallZ0=e.z0,wallZ1=e.z0+1.2;
  const first=-15.0,last=20.25;
  box(e.x0,0,wallZ0,first,l.roof,wallZ1,'concrete');
  box(last,0,wallZ0,e.x1,l.roof,wallZ1,'concrete');
  for(const [bottom,top] of [[0,1.0],[2.65,4.1],[5.85,7.1],[8.75,l.roof]])
    box(first,bottom,wallZ0,last,top,wallZ1,'concrete');
  let cursor=first;
  for(const x of a.rearWindowXs){
    if(x>cursor)box(cursor,1.0,wallZ0,x,8.75,wallZ1,'concrete');
    cursor=x+a.rearWindowWidth;
  }
  if(cursor<last)box(cursor,1.0,wallZ0,last,8.75,wallZ1,'concrete');
}
// The exterior shell is isolated from interiors and roof features.
// Its slabs and walls are intentionally separated to preserve camera access.
function m3dBuildEnergetikShell({box},cfg=M3D_PRIPYAT_BLOCKOUT.energetik){
  const ex0=cfg.x0,ex1=cfg.x1,front=cfg.z1,back=cfg.z0;
  const {levels,tower}=M3D_ENERGETIK_ARCH;
  box(ex0,0,back,ex0+1.2,9.4,front,'concrete');
  box(ex1-1.2,0,back,ex1,9.4,front,'concrete');
  m3dBuildEnergetikRearWall({box});
  box(ex0,0,-20.2,-46.8,9.4,front,'concrete');
  box(25.5,0,-20.2,ex1,9.4,front,'concrete');
  box(-46.8,8.8,-20.2,25.5,9.4,front,'stone');
  // Open rooftop volume: former solid block trapped the rooftop camera below
  // nearly two metres of concrete. Only a thin coping frames the roof.
  const copingY=cfg.mainHeight-0.25;
  box(-42.0,levels.roof+0.22,-41.8,-41.68,copingY,-20.4,'concrete');
  box(20.2,levels.roof+0.22,-41.8,20.5,copingY,-20.4,'concrete');
  box(-41.68,levels.roof+0.22,-41.8,20.2,copingY,-41.46,'concrete');
  box(-41.68,levels.roof+0.22,-20.72,20.2,copingY,-20.4,'concrete');
  // Hollow stair tower. The old solid prism capped the ascending camera and
  // made the roof door look like a wall; retain only the concrete shell.
  box(20.5,9.4,-40.8,21.7,tower.roofY,-22.2,'concrete');
  box(27.0,9.4,-40.8,30.5,tower.roofY,-22.2,'concrete');
  box(21.7,9.4,-40.8,27.0,tower.roofY,-39.6,'concrete');
  box(21.7,12.0,-22.2,25.3,tower.roofY,-21.0,'concrete');
  box(27.0,9.4,-22.2,30.5,tower.roofY,-21.0,'concrete');
  // Four thin coping strips retain the published external silhouette.
  box(-43.0,copingY,-42.5,18.4,copingY+0.20,-42.18,'stone');
  box(-43.0,copingY,-20.42,18.4,copingY+0.20,-20.1,'stone');
  box(-43.0,copingY,-42.18,-42.68,copingY+0.20,-20.42,'stone');
  box(18.08,copingY,-42.18,18.4,copingY+0.20,-20.42,'stone');


}


function m3dBuildEnergetikArchitecturalAccents({box,capsule}){
  // Photo-guided facade segmentation. Provisional detailing: avoids sealing
  // the foyer and preserves the existing stair and auditorium paths.
  // Two entrance wings and a distinct central recessed vestibule.
  for(const x of [-12.2,11.1]){
    box(x,0.2,-20.62,x+1.1,3.25,-19.72,'stone');
    box(x-0.18,3.25,-20.83,x+1.28,3.52,-19.46,'concrete');
  }
  // Horizontal entrance canopy split into structural elements.
  box(-11.1,3.35,-19.45,11.2,3.60,-17.95,'concrete');
  box(-11.3,3.59,-19.53,11.4,3.69,-17.88,'stone');
  for(const x of [-10.0,-5.1,0,5.1,10.0]){
    box(x-0.075,0.45,-19.47,x+0.075,3.35,-19.27,'metal');
    box(x-0.48,3.0,-19.51,x+0.48,3.15,-19.24,'rust');
  }
  // Facade bays remain open to the original interior; no opaque wall placed.
  // Stair-tower upper parapet and rear roof mechanical housing.
  box(20.45,13.95,-40.95,30.6,14.28,-40.5,'stone');
  box(20.45,13.95,-22.35,30.6,14.28,-21.95,'stone');
  box(22.2,11.55,-37.5,25.6,12.9,-35.2,'concrete');
  box(22.0,12.9,-37.7,25.8,13.03,-35.0,'stone');
  // Thin facade scars and metal remnants, not a repetitive decorative grid.
  for(const [x,y] of [[-42.5,2.2],[-29.4,4.7],[-16.8,6.1],[7.3,5.8],[19.2,3.1]]){
    capsule([x,y,-20.86],[x+0.4,y+0.24,-20.87],0.022,'rust');
  }
}

function m3dBuildEnergetikFacade({box,roundBox,objects}){
  // Projecting plinth, pilasters, capitals and layered cornice give the civic
  // façade real relief. The large bays stay open: the reference facade has no glazing.
  box(-48.0,0.0,-20.55,26.9,0.72,-19.25,'stone');
  box(-47.0,8.7,-20.48,25.6,9.02,-19.42,'concrete');
  box(-47.6,9.02,-20.74,26.2,9.28,-19.26,'stone');
  box(-47.9,9.28,-20.94,26.5,9.48,-19.08,'concrete');
  for (const x of [-45.8,-39.0,-32.2,-25.4,-18.6,-11.8,-5.0,1.8,8.6,15.4,22.2]) {
    if (x === -39.0) {
      buildEnergetikFacadeBay({ objects, roundBox }, x, 0, -19.63, { nativeMaterial: 'concrete' });
      continue;
    }
    box(x-0.42,0.40,-20.34,x+0.42,0.78,-19.34,'stone');
    box(x-0.27,0.78,-20.16,x+0.27,8.25,-19.54,'concrete');
    box(x-0.48,8.25,-20.42,x+0.48,8.62,-19.28,'stone');
    box(x-0.58,8.62,-20.50,x+0.58,8.78,-19.20,'concrete');
  }

  // Separate second-storey gallery bays, reveals and structural transoms.
  // No glazing is inserted into the abandoned facade: preserve sightlines
  // into the existing atrium and the walk-through camera path.
  const columns=[-45.8,-39.0,-32.2,-25.4,-18.6,-11.8,-5.0,1.8,8.6,15.4,22.2];
  for(let bay=0;bay<columns.length-1;bay++){
    const left=columns[bay]+0.40,right=columns[bay+1]-0.40;
    if(right<=left)continue;
    // Upper and lower reveals, with staggered weathering by bay.
    box(left,3.20,-20.26,right,3.34,-19.62,'stone');
    box(left,6.35,-20.26,right,6.49,-19.62,'stone');
    if(bay%3!==1){
      box(left+0.12,3.34,-20.17,left+0.23,6.35,-19.78,'rust');
      box(right-0.23,3.34,-20.17,right-0.12,6.35,-19.78,'rust');
    }
    // Partial vertical frame fragments, deliberately not closed windows.
    if(bay%4===0)box((left+right)*0.5-0.045,6.5,-20.11,(left+right)*0.5+0.045,8.2,-19.82,'metal');
  }
}

function m3dBuildPolissya({box,capsule}, cfg = M3D_PRIPYAT_BLOCKOUT.polissya) {
  const x0=cfg.x0??40.0, x1=cfg.x1??63.0, z0=cfg.z0??-42.0, z1=cfg.z1??-20.0;
  const height=cfg.height??15.5;
  // Separate podium, tower, recessed roof and facade components.
  box(x0,0,z0,x1,1.05,z1,'stone');
  // Hollow hotel shell: the former 22 × 23 × 14.5 m solid concrete prism
  // physically blocked all floors, rear windows and potential entry passages.
  // These openings follow existing facade bays; room layout is provisional.
  const wall=0.52;
  const westDoorZ0=-25.55,westDoorZ1=-21.0,westDoorTop=4.15;
  box(x0,1.05,z0,x0+wall,height,westDoorZ0,'concrete');
  box(x0,1.05,westDoorZ1,x0+wall,height,z1,'concrete');
  box(x0,westDoorTop,westDoorZ0,x0+wall,height,westDoorZ1,'concrete');
  box(x1-wall,1.05,z0,x1,height,z1,'concrete');
  box(x0+wall,1.05,z0,x1-wall,height,z0+wall,'concrete');
  const windowXs=[];
  for(let x=x0+2.0;x<x1-2.0;x+=3.1) windowXs.push(x);
  let span=x0+wall;
  for(const x of windowXs){
    if(x>span)box(span,1.05,z1-wall,x,height,z1,'concrete');
    let vertical=1.05;
    for(let floor=0;floor<5;floor++){
      const bottom=1.8+floor*2.65, top=bottom+1.55;
      if(bottom>vertical)box(x,vertical,z1-wall,x+1.45,bottom,z1,'concrete');
      vertical=top;
    }
    if(vertical<height)box(x,vertical,z1-wall,x+1.45,height,z1,'concrete');
    span=x+1.45;
  }
  if(span<x1-wall)box(span,1.05,z1-wall,x1-wall,height,z1,'concrete');
  // Independent thin hotel floor slabs, not five overlapping solid blocks.
  for(let floor=1;floor<=5;floor++){
    const y=Math.min(height-0.20,1.05+floor*2.65);
    box(x0+wall,y,z0+wall,x1-wall,y+0.14,z1-wall,'concrete');
  }
  // Photograph-inspired rooftop belvedere: the old solid penthouse cube
  // incorrectly filled the open terrace with concrete.
  box(x0+10.5,height,z0+1.4,x1-0.45,height+3.50,z0+4.0,'concrete');
  box(x0+10.5,height+3.50,z0+1.4,x1+0.1,height+3.80,z1+0.15,'stone');
  for(const x of [x0+12.2,x0+16.0,x0+19.8])
    box(x,height+0.12,z1-0.65,x+0.27,height+3.50,z1-0.28,'plaster');
  // Open terrace parapet and metal cap on the visible front edge.
  box(x0+10.5,height+0.15,z1-0.12,x1+0.1,height+0.90,z1+0.15,'concrete');
  box(x0+10.5,height+0.90,z1-0.18,x1+0.1,height+0.99,z1+0.19,'rust');
  box(x0-0.15,height-0.18,z0-0.15,x1+0.15,height+0.22,z1+0.15,'stone');
  // Front facade: windows as separate reusable bays.
  for(let floor=0;floor<5;floor++){
    const y=1.8+floor*2.65;
    for(let x=x0+2.0;x<x1-2.0;x+=3.1){
      box(x,y,z1+0.08,x+1.45,y+1.55,z1+0.20,(floor===2&&x>x0+11)?'windowlit':'windowdark');
      box(x-0.13,y-0.13,z1+0.06,x+1.58,y-0.04,z1+0.23,'stone');
      box(x-0.13,y+1.55,z1+0.06,x+1.58,y+1.67,z1+0.23,'stone');
    }
  }
  // Recessed ribbing and worn pale cladding seen in Polissya photographs.
  // Keep the five horizontal rows of existing window materials untouched.
  for(let x=x0+0.56;x<x1-0.4;x+=1.0)
    box(x,1.12,z1+0.16,x+0.065,height-0.20,z1+0.24,'plaster');
  for(const y of [4.2,6.85,9.5,12.15])
    box(x0+0.30,y,z1+0.12,x1-0.30,y+0.055,z1+0.27,'stone');

  // Entry canopy and vertical side facade bays.
  box(x0+8.0,2.85,z1+0.15,x0+15.0,3.13,z1+2.0,'concrete');
  for(let floor=0;floor<4;floor++)for(let z=z0+2.0;z<z1-2.0;z+=4.0)
    box(x1+0.06,2.2+floor*3.0,z,x1+0.18,3.7+floor*3.0,z+1.55,'windowdark');
  // Rooftop sign remains rusty, not actively lit in the abandoned era.
  // 5×7 stroke pixels are an original geometric asset (no imported font).
  const letters={
    'Г':['11111','10000','10000','10000','10000','10000','10000'],
    'О':['01110','10001','10001','10001','10001','10001','01110'],
    'Т':['11111','00100','00100','00100','00100','00100','00100'],
    'Е':['11111','10000','10000','11110','10000','10000','11111'],
    'Л':['00111','01001','10001','10001','10001','10001','10001'],
    'Ь':['10000','10000','10000','11110','10001','10001','11110'],
    'П':['11111','10001','10001','10001','10001','10001','10001'],
    'І':['11111','00100','00100','00100','00100','00100','11111'],
    'С':['01111','10000','10000','10000','10000','10000','01111'],
    'Я':['01111','10001','10001','01111','00101','01001','10001']
  };
  const label='ГОТЕЛЬ ПОЛІССЯ', px=0.095, pitch=0.72, signX=x0+1.1, signY=height+0.45;
  for(let k=0;k<label.length;k++){
    const glyph=letters[label[k]];
    if(!glyph) continue;
    for(let row=0;row<7;row++)for(let col=0;col<5;col++)if(glyph[row][col]==='1'){
      const sx=signX+k*pitch+col*px, sy=signY+(6-row)*px;
      box(sx,sy,z1-0.35,sx+px*0.80,sy+px*0.80,z1-0.27,'rust');
    }
  }
}


// Terrain and road extension for the Pripyat scene (2 km radius from Energetik).
// Streets and paths are clipped from OpenStreetMap (ODbL), snapshot 2026-11-05T23:29:21Z.
// Geographic-to-scene fit uses the existing Energetik/Polissya center spacing; y is
// relative to the central SRTM30m height, blended flat across the existing city core.
const M3D_PRIPYAT_EXPANSION_DATA = {"grid":[[null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,111.0,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,null,null,null,null,111.0,110.0,111.0,109.0,111.0,110.0,108.0,109.0,109.0,109.0,111.0,114.0,113.0,null,null,null,null,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,null,null,109.0,110.0,109.0,111.0,111.0,109.0,110.0,110.0,110.0,109.0,112.0,109.0,111.0,112.0,111.0,113.0,112.0,null,null,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,110.0,110.0,110.0,111.0,110.0,111.0,109.0,110.0,110.0,110.0,111.0,111.0,111.0,109.0,111.0,110.0,111.0,110.0,111.0,111.0,113.0,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,110.0,110.0,110.0,112.0,108.0,110.0,109.0,108.0,110.0,110.0,112.0,110.0,112.0,112.0,111.0,110.0,111.0,110.0,110.0,111.0,111.0,111.0,110.0,109.0,108.0,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,110.0,111.0,110.0,110.0,112.0,113.0,112.0,110.0,112.0,114.0,112.0,112.0,114.0,111.0,110.0,111.0,112.0,114.0,113.0,113.0,111.0,111.0,111.0,112.0,111.0,110.0,109.0,null,null,null,null,null,null,null],[null,null,null,null,null,null,112.0,112.0,112.0,112.0,111.0,113.0,113.0,112.0,112.0,111.0,112.0,112.0,114.0,112.0,111.0,113.0,112.0,113.0,113.0,114.0,113.0,112.0,111.0,113.0,110.0,112.0,110.0,110.0,110.0,null,null,null,null,null,null],[null,null,null,null,null,111.0,112.0,110.0,111.0,114.0,111.0,110.0,114.0,118.0,113.0,112.0,112.0,115.0,116.0,117.0,116.0,115.0,116.0,115.0,115.0,115.0,118.0,113.0,113.0,113.0,112.0,112.0,112.0,111.0,112.0,111.0,null,null,null,null,null],[null,null,null,null,111.0,112.0,112.0,113.0,110.0,111.0,114.0,111.0,118.0,120.0,119.0,111.0,113.0,116.0,112.0,116.0,115.0,119.0,115.0,112.0,116.0,120.0,119.0,116.0,116.0,116.0,112.0,111.0,111.0,111.0,111.0,112.0,112.0,null,null,null,null],[null,null,null,null,111.0,113.0,112.0,114.0,115.0,115.0,112.0,113.0,112.0,120.0,120.0,113.0,114.0,116.0,113.0,113.0,117.0,119.0,114.0,114.0,113.0,120.0,118.0,111.0,114.0,112.0,112.0,112.0,113.0,112.0,110.0,113.0,114.0,null,null,null,null],[null,null,null,112.0,113.0,112.0,113.0,110.0,110.0,112.0,114.0,112.0,112.0,111.0,112.0,112.0,112.0,119.0,117.0,118.0,118.0,119.0,114.0,117.0,116.0,115.0,112.0,112.0,111.0,109.0,110.0,111.0,111.0,113.0,114.0,112.0,114.0,114.0,null,null,null],[null,null,null,111.0,111.0,114.0,114.0,110.0,109.0,108.0,108.0,111.0,113.0,112.0,114.0,115.0,111.0,114.0,115.0,120.0,118.0,115.0,114.0,116.0,112.0,111.0,113.0,109.0,112.0,116.0,114.0,114.0,114.0,113.0,112.0,113.0,113.0,115.0,null,null,null],[null,null,110.0,111.0,113.0,115.0,113.0,110.0,108.0,109.0,110.0,116.0,120.0,113.0,113.0,110.0,110.0,111.0,114.0,115.0,112.0,115.0,117.0,114.0,116.0,111.0,111.0,112.0,115.0,118.0,120.0,115.0,114.0,115.0,114.0,113.0,114.0,115.0,113.0,null,null],[null,null,108.0,110.0,110.0,111.0,112.0,109.0,107.0,110.0,110.0,112.0,120.0,116.0,112.0,110.0,112.0,112.0,112.0,112.0,113.0,115.0,114.0,116.0,114.0,112.0,115.0,116.0,120.0,120.0,120.0,119.0,114.0,114.0,114.0,115.0,113.0,113.0,114.0,null,null],[null,108.0,110.0,118.0,108.0,107.0,114.0,114.0,110.0,111.0,110.0,111.0,115.0,111.0,114.0,111.0,111.0,111.0,111.0,112.0,114.0,114.0,115.0,116.0,115.0,116.0,116.0,119.0,120.0,120.0,120.0,116.0,114.0,113.0,115.0,118.0,114.0,113.0,112.0,111.0,null],[null,107.0,109.0,109.0,107.0,108.0,112.0,118.0,114.0,109.0,112.0,112.0,115.0,115.0,114.0,114.0,114.0,112.0,113.0,115.0,112.0,115.0,116.0,120.0,115.0,117.0,115.0,116.0,120.0,120.0,120.0,116.0,113.0,114.0,115.0,116.0,114.0,114.0,113.0,111.0,null],[null,108.0,108.0,108.0,107.0,108.0,117.0,120.0,115.0,116.0,116.0,113.0,117.0,115.0,113.0,114.0,117.0,116.0,113.0,115.0,112.0,114.0,117.0,116.0,118.0,116.0,117.0,114.0,118.0,120.0,120.0,116.0,112.0,116.0,114.0,114.0,113.0,113.0,113.0,113.0,null],[null,108.0,107.0,107.0,107.0,110.0,110.0,119.0,120.0,120.0,114.0,114.0,116.0,110.0,110.0,117.0,119.0,118.0,116.0,116.0,114.0,114.0,114.0,119.0,116.0,116.0,116.0,114.0,115.0,118.0,120.0,115.0,115.0,115.0,115.0,114.0,115.0,108.0,104.0,106.0,null],[null,108.0,109.0,106.0,109.0,111.0,117.0,120.0,120.0,116.0,114.0,114.0,112.0,115.0,116.0,114.0,117.0,120.0,117.0,115.0,113.0,114.0,116.0,116.0,113.0,113.0,116.0,114.0,116.0,113.0,117.0,115.0,113.0,114.0,113.0,113.0,102.0,102.0,102.0,102.0,null],[null,108.0,107.0,107.0,110.0,117.0,118.0,120.0,114.0,112.0,115.0,115.0,116.0,118.0,115.0,118.0,116.0,119.0,116.0,115.0,115.0,113.0,114.0,114.0,114.0,113.0,112.0,114.0,115.0,113.0,113.0,113.0,112.0,112.0,110.0,102.0,102.0,102.0,102.0,102.0,null],[111.0,113.0,111.0,108.0,114.0,120.0,118.0,110.0,111.0,112.0,112.0,113.0,111.0,113.0,114.0,113.0,113.0,118.0,114.0,112.0,113.0,113.0,113.0,110.0,111.0,113.0,112.0,114.0,113.0,112.0,111.0,112.0,113.0,117.0,102.0,102.0,102.0,102.0,102.0,102.0,102.0],[null,114.0,111.0,113.0,114.0,116.0,114.0,110.0,111.0,110.0,114.0,110.0,110.0,113.0,112.0,115.0,115.0,115.0,112.0,112.0,112.0,111.0,110.0,110.0,108.0,110.0,107.0,111.0,110.0,110.0,110.0,112.0,113.0,111.0,102.0,102.0,102.0,102.0,102.0,102.0,null],[null,113.0,112.0,114.0,112.0,112.0,112.0,111.0,108.0,109.0,112.0,113.0,111.0,111.0,112.0,115.0,117.0,111.0,111.0,114.0,112.0,112.0,110.0,111.0,111.0,109.0,105.0,103.0,102.0,106.0,105.0,105.0,104.0,103.0,102.0,102.0,102.0,102.0,102.0,102.0,null],[null,112.0,110.0,112.0,115.0,112.0,111.0,112.0,109.0,108.0,111.0,113.0,113.0,109.0,112.0,112.0,117.0,111.0,111.0,109.0,112.0,113.0,111.0,111.0,106.0,110.0,109.0,103.0,108.0,106.0,107.0,106.0,103.0,104.0,103.0,105.0,106.0,102.0,102.0,102.0,null],[null,113.0,112.0,112.0,113.0,111.0,111.0,109.0,107.0,107.0,111.0,111.0,112.0,115.0,115.0,113.0,111.0,114.0,112.0,111.0,113.0,114.0,112.0,109.0,109.0,107.0,114.0,104.0,108.0,107.0,108.0,108.0,109.0,102.0,102.0,106.0,108.0,108.0,104.0,102.0,null],[null,113.0,115.0,115.0,112.0,109.0,110.0,111.0,109.0,109.0,112.0,111.0,111.0,112.0,114.0,115.0,111.0,113.0,112.0,112.0,110.0,111.0,110.0,109.0,108.0,104.0,106.0,101.0,110.0,104.0,107.0,110.0,108.0,106.0,103.0,109.0,111.0,108.0,106.0,102.0,null],[null,114.0,115.0,113.0,112.0,111.0,110.0,110.0,111.0,109.0,109.0,109.0,112.0,114.0,110.0,108.0,112.0,113.0,112.0,113.0,110.0,109.0,108.0,109.0,106.0,105.0,108.0,109.0,110.0,111.0,108.0,109.0,105.0,103.0,104.0,107.0,110.0,108.0,105.0,104.0,null],[null,null,115.0,112.0,110.0,112.0,111.0,111.0,109.0,110.0,109.0,110.0,107.0,109.0,108.0,108.0,110.0,112.0,111.0,114.0,111.0,112.0,111.0,110.0,108.0,109.0,107.0,108.0,108.0,111.0,109.0,108.0,108.0,105.0,104.0,105.0,104.0,105.0,105.0,null,null],[null,null,108.0,108.0,107.0,106.0,107.0,109.0,110.0,111.0,109.0,110.0,106.0,109.0,110.0,108.0,112.0,110.0,111.0,114.0,110.0,112.0,109.0,109.0,109.0,113.0,108.0,104.0,108.0,108.0,108.0,109.0,104.0,105.0,105.0,106.0,103.0,105.0,107.0,null,null],[null,null,null,106.0,104.0,104.0,104.0,103.0,106.0,111.0,110.0,108.0,106.0,108.0,109.0,109.0,109.0,106.0,108.0,110.0,109.0,110.0,107.0,105.0,111.0,109.0,109.0,109.0,109.0,108.0,109.0,108.0,109.0,104.0,106.0,107.0,104.0,105.0,null,null,null],[null,null,null,103.0,104.0,103.0,103.0,104.0,101.0,103.0,105.0,108.0,108.0,104.0,101.0,101.0,101.0,101.0,101.0,103.0,108.0,110.0,111.0,106.0,110.0,108.0,108.0,109.0,105.0,109.0,109.0,108.0,109.0,108.0,106.0,106.0,105.0,106.0,null,null,null],[null,null,null,null,102.0,102.0,102.0,104.0,104.0,106.0,102.0,102.0,101.0,101.0,101.0,103.0,105.0,104.0,105.0,102.0,101.0,107.0,109.0,109.0,108.0,110.0,108.0,109.0,106.0,108.0,106.0,106.0,106.0,106.0,106.0,106.0,103.0,null,null,null,null],[null,null,null,null,104.0,104.0,103.0,102.0,101.0,103.0,103.0,102.0,102.0,101.0,101.0,105.0,106.0,102.0,105.0,102.0,103.0,101.0,103.0,106.0,106.0,106.0,107.0,106.0,106.0,105.0,104.0,104.0,105.0,106.0,106.0,104.0,101.0,null,null,null,null],[null,null,null,null,null,103.0,105.0,105.0,106.0,106.0,102.0,101.0,101.0,104.0,101.0,103.0,104.0,104.0,104.0,103.0,102.0,102.0,101.0,102.0,103.0,103.0,105.0,105.0,105.0,105.0,106.0,103.0,103.0,103.0,102.0,101.0,null,null,null,null,null],[null,null,null,null,null,null,106.0,105.0,103.0,104.0,103.0,101.0,102.0,104.0,101.0,101.0,105.0,101.0,102.0,104.0,103.0,103.0,104.0,101.0,101.0,104.0,104.0,103.0,100.0,103.0,103.0,103.0,101.0,102.0,101.0,null,null,null,null,null,null],[null,null,null,null,null,null,null,104.0,106.0,105.0,103.0,103.0,105.0,105.0,104.0,101.0,102.0,105.0,105.0,106.0,105.0,105.0,105.0,101.0,101.0,101.0,102.0,103.0,104.0,104.0,101.0,104.0,101.0,101.0,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,104.0,103.0,104.0,103.0,103.0,101.0,103.0,103.0,102.0,103.0,104.0,105.0,105.0,106.0,107.0,103.0,103.0,101.0,102.0,103.0,104.0,104.0,101.0,101.0,101.0,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,102.0,106.0,106.0,101.0,103.0,105.0,103.0,102.0,103.0,103.0,104.0,104.0,104.0,107.0,106.0,101.0,102.0,104.0,104.0,101.0,101.0,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,null,null,107.0,102.0,103.0,103.0,101.0,103.0,103.0,104.0,104.0,103.0,106.0,105.0,105.0,104.0,105.0,102.0,101.0,null,null,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,null,null,null,null,104.0,105.0,103.0,103.0,104.0,105.0,104.0,105.0,104.0,104.0,104.0,103.0,101.0,null,null,null,null,null,null,null,null,null,null,null,null,null,null],[null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,101.0,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null,null]],"routes":[{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[330.4,884.2],[-171.0,816.8],[-456.8,837.2]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-646.2,615.1],[-682.4,677.0]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-152.4,-138.4],[-47.6,-185.0],[103.9,-164.2]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-312.0,-505.3],[-289.0,-461.3],[-241.2,-357.9]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-512.6,-479.8],[-502.0,-446.4],[-434.6,-279.7]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-320.1,173.4],[-392.4,255.6],[-402.8,273.7],[-468.7,364.4]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[278.7,-15.8],[257.6,94.4],[238.2,180.9]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[269.8,661.9],[284.5,699.0]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-161.0,-155.7],[-428.5,-44.9],[-508.9,-13.8]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[-161.0,-155.7],[-152.4,-138.4],[-60.1,87.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[251.8,-223.7],[242.6,-240.7],[215.7,-262.6],[183.1,-280.1],[151.5,-284.0],[127.9,-281.6],[90.2,-287.7]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[20.7,34.5],[-67.8,68.3]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[9.5,38.7],[136.9,352.1],[150.2,358.5]]]},{"h":"tertiary_link","m":"asphalt","w":3.91,"p":[[[82.8,218.4],[92.7,214.8]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-468.7,364.4],[-536.6,456.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-604.3,95.1],[-563.7,142.1],[-560.5,142.3],[-541.5,127.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-487.0,-23.3],[-485.3,-18.1],[-486.8,-14.2],[-501.1,-5.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[122.0,-160.5],[94.2,-253.8],[90.2,-287.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-597.6,89.6],[-623.1,59.1],[-608.7,48.3],[-582.6,77.3],[-511.1,165.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-413.7,90.5],[-501.4,162.9],[-511.1,165.9],[-517.5,176.0],[-532.1,185.5],[-541.5,185.7],[-570.9,166.9],[-577.7,169.4],[-579.4,168.3],[-623.3,115.8],[-623.9,112.2],[-500.4,7.8],[-498.7,1.9],[-501.1,-5.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-532.0,138.9],[-525.4,138.6],[-472.8,96.4],[-470.7,91.8],[-522.3,25.9]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[12.8,834.8],[-48.9,727.6],[-49.9,716.2],[-44.5,685.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-41.6,667.6],[-20.6,668.6],[-18.0,656.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[276.0,423.3],[277.4,435.9],[280.3,439.0],[284.7,439.8],[286.9,434.9],[276.0,423.3],[261.3,386.2],[247.4,363.1]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-168.1,689.9],[-174.6,708.9],[-198.4,802.2],[-199.3,818.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-1.2,681.0],[16.1,680.2],[36.7,682.1],[62.5,677.0]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-50.0,721.2],[-137.6,698.7],[-178.4,685.5],[-181.7,683.8],[-185.6,677.8],[-190.0,653.5]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[22.1,37.9],[188.0,-32.5],[206.7,-31.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[468.4,-821.1],[440.6,-814.6],[436.6,-803.6],[436.9,-736.7],[431.1,-718.1],[421.1,-705.9],[380.1,-669.5],[353.7,-649.2],[320.8,-617.3],[244.9,-555.3],[207.7,-549.9],[200.0,-545.5],[191.4,-532.2],[185.0,-525.8],[126.8,-508.4],[80.7,-488.6]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[541.0,93.1],[538.2,96.2]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[187.1,455.9],[243.7,500.2]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-210.4,279.7],[-229.2,303.9],[-245.3,330.9],[-251.9,332.7],[-258.3,328.6]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[52.8,-170.1],[19.1,-33.2]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-5.2,685.4],[-42.9,739.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-18.9,663.4],[50.9,671.3],[60.7,674.9],[62.8,679.1],[59.8,684.1],[55.6,686.5],[16.7,687.1],[-5.2,685.4],[-43.6,676.5],[-79.7,662.4],[-92.8,655.1],[-94.7,650.6],[-92.8,645.5],[-86.4,646.5],[-79.7,662.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[8.8,478.4],[-6.0,575.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-18.7,-180.7],[-4.7,-142.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-101.9,-12.5],[-162.1,13.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-104.9,281.7],[-132.8,314.2]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[104.9,-0.4],[97.8,-16.9],[92.9,-19.4],[36.3,5.7],[29.3,20.7],[27.3,32.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-502.0,-446.4],[-483.7,-452.4],[-481.1,-457.6],[-507.1,-549.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-95.9,305.4],[-110.4,324.7],[-108.2,329.9],[-115.3,340.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-108.2,329.9],[-90.5,343.0],[-77.9,327.7],[-90.0,310.6],[-134.5,271.7],[-145.8,284.9],[-124.2,303.5]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[101.0,234.6],[175.1,207.1],[238.2,180.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[509.1,118.6],[534.8,132.1],[585.4,167.3],[617.8,200.0],[620.1,222.0],[615.2,255.2],[609.1,334.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-177.3,208.8],[-229.2,261.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[878.5,374.1],[855.7,371.7],[817.8,373.0],[796.6,364.6]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[32.3,420.5],[67.7,402.3],[69.4,398.8],[69.2,389.6],[138.9,354.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[63.5,-49.5],[177.7,-109.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-103.3,-15.8],[-53.1,-37.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-271.9,113.3],[-283.6,133.2],[-299.3,135.1],[-310.3,144.9],[-311.8,150.2],[-302.3,148.4],[-298.7,141.0],[-299.3,135.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[796.6,364.6],[762.0,339.7],[740.3,327.0],[700.8,309.1],[662.5,287.8],[625.6,271.3],[614.8,261.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[536.7,734.4],[578.4,735.0],[592.7,730.6],[600.6,731.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-346.4,202.3],[-388.8,167.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-397.0,-12.8],[-383.4,-25.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-202.1,184.8],[-209.9,193.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[644.4,527.6],[671.4,531.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-279.2,181.8],[-290.8,151.9],[-283.6,133.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-249.8,-118.6],[-262.1,-150.9],[-264.9,-153.2],[-314.8,-134.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[645.0,596.2],[644.9,592.5],[648.0,588.5],[647.6,543.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[726.7,320.5],[728.8,357.5],[739.8,395.5],[746.7,403.3],[745.8,427.4],[747.6,444.6],[756.4,477.2],[748.6,516.0],[752.0,546.6],[756.1,555.1],[769.6,557.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[363.5,583.2],[387.7,573.8],[381.5,550.3]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-231.8,-126.1],[-220.8,-112.9],[-219.3,-110.0],[-220.9,-105.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-358.9,26.2],[-372.3,35.7],[-392.0,55.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[619.0,503.6],[626.5,503.3],[632.1,505.6],[627.9,517.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[316.8,507.6],[327.4,549.0],[334.6,564.1],[352.1,618.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-663.1,-282.6],[-643.9,-304.3],[-590.0,-327.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-21.3,629.0],[-15.0,627.4],[-11.9,624.3],[-6.0,575.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[334.6,564.1],[347.0,577.8],[363.5,583.2],[388.4,638.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[18.7,477.7],[27.7,460.9],[56.5,442.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-52.4,333.6],[-58.1,350.1],[-76.3,364.5],[-77.3,368.1],[-72.8,374.2]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[556.2,570.9],[566.1,570.3],[671.8,580.6],[708.6,566.7],[711.3,554.8]]]},{"h":"track","m":"stone","w":1.95,"p":[[[633.3,541.6],[644.4,527.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-353.2,210.9],[-317.1,263.6],[-305.0,256.4],[-299.0,250.0],[-322.5,215.7],[-325.9,215.0],[-336.0,222.1],[-329.2,232.8],[-323.8,233.8],[-314.1,247.2],[-323.1,254.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-246.2,244.3],[-238.9,234.9],[-238.1,229.9],[-251.5,202.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[622.4,519.9],[618.9,512.5],[619.0,503.6],[622.9,484.0],[622.5,481.3],[615.8,478.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[171.6,-521.1],[196.1,-478.2],[208.4,-471.2],[267.0,-456.9],[282.8,-454.7],[309.6,-446.1],[352.5,-440.8],[465.2,-444.4],[538.4,-441.8],[550.0,-436.3],[572.1,-418.0],[602.5,-399.1],[614.7,-393.8],[628.2,-391.1],[681.2,-388.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-115.3,-44.6],[-111.9,-46.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[717.6,316.1],[715.5,287.2],[711.7,264.1],[708.5,259.6],[663.8,221.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[711.2,543.0],[704.5,532.2],[704.8,523.6],[702.2,514.5],[704.1,513.1],[705.6,495.6],[707.5,491.5],[705.7,487.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[620.1,222.0],[629.3,224.4],[641.2,222.1],[667.3,241.9],[670.9,241.7],[677.3,233.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-867.7,435.7],[-846.3,429.4],[-837.5,429.1],[-713.7,504.3]]]},{"h":"track","m":"stone","w":1.95,"p":[[[244.9,-555.3],[224.9,-538.4],[228.1,-532.9],[252.4,-521.1],[306.6,-503.4],[334.7,-488.0],[344.6,-480.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[756.1,555.1],[750.2,553.6],[746.8,555.7],[751.2,580.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-593.2,-211.6],[-577.7,-227.5],[-541.1,-240.9],[-539.9,-243.4],[-558.2,-291.0],[-561.9,-296.3],[-566.3,-298.5],[-571.3,-298.0],[-603.7,-285.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-120.1,112.3],[-102.9,152.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-50.0,433.2],[-45.0,430.5],[-41.0,424.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[163.2,45.5],[169.3,42.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[409.3,190.9],[399.9,166.2],[375.2,176.6],[369.7,163.3]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-125.2,361.8],[-160.9,405.3],[-191.6,449.8],[-193.7,455.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-202.6,454.6],[-224.4,459.3],[-268.7,460.2],[-308.8,457.0],[-348.2,458.3],[-396.4,493.3],[-434.8,526.5],[-433.0,531.4],[-422.5,539.4],[-402.0,545.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[314.9,185.8],[317.6,181.3],[314.6,175.4],[314.2,169.1],[323.3,160.9],[304.9,119.3],[299.9,119.0],[291.2,124.3],[271.4,131.4],[263.5,131.1],[250.6,125.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-427.0,-144.4],[-439.2,-164.3],[-446.0,-169.6],[-469.4,-231.1],[-484.4,-225.4],[-448.0,-136.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-396.4,493.3],[-373.0,521.6],[-340.7,525.9],[-333.6,529.3],[-331.5,533.8],[-333.7,554.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[611.7,301.7],[597.2,297.7],[525.5,244.7],[478.2,213.5],[457.2,209.4],[440.0,198.8],[435.6,183.6],[430.7,176.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[494.5,223.9],[469.3,240.6],[411.0,327.4],[387.6,356.4],[356.5,406.1],[303.3,484.1],[305.5,492.0],[313.3,495.2],[320.6,503.2],[321.5,508.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-10.2,616.4],[-176.4,593.1],[-223.9,585.4],[-259.1,576.6],[-292.0,582.0],[-368.2,564.7],[-392.8,565.3],[-441.8,560.3],[-517.4,509.0],[-533.0,502.6],[-537.5,502.8],[-541.4,498.7],[-544.2,494.7],[-536.6,456.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-94.3,653.0],[-103.2,653.7],[-131.4,639.5],[-147.9,635.9],[-163.0,639.0],[-190.0,653.5],[-233.7,647.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[921.9,-322.9],[681.2,-388.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[270.0,875.5],[263.9,906.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-255.8,822.5],[-254.4,791.4],[-246.9,759.8]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[-241.2,-357.9],[-161.0,-155.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[49.5,-286.7],[35.5,-172.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[40.2,-212.3],[77.5,-206.3],[74.0,-167.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-67.0,-377.0],[-35.3,-388.6],[-27.3,-413.0],[11.9,-429.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-163.4,-358.5],[-169.1,-354.4],[-170.3,-351.0],[-158.8,-322.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-169.1,-354.4],[-199.2,-342.5],[-203.9,-327.7],[-179.8,-265.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-84.9,-301.7],[-202.5,-258.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-5.2,-422.0],[3.3,-402.1]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[-227.8,157.6],[-60.1,87.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[645.0,524.6],[646.9,522.7],[646.0,480.8],[647.8,462.8],[661.9,440.8],[669.8,440.1],[677.6,442.6],[691.1,450.2],[711.5,491.3],[713.7,501.7],[711.0,549.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[548.0,517.4],[569.5,502.8],[575.8,495.0],[595.1,488.3],[599.7,462.6],[609.3,449.7],[615.0,431.9],[627.8,415.9],[633.4,412.3],[649.1,408.5],[659.6,409.3],[685.3,418.8],[718.7,427.6],[724.7,432.4],[731.5,484.7],[736.0,550.0],[744.7,552.4],[746.8,555.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[647.6,543.4],[645.0,524.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[647.9,555.7],[616.7,551.3],[616.6,545.8]]]},{"h":"track","m":"stone","w":1.95,"p":[[[711.0,549.0],[615.4,539.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[615.4,539.7],[623.3,529.6],[624.4,525.6],[622.4,519.9],[636.7,517.4],[639.4,521.0],[642.5,521.4],[642.4,524.2],[645.0,524.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[447.9,605.4],[441.1,595.1],[441.8,585.8],[444.5,582.2],[466.2,567.2],[470.5,561.6],[473.8,548.3],[469.4,527.6],[472.2,527.8],[472.9,526.1]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[558.7,538.6],[547.1,513.2],[557.4,496.8],[556.0,491.9],[570.1,460.5],[575.1,422.7],[563.3,408.4],[548.8,403.1],[543.3,404.6],[536.0,410.3],[525.9,425.3],[523.4,439.0],[522.0,474.4],[524.0,481.0],[547.1,513.2],[540.3,519.9],[517.3,529.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-272.0,23.2],[-256.4,63.6],[-257.7,66.4],[-272.9,72.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[323.3,160.9],[341.8,174.8],[361.2,223.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-334.9,49.7],[-268.7,21.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[254.9,105.9],[301.4,116.3],[304.9,119.3],[486.4,71.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[136.5,187.2],[186.4,212.8],[173.2,264.7],[122.1,285.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[68.6,154.0],[188.8,106.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[90.6,209.6],[135.0,192.9],[153.9,120.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-143.1,242.5],[-132.1,230.1],[-120.3,178.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-100.9,188.9],[-113.3,179.2],[-108.3,155.6],[-106.4,153.6],[-93.5,148.9],[-86.1,164.4],[-69.6,157.9],[-64.9,167.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-70.7,314.8],[-67.7,311.2],[-7.2,285.4],[4.0,237.3],[47.1,218.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[273.5,-43.9],[276.8,-33.7],[278.7,-15.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[711.0,347.0],[630.9,339.8],[609.1,334.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[713.8,361.4],[624.7,351.2],[613.5,345.8],[609.1,334.3]]]},{"h":"track","m":"stone","w":1.95,"p":[[[438.8,-809.7],[389.5,-851.8],[354.9,-846.4],[260.9,-862.2],[213.3,-848.9],[151.7,-816.1],[85.9,-761.6],[19.1,-702.9],[-23.2,-661.7],[-45.5,-629.7],[-88.3,-563.1],[-99.7,-534.6],[-107.3,-508.6],[-131.3,-470.6],[-134.2,-460.7],[-140.3,-398.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[781.8,-479.2],[756.2,-444.1],[722.0,-422.4],[716.8,-417.3],[701.0,-392.1],[698.1,-383.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-491.8,-496.1],[-434.7,-513.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[148.7,348.8],[138.9,354.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[21.5,52.6],[144.7,354.0]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[154.3,366.8],[159.7,385.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[41.6,85.8],[160.9,40.2]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[-60.1,87.2],[21.5,52.6]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[204.3,-44.1],[20.7,34.5]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[150.2,358.5],[148.7,348.8],[90.6,209.6],[27.3,50.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[258.2,440.7],[257.5,447.4],[254.1,453.1],[245.4,457.8],[238.7,457.5],[232.8,454.5],[228.7,449.3],[227.1,442.9],[228.3,436.3],[232.3,430.7],[241.8,426.9],[251.5,429.6],[256.1,434.4],[258.2,440.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[185.6,402.2],[193.0,410.9],[207.9,416.0],[223.8,416.2],[243.5,408.7],[252.0,408.7],[255.0,411.8],[255.5,416.4],[248.6,428.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[203.7,396.2],[208.3,404.3],[207.9,416.0],[210.5,423.9],[228.3,436.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[247.4,363.1],[248.5,358.5],[244.4,350.1]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[204.1,389.1],[198.5,381.5],[189.2,380.2],[181.6,386.0],[180.1,393.6],[185.6,402.2],[194.9,403.9],[202.6,398.4],[204.1,389.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[48.6,-98.4],[52.2,-102.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[68.6,-96.3],[135.3,-147.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[64.2,-93.0],[68.6,-96.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-137.1,-204.5],[-123.4,-203.5],[-112.2,-210.4],[-61.9,-263.3],[-54.0,-273.1],[-51.1,-282.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-114.6,-208.8],[-69.1,-218.9],[-35.7,-213.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-64.1,-218.4],[-69.9,-224.6],[-73.7,-234.4],[-74.1,-242.7],[-70.5,-254.1]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[177.9,376.9],[183.1,383.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-644.3,34.5],[-623.1,59.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-861.7,-508.8],[-859.9,-504.0],[-873.6,-470.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-859.9,-504.0],[-850.1,-527.1],[-836.6,-550.2],[-825.3,-549.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-660.2,-27.0],[-671.8,-40.8],[-686.3,-38.9],[-716.7,-49.4],[-730.1,-58.0],[-769.0,-60.8],[-800.0,-60.7],[-859.4,-36.9],[-887.5,-18.7],[-962.6,15.2],[-985.0,23.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-660.2,-27.0],[-667.7,-46.7],[-721.0,-149.7],[-730.2,-174.8],[-729.1,-179.2],[-724.9,-183.8]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[165.5,367.5],[156.8,374.1]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[538.2,96.2],[430.7,176.4],[361.2,223.1],[165.5,367.5]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[191.8,495.3],[203.4,496.8]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[263.8,-61.6],[258.9,-62.7],[256.0,-57.7],[279.9,-43.7],[278.4,-41.0],[251.8,-56.7],[257.9,-67.0],[267.2,-61.7],[268.4,-58.0],[280.3,-51.0],[278.5,-47.8],[264.1,-56.3],[263.8,-61.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[291.9,-43.5],[282.9,-48.8],[281.9,-47.1],[278.5,-47.8],[280.3,-51.0],[287.0,-52.4],[292.9,-45.2],[291.9,-43.5]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[288.3,-42.0],[294.0,-35.2],[286.8,-39.4],[288.3,-42.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[283.4,-56.3],[283.8,-56.9]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[278.5,-47.8],[283.4,-56.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[254.1,375.0],[364.6,307.1],[457.4,245.7],[469.3,240.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[324.7,249.8],[364.6,307.1],[387.6,356.4],[421.1,388.5],[474.6,432.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[457.2,209.4],[244.4,350.1]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-32.9,756.2],[-28.2,754.4],[-22.6,755.5],[8.7,767.7],[11.8,770.8],[6.6,782.6],[-9.7,795.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[3.2,669.3],[-1.2,681.0],[-5.2,685.4]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-248.0,645.2],[-233.7,647.0],[-239.1,644.4],[-246.6,637.2],[-250.9,640.0],[-250.8,642.6],[-248.0,645.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[709.7,486.8],[690.1,492.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[646.9,497.4],[666.4,497.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[693.9,554.0],[711.3,554.8],[711.0,549.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[614.0,271.8],[620.9,268.1]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[628.0,708.2],[541.3,700.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-609.7,-145.4],[-629.3,-146.7],[-638.3,-144.8],[-642.1,-142.8],[-645.8,-134.1],[-659.2,-31.1],[-663.9,-11.2],[-655.9,13.2],[-644.3,34.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-502.2,9.4],[-481.7,32.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-458.0,40.7],[-476.0,55.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-436.5,64.0],[-445.1,72.5],[-433.3,87.7],[-433.2,93.2],[-427.0,100.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-458.0,40.7],[-464.3,39.9],[-478.8,31.3],[-481.7,32.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-329.2,232.8],[-335.7,236.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-336.0,222.1],[-360.6,239.3],[-362.0,243.2],[-336.3,276.7],[-332.8,276.3],[-317.1,263.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-326.9,271.7],[-339.7,254.4],[-329.0,246.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-323.1,254.6],[-333.6,262.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-314.1,247.2],[-305.0,256.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-263.9,325.0],[-286.8,318.0],[-319.5,286.5],[-330.3,301.1],[-332.4,314.0],[-331.0,323.4],[-302.0,353.6],[-296.5,353.0],[-269.0,323.6]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-305.5,350.3],[-297.5,342.2],[-290.1,328.8]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-295.9,339.2],[-286.5,348.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-278.6,321.6],[-264.5,303.7],[-293.0,275.4],[-300.5,270.4],[-304.6,270.5],[-319.5,286.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-302.0,353.6],[-298.4,356.1],[-293.5,355.1],[-281.3,343.3],[-281.1,336.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-296.2,397.3],[-262.8,364.0],[-251.6,339.6],[-250.7,332.8]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-184.7,371.3],[-200.2,359.0],[-218.2,357.3],[-226.1,353.0],[-230.3,345.3],[-233.5,331.5],[-240.1,323.6]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-226.1,353.0],[-251.4,367.1],[-260.0,369.9],[-267.0,368.8]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-443.3,374.0],[-405.4,314.2],[-428.2,308.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[275.0,179.0],[267.3,181.4],[263.8,179.1],[264.5,173.8],[271.3,170.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[269.5,181.1],[270.2,182.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[234.2,-25.3],[223.9,-1.4],[223.7,5.3],[250.9,46.6],[264.6,58.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-664.8,-284.8],[-677.0,-278.4],[-694.0,-277.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[264.6,58.2],[249.9,58.2],[241.0,61.5],[221.0,62.6],[177.8,80.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-18.7,-180.7],[-35.7,-213.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[14.0,37.0],[12.7,33.4],[-8.6,24.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[809.8,370.8],[812.0,378.6],[806.8,408.4],[808.3,414.1],[803.0,419.6],[798.4,440.8],[802.9,481.1],[797.5,486.8],[796.7,490.2],[799.2,500.6],[798.4,509.0],[790.7,525.3],[783.7,532.4],[783.8,537.7]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-44.5,685.4],[-43.6,676.5]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[14.3,841.3],[12.8,834.8]]]},{"h":"track","m":"stone","w":1.95,"p":[[[324.3,883.3],[321.2,861.3],[312.9,838.4],[295.9,794.7],[286.6,785.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-3.5,945.4],[10.3,882.1],[7.1,850.2],[8.1,840.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-258.3,328.6],[-263.9,325.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-11.9,-162.0],[-38.1,-140.5],[-57.0,-118.1],[-67.0,-82.9],[-44.5,-66.1]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[558.7,538.6],[568.7,535.1],[615.4,539.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[527.8,246.4],[534.1,236.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[497.2,209.9],[588.6,274.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[535.2,237.3],[540.6,229.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[587.5,262.0],[502.0,200.8],[545.9,139.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[580.6,163.8],[538.1,227.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[509.5,190.3],[580.5,239.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[573.0,252.0],[596.8,217.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[581.7,258.0],[605.5,223.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[587.5,262.0],[610.5,227.8],[611.7,211.5],[533.1,157.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[540.7,146.7],[590.9,181.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[525.3,168.2],[610.5,227.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[517.7,178.9],[588.1,229.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[605.5,223.1],[611.7,211.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-502.5,-207.5],[-486.7,-168.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-465.0,-177.6],[-506.0,-159.9],[-522.8,-141.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-522.7,-195.5],[-508.0,-157.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-17.0,-295.3],[-19.8,-277.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-19.8,-277.1],[-21.5,-265.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-21.5,-265.8],[-23.4,-260.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[73.4,-207.6],[77.0,-238.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-92.1,100.6],[-107.9,62.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-99.9,58.7],[-116.6,17.7],[-123.0,18.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-107.9,62.4],[-75.9,49.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-100.1,81.9],[-206.1,126.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-244.6,140.7],[-208.0,123.8],[-197.6,144.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-274.8,24.4],[-272.9,72.6],[-233.2,74.9],[-227.9,90.2],[-250.8,99.8],[-233.3,135.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-162.2,223.8],[-154.0,213.6],[-138.8,144.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-169.3,155.6],[-177.8,136.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-129.5,256.7],[-113.6,241.7],[-104.4,190.4],[-120.3,178.9],[-138.8,144.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[17.7,145.9],[-11.2,156.8],[-19.2,149.8],[-31.7,147.0],[-45.6,112.8],[-61.9,108.5],[-68.9,90.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[60.7,163.9],[3.7,188.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[59.8,248.0],[34.9,258.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[59.8,248.0],[90.1,236.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-4.0,88.8],[-169.3,155.6],[-180.9,205.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-659.2,-287.0],[-684.4,-311.8],[-718.6,-330.5]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[-272.5,-109.3],[-261.6,-79.7],[-256.9,-77.2],[-245.5,-48.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-256.9,-77.2],[-246.7,-81.2]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[-261.6,-79.7],[-326.5,-52.5],[-338.1,-82.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-43.8,-382.6],[-44.8,-385.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-13.5,-390.2],[-10.4,-382.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-197.7,-246.4],[-208.9,-242.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-231.8,-233.2],[-178.4,-198.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-91.1,-382.9],[-74.4,-343.4],[-95.5,-335.2],[-89.0,-318.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-112.7,-309.4],[-172.9,-326.7],[-199.2,-315.6]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-65.5,-286.6],[-96.8,-273.2],[-116.4,-255.5],[-129.9,-229.2],[-132.4,-215.0],[-161.0,-155.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-84.9,-301.7],[-112.7,-309.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-86.4,-322.6],[-50.0,-336.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-76.2,-282.4],[-91.5,-317.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-91.5,-317.8],[-86.4,-322.6]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-84.2,-420.9],[-50.0,-336.6],[-31.7,-296.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-135.6,-315.9],[-146.4,-344.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[11.9,-429.0],[51.0,-334.8],[49.3,-326.4],[43.4,-322.2],[42.2,-317.4],[54.9,-286.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[54.9,-286.1],[74.8,-293.8],[89.3,-305.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-239.9,-230.1],[-235.0,-221.7],[-228.5,-217.6],[-219.8,-218.9],[-211.5,-223.5],[-203.9,-222.4],[-201.0,-218.7],[-199.8,-212.6],[-203.9,-200.6],[-210.9,-191.7],[-220.7,-191.3],[-246.6,-196.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-208.6,-218.2],[-192.2,-232.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-217.4,-294.9],[-272.2,-274.1],[-273.8,-272.0],[-245.8,-194.1],[-246.1,-188.6],[-249.9,-185.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-436.3,-89.5],[-381.8,-100.1],[-375.2,-105.0],[-339.5,-121.1],[-332.5,-171.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-341.8,-120.1],[-328.2,-86.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-397.9,-294.7],[-388.6,-270.0],[-434.4,-252.0],[-436.5,-250.8],[-440.3,-240.4],[-458.2,-232.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-318.3,-327.3],[-309.5,-305.5],[-309.9,-301.2],[-357.2,-283.3],[-365.4,-269.2],[-378.9,-263.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-191.6,86.8],[-179.9,115.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-255.7,-280.4],[-268.4,-312.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-311.1,-300.5],[-303.7,-298.9],[-261.2,-315.8],[-273.4,-345.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-249.9,-185.6],[-296.7,-167.6],[-320.6,-176.0],[-361.5,-159.0],[-375.7,-165.1],[-499.6,-117.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-440.2,-139.3],[-432.3,-120.5],[-436.3,-89.5],[-419.3,-48.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-392.2,-97.3],[-377.9,-65.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-334.4,-157.8],[-302.2,-147.1],[-233.9,-175.7],[-229.4,-193.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-210.9,-191.7],[-213.7,-171.8],[-231.5,-162.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-321.2,-153.5],[-301.1,-97.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-237.1,-174.3],[-218.5,-131.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-323.3,-174.9],[-313.1,-216.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-334.0,-208.6],[-261.0,-235.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-276.6,-230.6],[-303.7,-298.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-366.9,-161.3],[-409.2,-261.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-398.9,-237.4],[-261.5,-295.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-398.0,-155.8],[-390.7,-140.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-379.5,-134.4],[-375.3,-136.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-380.2,-100.7],[-380.7,-101.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-378.9,-245.9],[-363.7,-209.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-393.2,-47.6],[-383.4,-25.0],[-344.7,-41.4],[-340.5,-31.9],[-341.9,3.1],[-310.6,4.1],[-314.7,-5.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-359.7,-35.0],[-358.9,26.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-279.4,26.3],[-300.7,-27.6],[-284.4,-34.0],[-240.0,-33.8],[-245.5,-48.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-163.0,-75.4],[-157.1,-76.2],[-141.9,-42.3],[-149.3,-36.8],[-133.2,1.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[60.4,18.2],[81.1,69.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[149.0,11.7],[55.5,49.8],[65.7,76.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[130.0,19.4],[142.2,47.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[103.3,61.5],[100.9,56.0],[97.7,57.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[62.1,137.6],[110.4,117.1],[124.0,53.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[82.5,129.5],[61.9,77.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-109.5,218.8],[-67.1,203.8],[-60.1,220.5],[-36.2,210.3],[-22.3,149.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-113.6,241.7],[-78.6,275.8],[-75.3,276.4],[-70.1,274.8],[-68.7,271.8],[-81.6,237.7],[-71.8,232.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-48.2,281.2],[-90.4,295.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-81.0,208.7],[-57.9,267.3],[-53.6,267.7],[-40.9,299.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[4.0,237.3],[-25.6,220.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-69.6,237.7],[-25.6,220.6],[-11.2,156.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[29.3,338.2],[-11.1,354.5],[-36.3,327.6],[-31.6,311.1],[6.8,295.5],[20.5,229.9],[-11.2,156.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[77.2,289.0],[36.9,305.8],[24.9,357.3],[3.8,364.8],[-2.4,351.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-31.6,311.1],[-36.1,297.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[6.6,295.9],[-7.2,285.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[104.3,371.9],[81.7,320.4],[85.5,308.7],[30.3,177.1],[-12.5,67.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[111.1,288.3],[46.2,313.7],[34.4,365.7],[-5.2,382.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[24.9,357.3],[29.3,367.8]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[69.1,361.4],[78.6,384.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[46.2,313.7],[42.2,303.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[484.1,217.2],[492.6,204.2],[486.3,199.7],[536.0,132.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[534.1,135.5],[544.0,142.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[241.0,61.5],[229.4,115.5],[168.0,139.3],[155.8,197.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[248.6,133.9],[228.5,129.8],[218.5,177.8]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[138.0,-14.7],[218.5,177.8],[215.9,190.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[228.5,129.8],[202.2,138.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[173.2,264.7],[200.5,271.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[233.6,201.9],[201.1,213.3],[188.8,271.4],[184.0,278.5],[128.7,301.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[175.5,282.6],[182.9,298.9],[174.7,320.1],[142.2,333.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-375.7,-165.1],[-384.8,-188.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-390.7,-140.0],[-414.1,-141.7],[-417.2,-110.3],[-380.9,-109.1],[-379.3,-138.7],[-390.7,-140.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-338.1,-131.4],[-385.0,-161.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-147.7,65.9],[-134.7,95.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-927.6,-304.5],[-888.4,-281.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-906.3,-291.9],[-890.1,-315.0],[-854.2,-350.9],[-837.6,-364.1],[-790.4,-391.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-848.8,-529.3],[-832.1,-497.0],[-810.7,-438.8],[-778.4,-363.5],[-764.9,-337.6],[-754.5,-327.8],[-744.2,-324.4],[-731.3,-324.6],[-718.6,-330.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-802.5,-419.7],[-784.1,-446.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-810.7,-438.8],[-792.4,-465.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-818.1,-458.9],[-801.2,-483.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-797.1,-407.0],[-761.2,-423.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-792.0,-409.2],[-774.4,-435.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-4.7,-142.5],[163.4,-102.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[74.0,-167.5],[116.9,-77.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[52.2,-102.9],[68.4,-124.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[48.6,-98.4],[28.8,-74.6]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-25.6,-269.9],[-25.2,-272.2]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-16.1,-268.7],[-15.7,-271.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[183.5,-34.4],[195.0,-10.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[165.2,-0.5],[182.7,-7.7],[195.0,-10.0],[210.4,-5.4],[223.7,5.3]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-394.1,-113.6],[-396.5,-114.7]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-406.3,-123.3],[-409.8,-123.7],[-409.7,-125.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-102.8,184.7],[-99.2,192.7],[-59.2,174.6],[-62.9,166.6],[-102.8,184.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[167.8,39.3],[170.3,45.5],[211.2,29.2],[208.7,22.9],[167.8,39.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-483.8,-401.2],[-409.2,-421.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-450.2,-569.8],[-408.0,-416.8],[-406.1,-414.6],[-402.0,-414.4],[-362.5,-426.0],[-361.6,-428.6],[-405.9,-578.6],[-409.6,-581.3],[-450.2,-569.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[1.8,-82.9],[-17.7,-106.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-6.7,-84.7],[-5.2,-91.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-6.4,-88.4],[-5.8,-88.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-7.3,-85.9],[-6.5,-85.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[57.6,-39.7],[54.8,-38.6],[58.6,-29.4],[61.3,-30.4],[57.6,-39.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[58.6,-29.4],[59.6,-27.1],[53.3,-24.8],[52.5,-26.9],[58.6,-29.4]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[56.3,-28.4],[57.1,-26.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-354.7,-172.5],[-352.5,-167.0],[-327.0,-177.3],[-329.2,-182.8],[-354.7,-172.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[125.5,120.3],[138.2,67.9],[167.7,56.3]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[241.5,-23.3],[256.8,-8.1],[269.0,9.2],[271.7,16.0]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-450.2,-569.8],[-507.1,-549.0],[-543.0,-542.1],[-574.7,-539.9],[-660.8,-540.2],[-818.5,-578.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-481.1,-457.6],[-466.0,-406.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-402.0,-414.4],[-409.8,-390.9],[-439.6,-379.4],[-436.7,-371.7],[-467.0,-359.4]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[226.1,-27.5],[220.1,-34.3],[217.5,-43.5],[213.0,-46.5],[208.0,-45.8],[200.2,-40.9],[201.1,-34.9],[206.7,-31.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-8.6,45.5],[-16.0,27.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-12.0,36.0],[-4.6,22.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-409.0,188.1],[-370.0,147.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[309.6,-446.1],[311.3,-429.8],[307.9,-427.5],[304.0,-427.8],[295.4,-432.0],[267.0,-456.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[448.6,-24.6],[443.9,-4.6],[445.6,6.0],[454.2,30.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[444.8,-12.7],[437.9,-5.1],[423.9,-0.7],[403.4,2.3],[390.3,0.7],[382.1,9.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-162.3,-272.3],[-149.0,-242.5],[-165.7,-236.5],[-158.7,-219.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-184.1,-212.8],[-172.5,-218.0],[-131.2,-220.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-208.9,-242.1],[-208.0,-239.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-55.6,-83.8],[-49.8,-88.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-35.7,-213.8],[6.4,-208.9],[15.2,-212.0],[22.9,-221.5],[24.8,-232.2],[23.6,-244.6],[20.4,-253.1],[13.1,-259.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-49.1,-262.9],[-65.5,-286.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[13.1,-259.0],[13.8,-264.1],[10.5,-264.5]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-53.3,-275.2],[-50.2,-274.7]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-899.1,372.8],[-671.1,343.0],[-605.3,340.4],[-537.0,349.7],[-468.7,364.4],[-273.1,438.3],[-234.4,451.0],[-182.0,457.5],[-10.3,473.1],[8.8,478.4],[30.0,478.0],[191.8,495.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[567.2,411.6],[561.8,480.0]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-35.5,-27.1],[-36.2,-28.6]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-38.2,-33.1],[-37.5,-31.5]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[41.3,-22.9],[43.0,-23.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[43.0,-23.6],[58.1,-29.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[56.3,-28.4],[56.1,-28.8]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[58.8,-30.1],[61.0,-30.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[58.1,-29.6],[58.8,-30.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-424.1,-12.9],[-428.1,-19.1],[-438.6,-20.9],[-444.4,-16.6],[-445.4,-6.0],[-438.0,0.6],[-431.3,0.3],[-424.6,-6.2],[-424.1,-12.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-423.6,-40.0],[-434.8,-11.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-434.8,-11.5],[-445.4,-6.0],[-493.5,2.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-397.0,-12.8],[-456.7,42.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-434.8,-11.5],[-428.3,-1.3],[-419.5,7.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-416.9,-113.3],[-396.2,-128.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-406.3,-123.3],[-405.1,-121.7]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-394.1,-113.6],[-394.3,-109.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[99.8,-34.6],[111.2,-3.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[144.3,-44.3],[168.7,-28.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[159.5,-50.5],[182.8,-34.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[172.5,-55.9],[195.4,-39.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[195.4,-39.9],[184.2,-61.0],[139.6,-42.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[106.3,-16.7],[190.2,-49.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[124.6,-44.7],[138.0,-14.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[111.6,-39.4],[123.1,-8.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-49.2,82.6],[-43.6,58.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-6.9,64.9],[-3.6,43.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-26.3,73.0],[-22.1,50.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[192.0,-46.3],[227.4,-62.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[26.9,-134.3],[27.6,-157.7],[29.0,-159.3],[41.2,-163.1],[52.3,-162.0]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[154.6,290.8],[160.2,303.8],[158.0,303.2],[142.9,309.7],[138.8,297.5],[154.6,290.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[366.3,147.1],[371.0,162.4],[341.8,174.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[399.9,166.2],[399.8,161.2],[445.7,142.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[451.8,160.8],[443.2,135.2],[438.3,135.1],[430.8,116.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[410.9,156.7],[408.7,151.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-43.6,676.5],[-41.5,661.6]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[-560.8,-180.1],[-554.9,-181.3],[-505.7,-228.7],[-464.2,-260.0],[-448.6,-267.7],[-245.1,-353.4],[13.1,-455.7],[25.4,-465.0]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-280.2,267.8],[-253.1,237.3]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[20.7,-32.2],[54.2,-170.0]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-470.9,93.9],[-460.1,105.8]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-165.8,-0.9],[-162.3,7.2],[-172.5,13.0],[-176.5,3.6],[-165.8,-0.9]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-166.9,10.7],[-162.1,13.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-301.7,-139.8],[-300.7,-137.0],[-280.7,-145.0],[-279.0,-140.9],[-287.5,-137.5],[-285.8,-133.3],[-283.6,-134.2],[-283.0,-132.6],[-285.2,-131.7],[-281.9,-123.6],[-293.4,-119.0],[-292.5,-116.7],[-295.9,-118.0],[-293.1,-110.5],[-277.7,-117.0],[-279.3,-121.4],[-254.0,-129.5],[-262.1,-150.9],[-264.9,-153.2],[-301.7,-139.8]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-100.9,188.9],[-104.4,190.4]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-31.8,190.9],[-67.1,203.8],[-60.1,220.5],[-36.2,210.3],[-31.8,190.9]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-542.0,-247.9],[-524.9,-237.7]]]},{"h":"track","m":"stone","w":1.95,"p":[[[-545.8,-256.1],[-549.0,-256.6],[-574.1,-244.1],[-595.5,-237.2],[-599.3,-239.0],[-607.4,-261.7]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[360.5,872.4],[284.5,699.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-507.9,-465.0],[-524.6,-455.7],[-537.7,-444.8],[-558.8,-434.0],[-582.6,-425.7],[-594.7,-418.5],[-632.9,-370.1],[-646.2,-365.9],[-660.6,-365.6],[-667.3,-362.2],[-688.9,-341.6],[-700.7,-320.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-441.8,-19.2],[-451.4,-17.3],[-453.7,-14.0],[-455.0,-4.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-424.1,-12.9],[-419.1,-11.1],[-418.1,-7.0],[-425.0,2.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-445.9,-13.2],[-423.9,-9.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[271.4,131.4],[274.2,117.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[168.3,322.9],[154.6,290.8]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[208.1,242.2],[207.2,245.9],[213.6,247.3],[208.7,268.9],[201.4,267.3],[200.0,273.4],[212.0,276.2],[220.8,238.6],[217.0,237.8],[208.1,242.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[212.5,274.1],[217.3,275.2]]]},{"h":"path","m":"stone","w":0.78,"p":[[[195.3,241.2],[207.8,243.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[219.5,243.5],[224.1,244.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[166.7,-77.1],[174.4,-56.7],[184.2,-61.0],[192.0,-46.3],[227.4,-62.6],[218.0,-71.4],[204.5,-65.5],[192.8,-87.6],[166.7,-77.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[208.4,30.3],[221.0,62.6]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[278.7,-15.8],[385.1,9.8],[406.3,23.6],[498.0,38.3],[538.5,63.4],[543.2,85.1],[541.0,93.1]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[255.0,-37.6],[248.6,-37.2],[245.5,-34.2],[241.5,-23.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[110.4,117.1],[144.1,124.3]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-502.5,-207.5],[-498.1,-209.2],[-495.0,-201.3],[-499.4,-199.5],[-502.5,-207.5]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-522.7,-195.5],[-517.1,-197.6],[-514.0,-189.5],[-519.6,-187.4],[-522.7,-195.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-338.2,-218.8],[-291.7,-236.4]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-293.8,-289.8],[-291.1,-283.3],[-284.7,-286.0],[-287.5,-292.5],[-293.8,-289.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-270.7,-232.7],[-260.7,-278.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-208.0,-239.3],[-205.8,-233.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[11.6,19.2],[15.8,30.7],[23.7,27.4],[29.2,12.6],[11.6,19.2]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[16.4,14.6],[17.3,17.1],[14.9,18.0],[14.1,15.6],[16.4,14.6]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-17.9,22.3],[-19.1,19.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-15.3,21.2],[-19.1,19.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[90.9,-31.0],[136.4,-49.6],[149.7,-19.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-50.7,-47.9],[-44.0,-67.7]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-48.7,-16.9],[-43.6,-5.0]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-42.6,-93.0],[-49.4,-103.8],[-52.4,-102.9],[-51.6,-98.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-4.7,-142.5],[26.1,-63.8],[64.2,-93.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-4.7,-142.5],[12.5,-176.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-109.4,-79.9],[-111.2,-83.3],[-93.3,-91.6],[-85.1,-73.4],[-61.1,-85.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-53.6,267.7],[-60.3,270.2],[-59.6,272.2],[-54.7,270.3],[-52.0,277.3],[-50.1,276.5],[-53.6,267.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-93.3,276.6],[-99.6,283.7],[-91.3,291.8],[-78.6,275.8],[-86.2,268.5],[-91.2,274.2],[-84.3,280.2],[-90.3,287.0],[-95.6,282.3],[-91.8,278.0],[-93.3,276.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-63.7,271.5],[-65.5,269.4],[-68.9,270.7],[-70.1,274.8],[-68.8,278.6],[-62.0,275.9],[-63.7,271.5]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[111.5,211.2],[110.1,206.8],[112.9,213.4],[103.3,217.3],[99.9,209.1],[109.7,205.1],[110.2,206.4],[103.5,209.3],[105.3,213.7],[111.5,211.2]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[83.8,141.1],[86.3,147.1],[72.4,152.5],[65.8,136.1],[79.5,130.7],[81.5,135.4],[78.6,136.6],[77.4,133.5],[71.8,135.8],[75.4,144.6],[83.8,141.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[142.4,283.7],[139.5,284.8],[140.5,287.3],[134.8,289.6],[132.0,282.8],[139.0,280.0],[140.0,282.4],[141.6,281.8],[142.7,277.2],[125.9,283.7],[130.8,295.0],[148.7,287.9],[147.2,284.8],[143.5,286.4],[142.4,283.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-492.9,-158.7],[-495.4,-164.5],[-477.1,-172.5],[-468.8,-155.2],[-487.9,-147.2],[-492.9,-158.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-252.4,-117.5],[-243.7,-95.1],[-239.3,-98.2],[-237.8,-94.1],[-243.1,-93.7],[-239.7,-80.7],[-234.6,-83.0],[-233.2,-80.0],[-230.8,-81.0],[-229.2,-94.2],[-239.8,-122.8],[-252.4,-117.5]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-560.8,-180.1],[-605.0,-139.2]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[-664.8,-284.8],[-675.1,-287.5],[-680.6,-294.0],[-680.6,-298.1],[-678.2,-301.4],[-671.8,-301.0]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[-802.3,-520.8],[-761.2,-423.1],[-674.9,-237.9],[-581.5,-105.5],[-518.7,-23.4],[-436.5,64.0],[-404.6,100.5],[-364.2,131.2],[-299.1,191.1],[-210.4,279.7],[-10.3,473.1]]]},{"h":"unclassified","m":"asphalt","w":2.93,"p":[[[-833.4,-556.1],[-819.7,-544.4],[-802.3,-520.8]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-259.7,-278.8],[-257.6,-273.1],[-243.3,-278.6],[-245.3,-284.3],[-259.7,-278.8]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-223.6,-277.2],[-221.5,-272.1],[-218.7,-273.2],[-213.9,-261.4],[-205.2,-264.8],[-212.6,-283.2],[-234.9,-274.3],[-234.3,-272.9],[-223.6,-277.2]]]},{"h":"track","m":"stone","w":1.95,"p":[[[90.2,-287.7],[89.2,-314.8],[92.3,-342.2],[98.3,-370.5],[106.2,-395.2],[105.6,-429.6],[103.8,-440.2],[86.1,-460.1],[80.7,-488.6]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[90.2,-287.7],[61.7,-285.2],[7.7,-293.4],[-31.7,-296.4],[-65.5,-286.6]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[80.7,-488.6],[25.4,-465.0]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-45.7,14.7],[-43.9,19.4],[-50.4,21.9],[-52.2,17.3],[-45.7,14.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-40.5,10.0],[-30.6,34.6],[-32.9,35.5],[-31.9,38.0],[-47.1,44.1],[-48.2,41.5],[-52.4,43.2],[-59.0,26.7],[-54.8,25.0],[-61.5,8.5],[-71.4,12.4],[-67.3,22.1],[-65.5,21.4],[-52.7,53.4],[-46.7,50.9],[-48.2,47.6],[-22.4,38.8],[-33.0,10.9],[-27.4,8.6],[-34.1,-9.1],[-42.2,-5.6],[-41.2,-3.1],[-37.6,-4.5],[-33.0,7.0],[-40.5,10.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[177.9,250.1],[192.7,253.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-16.0,-173.5],[-4.6,-167.8],[8.5,-168.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[27.6,-157.7],[13.5,-164.1],[8.5,-168.3]]]},{"h":"path","m":"stone","w":0.78,"p":[[[26.9,-134.3],[22.4,-118.1],[12.9,-97.4]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-56.6,-35.5],[-48.7,-16.9]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-196.8,292.2],[-176.4,263.7],[-176.5,259.5],[-181.0,252.3],[-187.0,251.3],[-192.7,255.2],[-194.0,260.3],[-190.3,265.8],[-181.2,272.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-161.9,-153.2],[-153.9,-137.4],[-104.0,-11.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-104.0,-11.6],[-78.1,49.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-78.1,49.9],[-65.6,82.2],[-66.5,84.7],[-93.8,96.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-93.8,96.5],[-199.8,139.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-199.8,139.9],[-224.2,149.7],[-233.6,146.8],[-241.6,139.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-161.9,-153.2],[-412.5,-49.5],[-417.4,-43.3],[-417.3,-40.0],[-241.6,139.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-67.1,62.5],[-21.4,45.4],[-20.8,46.8],[25.2,27.9],[37.2,-6.6],[94.6,-30.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[95.5,-27.6],[35.4,-2.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[95.8,-26.6],[93.8,-32.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[61.6,-17.5],[63.1,-12.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[31.5,25.3],[36.4,12.8],[96.7,-13.4],[101.1,-3.3],[31.5,25.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[32.7,19.3],[98.8,-8.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[31.5,25.3],[25.2,27.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[63.1,-12.5],[66.9,-0.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-67.1,62.5],[-72.4,64.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-55.9,82.1],[10.3,55.3],[12.5,49.3],[8.7,44.0],[0.7,43.5],[-58.4,67.5],[-62.4,73.9],[-60.4,80.0],[-55.9,82.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-55.9,82.1],[-66.5,84.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-60.9,69.5],[-69.2,72.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-60.9,69.5],[-64.0,61.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[130.0,359.0],[132.2,353.8],[13.5,63.1],[-16.2,70.7],[-213.3,153.2],[-217.9,162.6],[-2.8,381.1],[24.8,411.4],[35.9,418.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[159.4,361.6],[156.3,361.1],[152.5,353.7],[152.8,347.4],[27.5,41.5],[28.8,38.5],[74.5,19.5],[83.8,19.0],[144.8,-5.1],[146.9,-9.1],[179.9,-23.4],[183.6,-27.1],[196.8,-26.7],[219.9,-21.1],[235.3,-15.9],[241.0,-11.8],[265.6,15.2],[267.2,20.5],[219.6,238.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[210.8,275.9],[206.1,297.0],[207.0,300.3],[198.3,324.6],[188.5,333.3],[179.0,336.6],[158.1,336.0],[151.7,333.3],[150.0,330.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[198.3,324.6],[202.0,326.1],[198.0,331.4],[187.6,341.3],[159.4,361.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[151.7,333.3],[147.4,334.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[121.3,-162.6],[39.2,-176.0],[38.4,-172.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[103.9,-164.2],[113.3,-189.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[33.1,-173.1],[33.7,-176.6],[-46.8,-188.0],[-52.7,-185.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-77.1,-173.3],[-151.8,-139.5],[-159.8,-155.5],[-144.3,-183.4],[-121.7,-242.3],[-112.7,-258.2],[-93.7,-273.9],[-35.4,-294.6],[7.2,-292.2],[67.4,-282.7],[90.4,-286.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-159.2,-154.4],[-161.9,-153.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-159.8,-155.5],[-160.9,-163.9],[-170.5,-187.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-170.5,-187.8],[-143.9,-196.0],[-135.0,-216.2],[-133.3,-228.9],[-115.3,-266.0],[-101.3,-272.1],[-95.6,-276.8],[-76.9,-284.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-76.9,-284.0],[-51.4,-293.5],[-32.4,-297.9],[8.0,-295.5],[62.4,-286.8],[90.1,-289.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[90.1,-289.6],[127.6,-283.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[90.4,-286.0],[98.0,-284.9],[99.6,-286.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-143.9,-196.0],[-157.4,-166.9],[-160.9,-163.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-170.5,-187.8],[-238.0,-354.2],[15.8,-454.5],[25.4,-463.4],[127.2,-506.0],[151.5,-514.2],[172.9,-518.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[301.0,10.5],[299.8,10.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[298.6,8.9],[299.1,6.7],[312.6,9.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[316.7,6.9],[313.3,6.2],[303.4,54.3],[393.3,72.9],[399.8,41.7],[342.0,29.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[342.8,25.8],[335.4,60.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[359.8,33.4],[363.0,17.6],[352.8,15.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[367.2,24.6],[361.8,23.5],[357.6,26.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[394.0,69.5],[387.7,68.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[390.0,68.6],[389.3,72.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[317.1,57.1],[307.3,103.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[363.0,80.5],[373.5,77.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[375.1,69.1],[373.5,77.8],[379.7,99.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[294.4,34.7],[293.6,34.6]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[295.3,30.7],[294.5,30.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[268.4,77.0],[267.7,80.3],[290.0,84.9]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[277.4,82.4],[282.5,58.4],[295.1,61.1],[287.0,99.0],[307.3,103.3],[365.8,90.3],[358.9,65.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[327.5,105.7],[333.1,104.3],[334.8,97.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[359.7,94.4],[359.1,91.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[362.2,93.8],[361.7,91.3]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[287.0,99.0],[293.9,105.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-99.7,71.6],[-98.7,74.2],[-102.6,76.0],[-103.8,73.4],[-99.7,71.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-98.9,47.7],[-99.4,46.3],[-104.2,48.2],[-103.7,49.6],[-98.9,47.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-103.5,36.1],[-102.8,37.8],[-107.7,39.5],[-108.4,37.8],[-103.5,36.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-111.7,15.6],[-111.1,17.3],[-116.0,19.2],[-116.8,17.4],[-111.7,15.6]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-107.6,25.9],[-107.0,27.5],[-111.8,29.4],[-112.5,27.8],[-107.6,25.9]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-119.3,12.9],[-120.7,13.5],[-119.3,17.2],[-117.8,16.9],[-119.3,12.9]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-184.8,92.0],[-185.5,90.3],[-189.5,91.9],[-188.7,93.7],[-184.8,92.0]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-181.3,100.0],[-180.2,102.6],[-184.3,104.3],[-185.4,101.8],[-181.3,100.0]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-132.4,64.7],[-136.8,53.5],[-106.6,42.2],[-102.5,52.4],[-132.4,64.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-138.7,-23.4],[-139.9,-26.2],[-144.1,-24.4],[-142.9,-21.7],[-138.7,-23.4]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-138.9,-48.1],[-139.7,-49.9],[-144.4,-47.7],[-143.6,-46.0],[-138.9,-48.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-143.1,-57.9],[-143.9,-59.7],[-148.8,-57.7],[-148.0,-55.8],[-143.1,-57.9]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-147.3,-68.1],[-148.1,-69.9],[-153.4,-67.9],[-152.6,-66.2],[-147.3,-68.1]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-159.2,-83.0],[-160.8,-82.3],[-158.8,-76.8],[-151.4,-78.5],[-152.2,-80.4],[-157.4,-78.1],[-159.2,-83.0]]]},{"h":"steps","m":"stone","w":0.59,"p":[[[-38.5,-28.3],[-38.1,-20.9],[-44.2,-26.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[287.0,436.5],[296.9,446.4],[320.5,431.5],[347.6,418.0],[418.8,386.4],[445.4,378.0],[461.0,376.7],[481.9,380.7],[474.6,432.5],[452.6,451.3],[446.9,460.5]]]},{"h":"track","m":"stone","w":1.95,"p":[[[461.1,444.4],[464.7,450.1]]]},{"h":"track","m":"stone","w":1.95,"p":[[[296.9,446.4],[293.2,452.8]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[159.7,385.2],[269.8,661.9]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[159.7,385.2],[143.2,392.8],[117.2,400.4],[60.6,440.9],[53.6,441.1],[-227.8,157.6]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[244.4,350.1],[159.7,385.2]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[148.7,-126.4],[150.6,-133.8],[152.1,-132.5],[150.4,-126.0],[148.7,-126.4]]]},{"h":"path","m":"stone","w":0.78,"p":[[[154.8,-120.5],[157.3,-119.7],[157.7,-117.2],[152.5,-108.7]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[38.1,-216.4],[38.6,-218.9],[30.7,-220.4],[32.7,-241.7],[30.4,-250.3],[13.8,-264.1],[13.5,-261.6],[17.4,-257.2],[25.8,-252.9],[30.0,-245.4],[23.0,-194.4],[31.9,-193.2],[32.5,-197.8],[36.0,-202.1],[30.8,-202.9],[32.9,-217.2],[38.1,-216.4]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[48.8,-36.0],[43.1,-50.0],[39.2,-50.9],[29.5,-30.5],[30.1,-28.4],[42.2,-34.7],[42.7,-33.5],[48.8,-36.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[38.6,-50.6],[38.0,-54.3],[26.6,-56.5]]]},{"h":"pedestrian","m":"stone","w":1.56,"p":[[[-481.3,-217.8],[-478.3,-219.1],[-481.4,-226.6],[-484.4,-225.4],[-481.3,-217.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-208.9,-242.1],[-256.7,-223.6]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-50.0,-336.6],[-35.9,-342.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-72.6,-390.3],[-108.2,-376.1],[-118.6,-354.0],[-146.4,-344.3],[-163.4,-358.5],[-173.8,-385.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[11.9,-429.0],[0.9,-454.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[238.2,180.9],[265.4,183.8],[274.1,181.9],[291.3,182.9],[294.7,180.9],[295.6,173.5],[292.7,161.5],[279.0,129.2]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[238.2,180.9],[205.4,327.6],[182.1,348.4],[154.3,366.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-653.2,18.3],[-614.1,-17.5],[-612.4,-14.5]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-344.7,-41.4],[-339.7,-53.0],[-336.1,-54.2],[-344.9,-77.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-330.4,-62.7],[-338.3,-60.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-269.8,-108.3],[-260.1,-81.3],[-254.6,-78.1],[-238.1,-34.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-274.1,-106.5],[-264.0,-80.6],[-325.2,-54.8],[-335.2,-81.1]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-242.5,-36.1],[-254.8,-67.8],[-253.3,-68.2]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-256.5,-76.1],[-327.1,-49.2],[-328.7,-53.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[432.9,85.3],[448.2,124.9],[451.8,127.2],[459.1,148.0],[449.5,154.0]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[459.1,148.0],[461.0,154.0]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[103.9,-164.2],[116.9,-162.5],[123.7,-159.1],[278.7,-15.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-250.8,99.8],[-275.3,109.8]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-646.2,615.1],[-673.7,608.9],[-679.5,604.1],[-679.2,594.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[-646.2,615.1],[-637.9,614.8],[-624.0,619.4],[-609.7,621.2]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[150.2,358.5],[154.3,366.8]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[22.1,37.9],[20.7,34.5]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[27.3,50.4],[22.1,37.9]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[213.5,-30.2],[217.7,-31.9],[219.0,-36.9]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[206.7,-31.4],[213.5,-30.2]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[213.5,-30.2],[226.1,-27.5]]]},{"h":"residential","m":"asphalt","w":3.42,"p":[[[226.1,-27.5],[278.7,-15.8]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[21.5,52.6],[27.3,50.4]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[144.7,354.0],[150.2,358.5]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[-299.1,191.1],[-227.8,157.6]]]},{"h":"living_street","m":"asphalt","w":2.93,"p":[[[-227.8,157.6],[-560.8,-180.1],[-615.6,-233.0],[-664.8,-284.8]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[529.7,782.7],[541.3,700.1],[545.6,646.4],[559.7,547.5],[558.7,538.6],[554.0,534.7],[544.0,531.8],[472.9,526.1],[321.5,508.1],[203.4,496.8]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[-565.1,570.9],[-553.1,517.8],[-541.4,498.7]]]},{"h":"footway","m":"stone","w":0.88,"p":[[[103.8,204.6],[104.9,207.1]]]},{"h":"path","m":"stone","w":0.78,"p":[[[-52.8,-86.2],[-46.1,-83.3]]]},{"h":"tertiary","m":"asphalt","w":4.39,"p":[[[25.4,-465.0],[-273.4,-345.7],[-511.1,-247.7],[-523.5,-239.5],[-528.5,-233.1],[-560.8,-180.1]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[138.9,354.3],[133.8,360.6],[130.3,370.5],[129.5,396.5]]]},{"h":"service","m":"asphalt","w":2.05,"p":[[[62.5,677.0],[-19.6,667.2]]]}],"scale":0.4882691,"theta":0.21319471,"anchorX":-10.0,"anchorZ":-31.1,"radius":2000.0,"step":100.0,"base":113.0,"timestamp":"2026-11-05T23:29:21Z"};
function addPripyatExpansion() {
  const d=M3D_PRIPYAT_EXPANSION_DATA, {grid,routes}=d, size=grid.length;
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  function world(e,n) { return [d.anchorX+d.scale*(e*Math.cos(d.theta)+n*Math.sin(d.theta)),d.anchorZ+d.scale*(e*Math.sin(d.theta)-n*Math.cos(d.theta))]; }
  function fade(e,n) { return clamp((Math.hypot(e,n)-180)/240,0,1); }
  function getElevation(e,n) {
    const gx=clamp((e+2000)/d.step,0,size-1), gy=clamp((n+2000)/d.step,0,size-1);
    const x0=Math.floor(gx),y0=Math.floor(gy),x1=Math.min(size-1,x0+1),y1=Math.min(size-1,y0+1),tx=gx-x0,ty=gy-y0;
    const samples=[[grid[y0][x0],(1-tx)*(1-ty)],[grid[y0][x1],tx*(1-ty)],[grid[y1][x0],(1-tx)*ty],[grid[y1][x1],tx*ty]];
    let sum=0,weight=0;for(const [v,w] of samples)if(v!==null){sum+=v*w;weight+=w;}
    return weight>0?sum/weight:d.base;
  }
  function height(e,n,offset=0) { return (getElevation(e,n)-d.base)*d.scale*fade(e,n)+offset; }
  function enFromWorld(x,z) { const X=x-d.anchorX,Z=z-d.anchorZ,c=Math.cos(d.theta),s=Math.sin(d.theta);return [(X*c+Z*s)/d.scale,(X*s-Z*c)/d.scale]; }
  // Low-poly ground follows the 100 m SRTM sampling grid, which is sufficient
  // for the documented 5–7 m terrace relief and avoids false building-scale detail.
  for(let j=0;j<size-1;j++)for(let i=0;i<size-1;i++){
    const a=grid[j][i],b=grid[j][i+1],c=grid[j+1][i+1],e=grid[j+1][i];if(a===null||b===null||c===null||e===null)continue;
    const east=-2000+i*d.step,north=-2000+j*d.step;
    const p0=world(east,north),p1=world(east+d.step,north),p2=world(east+d.step,north+d.step),p3=world(east,north+d.step);
    const center=Math.hypot(east+d.step*.5,north+d.step*.5);if(center>d.radius)continue;
    flatQuadMesh([[p0[0],height(east,north),p0[1]],[p1[0],height(east+d.step,north),p1[1]],[p2[0],height(east+d.step,north+d.step),p2[1]],[p3[0],height(east,north+d.step),p3[1]]],'grass',[0,1,0]);
  }
  // Build road surfaces as georeferenced strips. Keep the existing detailed
  // central plaza intact; this layer extends its connected streets outward.
  for(const route of routes)for(const path of route.p)for(let k=0;k<path.length-1;k++){
    const [x0,z0]=path[k],[x1,z1]=path[k+1],dx=x1-x0,dz=z1-z0,len=Math.hypot(dx,dz);if(len<.05)continue;
    const mx=(x0+x1)*.5,mz=(z0+z1)*.5;if(mx>-78&&mx<78&&mz>-88&&mz<105)continue;
    const nx=-dz/len*route.w*.5,nz=dx/len*route.w*.5;
    const corners=[[x0+nx,z0+nz],[x1+nx,z1+nz],[x1-nx,z1-nz],[x0-nx,z0-nz]];
    const quad=corners.map(([x,z])=>{const [e,n]=enFromWorld(x,z);return [x,height(e,n,.035),z];});
    flatQuadMesh(quad,route.m,[0,1,0]);
  }
}

function makeScene() {
  objects.length = 0;
  // The exporter owns the call to makeScene(). Reset local lights too so
  // repeated exports never accumulate duplicate illumination.
  M3D_SCENE_LIGHTS.length = 0;

  // Add the OSM road network and SRTM terrain around the preserved central scene.
  addPripyatExpansion();

  // Coordinate convention: Y is vertical. The camera approaches from +Z.
  // Zone 01 covers the central plaza, Energetik and Hotel Polissya frontages.
  const groundY = -0.08;
  box(-64, -0.32, -70, 64, groundY, 88, 'asphalt');
  box(-58, -0.10, -62, 58, -0.045, 70, 'concrete');

  // Independent plaza component (provisional coordinates).
  m3dBuildPlaza({box});

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

  // Independent building-to-building connector component.
  m3dBuildConnector({box});

  // Main civic building: separately assembled exterior and detailed facade.
  m3dBuildEnergetikShell({box});

  // Independent facade assembly: main building remains traversable.
  m3dBuildEnergetikFacade({box,roundBox,objects});

  m3dBuildEnergetikArchitecturalAccents({box,capsule});

  // Exposed aggregate, broken cladding and rust runoff on the street-facing
  // lintel. The patch layout is irregular so the facade reads as weathered
  // cast concrete rather than a clean repeating block texture.
  const facadeScars=[
    [-44.8,7.65,2.4,0.72],[-41.2,8.10,1.15,0.48],[-36.7,7.42,2.05,0.88],
    [-30.4,8.00,1.40,0.56],[-24.1,7.54,2.25,0.78],[-17.5,8.18,1.05,0.45],
    [-10.3,7.60,1.72,0.66],[-3.8,8.04,2.00,0.50],[3.8,7.46,1.54,0.86],
    [10.9,7.92,2.28,0.56],[18.3,7.50,1.50,0.72],[23.0,8.13,1.12,0.50]
  ];
  for(let i=0;i<facadeScars.length;i++){
    const [x,y,w,h]=facadeScars[i], m=i%3===0?'brick':i%3===1?'plaster':'stone';
    box(x,y,-20.99,x+w,y+h,-20.91,m);
    box(x+0.16,y-0.24,-20.99,x+0.26,y+0.02,-20.90,'rust');
    if(i%2===0) box(x+w-0.24,y+h-0.10,-20.99,x+w+0.28,y+h+0.03,-20.90,'concrete');
  }
  // A few cracked panel edges and exposed anchor bars make damage legible at
  // normal exploration distance without turning every surface into rubble.
  for(const [x,y,len] of [[-39.3,8.9,0.82],[-27.0,8.7,0.58],[-8.6,8.82,0.72],[12.2,8.72,0.64]]){
    capsule([x,y,-21.0],[x+len,y-0.16,-21.0],0.035,'rust');
  }
  // Broad ceremonial approach: six shallow steps and damaged handrails.
  for (let step=0; step<6; step++) {
    const y=0.12+step*0.105, z=-22.2+step*0.42;
    box(-8.6,y,z,8.6,y+0.13,z+0.46,'stone');
  }
  box(-9.1,0.70,-20.0,9.1,0.90,-19.36,'concrete');
  for (const x of [-9.5,9.5]) {
    capsule([x,0.72,-21.8],[x,1.32,-19.8],0.09,'rust');
    for (let z=-21.6;z<-19.9;z+=0.34) capsule([x,0.82,z],[x,1.14,z],0.035,'metal');
  }
  // Nine separately addressable Cyrillic neon letters, split into short glass
  // envelopes and luminous cores for believable tube construction and control.
  const neonManifest = buildEnergetikRoofNeonSign({ objects, box, roundBox, capsule }, 0, 9.43, -19.34, { sectionLength: 0.28 });
  for (const p of neonManifest.lighting.lightPositions)
    M3D_SCENE_LIGHTS.push([p[0], p[1], p[2], 5.4, 0.75, 70, 220, 255]);

  // Three readable levels. Slabs wrap the foyer atrium and the open stair core.
  box(-49.2,0.05,-42.2,29.2,0.38,-20.1,'stone');
  const {levels:archLevels,foyer:archFoyer}=M3D_ENERGETIK_ARCH;
  m3dBuildEnergetikSlabs({box});
  m3dBuildFoyerMuralArcade({box});

  // Open facade bays expose the deep galleries and interior structure, not glass.
  // Keep only damaged railings at the balcony edges, with irregular missing sections.
  for (const y of [archLevels.firstSlab+archLevels.slabThickness+0.03,
                   archLevels.secondSlab+archLevels.slabThickness+0.03]) {
    box(-45.0,y,-19.18,24.6,y+0.075,-19.06,'rust');
    box(-45.0,y+0.88,-19.18,24.6,y+0.96,-19.06,'rust');
    for (let x=-44.4; x<24.2; x+=0.68) {
      if (Math.sin(x*1.71+y*2.3)>0.82) continue; // missing balusters in the ruin
      capsule([x,y+0.08,-19.12],[x,y+0.86,-19.12],0.027,'rust');
    }
  }

  // Rear and end glazing, deep reveals, and long horizontal concrete bands.
  for(let x=-45.5;x<18;x+=6.2){
    const inAuditorium=x>=-47.5&&x<=-17.3;
    if(!inAuditorium){
      box(x,1.0,-42.10,x+3.5,2.65,-41.98,((Math.floor((x+45.5)/6.2)%4)===1)?'windowdark':'glass');
      box(x,4.1,-42.10,x+3.5,5.85,-41.98,'glass');
    }
    box(x,7.1,-42.10,x+3.5,8.75,-41.98,'windowdark');
  }
  for(const y of [3.05,6.15,9.05]) box(-49.2,y,-42.16,29.2,y+0.18,-41.92,'stone');

  // Interior room partitions: a long corridor serves rooms on both sides at each level.
  function doorwayWall(x0,x1,z,y,h,mat){
    const mid=(x0+x1)*0.5, door=1.55, thick=0.18;
    box(x0,y,z,mid-door*.5,y+h,z+thick,mat);
    box(mid+door*.5,y,z,x1,y+h,z+thick,mat);
    box(mid-door*.5,y+2.15,z,mid+door*.5,y+h,z+thick,'concrete');
  }
  function sideDoorWall(x,z0,z1,y,h,mat){
    const mid=(z0+z1)*0.5, door=1.8, thick=0.16;
    box(x,y,z0,x+thick,y+h,mid-door*.5,mat);
    box(x,y,mid+door*.5,x+thick,y+h,z1,mat);
    box(x,y+2.15,mid-door*.5,x+thick,y+h,mid+door*.5,'concrete');
  }
  for(let level=0;level<3;level++){
    const y=[archLevels.ground,archLevels.firstSlab+archLevels.slabThickness,archLevels.secondSlab+archLevels.slabThickness][level];
    const h=([archLevels.firstSlab,archLevels.secondSlab,archLevels.roof][level]-y)-0.12;
    // The mural occupies its own suspended wall / pier arcade, built once.
    // Only the forward-facing room partition continues toward the entrance.
    if(level===0) {
      sideDoorWall(archFoyer.mosaicWallX,-32.0,-26.2,y,h,'concrete');
      box(archFoyer.mosaicWallX,y,-26.2,archFoyer.mosaicWallX+0.16,y+h,-20.1,'concrete');
    } else {
      sideDoorWall(archFoyer.mosaicWallX,-32.0,-26.2,y,h,'concrete');
      box(archFoyer.mosaicWallX,y,-26.2,archFoyer.mosaicWallX+0.16,y+h,-20.1,'concrete');
    }
    for(const [x0,x1] of [[-48.7,-38.0],[-38.0,-27.0],[-27.0,-15.8],[8.2,17.6]]){
      // The west wing is the double-height auditorium, not a stack of small boxes.
      if(x0< -15.8) continue;
      // Low corridor walls and room dividers; the first-floor rear hall stays open as an auditorium.
      if(x0!==8.2) sideDoorWall(x0,-34.0,-27.0,y,h,'concrete');
      sideDoorWall(x0,-27.0,-20.1,y,h,'concrete');
      sideDoorWall(x1-0.16,-27.0,-20.1,y,h,'concrete');
      if(level>0){
        if(x0!==8.2) sideDoorWall(x0,-42.1,-34.0,y,h,'concrete');
        sideDoorWall(x1-0.16,-42.1,-34.0,y,h,'concrete');
      }
      doorwayWall(x0,x1,-27.12,y,h,'concrete');
      doorwayWall(x0,x1,-34.10,y,h,'concrete');
      sideDoorWall(x1-0.16,-34.0,-27.0,y,h,'concrete');
    }
    // Broken plaster and fallen ceiling sections are broad, readable shapes.
    box(-46.0,y+2.25,-26.92,-42.5,y+2.42,-26.72,'debris');
    box(-25.0,y+2.36,-34.28,-21.8,y+2.51,-34.08,'debris');
  }
  // The east stair core is connected through a real doorway in its west wall.
  // Align it with the rear corridor so the guided camera can reach the stairs.
  for(let level=0;level<3;level++){
    const y=[archLevels.ground,archLevels.firstSlab+archLevels.slabThickness,archLevels.secondSlab+archLevels.slabThickness][level];
    box(18.0,y,-42.2,20.0,y+2.15,-31.4,'concrete');
    box(18.0,y,-29.6,20.0,y+2.15,-20.1,'concrete');
    box(18.0,y+2.15,-31.4,20.0,y+2.72,-29.6,'concrete');
  }

  // Ground floor: foyer, abandoned auditorium, stage, and a few large room fixtures.
  for(let x=-12;x<=7;x+=6) capsule([x,0.38,-33.5],[x,8.9,-33.5],0.28,'stone');
  // Hero cinema: a two-storey volume, not a low box. The first upper slab is
  // cut away above the audience; concrete girders still carry the room's roof.
  // The recovered Pripyat finish is red/rose volcanic tuff, now visible under
  // missing panels instead of a generic repeating grey wall.
  const hallFloor=archLevels.ground, hallTop=archLevels.secondSlab;
  box(-49.18,hallFloor,-41.98,-48.94,hallTop,-24.96,'volcanicTuff');
  box(-16.04,hallFloor,-41.98,-15.80,hallTop,-31.52,'volcanicTuff');
  box(-16.04,hallFloor,-29.48,-15.80,hallTop,-24.96,'volcanicTuff'); // doorway to the hall
  box(-49.18,hallFloor,-42.00,-15.80,hallTop,-41.78,'volcanicTuff');
  // Structural girders, exposed services and broken suspended ceiling panels.
  for(const z of [-39.9,-36.5,-33.1,-29.7,-26.3]){
    box(-48.6,5.74,z-0.28,-16.3,6.17,z+0.28,'concrete');
    box(-48.6,5.62,z-0.34,-16.3,5.76,z-0.25,'rust');
  }
  for(const x of [-44.0,-36.5,-29.0,-21.5])
    box(x-0.22,5.80,-40.9,x+0.22,6.18,-25.2,'concrete');
  for(const [x,z,w,d] of [[-45.5,-38.7,4.1,1.5],[-39.0,-34.3,3.2,1.2],[-29.4,-28.8,4.0,1.7],[-21.6,-36.4,2.9,1.1]]){
    box(x,6.18,z,x+w,6.23,z+d,'windowdark'); // lost ceiling cassette
    box(x-0.16,5.96,z-0.10,x+w+0.20,6.04,z+d+0.14,'debris');
  }
  // Tuff panels are layered over the structural wall. Missing cladding exposes
  // rough concrete, with irregular edges and rust tracks at failed anchors.
  for(const [x,z,w,h] of [
    [-48.90,-39.0,0.74,0.96],[-48.90,-34.0,0.52,1.16],[-48.90,-28.4,0.88,0.76],
    [-15.78,-38.0,0.86,0.82],[-15.78,-34.8,0.48,1.22],[-15.78,-27.0,0.92,0.72],
    [-35.5,-41.74,1.10,0.58],[-27.0,-41.74,0.64,0.92],[-43.4,-41.74,0.78,0.62]
  ]){
    const vertical=x> -18.0&&x< -15.0;
    if(vertical) box(x,1.0,z,x+0.035,1.0+h,z+w,'concrete');
    else box(x,1.0,z,x+w,1.0+h,z+0.035,'concrete');
  }
  for(const side of [-1,1]){
    const wallX=side<0?-48.86:-15.76;
    for(let i=0;i<10;i++){
      const z=-40.5+i*1.48;
      if(side>0&&z>-32.3&&z<-29.1) continue;
      const y=1.20+(i%3)*0.64, height=1.02+(i%2)*0.28;
      const x2=wallX+side*0.045;
      box(Math.min(wallX,x2),y,z,Math.max(wallX,x2),y+height,z+0.74,(i%4===0)?'plaster':'volcanicTuff');
      if(i%3===1){
        const x3=wallX+side*0.05,x4=wallX+side*0.10;
        box(Math.min(x3,x4),y+height*0.43,z+0.70,Math.max(x3,x4),y+height*0.47,z+0.82,'rust');
      }
    }
  }
  // Stage has a deep proscenium, tuff returns, broken timber deck and layered
  // curtains. Vertical folds and ripped edges keep it from reading as flat boxes.
  box(-43.5,0.38,-41.72,-21.5,0.91,-38.48,'wood');
  box(-43.5,0.16,-38.62,-21.5,0.89,-38.43,'wood');
  for(let z=-41.58;z<-38.58;z+=0.19){
    const missing=Math.sin(z*4.7)>0.87;
    if(!missing) box(-43.38,0.91,z,-21.62,0.98,z+0.145,(Math.floor((z+42)*10)%6===0)?'debris':'wood');
  }
  box(-45.6,0.90,-41.80,-20.0,3.55,-41.62,'volcanicTuff');
  box(-42.0,1.06,-41.58,-24.0,3.34,-41.48,'rust');
  box(-41.68,1.20,-41.46,-24.32,3.10,-41.39,'cinemaScreen');
  box(-45.2,0.88,-41.42,-42.15,3.60,-41.30,'curtain');
  box(-23.85,0.88,-41.42,-20.8,3.60,-41.30,'curtain');
  box(-45.35,3.34,-41.43,-20.65,3.62,-41.29,'curtain');
  // Keep the draped folds at the sides of the opening; the previous full-span
  // strips sat in front of the screen and read as a cage of vertical bars.
  for(const [start,end] of [[-45.0,-42.0],[-24.0,-20.9]]){
    for(let x=start;x<=end;x+=0.42){
      const fold=Math.sin(x*1.7)*0.045;
      const torn=Math.sin(x*3.1)>0.72;
      capsule([x,0.92,-41.18+fold],[x+0.05,torn?2.35:3.45,-41.18+fold],0.095,(Math.floor((x+45)*10)%5===0)?'seatFabric':'curtain');
    }
  }
  for(const [x,y,w,h] of [[-44.7,1.0,0.42,0.78],[-43.0,2.1,0.34,0.56],[-22.6,1.25,0.50,0.94],[-20.8,2.4,0.44,0.56]])
    box(x,y,-41.22,x+w,y+h,-41.17,'debris');
  // Side-wall acoustic absorbers, oxidized mounting rails and damaged speakers.
  for(const side of [-1,1]) for(let i=0;i<7;i++){
    const z=-39.4+i*1.92, x=side<0?-48.78:-15.84;
    if(side>0&&z>-32.3&&z<-29.1) continue;
    const panelX=x+side*0.07, railX=x+side*0.08;
    if(i%5!==2) box(Math.min(x,panelX),3.55+(i%2)*0.42,z,Math.max(x,panelX),4.92+(i%3)*0.10,z+0.92,(i%3===0)?'wood':'curtain');
    box(Math.min(x,railX),3.42,z,Math.max(x,railX),3.49,z+1.06,'rust');
  }
  for(const [x,y,z] of [[-48.5,4.95,-37.2],[-15.96,4.85,-26.4],[-15.96,4.92,-38.5]]){
    box(x,y,z,x+0.22,y+0.54,z+0.36,'metal');
    box(x+0.04,y+0.05,z-0.02,x+0.25,y+0.47,z+0.38,'windowdark');
  }
  // Twelve stepped seating rows use the shared, clean chair asset. Keeping the
  // chair geometry in one module lets its preview and this room stay in sync.
  const bankCenters=[-45.0,-37.5,-29.8,-22.3], rowCount=12, seatsPerBank=10, seatPitch=0.58;
  for(let row=0;row<rowCount;row++){
    const z=-37.70+row*0.94, platformTop=0.15*(row+1);
    for(let bank=0;bank<bankCenters.length;bank++){
      const center=bankCenters[bank];
      const bankLeft=center-(seatsPerBank-1)*seatPitch*0.5-0.34;
      const bankRight=center+(seatsPerBank-1)*seatPitch*0.5+0.34;
      box(bankLeft,0.04,z-0.38,bankRight,platformTop,z+0.42,(row%4===0)?'concrete':'stone');
      for(let seat=0;seat<seatsPerBank;seat++){
        const x=center+(seat-(seatsPerBank-1)*0.5)*seatPitch;
        buildEnergetikCinemaSeat({ box, capsule, ellipse, roundBox }, x, platformTop, z, { folded: false, fabric: 'seatVinylBrown', trim: 'seatVinylBrown', support: 'seatSupportPaint' });
      }
    }
  }
  // Debris field: broken timber, tuff chips, torn fabric and fallen ceiling
  // rails are concentrated in the aisles; keep the exits and front row clear.
  for(const [x,z,w,d,h,mat] of [
    [-40.0,-33.0,0.82,0.22,0.08,'debris'],[-32.4,-31.5,0.42,0.72,0.06,'wood'],
    [-41.1,-27.8,0.74,0.20,0.05,'plaster'],[-25.9,-28.4,0.27,0.82,0.06,'debris'],
    [-18.8,-35.6,0.64,0.20,0.05,'wood'],[-48.0,-30.4,0.55,0.42,0.08,'debris'],
    [-19.2,-26.2,0.32,0.48,0.07,'plaster'],[-34.7,-38.3,1.35,0.12,0.035,'rust'],
    [-24.8,-32.1,0.46,0.56,0.05,'volcanicTuff'],[-44.5,-29.4,0.62,0.37,0.045,'seatFabric'],
    [-36.2,-26.0,0.82,0.20,0.035,'wood'],[-29.0,-38.1,0.38,0.72,0.05,'debris']
  ]) roundBox(x,0.06,z,x+w,0.06+h,z+d,0.025,mat);
  for(const [x,z] of [[-47.7,-36.6],[-15.9,-35.0],[-48.4,-26.0],[-17.2,-39.2]])
    M3D_SCENE_LIGHTS.push([x,4.8,z,6.6,0.62,246,177,112]);
  for(const [x,z,working] of [[-44.0,-38.8,true],[-37.0,-35.2,true],[-29.7,-31.6,false],[-22.3,-27.8,true],[-37.2,-27.6,true]]){
    box(x-0.72,5.32,z-0.18,x+0.72,5.47,z+0.18,'metal');
    box(x-0.58,5.30,z-0.12,x+0.58,5.39,z+0.12,working?'lamp':'windowdark');
    if(working) M3D_SCENE_LIGHTS.push([x,5.04,z,8.8,1.08,255,203,158]);
  }
  // A ruined reception desk, scattered panels and tall foyer openings.
  box(-4.8,0.42,-24.8,4.2,1.25,-23.8,'wood');
  box(-3.8,1.25,-24.45,3.0,1.42,-24.15,'stone');
  for(const p of [[-11,0.40,-25.5,-8,0.55,-24.8],[7,0.40,-31.8,9,0.60,-30.8],
                  [-20,0.40,-39.6,-17,0.57,-38.8],[12,3.55,-24.8,15,3.72,-24.1]])
    box(...p,'debris');

  m3dBuildFoyerStairFlights({box,capsule});
  // Decorative wall panels and faded mural fragments based on documented
  // Energetik lobby photographs, with peeled plaster around the remaining art.
  // Mount the panels on the INTERIOR faces. The earlier positive-X offsets
  // put both side-wall murals behind the exterior shell, so they never showed.
  for(const [wallX,z,y,w,h] of [[-49.32,-35.5,4.0,6.8,2.2],[-49.32,-27.0,4.2,5.2,1.8],[28.70,-39.3,3.9,14.8,2.35]]){
    const x0=wallX, x1=wallX+0.16;
    box(x0,y,z,x1,y+h,z+w,'rust');
    box(x0+0.12,y+0.12,z+0.12,x1+0.12,y+h-0.10,z+w-0.12,'mural');
    for(let i=0;i<5;i++){
      const zz=z+0.34+i*(w-0.75)/5;
      box(x1+0.13,y+0.32,zz,x1+0.18,y+h-0.28,zz+0.12,i%2?'plaster':'brick');
    }
    box(x1+0.13,y+h*0.50,z+0.18,x1+0.19,y+h*0.50+0.10,z+w*0.46,'stone');
    box(x1+0.13,y+0.24,z+w*0.57,x1+0.19,y+0.36,z+w-0.24,'plaster');
  }
  // The foyer mosaic is a reusable, photo-guided texture on the hall-facing stair wall.
  // Dimensions are the current 9.5 x 4.3 m estimate; update the manifest if surveyed.
  // The supporting wall now sits 3 cm behind the painted plane, not 26 cm.
  // The painted plane faces -X and spans toward +Z.
  // The mural spans the atrium across the first-floor level; never crop it
  // with a mezzanine slab. Keep the historically estimated 4.3 m height.
  const frescoX=archFoyer.mosaicSurfaceX, frescoY0=archFoyer.mosaicBottomY, frescoY1=archFoyer.mosaicTopY;
  const frescoZ0=archFoyer.mosaicStartZ, frescoZ1=archFoyer.mosaicEndZ;
  const frescoNormal=[-1,0,0];
  const frescoTopV=0.27294921875, frescoBottomV=0.72705078125;
  const frescoBL={p:[frescoX,frescoY0,frescoZ0],n:frescoNormal,uv:[0,frescoBottomV]};
  const frescoBR={p:[frescoX,frescoY0,frescoZ1],n:frescoNormal,uv:[1,frescoBottomV]};
  const frescoTL={p:[frescoX,frescoY1,frescoZ0],n:frescoNormal,uv:[0,frescoTopV]};
  const frescoTR={p:[frescoX,frescoY1,frescoZ1],n:frescoNormal,uv:[1,frescoTopV]};
  meshTriangle(frescoBL,frescoBR,frescoTL,'energetikFresco');
  meshTriangle(frescoBR,frescoTR,frescoTL,'energetikFresco');
  // Ceiling failures and exposed glazing frames leave specific traces of decay.
  for(const [x,z,w,d] of [[-39,-30,4.2,2.1],[-27,-23,3.0,1.5],[12,-29,3.6,2.0]]){
    box(x,8.36,z,x+w,8.52,z+d,'windowdark');
    box(x-0.2,8.40,z-0.18,x+w+0.26,8.58,z+0.22,'debris');
  }
  // Broken suspended ceiling at the foyer perimeter: surviving metal T-rails,
  // missing panels, hanging cable and collapsed tile fragments.
  for(const z of [-39.8,-37.2,-23.4,-21.4])
    box(-13.8,8.28,z,7.2,8.34,z+0.055,'metal');
  for(const x of [-12.8,-9.6,-6.4,-3.2,0,3.2,6.4])
    box(x,8.28,-40.1,x+0.05,8.34,-21.2,'metal');
  for(const [x,z,w,d] of [[-12.3,-39.4,2.0,1.3],[-7.2,-38.8,1.4,0.9],
                           [1.1,-23.1,2.2,1.2],[4.0,-37.8,1.6,1.1]])
    box(x,8.22,z,x+w,8.30,z+d,'debris');
  for(const [x,z] of [[-10.6,-36.4],[-2.0,-22.4],[5.1,-35.0]])
    capsule([x,8.30,z],[x+0.06,7.42,z+0.18],0.018,'rust');
  for(const [x,y] of [[-31.6,1.0],[-20.8,4.1],[8.4,7.1],[14.6,1.0]]){
    if(x< -17.3) continue; // no window frames behind the auditorium screen
    box(x,y,-42.24,x+3.5,y+1.65,-42.08,'windowdark');
    box(x+0.08,y+0.06,-42.30,x+0.18,y+1.56,-42.16,'rust');
    box(x+3.05,y+0.06,-42.28,x+3.16,y+1.56,-42.14,'rust');
  }

  // Upper rooms retain a few recognizable large-scale fixtures and rubble.
  for(let level=1;level<3;level++){
    const y=[archLevels.ground,archLevels.firstSlab+archLevels.slabThickness,archLevels.secondSlab+archLevels.slabThickness][level]+0.04;
    for(const x of [-43,-32,-21,12]){
      if(level===1&&x< -17.3) continue; // preserve the double-height hall void
      box(x-1.4,y,-24.6,x+1.4,y+0.72,-23.8,'wood');
      box(x+1.8,y,-25.2,x+2.65,y+2.2,-24.3,'debris');
      box(x-3.2,y,-38.5,x-1.2,y+1.9,-37.8,'wood');
    }
  }
  m3dBuildEastStairCore({box,capsule});
  // Leave a genuine 3.8 m rooftop doorway in the stair tower wall.
  box(20.0,archLevels.roof,-22.2,21.7,M3D_ENERGETIK_ARCH.tower.roofY-0.7,-21.0,'rust');
  box(25.3,archLevels.roof,-22.2,27.0,M3D_ENERGETIK_ARCH.tower.roofY-0.7,-21.0,'rust');
  box(21.7,M3D_ENERGETIK_ARCH.tower.roofY-2.0,-22.2,25.3,M3D_ENERGETIK_ARCH.tower.roofY-0.7,-21.0,'concrete');

  // Roof access: height driven by shared datum, retaining the existing materials.
  const mainRoofY=archLevels.roof, roofDeckTop=mainRoofY+0.30;
  box(-49.2,mainRoofY,-42.2,-15,roofDeckTop,-20.1,'concrete');
  box(-15,mainRoofY,-42.2,8,roofDeckTop,-35.8,'concrete');
  box(-15,mainRoofY,-26.5,8,roofDeckTop,-20.1,'concrete');
  box(-15,mainRoofY,-35.8,-7,roofDeckTop,-26.5,'concrete');
  box(6,mainRoofY,-35.8,8,roofDeckTop,-26.5,'concrete');
  box(8,mainRoofY,-42.2,18,roofDeckTop,-20.1,'concrete');
  box(18,mainRoofY,-42.2,20,roofDeckTop,-20.1,'concrete');
  box(27,mainRoofY,-42.2,29.2,roofDeckTop,-20.1,'concrete');
  box(20,mainRoofY,-42.2,27,roofDeckTop,-38,'concrete');
  box(20,mainRoofY,-23,27,roofDeckTop,-20.1,'concrete');
  for(const [x0,x1,z0,z1] of [[-49.2,29.2,-42.2,-41.2],[-49.2,29.2,-20.4,-19.6],[-49.2,-48.3,-42.2,-20.0],[28.3,29.2,-42.2,-20.0]])
    box(x0,roofDeckTop,z0,x1,roofDeckTop+0.85,z1,'concrete');
  for(const [x,z,h] of [[-36,-38,1.2],[-7,-40,1.8],[12,-22,1.1],[17,-39,1.4]])
    box(x,roofDeckTop,z,x+1.4,roofDeckTop+h,z+1.2,'rust');
  for(let x=-44;x<27;x+=9) ellipsoidMesh(x,mainRoofY+0.45,-20.8,1.0,0.12,0.55,'grass',8,6);

  // Separate reusable Polissya building module. Historical geometry still pending survey.
  m3dBuildPolissya({box,capsule});

  // Repeated small windows add depth to the visible west side of Energetik.
  for (let z = -43; z <= -22; z += 4.2) {
    for (let y = 2.0; y < 8.4; y += 2.9) {
      box(-50.58, y, z, -50.28, y + 1.5, z + 1.7, 'windowdark');
      box(-50.66, y - 0.12, z - 0.12, -50.22, y + 1.62, z + 1.82, 'stone');
    }
  }
  // Each residential block is generated independently from the scene registry.
  for(const building of M3D_PRIPYAT_BLOCKOUT.housing) m3dBuildHousing({box},building);

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

  // Cheap painted-tube public chairs with weathered timber slats. Keep a few
  // individually placed pieces around the plaza; these are not auditorium seats.
  for (const chair of [
    [-25, 11, 0, 'worn_intact'], [-22, 11, Math.PI/2, 'missing_back_slat'],
    [19, 11, Math.PI/2, 'worn_intact'], [22, 11, 0, 'missing_seat_slat'],
    [-3, 29, 0, 'broken_slat'], [1, 29, Math.PI/2, 'worn_intact']
  ]) buildPripyatOutdoorChair({objects,box,roundBox,capsule,ellipse},chair[0],0,chair[1],
    {angle:chair[2],variant:chair[3]});

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
  if (process.env.M3D_IMPORT_PRIPYAT_GLB !== '1') {
    for (let i=0;i<matureTrees.length;i++) poplar(...matureTrees[i],0.76+(i%4)*0.11);
  }
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

  // Interior ceiling fixtures and warm pools of light. Keep a few dead lamps
  // among the working ones so the abandoned rooms retain an uneven mood.
  const interiorFixtures=[
    [-43.4,-30.5],[-32.5,-30.5],[-21.4,-30.5],[-4.0,-30.5],[4.0,-30.5],[12.9,-30.5],[23.5,-30.5],
    [-43.4,-38.0],[-32.5,-38.0],[-21.4,-38.0],[12.9,-38.0],[23.5,-36.0]
  ];
  for(let level=0;level<3;level++){
    const ceilings=[archLevels.firstSlab,archLevels.secondSlab,archLevels.roof];
    const y=ceilings[level]-0.31;
    for(let i=0;i<interiorFixtures.length;i++){
      const [x,z]=interiorFixtures[i];
      if(level<=1&&x< -15.0&&z< -24.8) continue; // custom auditorium lighting sits at the 6 m ceiling
      const failed=(i+level*3)%7===0;
      box(x-0.48,y,z-0.12,x+0.48,y+0.055,z+0.12,failed?'rust':'lamp');
      if(!failed && i%3===0) M3D_SCENE_LIGHTS.push([x,y-0.20,z,8.2,1.65,255,190,126]);
    }
  }

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
  for (const x of [-43,-33,-23,-13,-3,7,17,27,37,47]) {
    M3D_SCENE_LIGHTS.push([x,5,-15.8,12,0.20,236,204,157]);
  }
  // Daylight bounce under the open Energetik colonnade. These broad, pale
  // fills lift the foyer and inner balcony faces without adding fake fixtures.
  for (const x of [-39,-24,-9,7,22]) {
    M3D_SCENE_LIGHTS.push([x,5.1,-24.2,13.5,0.78,255,232,204]);
  }
}
