// Pripyat city-centre scene — native Moteur 3D V14 scene source.
// Geometry is authored in metres and exported through the existing V4 pipeline.
// Documentary-photo references and map sources are listed in docs/pripyat/README.md.

const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  concrete: 2, plaster: 1, mural: 1, brick: 2, glass: 4, windowdark: 1, windowlit: 3,
  asphalt: 3, stone: 5, metal: 18, rust: 7, leaves: 2, grass: 1,
  bark: 1, wood: 4, sign: 3, debris: 2, puddle: 28
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
  box(20.5,9.4,-40.8,30.5,14.0,-22.2,'concrete'); // stair tower
  box(-43.0,11.25,-42.5,18.4,11.75,-20.1,'stone');

  // Projecting plinth, pilasters, capitals and layered cornice give the civic
  // façade real relief. The large bays stay open: the reference facade has no glazing.
  box(-48.0,0.0,-20.55,26.9,0.72,-19.25,'stone');
  box(-47.0,8.7,-20.48,25.6,9.02,-19.42,'concrete');
  box(-47.6,9.02,-20.74,26.2,9.28,-19.26,'stone');
  box(-47.9,9.28,-20.94,26.5,9.48,-19.08,'concrete');
  for (const x of [-45.8,-39.0,-32.2,-25.4,-18.6,-11.8,-5.0,1.8,8.6,15.4,22.2]) {
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
  // Recessed, weathered rooftop plaque and built-up Cyrillic lettering.
  box(-7.0,10.0,-20.9,7.0,11.25,-20.18,'rust');
  box(-6.78,10.10,-21.0,6.78,11.12,-20.96,'windowdark');
  const glyphs={
    'Э':['11110','10001','00001','01111','00001','10001','11110'],
    'Н':['10001','10001','10001','11111','10001','10001','10001'],
    'Е':['11111','10000','10000','11110','10000','10000','11111'],
    'Р':['11110','10001','10001','11110','10000','10000','10000'],
    'Г':['11111','10000','10000','10000','10000','10000','10000'],
    'Т':['11111','00100','00100','00100','00100','00100','00100'],
    'И':['10001','10011','10101','10101','11001','10001','10001'],
    'К':['10001','10010','10100','11000','10100','10010','10001']
  };
  const title='ЭНЕРГЕТИК', cell=0.13, gap=0.075;
  const titleWidth=title.length*5*cell+(title.length-1)*gap;
  let titleX=-titleWidth*0.5;
  for(const ch of title){
    const rows=glyphs[ch];
    for(let row=0;row<7;row++)for(let col=0;col<5;col++)if(rows[row][col]==='1'){
      const x=titleX+col*cell, y=10.20+(6-row)*cell;
      box(x,y,-21.04,x+cell*0.82,y+cell*0.82,-20.98,'brick');
    }
    titleX+=5*cell+gap;
  }

  // Three readable levels. Slabs wrap the foyer atrium and the open stair core.
  box(-49.2,0.05,-42.2,29.2,0.38,-20.1,'stone');
  function floorPlate(y){
    const t=y,b=y+0.24;
    box(-49.2,t,-42.2,-15,b,-20.1,'concrete');
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
  floorPlate(3.25); floorPlate(6.35);

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
    box(x,1.0,-42.10,x+3.5,2.65,-41.98,((Math.floor((x+45.5)/6.2)%4)===1)?'windowdark':'glass');
    box(x,4.1,-42.10,x+3.5,5.85,-41.98,'glass');
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
    for(const [x0,x1] of [[-48.7,-38.0],[-38.0,-27.0],[-27.0,-15.8],[8.2,17.6]]){
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
  box(-47.2,0.42,-41.4,-18.0,0.68,-35.0,'wood'); // auditorium stage
  for(let row=0;row<5;row++) for(let seat=0;seat<9;seat++){
    const x=-45.0+seat*3.0, z=-40.8+row*1.0;
    roundBox(x,0.46,z,x+0.75,0.95,z+0.62,0.16,'rust');
    box(x+0.08,0.48,z+0.38,x+0.67,1.25,z+0.58,'wood');
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
  // Large foyer fresco on the inside return beside the ceremonial stair. It
  // combines surviving Soviet-era colour fields, a central figure and flaked
  // edges, based on the supplied Energetik interior photographs.
  const frescoX=8.32;
  box(frescoX,3.72,-34.35,frescoX+0.08,6.32,-25.35,'plaster');
  box(frescoX+0.08,3.82,-34.22,frescoX+0.14,6.22,-25.48,'mural');
  // Surviving painted geometry sits just proud of the wall, like a worn mural
  // rather than a clean poster. Broad forms remain recognizable at a glance.
  box(frescoX+0.15,4.02,-33.98,frescoX+0.19,4.42,-25.76,'sportsPaint');
  box(frescoX+0.15,5.55,-33.90,frescoX+0.19,5.82,-25.80,'sportsViolet');
  ellipsoidMesh(frescoX+0.20,5.13,-26.45,0.055,0.57,0.57,'sportsRed',14,10);
  // Figure: ochre head, blue coat, one raised arm and a pale sleeve.
  ellipsoidMesh(frescoX+0.23,5.12,-30.25,0.065,0.28,0.27,'taxi',12,8);
  box(frescoX+0.16,4.36,-30.76,frescoX+0.23,4.91,-29.75,'sportsViolet');
  capsule([frescoX+0.21,4.78,-30.02],[frescoX+0.21,5.22,-29.52],0.105,'sportsPaint');
  capsule([frescoX+0.21,4.77,-30.47],[frescoX+0.21,5.48,-30.93],0.10,'sportsPaint');
  capsule([frescoX+0.21,5.48,-30.93],[frescoX+0.21,5.78,-31.21],0.075,'sportsRed');
  // A stylized wheat / energy motif and broken mosaic fragments.
  for(let i=0;i<6;i++){
    const z=-33.3+i*0.52, y=4.25+i*0.20;
    capsule([frescoX+0.22,y,z],[frescoX+0.22,y+0.54,z-0.38],0.045,i%2?'taxi':'sportsPaint');
    ellipsoidMesh(frescoX+0.24,y+0.37,z-0.25,0.045,0.13,0.20,i%2?'sportsPaint':'taxi',8,6);
  }
  for(let i=0;i<24;i++){
    const z=-34.05+(i*37%83)/10, y=3.90+(i*19%21)/10;
    if(y>4.35&&y<5.85&&z>-31.0&&z<-29.3) continue;
    const mat=['sportsRed','sportsPaint','sportsViolet','taxi','sea'][i%5];
    box(frescoX+0.17,y,z,frescoX+0.22,y+0.08+(i%3)*0.06,z+0.12+(i%4)*0.05,mat);
  }
  // Peeling plaster exposes pale substrate around the painted field.
  for(const [y,z,w,h] of [[5.94,-34.08,0.54,0.34],[5.82,-25.83,0.36,0.46],
                           [3.91,-33.94,0.62,0.21],[4.16,-25.93,0.44,0.32]])
    box(frescoX+0.23,y,z,frescoX+0.26,y+h,z+w,'stone');
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
    box(x,y,-42.24,x+3.5,y+1.65,-42.08,'windowdark');
    box(x+0.08,y+0.06,-42.30,x+0.18,y+1.56,-42.16,'rust');
    box(x+3.05,y+0.06,-42.28,x+3.16,y+1.56,-42.14,'rust');
  }

  // Upper rooms retain a few recognizable large-scale fixtures and rubble.
  for(let level=1;level<3;level++){
    const y=0.42+level*3.1;
    for(const x of [-43,-32,-21,12]){
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

  // The Polissya hotel stays a quiet contextual mass; Energetik owns the detail budget.
  box(40.0,0.0,-42.0,63.0,15.5,-20.0,'concrete');
  box(43.5,15.5,-39.0,59.5,20.0,-22.5,'concrete');
  for(let floor=0;floor<5;floor++){
    const y=1.8+floor*2.65;
    for(let x=42.0;x<61.0;x+=3.1)
      box(x,y,-19.92,x+1.45,y+1.55,-19.78,(floor===2&&x>51)?'windowlit':'windowdark');
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
  apartment(46, -37, 60, -12, 7, -12.0);
  apartment(-64, 20, -57, 52, 5, 20.0);
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
