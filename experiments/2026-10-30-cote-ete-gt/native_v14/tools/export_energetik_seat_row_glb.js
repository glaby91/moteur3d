'use strict';
// Dependency-free static GLB exporter for the reusable Energetik cinema row.
const fs=require('fs'),path=require('path');
const buildSeat=require('../assets/pripyat/energetik_cinema_seat.js');
const root=path.resolve(__dirname,'..');
const mats=['seatVinylBrown','seatSupportPaint','seatSteel','seatWood'];
const geom=Object.fromEntries(mats.map(m=>[m,{p:[],n:[],uv:[],i:[]}])) ;
const vadd=(a,b)=>a.map((v,k)=>v+b[k]),vsub=(a,b)=>a.map((v,k)=>v-b[k]),vscale=(a,s)=>a.map(v=>v*s);
const dot=(a,b)=>a.reduce((s,v,k)=>s+v*b[k],0),cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
const norm=a=>{const l=Math.hypot(...a)||1;return a.map(v=>v/l)};
function face(mesh,verts,normals){
  const base=mesh.p.length/3;
  for(let k=0;k<4;k++){const p=verts[k],n=normals[k];mesh.p.push(...p);mesh.n.push(...n);mesh.uv.push(p[0]*2,p[2]*2);}
  for(const tri of [[0,1,2],[0,2,3]]){let [a,b,c]=tri;const q=cross(vsub(verts[b],verts[a]),vsub(verts[c],verts[a]));if(dot(q,normals[0])<0)[b,c]=[c,b];mesh.i.push(base+a,base+b,base+c);}
}
function roundedBox(mesh,o){
  const mn=[o.x0,o.y0,o.z0],mx=[o.x1,o.y1,o.z1];let r=Math.max(0,Math.min(o.r||0,(mx[0]-mn[0])/2,(mx[1]-mn[1])/2,(mx[2]-mn[2])/2));
  const fs=[{a:0,s:1,u:2,v:1},{a:0,s:-1,u:1,v:2},{a:1,s:1,u:0,v:2},{a:1,s:-1,u:2,v:0},{a:2,s:1,u:1,v:0},{a:2,s:-1,u:0,v:1}];
  for(const f of fs){const su=Math.max(1,Math.min(24,Math.ceil((mx[f.u]-mn[f.u])*8))),sv=Math.max(1,Math.min(24,Math.ceil((mx[f.v]-mn[f.v])*8))),base=mesh.p.length/3;
    for(let j=0;j<=sv;j++)for(let i=0;i<=su;i++){
      const p=[0,0,0];p[f.a]=f.s>0?mx[f.a]:mn[f.a];p[f.u]=mn[f.u]+(mx[f.u]-mn[f.u])*i/su;p[f.v]=mn[f.v]+(mx[f.v]-mn[f.v])*j/sv;
      let n;if(r){const q=p.map((v,k)=>Math.max(mn[k]+r,Math.min(mx[k]-r,v))),d=vsub(p,q),l=Math.hypot(...d);if(l>1e-9){p.splice(0,3,...vadd(q,vscale(d,r/l)));n=norm(d);}}
      if(!n){n=[0,0,0];n[f.a]=f.s;}mesh.p.push(...p);mesh.n.push(...n);mesh.uv.push(p[0]*2,p[2]*2);
    }
    for(let j=0;j<sv;j++)for(let i=0;i<su;i++){const a=base+j*(su+1)+i,b=a+1,c=a+su+1,d=c+1;for(const tri of [[a,b,c],[b,d,c]]){let [u,v,w]=tri;const nn=vscale(vadd(vadd(mesh.n.slice(u*3,u*3+3),mesh.n.slice(v*3,v*3+3)),mesh.n.slice(w*3,w*3+3)),1/3);if(dot(cross(vsub(mesh.p.slice(v*3,v*3+3),mesh.p.slice(u*3,u*3+3)),vsub(mesh.p.slice(w*3,w*3+3),mesh.p.slice(u*3,u*3+3))),nn)<0)[v,w]=[w,v];mesh.i.push(u,v,w);}}
  }
}
function ellipsoid(mesh,o){
  const lon=20,lat=12,base=mesh.p.length/3;
  for(let j=0;j<=lat;j++){const t=Math.PI*j/lat;for(let i=0;i<=lon;i++){const p=2*Math.PI*i/lon;const n=[Math.sin(t)*Math.cos(p),Math.cos(t),Math.sin(t)*Math.sin(p)];const q=[o.cx+o.rx*n[0],o.cy+o.ry*n[1],o.cz+o.rz*n[2]];mesh.p.push(...q);mesh.n.push(...norm([n[0]/o.rx,n[1]/o.ry,n[2]/o.rz]));mesh.uv.push(i/lon,j/lat);}}
  for(let j=0;j<lat;j++)for(let i=0;i<lon;i++){const a=base+j*(lon+1)+i,b=a+1,c=a+lon+1,d=c+1;mesh.i.push(a,b,c,b,d,c);}
}
function capsule(mesh,o){
  const a=o.a,b=o.b,r=o.r,axis=norm(vsub(b,a)),ref=Math.abs(axis[1])<.9?[0,1,0]:[1,0,0],u=norm(cross(axis,ref)),v=norm(cross(axis,u));const rings=[];const half=8;
  for(let k=0;k<=half;k++){const t=(Math.PI/2)*k/half;rings.push([-r*Math.cos(t),r*Math.sin(t),-Math.cos(t),Math.sin(t)]);}
  for(let k=1;k<=half;k++){const t=(Math.PI/2)*k/half;rings.push([o.len+r*Math.sin(t),r*Math.cos(t),Math.sin(t),Math.cos(t)]);}
  const base=mesh.p.length/3,around=20;
  for(const [along,radial,na,nr] of rings)for(let j=0;j<=around;j++){const t=2*Math.PI*j/around,c=Math.cos(t),s=Math.sin(t),off=vadd(vscale(u,radial*c),vscale(v,radial*s)),p=vadd(vadd(a,vscale(axis,along)),off),n=norm(vadd(vscale(axis,na),vscale(vadd(vscale(u,c),vscale(v,s)),nr)));mesh.p.push(...p);mesh.n.push(...n);mesh.uv.push(j/around,along/(o.len+2*r));}
  for(let k=0;k<rings.length-1;k++)for(let j=0;j<around;j++){const a0=base+k*(around+1)+j,b0=a0+1,c0=a0+around+1,d0=c0+1;mesh.i.push(a0,b0,c0,b0,d0,c0);}
}
const objects=[];
const api={objects,
  box:(x0,y0,z0,x1,y1,z1,mat)=>objects.push({kind:'box',x0,y0,z0,x1,y1,z1,r:0,mat}),
  roundBox:(x0,y0,z0,x1,y1,z1,r,mat)=>objects.push({kind:'box',x0,y0,z0,x1,y1,z1,r,mat}),
  ellipse:(cx,cy,cz,rx,ry,rz,mat)=>objects.push({kind:'ellipse',cx,cy,cz,rx,ry,rz,mat}),
  capsule:(a,b,r,mat)=>objects.push({kind:'capsule',a,b,r,len:Math.hypot(...vsub(b,a)),mat})
};
for(const [x,opts] of [[-.82,{variantId:'left_missing_pan',seatPanMissing:true}],[0,{variantId:'center_raised_pan',folded:true}],[.82,{variantId:'right_lowered_pan'}]])buildSeat(api,x,0,0,{...opts,omitSharedFrame:true,sideStyle:'padded'});
api.capsule([-1.235,.405,.025],[1.235,.405,.025],.027,'seatSupportPaint');
for(const x of [-1.235,-.41,.41,1.235])api.capsule([x,.405,.025],[x,.485,.025],.019,'seatSteel');
api.capsule([0,.405,.025],[0,.025,.025],.031,'seatSupportPaint');
for(const o of objects){const m=geom[o.mat]||geom.seatWood;if(o.kind==='ellipse')ellipsoid(m,o);else if(o.kind==='capsule')capsule(m,o);else roundedBox(m,o);}

const binParts=[],bufferViews=[],accessors=[];let offset=0;
function append(buf,target){const pad=(4-offset%4)%4;if(pad){binParts.push(Buffer.alloc(pad));offset+=pad;}const start=offset;binParts.push(buf);offset+=buf.length;const idx=bufferViews.push({buffer:0,byteOffset:start,byteLength:buf.length,...(target?{target}:{})})-1;return idx;}
function addFloats(arr,type,target,min,max){const b=Buffer.alloc(arr.length*4);arr.forEach((v,i)=>b.writeFloatLE(v,i*4));const view=append(b,target);const count=arr.length/(type==='VEC3'?3:type==='VEC2'?2:1);accessors.push({bufferView:view,componentType:5126,type,count,...(min?{min}:{}),...(max?{max}:{})});return accessors.length-1;}
const images=[],textures=[],materials=[],nodes=[],meshes=[];
const texFiles={seatVinylBrown:'seat_vinyl',seatSupportPaint:'seat_support_paint',seatSteel:'seat_steel',seatWood:'seat_wood'};
const display={seatVinylBrown:'Energetik worn brown vinyl',seatSupportPaint:'Painted steel with sparse corrosion',seatSteel:'Dark steel hardware',seatWood:'Aged wood trim'};
for(const [mi,mat] of mats.entries()){
  const g=geom[mat];if(!g.i.length)continue;
  const baseIndex=accessors.length;
  const min=[0,0,0],max=[0,0,0];for(let i=0;i<g.p.length;i+=3)for(let k=0;k<3;k++){if(i===0){min[k]=g.p[i+k];max[k]=g.p[i+k];}else{min[k]=Math.min(min[k],g.p[i+k]);max[k]=Math.max(max[k],g.p[i+k]);}}
  const pa=addFloats(g.p,'VEC3',34962,min,max),na=addFloats(g.n,'VEC3',34962),ua=addFloats(g.uv,'VEC2',34962);
  const ib=Buffer.alloc(g.i.length*4);g.i.forEach((v,i)=>ib.writeUInt32LE(v,i*4));const iv=append(ib,34963);accessors.push({bufferView:iv,componentType:5125,type:'SCALAR',count:g.i.length,min:[0],max:[g.p.length/3-1]});const ia=accessors.length-1;
  const prefix=texFiles[mat],imageIndices=[];
  for(const suffix of ['basecolor','normal','metallic_roughness']){const file=path.join(root,'assets/pripyat/textures/seats',`${prefix}_${suffix}.png`);const bytes=fs.readFileSync(file);const bv=append(bytes);imageIndices.push(images.length);images.push({bufferView:bv,mimeType:'image/png'});textures.push({source:images.length-1,sampler:0});}
  const rough={seatVinylBrown:.59,seatSupportPaint:.69,seatSteel:.44,seatWood:.74}[mat];const metal=mat==='seatSteel'?.72:0;
  materials.push({name:display[mat],pbrMetallicRoughness:{baseColorTexture:{index:imageIndices[0]},metallicRoughnessTexture:{index:imageIndices[2]},metallicFactor:metal,roughnessFactor:rough},normalTexture:{index:imageIndices[1],scale:.65},doubleSided:false,extras:{catalogMaterial:mat,textureSource:'procedurally authored, non-photographic',units:'metres'}});
  meshes.push({name:display[mat],primitives:[{attributes:{POSITION:pa,NORMAL:na,TEXCOORD_0:ua},indices:ia,material:materials.length-1,mode:4}]});nodes.push({name:display[mat],mesh:meshes.length-1});
}
const embedded=Buffer.concat(binParts),imageJson=images;let json=Buffer.from(JSON.stringify({asset:{version:'2.0',generator:'Moteur3D Energetik Seat Asset Pipeline'},scene:0,scenes:[{nodes:nodes.map((_,i)=>i)}],nodes,meshes,materials,textures,images:imageJson,samplers:[{wrapS:10497,wrapT:10497,magFilter:9729,minFilter:9987}],buffers:[{byteLength:embedded.length}],bufferViews,accessors}));
const align=b=>Buffer.concat([b,Buffer.alloc((4-b.length%4)%4)]);json=align(json);const bin=align(embedded),total=12+8+json.length+8+bin.length,glb=Buffer.alloc(total);glb.write('glTF',0);glb.writeUInt32LE(2,4);glb.writeUInt32LE(total,8);let p=12;glb.writeUInt32LE(json.length,p);glb.writeUInt32LE(0x4e4f534a,p+4);json.copy(glb,p+8);p+=8+json.length;glb.writeUInt32LE(bin.length,p);glb.writeUInt32LE(0x004e4942,p+4);bin.copy(glb,p+8);
const out=path.join(root,'assets/pripyat/energetik_cinema_seat_row.glb');fs.writeFileSync(out,glb);console.log(`${out}: ${objects.length} authored primitives, ${materials.length} materials, ${(glb.length/1048576).toFixed(2)} MiB`);
