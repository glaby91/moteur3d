'use strict';
// An original, scene-only, deterministic tessera mosaic for the entry.
// The Energetik wall fresco and the supplied facade artwork are untouched.
// This is authored art, not a historical document nor a photo projection.
const fs=require('node:fs');
const path=require('node:path');
const zlib=require('node:zlib');
const palette={
 cream:[210,193,161],ivory:[231,217,186],terracotta:[168,92,66],
 ochre:[195,150,76],olive:[103,109,86],coal:[63,65,59],
 sandstone:[179,163,136],skin:[214,179,143],hair:[82,67,57],
 grout:[178,165,144]
};
const W=1024,H=1024;
function poly(x,y,a){let inside=false;for(let i=0,j=a.length-1;i<a.length;j=i++){
 const xi=a[i][0],yi=a[i][1],xj=a[j][0],yj=a[j][1];
 if((yi>y)!=(yj>y)&&x<(xj-xi)*(y-yi)/(yj-yi)+xi)inside=!inside;
}return inside;}
function ellipse(x,y,cx,cy,rx,ry,rotation=0){
 const co=Math.cos(rotation),si=Math.sin(rotation),dx=x-cx,dy=y-cy;
 const u=(co*dx+si*dy)/rx,v=(-si*dx+co*dy)/ry;
 return u*u+v*v<=1;
}
function ring(x,y,cx,cy,rx,ry,r=.04,rot=0){
 const c=Math.cos(rot),s=Math.sin(rot),dx=x-cx,dy=y-cy;
 const u=(c*dx+s*dy)/rx,v=(-s*dx+c*dy)/ry;
 const d=Math.abs(Math.sqrt(u*u+v*v)-1);
 return d<r;
}
function pick(x,y){
 let c='cream';
 // Framed architectural mosaic, including warm radial geometry.
 if(x<.022||x>.978||y<.025||y>.977)return 'coal';
 if(x<.034||x>.966||y<.037||y>.965)return 'terracotta';
 const rays=[
  [[.48,.05],[.55,.05],[.94,.96],[.75,.96],'terracotta'],
  [[.43,.05],[.49,.05],[.69,.96],[.56,.96],'ochre'],
  [[.35,.05],[.42,.05],[.34,.96],[.19,.96],'olive'],
  [[.28,.05],[.35,.05],[.13,.96],[.035,.96],'sandstone']
 ];
 for(const [a,b,c1,d,color] of rays)if(poly(x,y,[a,b,c1,d]))c=color;
 // Soft sweeping arcs at the foot of the image.
 if(ring(x,y,.52,1.08,.70,.33,.035,-.10))c='ivory';
 if(ring(x,y,.43,1.1,.54,.24,.033,-.09))c='ochre';
 if(ring(x,y,.44,1.2,.79,.40,.028,.04))c='coal';
 // Laurel leaf cluster, left, viewed from above.
 if(ring(x,y,.10,.90,.36,.47,.028,.39)&&x<.39)c='coal';
 for(let i=0;i<11;i++){
  const yy=.51+i*.032,xx=.12+i*.014;
  if(ellipse(x,y,xx,yy,.040,.016,-.55))c='ochre';
  if(ellipse(x,y,xx+.055,yy+.014,.042,.018,.49))c='ivory';
 }
 // The atomic emblem refers to an era of artistic and industrial progress,
 // matching the approved design without imitating the protected wall image.
 if(ellipse(x,y,.75,.47,.18,.16))c='olive';
 for(const rot of [-1.1,0,1.1])
  if(ring(x,y,.75,.47,.115,.050,.053,rot))c='ochre';
 if(ellipse(x,y,.75,.47,.019,.019))c='ivory';
 // Two large, graphic people in cream, terracotta and charcoal tesserae.
 // Their expression and profiles are distinct from the existing fresco.
 const portraits=[
  {x:.39,y:.65,rx:.092,ry:.115,r:-.16},
  {x:.56,y:.63,rx:.089,ry:.115,r:.12}
 ];
 for(const [i,p] of portraits.entries()){
   if(ellipse(x,y,p.x-.012,p.y-.022,p.rx*1.09,p.ry*1.12,p.r))c='hair';
   if(ellipse(x,y,p.x,p.y,p.rx*.84,p.ry*.92,p.r))c='skin';
   if(poly(x,y,[[p.x-.06,p.y+.082],[p.x+.06,p.y+.081],
     [p.x+.15,p.y+.22],[p.x-.13,p.y+.22]]))c=i?'terracotta':'olive';
   if(ellipse(x,y,p.x+.030,p.y-.020,.012,.007))c='coal';
   if(poly(x,y,[[p.x+.054,p.y-.005],[p.x+.11,p.y+.025],
     [p.x+.045,p.y+.038]]))c='skin';
   if(poly(x,y,[[p.x-.063,p.y-.075],[p.x+.038,p.y-.102],
     [p.x+.005,p.y-.065],[p.x-.025,p.y-.046]]))c='hair';
   if(ring(x,y,p.x+.021,p.y+.035,.045,.022,.09,.08)&&y>p.y+.035)c='hair';
 }
 return c;
}
function crc(bytes){let c=0xffffffff;for(const b of bytes){
 c^=b;for(let j=0;j<8;j++)c=c&1?0xedb88320^(c>>>1):c>>>1;
}return (c^0xffffffff)>>>0;}
function chunk(type,data){const t=Buffer.from(type);const len=Buffer.alloc(4);
 len.writeUInt32BE(data.length);const sum=Buffer.alloc(4);
 sum.writeUInt32BE(crc(Buffer.concat([t,data])));
 return Buffer.concat([len,t,data,sum]);}
function generate(file){
 const scan=Buffer.alloc(H*(W*4+1));let off=0;
 for(let iy=0;iy<H;iy++){
   scan[off++]=0;
   for(let ix=0;ix<W;ix++){
     const tess=8,tx=Math.floor(ix/tess),ty=Math.floor(iy/tess);
     const xx=(tx+.5)/Math.floor(W/tess),yy=(ty+.5)/Math.floor(H/tess);
     let [r,g,b]=palette[pick(xx,yy)];
     const mortar=(ix%tess===0||iy%tess===0);
     if(mortar)[r,g,b]=palette.grout;
     else{
       const n=((tx*1103515245+ty*12345)>>>0)%21-10;
       r=Math.max(0,Math.min(255,r+n));g=Math.max(0,Math.min(255,g+n));
       b=Math.max(0,Math.min(255,b+n));
     }
     scan[off++]=r;scan[off++]=g;scan[off++]=b;scan[off++]=255;
   }
 }
 const sig=Buffer.from([137,80,78,71,13,10,26,10]);
 const ih=Buffer.alloc(13);ih.writeUInt32BE(W,0);ih.writeUInt32BE(H,4);ih[8]=8;ih[9]=6;
 const png=Buffer.concat([sig,chunk('IHDR',ih),chunk('IDAT',zlib.deflateSync(scan,{level:9})),chunk('IEND',Buffer.alloc(0))]);
 fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,png);
 return {file,width:W,height:H,size:png.length};
}
if(require.main===module)console.log(generate(process.argv[2]||path.join(__dirname,'textures','cse_parvis_mosaique.png')));
module.exports={generate};
