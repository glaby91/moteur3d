#!/usr/bin/env node
// CSE WARP V17 - deterministic heavy scene generator.
// No network calls and no external dependencies. Original Pripyat files stay
// untouched; only stable material IDs are reused by V17 procedural PBR recipes.
'use strict';
const fs=require('node:fs');
const path=require('node:path');
const root=__dirname;
const repoRoot=path.resolve(root,'../../..');
const catalogPath=path.join(repoRoot,'assets/materials/pripyat-v17/catalogue.json');
const catalog=JSON.parse(fs.readFileSync(catalogPath,'utf8'));
const profiles={
  leger: {target:20000, textureCap:512, pave:4.5, trees:18, benches:12, lamps:12, blocks:3, seatCols:16, seatRows:9, detail:3},
  lourd: {target:150000, textureCap:1024, pave:2.2, trees:56, benches:35, lamps:26, blocks:10, seatCols:40, seatRows:25, detail:8},
  extreme:{target:500000, textureCap:1024, pave:1.1, trees:130, benches:85, lamps:48, blocks:18, seatCols:64, seatRows:40, detail:20}
};
const geometry={format:'m3d-geometry-v1',units:'m',meshes:[
 {id:'unit_box',type:'box',size:[1,1,1],uv:'planar_per_face'},
 {id:'unit_sphere',type:'uv_sphere',segments:12,rings:8,uv:'spherical'},
 {id:'foliage_canopy',type:'foliage_canopy'}
]};
const cameras={format:'m3d-cameras-v1',cameras:[
 {id:'facade',name:'01 · Vue générale, palais et bassin',position:[93,37,102],target:[0,10,2],fovDegrees:67},
 {id:'escalier',name:'02 · Grand escalier et verrière',position:[18,9,41],target:[0,8,-16],fovDegrees:66},
 {id:'passerelle',name:'03 · Passerelle et aile ouest',position:[-115,14,22],target:[-50,9,-27],fovDegrees:70},
 {id:'bassin',name:'04 · Bassin, sculpture et façade',position:[73,6,70],target:[18,9,-15],fovDegrees:68},
 {id:'hall',name:'05 · Hall intérieur',position:[0,3.7,-14],target:[0,6,-35],fovDegrees:74},
 {id:'vue_haut',name:'06 · Vue aérienne',position:[108,85,106],target:[0,3,-13],fovDegrees:67}
]};
const lighting={format:'m3d-lighting-v1',hour:14.75,
 sun:{direction:[-.68,.65,-.35],intensity:3.1},ambient:{color:[.63,.73,.91],intensity:.39},
 lights:[{position:[-5,8,-12],radius:20,intensity:1.8,color:[255,218,174]},
         {position:[8,8,-22],radius:22,intensity:1.2,color:[255,217,180]}]};
const environment={format:'m3d-environment-v1',terrain:'finite',units:'m',
 note:'Fictional benchmark architecture, not historically accurate Pripyat. No external image textures are assumed.'};
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
 // Ground, roads and grounds of the fictional cultural campus.
 B('Terrain/Sol','prp_soil_leaf_litter',0,-.32,0,260,.55,235);
 B('Terrain/Place','prp_concrete_raw',0,-.04,33,144,.17,117);
 B('Terrain/Routes','prp_asphalt_old',-93,.015,22,29,.14,210);
 B('Terrain/Routes','prp_asphalt_old',0,.015,105,250,.14,23);
 B('Terrain/Trottoirs','prp_concrete_panel',-73,.09,24,7,.18,195);
 B('Terrain/Trottoirs','prp_concrete_panel',0,.1,93,210,.18,5);
 // Three major building blocks: main palace 100 x 46, left wing and loading wing.
 for(const [name,x,z,w,d,height] of [
   ['Palais',0,-30,101,50,19],
   ['Aile ouest',-71,-32,39,36,12],
   ['Aile est',75,-33,40,38,10]]) {
   const g='Architecture/'+name;
   B(g+'/Fondations','prp_concrete_raw',x,.27,z,w,.55,d);
   const floors=Math.floor(height/4.5);
   for(let f=1;f<=floors;f++)
     B(g+'/Planchers','prp_concrete_raw',x,f*4.5,z,w,.3,d);
   B(g+'/Toiture','prp_roof_bitumen',x,height+.55,z,w+1.7,.55,d+1.8);
   B(g+'/Toiture','prp_roof_metal',x,height+.94,z+d/2,w+1.6,.27,.45);
   B(g+'/Toiture','prp_roof_metal',x,height+.94,z-d/2,w+1.6,.27,.45);
   B(g+'/Volume','prp_concrete_panel',x,2.4,z-d/2+.35,w,4.5,.65);
   B(g+'/Volume','prp_concrete_panel',x-w/2+.35,height/2,z,.65,height,d);
   B(g+'/Volume','prp_concrete_panel',x+w/2-.35,height/2,z,.65,height,d);
 }
 // Main glass atrium, colonnade and real structural cross-beams.
 for(let i=0;i<17;i++){
   const x=-48+i*6;
   B('Architecture/Palais/Colonnade','prp_concrete_raw',x,9.5,-4.4,.72,19,.9);
   B('Architecture/Palais/Poutres','prp_concrete_raw',x,15,-8.5,.55,.72,9);
 }
 for(let level=0;level<4;level++){
   const y=2.6+level*4.15;
   B('Architecture/Palais/Poutres','prp_concrete_raw',0,level*4.15+4.35,-4.6,96,.4,.74);
   for(let k=0;k<27;k++){
     const x=-44+k*3.35;
     if(Math.abs(x)>22&&k%4===0)continue;
     B('Architecture/Palais/Verriere','prp_window_glass',x,y,-4.03,3.04,3.65,.085);
     B('Architecture/Palais/Chassis','prp_window_frame',x,y,-3.98,.10,3.8,.14);
     B('Architecture/Palais/Chassis','prp_window_frame',x,level*4.15+4.42,-3.95,3.4,.12,.15);
   }
 }
 // Massive stairs, balustrades, terraces, planters.
 for(let i=0;i<18;i++){
   B('Place/Escalier monumental','prp_terrazzo_floor',0,.13+i*.095,22-i*.73,70,.2,1.03);
 }
 for(let side of [-1,1]){
   B('Place/Retenues','prp_concrete_raw',side*41,1.3,13,3.1,2.6,30);
   for(let n=0;n<21;n++){
     B('Place/Garde corps','prp_steel_rusted',side*39,2.9,1+n*1.1,.09,1.15,.09);
     B('Place/Garde corps','prp_steel_painted',side*39,3.4,1+n*1.1,.14,.08,1.15);
   }
 }
 // Left enclosed walkway with accessible road beneath.
 for(let i=0;i<18;i++){
   const x=-108+i*3;
   B('Architecture/Passerelle','prp_concrete_panel',x,7.2,-20,3.05,.34,6);
   B('Architecture/Passerelle','prp_concrete_panel',x,11.3,-20,3.05,.3,6);
   B('Architecture/Passerelle/Vitres','prp_window_glass',x,9.1,-16.92,2.7,3.8,.1);
   B('Architecture/Passerelle/Chassis','prp_window_frame',x,9.1,-16.85,.10,3.8,.15);
 }
 // Great mural composed of deliberately distinct painted and concrete plates.
 for(let row=0;row<14;row++)for(let col=0;col<12;col++){
   const warm=(row+col*3)%9<3;
   B('Architecture/Palais/Fresque originale fictive',
     warm?'prp_sign_paint':'prp_plaster_pale',32+col*1.24,8+row*.64,-3.9,1.22,.62,.07);
 }
 // A long line of worn granite slabs; scene fills in from broad to small units.
 const step=p.pave;
 for(let x=-68;x<=67;x+=step)for(let z=26;z<=90;z+=step){
   if(x>17&&x<59&&z>39&&z<65)continue; // water basin
   if(Math.abs(x)<40&&z<30)continue; // stairs
   if(rand()<.035)continue; // missing/repaired tiles
   B('Place/Dalles','prp_terrazzo_floor',x,.118,z,step*.91,.075,step*.91);
 }
 // Reflecting pool: sides, low sill, water as a distinct transmissive surface.
 B('Place/Bassin','prp_concrete_raw',37,.14,52,33,.28,22);
 B('Place/Bassin','prp_puddle_water',37,.32,52,30,.03,19);
 for(let z of [41,63])B('Place/Bassin/Bordure','prp_terrazzo_floor',37,.47,z,34,.65,.85);
 for(let x of [20,54])B('Place/Bassin/Bordure','prp_terrazzo_floor',x,.47,52,.85,.65,22);
 // A wholly original modernist abstract fountain sculpture.
 for(let k=0;k<26;k++){
   const a=2*Math.PI*k/26;
   B('Place/Monument abstrait','prp_steel_rusted',37+6*Math.cos(a),3.4+2*Math.sin(a),52,
     .37,.38,.35,a);
 }
 B('Place/Monument abstrait','prp_concrete_raw',37,.7,52,5,1.1,4);
 // Roof equipment, railings and service frames.
 for(let x=-46;x<=46;x+=3.2)for(let z=-52;z<=-9;z+=6.4){
   B('Architecture/Palais/Toiture techniques','prp_roof_metal',x,19.9,z,1,.6,2.4);
 }
 for(let i=0;i<80;i++){
   const x=-43+i*1.1;
   B('Architecture/Palais/Enseigne fictive','prp_steel_rusted',x,20.8,-3.4,.13,2.0,.16);
 }
 // Benches and lampposts, deliberately modeled from several material instances.
 function bench(x,z,rot){
   B('Mobilier/Bancs','prp_steel_rusted',x, .43,z,2.05,.12,.6,rot);
   B('Mobilier/Bancs','prp_wood_weathered',x,.57,z,1.9,.18,.52,rot);
   B('Mobilier/Bancs','prp_wood_weathered',x,.96,z-.31,1.9,.8,.1,rot);
   for(let side of [-.78,.78]){
     B('Mobilier/Bancs','prp_steel_painted',x+side,.27,z,.09,.54,.68,rot);
     B('Mobilier/Bancs','prp_steel_painted',x+side,1.0,z-.32,.09,.9,.1,rot);
   }
 }
 for(let i=0;i<p.benches;i++){
   const col=i%4,row=Math.floor(i/4);
   bench(-62+col*39,31+row*5,(i%3)*.08);
 }
 for(let i=0;i<p.lamps;i++){
   const x=-65+(i%2)*130,z=13+Math.floor(i/2)*6;
   B('Mobilier/Lampadaires','prp_steel_painted',x,3.8,z,.24,7.6,.24);
   S('Mobilier/Lampadaires','prp_window_glass',x,7.68,z,.82,.82,.82);
 }
 // Bus stop and loading shelters.
 B('Mobilier/Abribus','prp_steel_rusted',-101,3.3,46,8.6,.3,3.8);
 for(let k=0;k<4;k++){
   B('Mobilier/Abribus','prp_steel_painted',-104+k*2,1.7,44.3,.11,3.2,.11);
   B('Mobilier/Abribus','prp_window_glass',-103+k*2,1.75,44.3,1.85,2.8,.07);
 }
 for(let i=0;i<12;i++){
   B('Mobilier/Barrières','prp_concrete_raw',-84+i*1.4,.52,91,1.2,.9,.45);
   B('Mobilier/Barrières','prp_sign_paint',-84+i*1.4,.67,91.3,.75,.18,.07);
 }
 // Street trees and green pockets with repeated but distinct crown geometry.
 for(let i=0;i<p.trees;i++){
   const col=i%2,row=Math.floor(i/2);
   const x=(col===0?-127:119)+(rand()-.5)*8;
   const z=-95+(row%38)*5.2+(rand()-.5)*2;
   const h=9+rand()*7;
   B('Nature/Arbres','prp_bark_wet',x,h*.35,z,.55,h*.7,.55);
   F('Nature/Arbres','prp_foliage',x,h*.83,z,4+rand()*2,5+rand()*3,4+rand()*2);
   if(p.detail>8){
     F('Nature/Arbres','prp_foliage',x+.9,h*.72,z+.8,3.5,3.1,3);
   }
 }
 // Original indoor public auditorium with actual repetitive furniture modules.
 for(let row=0;row<p.seatRows;row++)for(let col=0;col<p.seatCols;col++){
   const x=-23+col*.72,z=-47+row*.75;
   const y=1.10+row*.028;
   B('Interieur/Auditorium/Fauteuils','prp_steel_painted',x,y,z,.065,.6,.58);
   B('Interieur/Auditorium/Fauteuils','prp_wood_weathered',x,y+.22,z,.59,.11,.53);
   B('Interieur/Auditorium/Fauteuils','prp_wood_weathered',x,y+.48,z-.22,.59,.6,.13);
   B('Interieur/Auditorium/Fauteuils','prp_steel_rusted',x+.3,y+.16,z,.05,.45,.58);
   if(p.detail>4)
     B('Interieur/Auditorium/Fauteuils','prp_steel_rusted',x-.3,y+.16,z,.05,.45,.58);
 }
 // Residential distant blocks: repeated real façade modules, not hidden dummy cubes.
 for(let i=0;i<p.blocks;i++){
   const x=-112+(i%7)*31,z=-105-Math.floor(i/7)*26;
   B('Horizon/Logements','prp_concrete_panel',x,13,z,20,26,12);
   B('Horizon/Logements','prp_roof_bitumen',x,26.3,z,21,.35,13);
   for(let floor=0;floor<7;floor++)for(let bay=0;bay<6;bay++){
     const bx=x-8.5+bay*3.2,by=2.8+floor*3.5;
     B('Horizon/Logements/Baies','prp_window_glass',bx,by,z+6.06,2.1,2.4,.08);
     B('Horizon/Logements/Baies','prp_window_frame',bx,by+1.22,z+6.1,2.3,.09,.12);
   }
 }
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
 origin:'assets/materials/pripyat-v17/catalogue.json',materials:catalog.materials});
for(const name of selected){
 const p=profiles[name];
 const sets=catalog.sets.map(v=>Object.assign({},v,{resolution:Math.min(p.textureCap,v.resolution)}));
 save('pbr/pripyat-reuse-'+name+'.json',{format:'m3d-pbr-v1',library:'Pripyat-V17',mipmaps:'auto',
   textureResolutionTarget:p.textureCap,channels:['albedo','normal','roughness'],
   note:'Procedural V17 fallback; not an exact texture extraction of legacy V14 binary PBR maps.',
   sets});
 const built=build(name,p);
 save('instances/'+name+'.json',{format:'m3d-instances-v1',
   notes:'Original fictional complex. Generated, deterministic; do not hand-edit this file.',
   instances:built.objects});
 const sceneName=name==='leger'?'scene.json':'scene-'+name+'.json';
 save(sceneName,manifest(name));
 save('stats/'+name+'.json',{
   profile:name,targetTriangles:p.target,
   estimatedMinTriangles:built.estimatedMinTriangles,
   boxes:built.boxInstances,spheres:built.sphereInstances,
   foliageCanopies:built.canopyInstances,totalInstances:built.objects.length,
   maxPbrResolution:p.textureCap,
   safetyNote:'Triangles are a box-derived lower bound; canopy and spheres add extra geometry. Extreme is manual/opt-in.'
 });
 console.log(name+' — '+built.objects.length+' instances, >= '+
             built.estimatedMinTriangles+' box triangles, PBR <= '+p.textureCap);
}
