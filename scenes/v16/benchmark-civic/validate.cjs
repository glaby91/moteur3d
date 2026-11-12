#!/usr/bin/env node
// V17.317: structural and spatial validation, not just instance accounting.
'use strict';
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const base=__dirname;
const root=path.resolve(base,'../../..');
const origin=JSON.parse(fs.readFileSync(path.resolve(root,'assets/materials/pripyat-v17/catalogue.json'),'utf8'));
function check(ok,message){if(!ok)throw new Error(message);}
const sourceIds=origin.materials.map(m=>m.id);
check(sourceIds.length===22&&new Set(sourceIds).size===22,'Original Pripyat material set changed');
const required=['geometry/benchmark.json','materials/pripyat-reuse.json','cameras/views.json',
 'lighting/day.json','scene.json','scene-lourd.json','scene-extreme.json',
 'textures/energetik_fresco_mosaic.jpg',
 'textures/personnage_facade_mosaique.jpg'];
for(const file of required)check(fs.existsSync(path.resolve(base,file)),'Missing '+file);
const sceneMosaic=fs.readFileSync(path.resolve(base,'textures/energetik_fresco_mosaic.jpg'));
const originalMosaic=fs.readFileSync(path.resolve(root,
 'experiments/2026-10-30-cote-ete-gt/native_v14/assets/pripyat/textures/energetik_fresco_mosaic.jpg'));
check(sceneMosaic.equals(originalMosaic),'The copied Energetik mural is not byte-exact');
const portrait=fs.readFileSync(path.resolve(base,'textures/personnage_facade_mosaique.jpg'));
const portraitDigest=crypto.createHash('sha256').update(portrait).digest('hex');
check(portraitDigest==='9336fd983194cfc4d1f13475049a5405fc182e4f20a6220c6f447fc454c1c9eb',
 'User-supplied exterior artwork was altered or replaced');
check(portrait[0]===0xff&&portrait[1]===0xd8&&portrait.at(-2)===0xff&&
 portrait.at(-1)===0xd9,'Exterior mosaic is not a complete JPEG');
function read(file){return JSON.parse(fs.readFileSync(path.resolve(base,file),'utf8'));}
const materials=read('materials/pripyat-reuse.json').materials;
const allIds=materials.map(m=>m.id);
check(materials.length===30&&new Set(allIds).size===materials.length,'Expected 22 source + 8 scene materials');
check(origin.materials.every((m,i)=>JSON.stringify(m)===JSON.stringify(materials[i])),
 'Original material recipes or ordering changed');
const validMaterials=new Set(allIds);
const meshes=new Set(read('geometry/benchmark.json').meshes.map(m=>m.id));
const cameras=read('cameras/views.json').cameras;
check(cameras.length>=7&&cameras.some(c=>c.id==='bassin')&&
 cameras.some(c=>c.id==='etage')&&cameras.some(c=>c.id==='vestiaires'),
 'Pool, gallery and changing rooms cameras missing');
for(const [profile,target,cap,sceneFile] of [
 ['leger',20000,512,'scene.json'],['lourd',150000,1024,'scene-lourd.json'],
 ['extreme',500000,1024,'scene-extreme.json']
]){
 const scene=read(sceneFile),sector=scene.sectors?.[0];
 check(scene.format==='m3d-scene-v16'&&sector,'Bad scene '+profile);
 for(const kind of ['geometry','materials','pbr','instances','cameras','lighting','environment']){
  for(const item of sector[kind]||[]){
   const full=path.resolve(base,item);
   check(full.startsWith(base+path.sep)&&fs.existsSync(full),'Escaping/missing '+item);
  }
 }
 const inst=read('instances/'+profile+'.json').instances;
 const pbr=read('pbr/pripyat-reuse-'+profile+'.json');
 const stats=read('stats/'+profile+'.json');
 const seen=new Set();let boxCount=0;
 for(const o of inst){
  check(o.id&&!seen.has(o.id),'Duplicate object id '+o.id);
  seen.add(o.id);
  check(meshes.has(o.geometry)&&validMaterials.has(o.material),'Unresolved material/geometry '+o.id);
  check(o.size?.length===3&&o.position?.length===3,'Invalid geometry '+o.id);
  check([...o.size,...o.position].every(Number.isFinite)&&o.size.every(v=>v>0),
   'Degenerate geometry '+o.id);
  if(o.geometry==='unit_box')boxCount++;
  check(!/^(Architecture\/Aile|Architecture\/Passerelle|Horizon\/|Mobilier\/Bancs|Mobilier\/Abribus|Place\/Bassin|Interieur\/Auditorium)/.test(o.parent),
   'Unwanted additional building or external furniture '+o.parent);
 }
 check(boxCount*12>=target&&inst.length<=65000,'Stress profile out of range');
 check(stats.totalInstances===inst.length&&stats.boxes===boxCount,'Incorrect geometry stats');
 check(pbr.sets.length===30&&pbr.sets.every(x=>x.resolution<=cap),
  'Unexpected PBR cap or missing local textures');
 const frescoSet=pbr.sets.find(m=>m.id==='tex_cse_energetik_fresco');
 check(frescoSet&&frescoSet.imageFile==='textures/energetik_fresco_mosaic.jpg',
  'The genuine Energetik fresco is not used by PBR');
 const portraitSet=pbr.sets.find(m=>m.id==='tex_cse_personnage_facade');
 check(portraitSet&&portraitSet.imageFile==='textures/personnage_facade_mosaique.jpg',
  'User supplied character artwork must be loaded as the facade texture');
 const select=prefix=>inst.filter(o=>o.parent.startsWith(prefix));
 const unique=(prefix)=>{const a=select(prefix);check(a.length>=1,'Missing '+prefix);return a;};
 const pool=unique('Interieur/Piscine olympique/Fond');
 check(pool.some(p=>p.size[0]===50&&p.size[2]===25),
  'Olympic 50 x 25 m basin not dimensionally correct');
 check(unique('Interieur/Piscine olympique/Eau').some(o=>o.position[2]===-32),
  'Water should remain in the cut-out');
 const poolRegion=[-25,25,-44.5,-19.5];
 // No solid central gallery floors: preserve tall clear water volume.
 for(const x of select('Interieur/Galeries')){
  const [px,,pz]=x.position,[w,,d]=x.size;
  const intersects=px+w/2>poolRegion[0]&&px-w/2<poolRegion[1]&&
   pz+d/2>poolRegion[2]&&pz-d/2<poolRegion[3];
  check(!intersects,'Upper floor collides with pool air volume: '+x.id);
 }
 check(select('Interieur/Escalier galerie').length>=27,'Gallery stairs missing');
 check(select('Interieur/Piscine olympique/Lumière linéaire').length>=4,'Linear lamps missing');
 check(select('Interieur/Piscine olympique/Lignes d’eau').length>120,'Lane dividers missing');
 const fresco=unique('Interieur/Hall/Fresque Energetik authentique en image');
 check(fresco.length===1&&fresco[0].material==='cse_energetik_fresco'&&
  fresco[0].size[0]===9.5&&fresco[0].size[1]===4.3&&
  fresco[0].position[0]===-15&&fresco[0].position[2]>-19.5,
  'Energetik image must remain over the HALL staircase, not the pool');
 check(!inst.some(o=>o.material==='cse_energetik_fresco'&&
   !o.parent.startsWith('Interieur/Hall/')),
  'Historic Energetik mosaic used outside the main hall');
 const lobbyStairs=select('Interieur/Hall/Grand escalier').filter(o=>
   o.parent==='Interieur/Hall/Grand escalier');
 check(lobbyStairs.length===25,'Grand foyer staircase needs 25 accessible treads');
 for(let i=1;i<lobbyStairs.length;i++){
  const previous=lobbyStairs[i-1],next=lobbyStairs[i];
  check(next.position[2]-previous.position[2]<.35&&
    next.position[1]-previous.position[1]<.34,
    'Grand staircase has an unwalkable gap or rise');
 }
 const lastStair=lobbyStairs.at(-1);
 check(Math.abs(lastStair.position[1]+lastStair.size[1]/2-4.5)<.03&&
   lastStair.position[2]>-13.7,
  'Grand staircase must meet the y=4.5 entrance gallery');
 check(unique('Interieur/Hall/Mur fresque').length===1,
  'Hall fresco support wall missing');
 // First-floor access MUST have a cut-out so the player does not hit
 // an overhead slab while climbing the last steps (1.70 m eye height).
 for(const g of select('Interieur/Galeries').filter(o=>o.position[1]===4.5)){
  const [x,,z]=g.position,[w,,d]=g.size;
  const intersectsStairShaft=(x+w/2>-17.6&&x-w/2<-12.4&&
    z+d/2>-13.5&&z-d/2<-9.2);
  check(!intersectsStairShaft,'Headroom obstructed above hall staircase');
 }
 const west=select('Interieur/Vestiaires et douches ouest');
 const east=select('Interieur/Vestiaires est et locaux techniques');
 check(west.length>=35&&east.length>=8,
  'Indoor locker rooms, shower fixtures or service rooms missing');
 for(const o of [...west,...east]){
  const [x,,z]=o.position;
  check(Math.abs(x)>30.5,'Locker facilities intersect pool promenade');
  check(z>=-55&&z<=-26,'Locker facilities escaped the main building footprint');
 }
 const corridorDoor=select('Interieur/Vestiaires et douches ouest/Mur accès');
 check(corridorDoor.length===2&&
  Math.abs(corridorDoor[0].position[2]-corridorDoor[1].position[2])===11,
  'Poolside locker-room entrance is not 4 m wide');
 const exteriorArt=inst.filter(o=>o.material==='cse_personnage_facade');
 check(exteriorArt.length===1&&
   exteriorArt[0].parent==='Architecture/Palais/Mosaique extérieure du personnage/Image fournie par utilisateur'&&
   exteriorArt[0].position[0]===0&&
   exteriorArt[0].position[2]>-4.0&&
   exteriorArt[0].size[0]===8&&
   exteriorArt[0].size[1]===13.05,
  'External character mosaic is not on the central facade');
 check(exteriorArt[0].position[1]-exteriorArt[0].size[1]/2>4.5,
  'Outdoor mural overlaps the walkable entrance');
 const otherFresco=inst.filter(o=>o.material==='cse_energetik_fresco');
 check(otherFresco.length===1&&otherFresco[0].position[0]===-15,
  'Energetik artwork moved outside the hall');
 check(select('Architecture/Palais/Mosaique extérieure du personnage/Joints').length>=95,
  'Exterior mosaic has no physical tile grout');
 check(select('Architecture/Palais/Mosaique extérieure du personnage/Encadrement').length===4,
  'External mural is not framed');
 check(!inst.some(o=>o.parent.startsWith('Interieur/Piscine olympique/')&&
    ['cse_personnage_facade','cse_energetik_fresco'].includes(o.material)),
  'An artwork improperly covers the pool wall');
 const seat=select('Place/Banc ovale béton/Assise');
 check(seat.length===96,'Concrete oval must have 96 uninterrupted sections');
 for(const x of seat){
  const dx=(x.position[0]-37)/16.45,dz=(x.position[2]-52)/10.5;
  check(Math.abs(dx*dx+dz*dz-1)<.01,'Seat is not an oval: '+x.id);
 }
 // Visual-structure regressions: do not bring back the rooftop spikes or
 // turn street trees into simple poles with a single spherical canopy.
 check(select('Architecture/Palais/Acrotere').length===1,
  'Continuous roof parapet missing');
 check(select('Architecture/Palais/Enseigne fictive').length===0,
  'The obsolete 80-post rooftop sign has returned');
 check(select('Nature/Arbres/Branches').length===2*select('Nature/Arbres').filter(o=>o.material==='prp_bark_wet').length,
  'Tree boughs missing');
 check(select('Nature/Arbres/Houppier').length>=3*select('Nature/Arbres').filter(o=>o.material==='prp_bark_wet').length,
  'Asymmetric multi-lobe tree crowns missing');
 check(select('Place/Butte végétalisée/Fleurs et feuillages').length>200,
  'Multicolour vegetated mound incomplete');
 check(select('Place/Monument soviétique fictif').length>=25,
  'Original monumental civic statue incomplete');
 const entranceGlass=select('Architecture/Palais/Verriere').filter(o=>
    o.position[1]<5&&Math.abs(o.position[0])<5);
 check(entranceGlass.length===0,'Glass blocks the 10-m main doorway');
 const steps=select('Place/Escalier monumental');
 const landing=unique('Place/Parvis d’entrée')[0];
 check(steps.length===4&&
  steps.some(o=>o.position[2]-o.size[2]/2<=landing.position[2]+landing.size[2]/2+.03),
  'Monumental steps do not physically meet the entrance terrace');
 process.stdout.write(profile+' PASS: single palace, pool 50x25, two distinct artworks (exterior portrait/hall Energetik), oval bench, '+ 
    inst.length+' objects, '+boxCount*12+' minimum triangles\n');
}
