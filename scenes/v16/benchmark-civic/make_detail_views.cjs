#!/usr/bin/env node
// Additional art-review cameras — separate generated scene manifest.
// Does not alter the 11 official cameras, scene.json, or the 3D engine.
'use strict';
const fs=require('node:fs'),path=require('node:path');
const root=__dirname;
const main=JSON.parse(fs.readFileSync(path.join(root,'scene.json'),'utf8'));
const data=JSON.parse(fs.readFileSync(path.join(root,'cameras/views.json'),'utf8'));
const addon=[
 {id:'monument_detail',name:'Monument original — modelé rapproché',
  position:[48.0,3.9,62],target:[37,5.2,52],fovDegrees:54},
 {id:'parterre_detail',name:'Massif fleuri — pétales et feuillages',
  position:[53.5,1.85,61.0],target:[42,1.12,53.0],fovDegrees:61},
 {id:'arbres_detail',name:'Peupliers et bouleaux — ramifications',
  position:[61,2.5,53],target:[73,9,43],fovDegrees:57}
];
for(const c of addon)if(data.cameras.some(x=>x.id===c.id))throw Error('Duplicate camera '+c.id);
const detailed={...data,cameras:[...data.cameras,...addon]};
fs.writeFileSync(path.join(root,'cameras/details.json'),JSON.stringify(detailed,null,2)+'\n');
const manifest=structuredClone(main);
for(const sector of manifest.sectors)sector.cameras=['cameras/details.json'];
manifest.name=String(main.name||'CSE WARP')+' — détail géométrique';
fs.writeFileSync(path.join(root,'scene-detail.json'),JSON.stringify(manifest,null,2)+'\n');
console.log('3 close-up art-review cameras generated; base scene and engine untouched');
