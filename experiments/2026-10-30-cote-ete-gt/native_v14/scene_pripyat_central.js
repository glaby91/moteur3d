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

function makeScene() {
  objects.length = 0;
  // The exporter owns the call to makeScene(). Reset local lights too so
  // repeated exports never accumulate duplicate illumination.
  M3D_SCENE_LIGHTS.length = 0;

  // Coordinate convention: Y is vertical. The camera approaches from +Z.
  // Zone 01 covers the central plaza, Energetik and Hotel Polissya frontages.
  const groundY = -0.08;
  box(-64, -0.32, -70, 94, groundY, 88, 'asphalt');
  box(-58, -0.10, -62, 89, -0.045, 70, 'concrete');

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
  box(-64, -0.015, 65, 94, 0.025, 88, 'asphalt');
  box(-64, 0.025, 61, 94, 0.13, 65, 'concrete');
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

  // The low connector is kept as context between the hero building and the hotel.
  box(30.2,0.0,-24.0,40.2,5.8,-18.6,'concrete');
  box(30.0,5.65,-24.2,40.4,6.05,-18.4,'stone');
  for(let x=31.0;x<39.5;x+=2.05){
    box(x,0.65,-18.78,x+1.25,3.4,-18.58,'glass');
    box(x-0.10,0.48,-18.83,x+1.35,0.62,-18.52,'stone');
  }

  // Hero asset: the Palace of Culture Energetik is the only fully developed building.
  // Approximate envelope: 81 x 24 m, 11.4 m main roof and a 14 m stair tower.
  // The outer shell is deliberately split into walls and slabs so the camera can enter.
  const ex0=-50.5, ex1=30.5, front=-18.8, back=-43.4;
  box(ex0,0,back,ex0+1.2,9.4,front,'concrete');
  box(ex1-1.2,0,back,ex1,9.4,front,'concrete');
  box(ex0,0,back,ex1,9.4,back+1.2,'concrete');
  box(ex0,0,-20.2,-46.8,9.4,front,'concrete');
  box(25.5,0,-20.2,ex1,9.4,front,'concrete');
  box(-46.8,8.8,-20.2,25.5,9.4,front,'stone');
  box(-42.0,9.4,-41.8,20.5,11.35,-20.4,'concrete');
  // Hollow stair tower. The old solid prism capped the ascending camera and
  // made the roof door look like a wall; retain only the concrete shell.
  box(20.5,9.4,-40.8,21.7,14.0,-22.2,'concrete');
  box(27.0,9.4,-40.8,30.5,14.0,-22.2,'concrete');
  box(21.7,9.4,-40.8,27.0,14.0,-39.6,'concrete');
  box(21.7,12.0,-22.2,25.3,14.0,-21.0,'concrete');
  box(27.0,9.4,-22.2,30.5,14.0,-21.0,'concrete');
  box(-43.0,11.25,-42.5,18.4,11.75,-20.1,'stone');

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
  function floorPlate(y, openAuditorium=false){
    const t=y,b=y+0.24;
    if(openAuditorium){
      // Keep the west-side audience hall open through the first upper slab.
      // Narrow side strips and a rear cross-corridor still connect the upper floor.
      box(-49.2,t,-42.2,-47.5,b,-25.2,'concrete');
      box(-17.3,t,-42.2,-15,b,-25.2,'concrete');
      box(-49.2,t,-25.2,-15,b,-20.1,'concrete');
    } else box(-49.2,t,-42.2,-15,b,-20.1,'concrete');
    box(-15,t,-42.2,8,b,-35.8,'concrete');
    box(-15,t,-26.5,8,b,-20.1,'concrete');
    box(-15,t,-35.8,-7,b,-26.5,'concrete');
    box(6,t,-35.8,8,b,-26.5,'concrete');
    box(8,t,-42.2,18,b,-20.1,'concrete');
    box(18,t,-42.2,20,b,-20.1,'concrete');
    box(27,t,-42.2,29.2,b,-20.1,'concrete');
    box(20,t,-42.2,27,b,-38.0,'concrete');
    box(20,t,-23.0,27,b,-20.1,'concrete');
  }
  floorPlate(3.25,true); floorPlate(6.35);

  // Open facade bays expose the deep galleries and interior structure, not glass.
  // Keep only damaged railings at the balcony edges, with irregular missing sections.
  for (const y of [3.28, 6.38]) {
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
    const y=0.38+level*3.1, h=2.72;
    // A real return wall frames the foyer stair and gives the side-room door
    // a proper opening; the mural is mounted on its hall-facing surface.
    sideDoorWall(8.38,-34.8,-26.2,y,h,'concrete');
    // Continue that wall to both ends of the fresco. Previously those spans
    // had no backing, leaving most of the mural suspended in the atrium.
    box(8.38,y,-42.2,8.54,y+h,-34.8,'concrete');
    box(8.38,y,-26.2,8.54,y+h,-20.1,'concrete');
    for(const [x0,x1] of [[-48.7,-38.0],[-38.0,-27.0],[-27.0,-15.8],[8.2,17.6]]){
      // The west wing is the double-height auditorium, not a stack of small boxes.
      if(x0< -15.8) continue;
      // Low corridor walls and room dividers; the first-floor rear hall stays open as an auditorium.
      sideDoorWall(x0,-34.0,-27.0,y,h,'concrete');
      sideDoorWall(x0,-27.0,-20.1,y,h,'concrete');
      sideDoorWall(x1-0.16,-27.0,-20.1,y,h,'concrete');
      if(level>0){
        sideDoorWall(x0,-42.1,-34.0,y,h,'concrete');
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
    const y=0.38+level*3.1;
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
  const hallFloor=0.38, hallTop=6.35;
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

  // Foyer stair flights and wall cladding add a readable human scale indoors.
  for(const side of [-1,1]){
    const x0=side<0?-11.0:-1.2, x1=side<0?-1.7:8.0;
    for(let i=0;i<17;i++){
      const y=0.48+i*0.162, z=-25.0-i*0.50;
      box(x0,y,z,x1,y+0.17,z+0.50,'stone');
    }
    const railX=side<0?-10.75:7.75;
    capsule([railX,0.80,-24.8],[railX,3.25,-33.0],0.10,'rust');
    for(let i=0;i<10;i++){
      const z=-25.1-i*0.73, y=1.0+i*0.22;
      capsule([railX,y,z],[railX,y+0.43,z],0.035,'metal');
    }
  }
  box(-12.4,3.25,-34.1,9.2,3.48,-32.8,'stone');
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
  // The supporting wall sits at X=8.38 m. This plane faces -X and spans toward +Z.
  // The mural belongs to the upper stair-hall wall, not the ground-floor partition.
// Keep the measured-from-photo 9.5 x 4.3 m estimate, but position its lower
// edge above the first mezzanine slab (top = 3.49 m). This prevents the image
// crossing into the lower storey. Heights remain provisional pending survey.
// Reference photos: Wikimedia Commons, "Palace of Culture Lobby-pripyat.jpg"
// and "Pripyat - cultural center 08.jpg".
const frescoX=8.12, frescoY0=3.72, frescoY1=8.02;
// A continuous upper wall is required behind the mural, instead of relying
// on the repeating storey partitions which break at each slab.
box(8.38,3.50,-41.80,8.54,8.24,-31.80,'concrete');
  const frescoZ0=-41.55, frescoZ1=-32.05;
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
    const y=0.42+level*3.1;
    for(const x of [-43,-32,-21,12]){
      if(level===1&&x< -17.3) continue; // preserve the double-height hall void
      box(x-1.4,y,-24.6,x+1.4,y+0.72,-23.8,'wood');
      box(x+1.8,y,-25.2,x+2.65,y+2.2,-24.3,'debris');
      box(x-3.2,y,-38.5,x-1.2,y+1.9,-37.8,'wood');
    }
  }
  // Stairs wind through the open east core; landings meet all three floors and the roof.
  for(let flight=0;flight<3;flight++){
    const baseY=0.45+flight*3.1, baseZ=-40.5+flight*0.4;
    for(let step=0;step<11;step++){
      const y=baseY+step*0.255, z=baseZ+step*1.42;
      box(21.0,y,z,25.7,y+0.18,z+1.35,'stone');
    }
    box(20.0,baseY+2.7,-24.8,27.0,baseY+3.0,-21.2,'concrete');
    box(20.0,baseY+2.7,-26.7,27.0,baseY+3.0,-23.0,'concrete');
  }
  // Leave a genuine 3.8 m rooftop doorway in the stair tower wall.
  box(20.0,9.4,-22.2,21.7,13.3,-21.0,'rust');
  box(25.3,9.4,-22.2,27.0,13.3,-21.0,'rust');
  box(21.7,12.0,-22.2,25.3,13.3,-21.0,'concrete');

  // Roof access, low parapet, vents, weeds, and an open atrium that admits direct sun.
  box(-49.2,9.4,-42.2,-15,9.7,-20.1,'concrete');
  box(-15,9.4,-42.2,8,9.7,-35.8,'concrete');
  box(-15,9.4,-26.5,8,9.7,-20.1,'concrete');
  box(-15,9.4,-35.8,-7,9.7,-26.5,'concrete');
  box(6,9.4,-35.8,8,9.7,-26.5,'concrete');
  box(8,9.4,-42.2,18,9.7,-20.1,'concrete');
  box(18,9.4,-42.2,20,9.7,-20.1,'concrete');
  box(27,9.4,-42.2,29.2,9.7,-20.1,'concrete');
  box(20,9.4,-42.2,27,9.7,-38,'concrete');
  box(20,9.4,-23,27,9.7,-20.1,'concrete');
  for(const [x0,x1,z0,z1] of [[-49.2,29.2,-42.2,-41.2],[-49.2,29.2,-20.4,-19.6],[-49.2,-48.3,-42.2,-20.0],[28.3,29.2,-42.2,-20.0]])
    box(x0,9.7,z0,x1,10.55,z1,'concrete');
  for(const [x,z,h] of [[-36,-38,1.2],[-7,-40,1.8],[12,-22,1.1],[17,-39,1.4]])
    box(x,9.7,z,x+1.4,9.7+h,z+1.2,'rust');
  for(let x=-44;x<27;x+=9) ellipsoidMesh(x,9.85,-20.8,1.0,0.12,0.55,'grass',8,6);

  // Polissya hotel: separate urban landmark, NOT attached to Energetik.
  // Preliminary spatial layout, pending a georeferenced survey: nearest edges
  // are 24 m apart in X (Energetik ends at 30.5; hotel starts at 54.5).
  // Preserve the pedestrian corridor and avoid a false connecting volume.
  const hotelX0=54.5, hotelX1=83.5, hotelFront=-17.2, hotelBack=-43.8;
  box(hotelX0,0,hotelBack,hotelX1,15.5,hotelFront,'concrete');
  box(hotelX0+4.2,15.5,hotelBack+2.2,hotelX1-3.8,20.0,hotelFront-2.5,'concrete');
  // Late-Soviet hotel: concrete grid, recessed windows, continuous sill
  // courses, projecting vertical stair bay, entrance canopy and parapet.
  for(let floor=0;floor<5;floor++){
    const y=1.8+floor*2.65;
    box(hotelX0+.25,y-.30,hotelFront-.34,hotelX1-.25,y-.16,hotelFront-.08,'stone');
    for(let x=hotelX0+1.65;x<hotelX1-1.4;x+=3.15){
      box(x-.20,y-.18,hotelFront-.40,x+1.70,y+1.78,hotelFront-.10,'stone');
      box(x,y,hotelFront-.48,x+1.30,y+1.50,hotelFront-.38,
          (floor===2&&x>hotelX0+13)?'windowlit':'windowdark');
      box(x,y-.15,hotelFront-.55,x+1.30,y-.05,hotelFront-.37,'concrete');
    }
  }
  box(hotelX0+1.1,0,hotelFront-.85,hotelX0+6.2,15.5,hotelFront-.18,'concrete');
  for(let y=1.6;y<14.0;y+=2.65)
    box(hotelX0+2.4,y,hotelFront-.95,hotelX0+4.7,y+1.65,hotelFront-.82,'windowdark');
  box(hotelX0+10.0,3.1,hotelFront-3.0,hotelX0+21.0,3.45,hotelFront+.4,'stone');
  for(let x=hotelX0+11.0;x<hotelX0+21.0;x+=4.2)
    box(x,0.15,hotelFront-2.6,x+.38,3.1,hotelFront-2.2,'concrete');
  box(hotelX0-.3,15.5,hotelBack-.3,hotelX1+.3,16.2,hotelFront+.3,'stone');
  // Transitional landscaping, curb stones and a readable pedestrian approach.
  for(let x=33.0;x<52.0;x+=2.2){
    box(x,.03,-16.4,x+1.3,.08,-15.7,'stone');
    if(Math.round(x)%3===0)box(x,.09,-15.2,x+.85,.25,-14.3,'grass');
  }
  // Low concrete planters, lamp pedestals and architectural bollards.
  for(const [x,z] of [[35,-9],[42,-9],[49,-9],[39,4],[48,4]]){
    box(x-.75,.02,z-.75,x+.75,.55,z+.75,'concrete');
    box(x-.52,.55,z-.52,x+.52,.68,z+.52,'stone');
    box(x-.35,.68,z-.35,x+.35,.82,z+.35,'grass');
  }
  for(const x of [34.5,43.5,51.0]){
    box(x-.14,.02,-12.4,x+.14,1.15,-12.12,'rust');
    box(x-.24,1.12,-12.52,x+.24,1.28,-12.02,'stone');
  }

  // Central-square urban ensemble (schematic, metres; not surveyed).
  // The historical Energetik-Polissya link is a curved arch, not an isolated gap.
  // Construct a faceted shallow arch with an open pedestrian passage below.
  // The actual gap and orientation must be calibrated from aerial photographs.
  const archLeft=30.5, archRight=54.5, archFront=-19.0;
  const archSegments=18;
  for(let i=0;i<archSegments;i++){
    const a=archLeft+(archRight-archLeft)*i/archSegments;
    const c=archLeft+(archRight-archLeft)*(i+1)/archSegments;
    const u=(a+c)*.5, q=(u-(archLeft+archRight)*.5)/((archRight-archLeft)*.5);
    const archSoffit=3.2+2.1*Math.sqrt(Math.max(0,1-q*q));
    const archCrown=7.5+0.9*Math.sqrt(Math.max(0,1-q*q));
    box(a,archSoffit,archFront-.70,c,archCrown,archFront+.70,'stone');
    box(a,archCrown,archFront-.82,c,archCrown+.18,archFront+.82,'concrete');
  }
  box(archLeft-.4,0,archFront-1.15,archLeft+.75,7.8,archFront+1.15,'concrete');
  box(archRight-.75,0,archFront-1.15,archRight+.4,7.8,archFront+1.15,'concrete');
  // Street and pedestrian approach continue into the outer 100 m ring.
  box(-92,-.28,65,105,-.12,104,'asphalt');
  box(-92,-.09,62,105,.04,65,'concrete');
  box(-92,-.09,104,105,.04,107,'concrete');
  for(let x=-84;x<96;x+=9.5){
    box(x,-.10,84.3,x+4.2,-.07,84.48,'stone');
  }
  // Administrative block, shopping arcade and restaurant: massing placeholders
  // at the square's far edge. Their surveyed footprints remain to be fitted.
  // Distinct rooflines and facade rhythms avoid generic identical boxes.
  const civic=[{x0:-85,x1:-48,z0:109,z1:129,h:13.2,kind:'admin'},
               {x0:-38,x1:13,z0:112,z1:133,h:8.5,kind:'shops'},
               {x0:22,x1:56,z0:110,z1:131,h:9.8,kind:'restaurant'}];
  for(const c of civic){
    box(c.x0,0,c.z0,c.x1,c.h,c.z1,'concrete');
    box(c.x0-.3,c.h,c.z0-.3,c.x1+.3,c.h+.55,c.z1+.3,'stone');
    box(c.x0,0,c.z0-.8,c.x1,.6,c.z0+.4,'stone');
    const floors=c.kind==='admin'?4:c.kind==='shops'?2:3;
    for(let f=0;f<floors;f++){
      const y=.9+f*(c.h-.9)/floors;
      for(let x=c.x0+1.5;x<c.x1-1.8;x+=c.kind==='shops'?3.9:2.7){
        box(x-.12,y-.12,c.z0-.34,x+1.75,y+1.7,c.z0-.12,'stone');
        box(x,y,c.z0-.43,x+1.50,y+1.48,c.z0-.33,
            (f===0&&c.kind==='shops')?'glass':'windowdark');
      }
    }
    if(c.kind==='restaurant'){
      box(c.x0+2,c.h,c.z0+2,c.x1-2,c.h+1.5,c.z1-2,'stone');
      for(let x=c.x0+3;x<c.x1-2;x+=4.5)
        box(x,c.h+.3,c.z0-.46,x+2.5,c.h+1.15,c.z0-.34,'glass');
    }
  }
  // Characteristic Soviet-era square: long paving seams, raised beds and
  // low steps; no invented monuments or inaccessible interiors.
  for(let x=-50;x<55;x+=6.4)
    box(x,.025,54,x+.06,.038,62,'concrete');
  for(const [x,z] of [[-43,57],[-26,58],[23,58],[42,57]]){
    box(x-2,.025,z-1,x+2,.45,z+1,'concrete');
    box(x-1.65,.45,z-.7,x+1.65,.56,z+.7,'stone');
    box(x-1.3,.56,z-.5,x+1.3,.68,z+.5,'grass');
  }

  // Repeated small windows add depth to the visible west side of Energetik.
  for (let z = -43; z <= -22; z += 4.2) {
    for (let y = 2.0; y < 8.4; y += 2.9) {
      box(-50.58, y, z, -50.28, y + 1.5, z + 1.7, 'windowdark');
      box(-50.66, y - 0.12, z - 0.12, -50.22, y + 1.62, z + 1.82, 'stone');
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
  apartment(-64, -34, -57, -10, 6, -10.0);
  // Former generic slab removed: it intersected the relocated Polissya hotel.
  apartment(-64, 20, -57, 52, 5, 20.0);
  // Eastern generic slab removed to preserve the hotel forecourt and view corridor.

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
    const y=2.94+level*3.1;
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
