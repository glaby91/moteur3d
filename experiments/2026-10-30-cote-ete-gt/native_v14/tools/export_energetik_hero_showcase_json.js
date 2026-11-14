'use strict';
// Small geometry snapshot used by the deterministic asset preview renderer.
const fs=require('fs'),path=require('path');
const {buildEnergetikRoofNeonSign}=require('../assets/pripyat/energetik_roof_neon_sign.js');
const {buildEnergetikFacadeBay}=require('../assets/pripyat/energetik_facade_bay.js');
const buildSeat=require('../assets/pripyat/energetik_cinema_seat.js');
const objects=[];
const api={objects,
 box:(x0,y0,z0,x1,y1,z1,mat)=>objects.push({kind:'box',x0,y0,z0,x1,y1,z1,mat}),
 roundBox:(x0,y0,z0,x1,y1,z1,r,mat)=>objects.push({kind:'box',x0,y0,z0,x1,y1,z1,r,mat}),
 capsule:(a,b,r,mat)=>objects.push({kind:'capsule',a,b,r,mat}),
 ellipse:(cx,cy,cz,rx,ry,rz,mat)=>objects.push({kind:'ellipse',cx,cy,cz,rx,ry,rz,mat})};
const facades=[buildEnergetikFacadeBay(api,-3.4,0,0,{nativeMaterial:'concrete'}),buildEnergetikFacadeBay(api,3.4,0,0,{nativeMaterial:'concrete'})];
const sign=buildEnergetikRoofNeonSign(api,0,9.43,.12,{sectionLength:.28});
for(const [x,opts] of [[-.82,{variantId:'seat_left_missing_pan',seatPanMissing:true}],[0,{variantId:'seat_center_raised_pan',folded:true}],[.82,{variantId:'seat_right_lowered_pan'}]])buildSeat(api,x,.06,1.18,{...opts,omitSharedFrame:true,sideStyle:'padded',fabric:'seatVinylBrown',trim:'seatVinylBrown',support:'seatSupportPaint'});
api.capsule([-1.235,.465,1.205],[1.235,.465,1.205],.027,'seatSupportPaint');
for(const x of [-1.235,-.41,.41,1.235])api.capsule([x,.465,1.205],[x,.545,1.205],.019,'seatSteel');
api.capsule([0,.465,1.205],[0,.085,1.205],.031,'seatSupportPaint');
const out=process.argv[2]||path.join(__dirname,'energetik_hero_showcase.geometry.json');
fs.writeFileSync(out,JSON.stringify({schema:'m3d-preview-geometry-v1',units:'metres',objects,assets:{facades,sign},neonCoreSegments:objects.filter(o=>o.mat==='neon').length,neonGlassSegments:objects.filter(o=>o.mat==='glass'&&o.assetPartId).length}));
console.log(`${out}: ${objects.length} primitives, ${objects.filter(o=>o.mat==='neon').length} individually segmented neon cores`);
