#!/usr/bin/env node
// CSE WARP V17 - deterministic heavy scene generator.
// No network calls and no external dependencies. Original Pripyat files stay
// untouched; only stable material IDs are reused by V17 procedural PBR recipes.
'use strict';
const fs=require('node:fs');
const path=require('node:path');
const {rebuildMonument,rebuildGarden,rebuildTrees}=require('./rebuild-objects.cjs');
const {generate:generateEntryMosaic}=require('./build-entry-mosaic.cjs');
const root=__dirname;
const repoRoot=path.resolve(root,'../../..');
const catalogPath=path.join(repoRoot,'assets/materials/pripyat-v17/catalogue.json');
const catalog=JSON.parse(fs.readFileSync(catalogPath,'utf8'));
const radioLayout=JSON.parse(fs.readFileSync(path.join(repoRoot,'assets/audio/pripyat_radio/speaker_layout.json'),'utf8'));
if(radioLayout.loudspeakers.length!==4)throw new Error('Radio facade requires exactly four loudspeakers');
// Scene-only PBR additions; original Pripyat IDs, materials and files remain
// completely unchanged. The fresco is the actual legacy JPEG, not a fake mural.
const specials=[
 ['cse_radio_orange',[.75,.23,.10],.62,'metal'],
 ['cse_radio_horn_dark',[.19,.20,.19],.66,'metal'],
 ['cse_radio_mount',[.44,.43,.40],.76,'metal'],
 ['cse_flower_red',[.54,.20,.19],.88,'foliage'],
 ['cse_flower_gold',[.74,.54,.25],.89,'foliage'],
 ['cse_flower_blue',[.33,.43,.54],.89,'foliage'],
 ['cse_flower_purple',[.44,.33,.49],.89,'foliage'],
 ['cse_monument_bronze',[.36,.29,.19],.39,'metal'],
 ['cse_pool_strip',[.79,.86,.91],.27,'metal'],
 ['cse_energetik_fresco',[1,1,1],.72,'image'],
 ['cse_personnage_facade',[1,1,1],.78,'image'],
 ['cse_neon_red',[1,.10,.035],.18,'metal'],
 ['cse_neon_cyan',[.02,.86,1],.17,'metal'],
 ['cse_neon_gold',[1,.69,.21],.20,'metal'],
 ['cse_banner_red',[.52,.115,.10],.91,'fabric'],
 ['cse_banner_cream',[.77,.71,.59],.87,'fabric'],
 ['cse_birch_bark',[.80,.79,.73],.89,'wood'],
 ['cse_leaf_olive',[.28,.43,.19],.94,'foliage'],
 ['cse_leaf_gold',[.51,.48,.20],.94,'foliage'],
 ['cse_pool_water',[.10,.53,.69],.10,'water'],
 ['cse_ivory_plaster',[.88,.84,.77],.91,'plaster'],
 ['cse_concrete_waxed',[.57,.56,.53],.50,'concrete'],
 ['cse_gallery_limestone',[.77,.72,.65],.61,'concrete'],
 ['cse_gallery_warm',[.68,.66,.61],.67,'concrete'],
 ['cse_anti_slip',[.43,.43,.40],.89,'ceramic'],
 ['cse_coping_beige',[.74,.70,.63],.78,'stone'],
 ['cse_entry_mosaic',[1,1,1],.78,'image'],
 ['cse_terra_tile',[.71,.41,.32],.82,'ceramic'],
 ['cse_ochre_tile',[.79,.65,.39],.84,'ceramic'],
 ['cse_olive_tile',[.42,.46,.39],.82,'ceramic'],
 ['cse_glaze_orange',[.98,.24,.028],.16,'ceramic'],
 ['cse_glaze_yellow',[.99,.82,.025],.16,'ceramic'],
 ['cse_glaze_cobalt',[.015,.19,.91],.14,'ceramic'],
 ['cse_glaze_lime',[.35,.78,.03],.17,'ceramic'],
 ['cse_glaze_red',[.93,.05,.11],.17,'ceramic']
];
const localMaterials=specials.map(([id,baseColor,roughness])=>({
 id,baseColor,roughness,metallic:id==='cse_monument_bronze'?.72:0,
 transmission:id==='cse_pool_water'?0.88:0,ior:id==='cse_pool_water'?1.333:1.5,textureSet:'tex_'+id,
 origin:'CSE WARP single-palace scene material; original Pripyat library unchanged'
}));
const localSets=specials.map(([id,, ,pattern])=>({
 id:'tex_'+id,material:id,pattern,
 resolution:id==='cse_energetik_fresco'?2048:512,
 ...(id==='cse_energetik_fresco'?{imageFile:'textures/energetik_fresco_mosaic.jpg'}:{}),
 ...(id==='cse_personnage_facade'?{imageFile:'textures/personnage_facade_mosaique.jpg'}:{}),
 ...(id==='cse_entry_mosaic'?{imageFile:'textures/cse_parvis_mosaique.png'}:{})
}));
const profiles={
  leger: {target:20000, textureCap:512, pave:4.5, trees:26, benches:12, lamps:12, blocks:3, seatCols:16, seatRows:9, detail:3},
  lourd: {target:150000, textureCap:1024, pave:2.2, trees:72, benches:35, lamps:26, blocks:10, seatCols:40, seatRows:25, detail:8},
  extreme:{target:500000, textureCap:1024, pave:1.1, trees:140, benches:85, lamps:48, blocks:18, seatCols:64, seatRows:40, detail:20}
};
const geometry={format:'m3d-geometry-v1',units:'m',meshes:[
 {id:'unit_box',type:'box',size:[1,1,1],uv:'planar_per_face'},
 {id:'unit_sphere',type:'uv_sphere',segments:12,rings:8,uv:'spherical'},
 {id:'foliage_canopy',type:'foliage_canopy'}
]};
const cameras={format:'m3d-cameras-v1',cameras:[
 {id:'facade',name:'01 · Façade et mosaïque du personnage',position:[0,4.2,62],target:[0,10.7,-4],fovDegrees:66},
 {id:'mosaique',name:'02 · Mosaïque extérieure en détail',position:[12,8.0,19],target:[0,12.25,-3.8],fovDegrees:52},
 {id:'escalier',name:'02 · Escalier continu vers le palais',position:[-17,2.2,29],target:[0,2.6,-4],fovDegrees:66},
 {id:'passerelle',name:'03 · Grand banc ovale et statue',position:[60,3.2,71],target:[37,4.8,52],fovDegrees:68},
 {id:'bassin',name:'04 · Piscine olympique depuis l’entrée',position:[0,2.4,-10],target:[0,1.0,-34],fovDegrees:73},
 {id:'hall',name:'05 · Hall, escalier et fresque Energetik',position:[-15,10.35,-11.6],target:[-15,10.35,-18.0],fovDegrees:67},
 {id:'hall_ensemble',name:'06 · Hall et escalier - vue d ensemble',position:[-4.5,2.55,-8.0],target:[-15,6.4,-15.4],fovDegrees:61},
 {id:'piscine_soir',name:'07 · Piscine vue latérale a 19h',position:[32.5,6.05,-19.0],target:[0,1.1,-33],fovDegrees:67},
 {id:'etage',name:'06 · Galerie au-dessus des lignes d’eau',position:[34,6.6,-44],target:[0,2,-32],fovDegrees:71},
 {id:'vestiaires',name:'07 · Vestiaires et douches RDC',position:[-33.5,2.15,-35.4],target:[-43.5,2,-36],fovDegrees:68},
 {id:'vue_haut',name:'07 · Vue aérienne du bâtiment unique',position:[95,68,105],target:[15,5,11],fovDegrees:67}
]};
// V17.324: ~10x more independent light sources, not 10x the power of each bulb.
// Designed for night/evening exploration at 19h. Keep under the D3D11
// FrameConstants lightPositionRadius[256] and lightColorIntensity[256] limit.
// V17.380: functional sources ≠ decorative visible fixtures.
 // Reduced from 171 point lights to 60, distributed in architectural rows.
 // Source count is an initial design target; photometry/visual checks follow.
const indoorFixtures=[
  // Swimming hall: two longitudinal rows of 6 wide-coverage lights.
  ...[-38,-26].flatMap(z=>[-23,-14,-5,5,14,23].map(x=>({
    position:[x,16.0,z],radius:15.5,intensity:1.85,color:[248,250,255]
  }))),
  // Main hall and monumental stairwell: 8 sources on paired beams.
  ...[-10.5,-15.5].flatMap(z=>[-24,-8,8,24].map(x=>({
    position:[x,7.1,z],radius:13,intensity:1.65,color:[255,244,223]
  }))),
  // Galleries: 24 lights, four on each side of three levels.
  ...[4.5,9,13.5].flatMap(y=>[-38,38].flatMap(x=>[-45,-34,-23,-12].map(z=>({
    position:[x,y-.32,z],radius:11.2,intensity:1.55,color:[247,248,251]
  })))),
  // Service and changing rooms: 6 sources.
  ...[-42,42].flatMap(x=>[-43,-32,-21].map(z=>({
    position:[x,3.4,z],radius:10,intensity:1.45,color:[250,248,241]
  })))
];
const lighting={format:'m3d-lighting-v1',hour:14.75,
  sun:{direction:[-.68,.65,-.35],intensity:3.1},
  ambient:{color:[.63,.73,.91],intensity:.39},
  lights:[
    // Mounted interior mural and the stair access.
    {position:[0,3.95,-49.3],radius:10,intensity:1.65,color:[255,244,224]},
    {position:[-15,3.9,-12],radius:9,intensity:1.5,color:[255,248,236]},
    ...indoorFixtures,
    // Architectural facade: few real lights, continuous visual strips.
    ...[-39,-26,-13,0,13,26,39,47].map(x=>({
      position:[x,4.0,-2.7],radius:13,intensity:1.3,
      color:[255,225,184]
    }))
  ]
};
if(lighting.lights.length!==60)
  throw new Error('Expected 60 carefully positioned architectural light sources');
const environment={format:'m3d-environment-v1',terrain:'finite',units:'m',
 note:'Fictional, explorable single modernist palace with olympic pool, a copied real Energetik mosaic artwork asset, and original Soviet-inspired monument; NOT historical reconstruction.'};
function round(n){return Math.round(n*1000)/1000;}
function ensure(p){fs.mkdirSync(path.join(root,p),{recursive:true});}
function save(p,obj){const full=path.join(root,p);ensure(path.dirname(p));fs.writeFileSync(full,JSON.stringify(obj,null,2)+'\n','utf8');}
function build(profileName,p){
 let seed=0x17ca2026;
 function rand(){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return (seed>>>0)/4294967296;}
 const objects=[];
 let boxes=0, spheres=0, canopies=0;
 function add(parent,material,x,y,z,w,h,d,kind='unit_box',rotationY=0){
   if(!(w>0&&h>0&&d>0))throw new Error('Nonpositive volume: '+parent);
   if(objects.length>=65000)throw new Error('Instance safety cap reached');
   const instance={id:'v17_'+String(objects.length+1).padStart(6,'0'),
     parent,geometry:kind,material,
     position:[round(x),round(y),round(z)],size:[round(w),round(h),round(d)],rotationY:round(rotationY)};
   objects.push(instance);
   if(kind==='unit_box')boxes++;
   else if(kind==='unit_sphere')spheres++;
   else canopies++;
 }
 const B=(group,mat,x,y,z,w,h,d,rot=0)=>add(group,mat,x,y,z,w,h,d,'unit_box',rot);
 const S=(group,mat,x,y,z,w,h,d)=>add(group,mat,x,y,z,w,h,d,'unit_sphere');
 const F=(group,mat,x,y,z,w,h,d)=>add(group,mat,x,y,z,w,h,d,'foliage_canopy');
 // Outdoor site; the soil and plaza have a real hole under the indoor pool.
 // The four soil bands prevent the terrain from covering the swimming basin.
 const earth=[
  [-78,35,104,305],[78,35,104,305],
  [0,84.25,52,206.5],[0,-81.25,52,72.5]
 ];
 for(const [x,z,w,d] of earth)B('Terrain/Sol','prp_soil_leaf_litter',x,-.32,z,w,.55,d);
 B('Terrain/Place','prp_concrete_raw',0,-.04,36.25,144,.17,110.5);
 B('Terrain/Routes','prp_asphalt_old',-93,.015,22,29,.14,210);
 B('Terrain/Routes','prp_asphalt_old',0,.015,105,250,.14,23);
 B('Terrain/Trottoirs','prp_concrete_panel',-73,.09,24,7,.18,195);
 B('Terrain/Trottoirs','prp_concrete_panel',0,.1,93,210,.18,5);
 // A SINGLE public building (101 m × 50 m); all side wings, sky-blocks,
 // connecting bridge and bus shelters are intentionally absent.
 const shell='Architecture/Palais';
 const zFront=-5,zBack=-55;
 // Perimeter deck, deliberately leaving a 50 x 25 m opening in the floor.
 for(const [x,z,w,d] of [
  [-37.75,-30,25.5,50],[37.75,-30,25.5,50],
  [0,-12.25,50,14.5],[0,-49.75,50,10.5]
 ])B(shell+'/Fondations','cse_concrete_waxed',x,.27,z,w,.55,d);
 B(shell+'/Volume','prp_concrete_panel',0,9.5,-54.66,101,19,.68);
 for(const side of [-1,1])
  B(shell+'/Volume','prp_concrete_panel',side*50.15,9.5,-30,.70,19,50);
 // Long galleries leave the entire natatorium volume open to the roof.
 for(const y of [4.5,9,13.5]){
  for(const side of [-1,1]){
    if(y===4.5&&side===1){
      // Stairwell: remove the overhead slab along the upper 15 m of the
      // east-side stair, leaving a clear 5 m wide opening for a 1.70m avatar.
      // Structural deck remains on both sides and at each end.
      for(const [x,z,w,d] of [
        [31.5,-30,6.0,49],[45,-30,11.0,49],
        [37,-17.25,5.0,23.5],[37,-49.10,5.0,10.8]
      ])B('Interieur/Galeries','prp_concrete_raw',x,y,z,w,.30,d);
    }else{
      B('Interieur/Galeries','prp_concrete_raw',side*39.5,y,-30,22,.30,49);
    }
  }
  B('Interieur/Galeries','prp_concrete_raw',0,y,-50.15,57,.30,9.7);
  // Scene-only floor finishes distinguish levels without recoloring the structural underside.
  const floorMat=y===4.5?'cse_gallery_limestone':y===9?'cse_gallery_warm':'cse_concrete_waxed';
  for(const side of [-1,1])
    B('Interieur/Galeries/Sol niveau '+y,floorMat,side*40,y+.17,-29.5,19.5,.035,46);
  B('Interieur/Galeries/Sol niveau '+y,floorMat,0,y+.17,-50.15,56,.035,9.4);
  // A genuine staircase shaft must remain open through the first
  // mezzanine. Otherwise the avatar's head hits the floor at y=4.5.
  if(y===4.5){
   B('Interieur/Galeries','prp_concrete_raw',-23.325,y,-9.45,10.35,.30,8.5);
   B('Interieur/Galeries','prp_concrete_raw',8.325,y,-9.45,40.35,.30,8.5);
   B('Interieur/Galeries','prp_concrete_raw',-15,y,-6.9,6.3,.30,3.4);
  }else{
   B('Interieur/Galeries','prp_concrete_raw',0,y,-9.45,57,.30,8.5);
  }
 }
 // Protective guardrails must belong to the slabs, not float in space.
 // First floor railing: posts embedded into the top of the mezzanine y=4.65.
 // Preserve the central stair/landing access gap.
 const guard='Interieur/Galeries/Garde-corps 1er etage';
 const ground=4.65,top=5.75;
 const rail=(group,cx,cz,dx,dz)=>{
   if(dx>0){
     B(group+'/Main courante','prp_steel_painted',cx,top,cz,dx,.075,.075);
     B(group+'/Lisse','prp_steel_painted',cx,5.18,cz,dx,.055,.055);
     for(let x=cx-dx/2;x<=cx+dx/2+.01;x+=2.5)
       B(group+'/Montants','prp_steel_painted',x,5.18,cz,.085,1.14,.085);
   } else {
     B(group+'/Main courante','prp_steel_painted',cx,top,cz,.075,.075,dz);
     B(group+'/Lisse','prp_steel_painted',cx,5.18,cz,.055,.055,dz);
     for(let z=cz-dz/2;z<=cz+dz/2+.01;z+=2.5)
       B(group+'/Montants','prp_steel_painted',cx,5.18,z,.085,1.14,.085);
   }
 };
 rail(guard,-28.5,-30,0,49);
 rail(guard,28.5,-30,0,49);
 rail(guard,0,-45.30,57,0);
 rail(guard,-23.325,-13.7,10.35,0);
 rail(guard,8.325,-13.7,40.35,0);
 // Protect the new stairwell opening without blocking the upper exit.
 rail(guard,34.50,-36.35,0,14.7);
 rail(guard,39.50,-36.35,0,14.7);
 // Small beam-to-slab closures in front of the glass curtain wall.
 for(const y of [4.5,9,13.5])
   B('Architecture/Palais/Raccords structurels','prp_concrete_raw',
     0,y-.21,-4.60,96,.36,.32);
 // Perimeter roof + continuous glazed clerestory over the 50-m pool.
 for(const side of [-1,1])
  B(shell+'/Toiture','prp_roof_bitumen',side*40.2,19.55,-30,21.4,.55,51);
 for(const z of [-51,-9])
  B(shell+'/Toiture','prp_roof_bitumen',0,19.55,z,60,.55,9);
 B(shell+'/Verriere haute','prp_window_glass',0,19.55,-30,60,.12,34);
 for(let x=-28;x<=28;x+=4)
  B(shell+'/Verriere haute/Chassis','prp_window_frame',x,19.70,-30,.12,.25,34.5);
 for(const z of [-46,-30,-14])
  B(shell+'/Verriere haute/Chassis','prp_window_frame',0,19.70,z,60,.25,.12);
 // Colonnade and external glazed entry: continuous 10-m doorway into hall.
 for(let i=0;i<17;i++){
  const x=-48+i*6;
  if(Math.abs(x)<4.5)continue; // never place a concrete pillar in the doorway
  B('Architecture/Palais/Colonnade','prp_concrete_raw',x,9.5,-4.4,.72,19,.9);
  B('Architecture/Palais/Poutres','prp_concrete_raw',x,15,-8.5,.55,.72,9);
 }
 for(let level=0;level<4;level++){
  const y=2.6+level*4.15;
  B('Architecture/Palais/Poutres','prp_concrete_raw',0,level*4.15+4.35,-4.6,96,.4,.74);
  for(let k=0;k<27;k++){
   const x=-44+k*3.35;
   if(level===0&&Math.abs(x)<11.0)continue; // 20 m-wide entrance: no black uprights in pedestrian path
   if(level>0&&Math.abs(x)<5.0)continue; // central solid mosaic wall
   if(Math.abs(x)>22&&k%4===0)continue;
   B('Architecture/Palais/Verriere','prp_window_glass',x,y,-4.03,3.04,3.65,.085);
   B('Architecture/Palais/Chassis','prp_window_frame',x,y,-3.88,.16,3.92,.18);
   B('Architecture/Palais/Chassis','prp_window_frame',x,level*4.15+4.42,-3.87,3.40,.16,.18);
  }
 }
 // Four recognizable PA horns on existing entry pilasters, not the glazed panes.
 // Share the EXACT world positions from speaker_layout.json with X3DAudio.
 // Boxes form a stepped flared horn and solid rear housing; depth extends
 // toward the public square (+Z). No change to columns, doors or glazing.
 for(const sp of radioLayout.loudspeakers){
   const [x,y,z]=sp.position;
   const group='Architecture/Palais/Sonorisation exterieure/'+sp.id;
   B(group+'/Platine','cse_radio_mount',x,y,z-.37,.33,.39,.08);
   B(group+'/Bras','cse_radio_mount',x,y,z-.16,.13,.17,.47);
   B(group+'/Boitier','cse_radio_orange',x,y,z+.12,.47,.43,.45);
   B(group+'/Col','cse_radio_horn_dark',x,y-.06,z+.43,.30,.24,.16);
   B(group+'/Pavillon arriere','cse_radio_orange',x,y-.10,z+.54,.54,.33,.17);
   B(group+'/Pavillon bouche','cse_radio_orange',x,y-.16,z+.70,.79,.46,.15);
   B(group+'/Interieur sombre','cse_radio_horn_dark',x,y-.16,z+.784,.59,.29,.015);
   S(group+'/Diffuseur','cse_radio_mount',x,y-.16,z+.81,.14,.14,.06);
   for(const dx of [-.12,.12])for(const dy of [-.13,.13])
     S(group+'/Visserie','cse_radio_mount',x+dx,y+dy,z-.323,.045,.045,.04);
 }
 // User-provided costume figure, on the OUTER facade only.
 // 8 × 13 m portrait mosaic; the 10 m entrance below remains open.
 const mural='Architecture/Palais/Mosaique extérieure du personnage';
 B(mural+'/Support','prp_concrete_panel',0,12.25,-4.18,8.6,13.15,.36);
 B(mural+'/Image fournie par utilisateur','cse_personnage_facade',
   0,12.25,-3.88,8.00,13.05,.09);
 // Avoid subpixel flicker: no hundreds of 8-mm physical grout strips.
 // The authentic image is kept intact; seamless joints belong in its authored
 // texture/mip chain, not free-standing coplanar opaque rectangles.
 // Matching 1970s geometric side bands extend it between the center piers.
 for(const side of [-1,1]){
   const x=side*6.55;
   B(mural+'/Decor beton peint','prp_concrete_panel',x,12.25,-3.96,4.45,12.95,.14);
   for(let n=0;n<7;n++){
     const y=6.45+n*1.85;
     B(mural+'/Bandes geometriques',n%3===0?'cse_neon_red':
       (n%3===1?'cse_neon_gold':'cse_neon_cyan'),
       x,y,-3.84,3.9,.42,.03);
   }
 }
 for(const x of [-4.13,4.13])
  B(mural+'/Encadrement','prp_concrete_raw',x,12.25,-3.70,.22,13.45,.26);
 for(const y of [5.54,18.96])
  B(mural+'/Encadrement','prp_concrete_raw',0,y,-3.70,8.5,.20,.26);
 // Sober architectural enrichment, without altering the original mosaic.
 // Two horizontal cornices frame the glazed wings but never bridge the entry.
 for(const side of [-1,1]){
  const fx=side*31;
  for(const y of [5.15,18.85])
   B('Architecture/Palais/Facade/Corniches','prp_concrete_panel',
     fx,y,-3.72,27,.18,.26);
  for(const x of [side*20,side*30,side*40]){
   B('Architecture/Palais/Facade/Pilastres','prp_concrete_raw',
     x,10.9,-3.75,.23,10.6,.28);
   B('Architecture/Palais/Facade/Soubassement','prp_terrazzo_floor',
     x,4.85,-3.70,2.8,.22,.36);
  }
 }
 // Main steps extend fully to the building's front door, replacing the
 // old stair flight which stopped fourteen metres short of the façade.
 B('Place/Parvis d’entrée','prp_terrazzo_floor',0,.28,6.65,68,.55,20.7);
 // The external slab now ends at z=-3.70, ahead of the glass line z=-4.03.
 // Only a narrow, dedicated threshold joins the dry indoor circulation.
 B('Interieur/Hall/Seuil principal','cse_concrete_waxed',0,.57,-3.79,19.2,.07,.70);
 // Approved 1970s monumental entrance artwork: original warm-toned tessera
 // mosaic with two portraits, laurel, atom, ocre/terracotta rays. The colored
 // treatment gradually opens toward the public square. Base remains solid.
 B('Place/Parvis/Mosaique polychrome','cse_entry_mosaic',0,.590,7.3,10.2,.045,13.2);
 for(let band=0;band<16;band++){
   const z=-2.8+band*1.17;
   const half=5.5+band*.52;
   for(const side of [-1,1]){
     const x=side*(half+1.20);
     const palette=['cse_terra_tile','cse_ochre_tile','cse_olive_tile'];
     B('Place/Parvis/Rayons de mosaique',palette[(band+((side+1)/2))%3],
       x,.579,z,1.0,.023,1.16);
     B('Place/Parvis/Bordure en pierre','cse_coping_beige',
       side*(half+2.06),.572,z,.18,.024,1.16);
   }
 }
 for(let i=0;i<4;i++){
  const top=.17+i*.127;
  B('Place/Escalier monumental','prp_terrazzo_floor',0,top-.077,
   21.5-i*1.30,61,.155,1.38);
 }
 for(const side of [-1,1]){
  B('Place/Retenues','prp_concrete_raw',side*35.2,.48,9,1.1,.95,27);
  for(let n=0;n<18;n++){
   B('Place/Garde corps','prp_steel_painted',side*33.5,1.33,-2+n*1.2,.09,.95,.09);
  }
 }
 // 50 × 25 m olympic swimming pool, 10 visible lanes and tiled margins.
 const pool='Interieur/Piscine olympique';
 B(pool+'/Fond','cse_coping_beige',0,-1.48,-32,50,.22,25);
 for(const x of [-25.12,25.12])
  B(pool+'/Parois','cse_coping_beige',x,-.45,-32,.26,2.1,25.2);
 for(const z of [-44.62,-19.38])
  B(pool+'/Parois','cse_coping_beige',0,-.45,z,50.5,2.1,.26);
 // Dedicated translucent water, rough dielectric with shallow wave detail.
 // Dark, textured anti-slip band ONLY around the wet pool deck; the rest of
 // the building retains the approved warm-grey waxed concrete.
 for(const side of [-1,1]){
   B(pool+'/Plages antiderapantes','cse_anti_slip',
     side*27.40,.569,-32,4.55,.045,25.60);
   B(pool+'/Margelles antiderapantes','cse_coping_beige',
     side*25.40,.600,-32,.48,.060,25.1);
 }
 for(const z of [-46.00,-18.00]){
   B(pool+'/Plages antiderapantes','cse_anti_slip',
     0,.569,z,49.9,.045,2.5);
   B(pool+'/Margelles antiderapantes','cse_coping_beige',
     0,.600,z> -30?-19.37:-44.63,49.9,.060,.40);
 }
 // Ivory plaster is mounted as INNER lining, keeping outer brutalist
 // concrete untouched and the original rear mural unobstructed.
 for(const side of [-1,1]){
   B(pool+'/Murs interieur enduit ivoire','cse_ivory_plaster',
     side*49.72,9.6,-32,.05,17.6,42);
   B(pool+'/Murs interieur enduit ivoire','cse_ivory_plaster',
     side*27.5,9.6,-54.20,43,17.6,.05); // leave central fresco bay clear
 }
 // Dedicated translucent water, rough dielectric with shallow wave detail.
 // Water surface extends to the inner faces of all 4 basin walls
 // (x inner = ±24.99; z inner = -44.49/-19.51), leaving no walkable slot.
 B(pool+'/Eau','cse_pool_water',0,.18,-32,49.96,.025,24.94);
 // Reinforce floor lane visibility beneath the transparent surface.
 for(const z of [-43.25,-38.25,-33.25,-28.25,-23.25])
  B(pool+'/Bandes carrelage fond','prp_tile_ceramic',0,-1.34,z,48,.018,.32);
 // Underwater lane markers and floating colored lane dividers.
 for(let lane=0;lane<10;lane++){
  const z=-43.25+lane*2.5;
  B(pool+'/Marquage fond','prp_steel_painted',0,-1.346,z,47.8,.02,.075);
  if(lane<9){
   const line=z+1.25;
   for(let j=0;j<19;j++)
    S(pool+'/Lignes d’eau',j%5===0?'cse_flower_gold':'cse_flower_blue',
      -23.6+j*2.62,.245,line,.18,.07,.18);
  }
  B(pool+'/Plots','prp_tile_ceramic',-26.7,.83,z,1.2,.5,.85);
  B(pool+'/Plots','prp_tile_ceramic',26.7,.83,z,1.2,.5,.85);
 }
 // Robust deck guardrails; the player cannot simply walk into the water.
 for(const z of [-45.6,-18.4])for(let x=-24;x<=24;x+=3){
  B(pool+'/Garde-corps','prp_steel_painted',x,1.15,z,.07,1.2,.07);
  B(pool+'/Garde-corps','prp_steel_painted',x,1.70,z,3,.08,.1);
 }
 // Luminaire rails run the FULL length of the water, not sporadic lamps.
 for(const z of [-15.8,-48.2]){
  B(pool+'/Lumière linéaire','cse_pool_strip',0,16.70,z,53,.14,.32);
  B(pool+'/Lumière linéaire','prp_window_frame',0,16.88,z,53,.24,.07);
  for(let x=-26;x<=26;x+=4)
   B(pool+'/Lumière linéaire','prp_window_glass',x,16.61,z,3.85,.06,.20);
 }
 // Decorative ceiling luminaires share the 12 effective pool lights.
 // Each light is a luminous diffuser inside a discrete metal tray.
 for(const z of [-41,-32,-23]){
  for(let i=0;i<8;i++){
   const x=-21+i*6;
   B(pool+'/Plafonniers/Chassis','prp_window_frame',x,16.73,z,2.75,.16,.56);
   B(pool+'/Plafonniers/Diffuseur','cse_pool_strip',x,16.63,z,2.48,.075,.32);
  }
 }
 // A continuous pale ceramic waterline and coloured lane-end accents make
 // the natatorium legible under artificial light without faking exposure.
 for(const z of [-44.64,-19.36]){
  B(pool+'/Frise ligne eau','prp_tile_ceramic',0,.34,z,50,.22,.055);
  B(pool+'/Main courante','prp_steel_painted',0,1.27,z,50,.065,.065);
 }
 for(const x of [-25.15,25.15]){
  B(pool+'/Frise ligne eau','prp_tile_ceramic',x,.34,-32,.055,.22,25.0);
 }
 // Sparse internal architectural ribs give roof depth rather than a flat slab.
 for(const x of [-25,-15,-5,5,15,25])
  B(pool+'/Poutres plafond','prp_concrete_panel',x,17.58,-32,.38,.38,34);
 // Architectural depth: side-wall pilasters, galleries and ceramic details,
 // kept outside the 50x25m unobstructed water volume.
 for(const side of [-1,1]) {
  for(const z of [-48,-42,-36,-30,-24,-18]){
   B(pool+'/Pilastres latéraux','prp_plaster_pale',side*49.15,7.1,z,.42,13.8,.68);
   B(pool+'/Appliques galerie','cse_pool_strip',side*48.83,5.1,z,.16,1.35,.28);
  }
  for(const y of [4.75,9.25,13.75])
   B(pool+'/Corniches galerie','prp_terrazzo_floor',side*30.15,y,-32,.40,.20,37);
 }
 // Pool bench seating under lateral windows for scale and room detail.
 for(const side of [-1,1])for(const z of [-45,-35,-25]){
  B(pool+'/Bancs latéraux','prp_wood_weathered',side*28.0,.88,z,2.6,.18,.55);
  for(const offset of [-1,1])
   B(pool+'/Bancs latéraux/Pieds','prp_steel_painted',side*28.0+offset*.98,.49,z,.11,.80,.43);
 }
 // Two simple rows of swimming flags and four narrow access ladders.
 // These are actual 3D parts rather than a 2D overlay.
 for(const x of [-16,16]){
  B(pool+'/Fanions/Fil','prp_steel_painted',x,4.16,-32,.035,.035,25.0);
  for(let j=0;j<10;j++){
   const z=-43.5+j*2.50;
   B(pool+'/Fanions/Tissu',j%2===0?'cse_banner_red':'cse_banner_cream',
     x,3.96,z,.48,.38,.045);
  }
 }
 // V17.409 integration: four realistic swimming pool ladders.
 // Smooth continuous handrails approximated with supported sphere primitives.
 function ladderBezier(a,b,c,d,t){
   const q=1-t;
   return q*q*q*a+3*q*q*t*b+3*q*t*t*c+t*t*t*d;
 }
 function ladderProfile(t){
   if(t<.54){
     const u=t/.54;
     return [ladderBezier(0,0,.03,.20,u),ladderBezier(.48,.98,1.35,1.40,u)];
   }
   const u=(t-.54)/.46;
   return [ladderBezier(.20,.59,1.15,1.15,u),ladderBezier(1.40,1.43,1.28,.62,u)];
 }
 for(const side of [-1,1])for(const z of [-41.5,-22.5]){
   const xIn=side*24.60,xDeck=side*25.75;
   for(const dz of [-.36,.36]){
     B(pool+'/Echelles/Montants immerges','prp_steel_painted',
       xIn,-.41,z+dz,.115,1.97,.115);
     for(let k=0;k<67;k++){
       const [u,y]=ladderProfile(k/66);
       S(pool+'/Echelles/Mains courantes continues',
         'prp_steel_painted',side*(24.60+1.15*u),y,z+dz,.135,.135,.135);
     }
     B(pool+'/Echelles/Platines fixation plage','prp_steel_painted',
       xDeck,.58,z+dz,.26,.10,.23);
   }
   for(const y of [-1.10,-.77,-.44,-.11,.22])
     B(pool+'/Echelles/Marches immergees','prp_steel_painted',
       xIn,y,z,.17,.095,.72);
 }
 // Open lobby at ground level with a clear line of sight into the pool.
 B('Interieur/Hall/Accueil','prp_wood_weathered',-19,1.05,-10,6,1.1,1.2);
 for(const side of [-1,1])
  B('Interieur/Hall/Panneaux','cse_ivory_plaster',side*11,2.8,-11,.25,4.1,5);
 // Hall light bars are visible architectural fixtures, not invisible bulbs.
 // 20 elegant visible hall fixtures (previously 42), operated by the
 // same 8 functional hall lights; avoids an overpacked fluorescent ceiling.
 for(const y of [7.55,11.95]){
  for(const z of [-10,-16]){
   for(const x of [-24,-12,0,12,24]){
    B('Interieur/Hall/Plafonniers/Chassis','prp_window_frame',x,y+.18,z,3.0,.12,.46);
    B('Interieur/Hall/Plafonniers/Diffuseur','cse_pool_strip',x,y+.06,z,2.75,.075,.30);
   }
  }
 }
 // A walkable 27-tread staircase at the pool's east deck to the gallery.
 for(let i=0;i<27;i++){
  const top=.70+i*(3.95/26);
  const z=-16.4-i*1.05;
  B('Interieur/Escalier galerie','prp_terrazzo_floor',37,top-.09,z,4.4,.18,1.1);
  for(const side of [-1,1])
   B('Interieur/Escalier galerie/Contremarches',
    'prp_concrete_raw',37+side*2.12,(.55+top)*.5,z,.14,top-.55,.98);
 }
 // Foyer visual rhythm: framed ceramic friezes and wall sconces.
 for(const side of [-1,1]){
  for(const z of [-9,-13,-17]){
   B('Interieur/Hall/Appliques murales','cse_pool_strip',side*27,5.4,z,.16,1.35,.24);
   B('Interieur/Hall/Encadrements','prp_terrazzo_floor',side*27,4.4,z,.24,.12,3.4);
  }
 }
 // Hall staircase: a separate public stair from ground to the y=4.5 gallery.
 // Top tread overlaps the landing, no floating stairs and no blocked doorway.
 for(let i=0;i<25;i++){
  const top=.73+i*(3.77/24);
  const z=-17.5+i*.33;
  B('Interieur/Hall/Grand escalier','prp_terrazzo_floor',
    -15,top-.12,z,4.2,.24,.43);
  for(const side of [-1,1])
   B('Interieur/Hall/Grand escalier/Garde-corps','prp_steel_painted',
     -15+side*2.2,top+.53,z,.07,1.05,.08);
 }
 // Landing tongue overlaps the final tread and the existing y=4.5
 // first-floor gallery, so feet never have to jump a 0.7 m void.
 B('Interieur/Hall/Palier raccord escalier','prp_terrazzo_floor',
   -15,4.50,-9.02,4.4,.30,1.30);
 // Energetik artwork mounted INTO a real ground-floor wall facing the pool.
 // Original aspect ratio 9.5/4.3 preserved. Rear shell inner face z=-54.32.
 B('Interieur/Piscine/Fresque/Mur porteur','prp_plaster_pale',
   0,2.46,-54.25,10.4,4.0,.15);
 B('Interieur/Piscine/Fresque Energetik originale','cse_energetik_fresco',
   0,2.46,-54.14,8.36,3.78,.065);
 for(const x of [-4.9,4.9])
   B('Interieur/Piscine/Fresque/Encadrement','prp_concrete_raw',
     x,2.46,-54.11,.16,4.1,.15);
 // Under-gallery strips share functional lights: no individual point light.
 for(const side of [-1,1])
   for(const z of [-45,-33,-21,-10]){
     B('Interieur/Galeries/Sous mezzanine/Reglette','cse_pool_strip',
       side*38.0,4.24,z,4.8,.065,.30);
     B('Interieur/Galeries/Sous mezzanine/Support','prp_window_frame',
       side*38.0,4.30,z,5.0,.12,.38);
   }
 // Locker rooms and showers inside the WEST ground-floor deck, accessed
 // through a 4 m opening from the western poolside corridor. No new building.
 const cloak='Interieur/Vestiaires et douches ouest';
 for(const z of [-45,-27])
  B(cloak+'/Murs','cse_ivory_plaster',-40,2.15,z,17,3.25,.24);
 B(cloak+'/Murs','cse_ivory_plaster',-48.38,2.15,-36,.25,3.25,18.0);
 for(const z of [-41.5,-30.5])
  B(cloak+'/Mur accès','prp_tile_ceramic',-31.35,2.15,z,.25,3.25,7);
 // The shower suite is behind a split wall with a passage.
 for(const [x,w] of [[-45,7],[-34.7,6.8]])
  B(cloak+'/Douches/Cloisons','prp_tile_ceramic',x,2.1,-41,w,3.15,.23);
 for(let i=0;i<12;i++){
  const z=-43.5+i*1.12;
  B(cloak+'/Casiers','prp_steel_painted',-47.5,1.55,z,.68,1.88,.69);
  B(cloak+'/Casiers/Poignées','prp_window_frame',-47.13,1.7,z,.055,.18,.20);
 }
 for(const z of [-36,-33.4,-30.8])
  B(cloak+'/Bancs intérieurs','prp_wood_weathered',-40,.91,z,3.0,.19,.62);
 for(let i=0;i<5;i++){
  const x=-44.5+i*1.75;
  B(cloak+'/Douches/Colonnes','prp_steel_painted',x,2.0,-43.6,.09,2.8,.09);
  S(cloak+'/Douches/Pommeaux','prp_steel_painted',x,3.10,-43.5,.42,.14,.35);
 }
 for(const z of [-38,-31])
  B(cloak+'/Plafonniers','cse_pool_strip',-40,4.10,z,7.5,.09,.45);
 // Small eastern service/changing room, entirely clear of east gallery
 // stairs, reached from the rear walkway of the pool.
 const east='Interieur/Vestiaires est et locaux techniques';
 for(const z of [-52,-45])
  B(east+'/Murs','cse_ivory_plaster',44.5,2.10,z,8,3.20,.24);
 B(east+'/Mur latéral','prp_tile_ceramic',48.5,2.10,-48.5,.24,3.20,7.0);
 for(let k=0;k<6;k++)
  B(east+'/Casiers','prp_steel_painted',46.8,1.55,-51+k*.92,.65,1.88,.70);
 // A long line of worn granite slabs; scene fills in from broad to small units.
 const step=p.pave;
 for(let x=-68;x<=67;x+=step)for(let z=26;z<=90;z+=step){
   if(x>17&&x<59&&z>39&&z<65)continue; // water basin
   if(Math.abs(x)<40&&z<30)continue; // stairs
   if(rand()<.035)continue; // missing/repaired tiles
   B('Place/Dalles','prp_terrazzo_floor',x,.118,z,step*.91,.075,step*.91);
 }
 // Oval concrete bench and raised planted mound REPLACE the old outdoor
 // reflecting basin, sculpture and all freestanding exterior benches.
 const cx=37,cz=52,rx=16.45,rz=10.5,segments=96;
 for(let i=0;i<segments;i++){
  const a=2*Math.PI*(i+.5)/segments;
  const dx=-rx*Math.sin(a),dz=rz*Math.cos(a);
  const tangent=Math.atan2(-dz,dx)*180/Math.PI;
  const chord=Math.hypot(dx,dz)*2*Math.PI/segments;
  const x=cx+rx*Math.cos(a),z=cz+rz*Math.sin(a);
  B('Place/Banc ovale béton/Assise','prp_concrete_panel',x,.48,z,chord+0.10,.16,.76,tangent);
  if(i%2===0)
   B('Place/Banc ovale béton/Piétement','prp_concrete_raw',x,.235,z,
     chord+.10,.35,.42,tangent);
 }
 // Low earth dome: supports dense plants but does not occlude the seat.
 S('Place/Butte végétalisée','prp_soil_leaf_litter',cx,.32,cz,29,2.35,17.2);
 // Scene-only replacement — the earlier primitive statue and floating flower
 // balls are not generated. Existing earth dome and oval concrete seat remain.
 rebuildGarden({B,S,F,rand,cx,cz});
 rebuildMonument({B,S,cx,cz});
 // Roof equipment, railings and service frames.
 for(let x=-46;x<=46;x+=3.2)for(let z=-52;z<=-9;z+=6.4){
   B('Architecture/Palais/Toiture techniques','prp_roof_metal',x,19.9,z,1,.6,2.4);
 }
 // Retro-futurist rooftop sign designed for the fictional Energetik palace.
 // The lettering is real geometry, not a texture; it works in raster and CPU.
 const neon='Architecture/Palais/Enseigne lumineuse toit';
 B('Architecture/Palais/Acrotere','prp_concrete_panel',0,19.85,-3.75,97.5,.72,.55);
 B('Architecture/Palais/Corniche','prp_terrazzo_floor',0,19.35,-3.65,98,.22,.8);
 B(neon+'/Panneau','prp_steel_rusted',0,23.05,-3.32,30.9,4.45,.36);
 B(neon+'/Retrofond','prp_concrete_panel',0,23.15,-3.08,29.8,3.8,.14);
 for(const x of [-14,-7,0,7,14])
  B(neon+'/Structure','prp_steel_rusted',x,21.55,-3.72,.28,3.7,.26);
 for(const y of [20.9,25.2])
  B(neon+'/Structure','prp_steel_painted',0,y,-3.36,31.3,.2,.22);
 // Bright red and cyan marquees, warm incandescent bulbs.
 for(const y of [21.15,25.0])
  B(neon+'/Neon rouge','cse_neon_red',0,y,-2.96,29.8,.10,.12);
 for(const x of [-14.9,14.9])
  B(neon+'/Encadrement lumineux','cse_neon_gold',x,23.05,-2.94,.09,3.8,.11);
 for(let i=0;i<16;i++){
  const x=-14.4+i*1.92;
  S(neon+'/Ampoules sobres','cse_neon_gold',x,25.30,-2.91,.14,.14,.12);
 }
 // Sober sign: the exaggerated orbit arches and rooftop spikes are removed.
 // 5x7 pixel-serif neon font, each letter built from individually illuminated
 // metal/glass modules. Full-width readable Cyrillic « ЭНЕРГЕТИК ».
 const glyph={
  'Э':['01110','10001','00001','00111','00001','10001','01110'],
  'Н':['10001','10001','10001','11111','10001','10001','10001'],
  'Е':['11111','10000','10000','11110','10000','10000','11111'],
  'Р':['11110','10001','10001','11110','10000','10000','10000'],
  'Г':['11111','10000','10000','10000','10000','10000','10000'],
  'Т':['11111','00100','00100','00100','00100','00100','00100'],
  'И':['10001','10011','10101','10101','11001','10001','10001'],
  'К':['10001','10010','10100','11000','10100','10010','10001']
 };
 const word='ЭНЕРГЕТИК';
 for(let k=0;k<word.length;k++){
  const rows=glyph[word[k]];
  for(let row=0;row<7;row++)
   for(let col=0;col<5;col++)
    if(rows[row][col]==='1')
     B(neon+'/Lettres ЭНЕРГЕТИК','cse_neon_red',
       -11.35+k*2.77+col*.36,24.4-row*.40,-2.82,.30,.34,.18);
 }
 // Two simple roof fabric banners, not historically authenticated signage.
 for(const side of [-1,1]){
  const bx=side*36.5;
  B('Architecture/Palais/Banderoles toit/Mat','prp_steel_painted',
    bx-2.35,22.2,-3.52,.11,5.0,.11);
  B('Architecture/Palais/Banderoles toit/Traverse','prp_steel_painted',
    bx,24.75,-3.52,4.70,.10,.10);
  for(let strip=0;strip<5;strip++){
   const z=-3.31+Math.sin(strip*.85+side)*.065;
   B('Architecture/Palais/Banderoles toit/Tissu',
     side<0?'cse_banner_red':'cse_banner_cream',
     bx+(strip-2)*.86,22.55,z,.875,4.05,.065);
  }
  S('Architecture/Palais/Banderoles toit/Insigne','cse_flower_gold',
    bx,22.9,-3.19,.66,.66,.13);
  for(const dx of [-.43,0,.43])
   B('Architecture/Palais/Banderoles toit/Insigne','cse_banner_red',
    bx+dx,22.22,-3.12,.075,.46,.07);
 }
 // Exterior benches removed: the single oval planter seat is the only one.
 for(let i=0;i<Math.min(p.lamps,18);i++){
  const x=-65+(i%2)*130,z=24+Math.floor(i/2)*8;
  B('Mobilier/Lampadaires','prp_steel_painted',x,3.8,z,.24,7.6,.24);
  S('Mobilier/Lampadaires','prp_window_glass',x,7.68,z,.82,.82,.82);
 }
 // Four modest planted groups frame the entrance. They are inside the
 // default camera frustum for the actual WARP foliage-instancing checks
 // but outside the 10-m central access corridor.
 for(const x of [-16,-11,11,16])
  F('Place/Jardins devant entrée','prp_foliage',x,1.15,-1.5,2.1,2.35,2.1);
 // Scene-only replacement: articulated poplars and birches at fixed
 // documented coordinates; no 5-blob-on-a-pole former silhouettes.
 rebuildTrees({B,S,F,rand,p});
 // Low mixed planting along the pavements: no scattered park benches,
 // and no bushes across the entrance stairway.
 for(const side of [-1,1]){
  for(const z of [9,22,78,88]){
   for(const distance of [36,44,53]){
    const x=side*(distance+rand()*1.3);
    F('Place/Jardins/Arbustes','cse_leaf_olive',x,.75,z,
       1.85,.95+rand()*.7,1.55);
    S('Place/Jardins/Fleurs basses',
      ((distance+z)%2===0?'cse_flower_red':'cse_flower_gold'),
      x+side*.60,.63,z+.65,.34,.23,.34);
   }
  }
 }
 // The former auditorium seating was removed for the olympic pool.
 // The three upper galleries, staircase and the pool lanes are all physical
 // geometry; no hidden decorative dummy models are generated.
 // Technical rooftop ribs, visible from camera 06.
 for(let x=-45;x<=45;x+=100/p.detail)for(let z=-49;z<=-15;z+=120/p.detail){
   B('Architecture/Palais/Toiture techniques','prp_steel_painted',x,20.1,z,.07,.18,1.6);
 }
 // Detailed masonry cladding on service façades. This is intentionally real
 // visible geometry used as the adjustable 20K/150K/500K triangle workload.
 // It stays within a physically meaningful grid and never creates a dummy
 // invisible stress mesh. With box primitives, one instance = 12 triangles.
 let row=0,col=0;
 while(boxes*12<p.target){
   const face=Math.floor(row/118)%3;
   let x,y,z,w=.30,h=.135,d=.055;
   if(face===0){x=-47+col*.34;y=.75+(row%118)*.155;z=-55.04;}
   else if(face===1){x=-47+col*.34;y=.75+(row%118)*.155;z=-4.95;}
   else{x=-50.55;y=.75+(row%118)*.155;z=-54+col*.18;w=.055;d=.165;}
   // Avoid bricking up entrance; raised galleries stay separate from façade.
   if(face===1&&Math.abs(x)<7.5&&y<4.15){
     col++;if(col>=275){col=0;row++;}continue;
   }
   B('Architecture/Revêtements/Clinker haute densité',
     ((row+col)%17<3?'prp_concrete_raw':'prp_brick_red'),x,y,z,w,h,d);
   col++;
   if(col>=275){col=0;row++;}
   if(row>=354){throw new Error('Requested geometry exceeds architectural detail cap');}
 }
 // Basic geometric accounting: foliage canopy and sphere triangle counts are
 // additional to the strictly determined box count * 12.
 return {objects,estimatedMinTriangles:boxes*12,boxInstances:boxes,
         sphereInstances:spheres,canopyInstances:canopies,profile:profileName,
         textureCap:p.textureCap};
}
function manifest(profile){
 const name='CSE WARP V17 — Complexe civique fictif ['+profile+']';
 return {format:'m3d-scene-v16',name,version:1,units:'m',
  source:'CSE WARP fictional benchmark; Pripyat PBR identifiers reused, not Pripyat geometry',
  sectors:[{id:'benchmark-'+profile,
   geometry:['geometry/benchmark.json'],
   instances:['instances/'+profile+'.json'],
   materials:['materials/pripyat-reuse.json'],
   pbr:['pbr/pripyat-reuse-'+profile+'.json'],
   lighting:['lighting/day.json'],cameras:['cameras/views.json'],
   environment:['environment/site.json']}]};
}
const arguments_=process.argv.slice(2);
if(arguments_.some(s=>s==='--help')){
 process.stdout.write('node generate.mjs [--all | --profile leger|lourd|extreme]\n');
 process.exit(0);
}
const selected=arguments_.includes('--all')?Object.keys(profiles):
 arguments_.includes('--profile')?[arguments_[arguments_.indexOf('--profile')+1]]:['leger'];
for(const p of selected)if(!profiles[p])throw new Error('Unknown profile '+p);
save('geometry/benchmark.json',geometry);
save('cameras/views.json',cameras);
save('lighting/day.json',lighting);
save('environment/site.json',environment);
save('materials/pripyat-reuse.json',{format:'m3d-materials-v1',
 origin:'assets/materials/pripyat-v17/catalogue.json',
 originalsRetained:catalog.materials.length,materials:[...catalog.materials,...localMaterials]});
generateEntryMosaic(path.join(root,'textures/cse_parvis_mosaique.png'));
for(const name of selected){
 const p=profiles[name];
 const sets=[...catalog.sets,...localSets].map(v=>Object.assign({},v,{
  resolution:v.imageFile?2048:Math.min(p.textureCap,v.resolution)}));
 save('pbr/pripyat-reuse-'+name+'.json',{format:'m3d-pbr-v1',library:'Pripyat-V17',mipmaps:'auto',
   textureResolutionTarget:p.textureCap,channels:['albedo','normal','roughness'],
   note:'22 original PBR recipes unchanged. Energetik hall image and supplied exterior portrait use separate scene-only JPEGs.',
   sets});
 const built=build(name,p);
 save('instances/'+name+'.json',{format:'m3d-instances-v1',
   notes:'Single-palace Olympic pool; two exact artworks retained; scene-only reconstructed sculpted figures, articulated trees and individual flowers.',
   instances:built.objects});
 const sceneName=name==='leger'?'scene.json':'scene-'+name+'.json';
 save(sceneName,manifest(name));
 save('stats/'+name+'.json',{
   profile:name,targetTriangles:p.target,
   estimatedMinTriangles:built.estimatedMinTriangles,
   boxes:built.boxInstances,spheres:built.sphereInstances,
   foliageCanopies:built.canopyInstances,totalInstances:built.objects.length,
   maxPbrResolution:p.textureCap,
   safetyNote:'Triangles are a box-derived lower bound. Intact original Pripyat library and mosaic image; no secondary buildings. Extreme remains manual/opt-in.'
 });
 console.log(name+' — '+built.objects.length+' instances, >= '+
             built.estimatedMinTriangles+' box triangles, PBR <= '+p.textureCap);
}
