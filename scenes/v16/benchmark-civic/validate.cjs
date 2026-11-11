#!/usr/bin/env node
'use strict';
const fs=require('node:fs');
const path=require('node:path');
const base=__dirname;
const origin=JSON.parse(fs.readFileSync(path.resolve(base,'../../../assets/materials/pripyat-v17/catalogue.json'),'utf8'));
const ids=origin.materials.map(x=>x.id);
if(ids.length!==22||new Set(ids).size!==ids.length)throw Error('Legacy materials not preserved');
const required=['geometry/benchmark.json','materials/pripyat-reuse.json',
  'cameras/views.json','lighting/day.json',
  'scene.json','scene-lourd.json','scene-extreme.json'];
for(const file of required){
 if(!fs.existsSync(path.join(base,file)))throw Error('Missing '+file);
}
function read(file){return JSON.parse(fs.readFileSync(path.join(base,file),'utf8'));}
const mats=read('materials/pripyat-reuse.json').materials;
if(mats.length!==22||mats.some((m,i)=>m.id!==origin.materials[i].id))
 throw Error('Material source IDs were altered');
for(const [profile,target,cap,filename] of [
 ['leger',20000,512,'scene.json'],
 ['lourd',150000,1024,'scene-lourd.json'],
 ['extreme',500000,1024,'scene-extreme.json']
]){
 const scene=read(filename);
 const sector=scene.sectors?.[0];
 if(scene.format!=='m3d-scene-v16'||!sector)throw Error('Bad scene '+profile);
 const allRefs=['geometry','materials','pbr','instances','cameras','lighting','environment'];
 for(const kind of allRefs)for(const relative of sector[kind]||[]){
   const real=path.resolve(base,relative);
   if(!real.startsWith(base+path.sep)||!fs.existsSync(real))throw Error('Missing or escaping resource '+real);
 }
 const inst=read('instances/'+profile+'.json').instances;
 const pbr=read('pbr/pripyat-reuse-'+profile+'.json');
 const stats=read('stats/'+profile+'.json');
 const validMaterials=new Set(ids);
 const validGeometry=new Set(read('geometry/benchmark.json').meshes.map(x=>x.id));
 const used=new Set();
 let boxCount=0;
 for(const x of inst){
   if(!x.id||used.has(x.id))throw Error('Duplicate or missing instance id '+x.id);
   used.add(x.id);
   if(!validGeometry.has(x.geometry)||!validMaterials.has(x.material))throw Error('Unknown mesh/material '+x.id);
   if(x.size?.length!==3||x.position?.length!==3)throw Error('Invalid dimensions '+x.id);
   for(const v of [...x.size,...x.position])if(!Number.isFinite(v))throw Error('Nonfinite value '+x.id);
   if(x.size.some(v=>v<=0))throw Error('Degenerate object '+x.id);
   if(x.geometry==='unit_box')boxCount++;
 }
 if(boxCount*12<target)throw Error('Too few box triangles for '+profile);
 if(inst.length>65000)throw Error('More than safety cap');
 if(pbr.sets.length!==22||pbr.sets.some(x=>x.resolution>cap))throw Error('PBR texture cap mismatch');
 if(stats.totalInstances!==inst.length||stats.boxes!==boxCount)throw Error('Stats mismatch');
 process.stdout.write(profile+' PASS: '+inst.length+' instances, '+boxCount*12+' box triangles, PBR '+cap+'\n');
}
