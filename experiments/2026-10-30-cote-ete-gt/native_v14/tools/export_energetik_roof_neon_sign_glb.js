'use strict';
// Exports the same native sign module as a segmented, metre-scale glTF asset.
const fs=require('fs'),path=require('path');
const {buildEnergetikRoofNeonSign}=require('../assets/pripyat/energetik_roof_neon_sign.js');
const root=path.resolve(__dirname,'..'),objects=[];
const api={objects,
  box:(x0,y0,z0,x1,y1,z1,mat)=>objects.push({kind:'box',x0,y0,z0,x1,y1,z1,mat}),
  roundBox:(x0,y0,z0,x1,y1,z1,r,mat)=>objects.push({kind:'box',x0,y0,z0,x1,y1,z1,r,mat}),
  capsule:(a,b,r,mat)=>objects.push({kind:'capsule',a,b,r,mat})};
const manifest=buildEnergetikRoofNeonSign(api,0,0,0,{sectionLength:.28});
const positions=[],normals=[],indices=[],nodes=[],meshes=[];
const materials=[
 {name:'Weathered rusted steel',pbrMetallicRoughness:{baseColorFactor:[.31,.16,.09,1],metallicFactor:.67,roughnessFactor:.83}},
 {name:'Painted and bare steel',pbrMetallicRoughness:{baseColorFactor:[.19,.22,.23,1],metallicFactor:.78,roughnessFactor:.54}},
 {name:'Ceramic insulator',pbrMetallicRoughness:{baseColorFactor:[.54,.49,.37,1],metallicFactor:.05,roughnessFactor:.39}},
 {name:'Neon tube glass',pbrMetallicRoughness:{baseColorFactor:[.25,.83,1,.28],metallicFactor:0,roughnessFactor:.12},alphaMode:'BLEND',doubleSided:true},
 {name:'Cyan neon core',pbrMetallicRoughness:{baseColorFactor:[.16,.78,1,1],metallicFactor:0,roughnessFactor:.18},emissiveFactor:[.1,.58,.9],extensions:{KHR_materials_emissive_strength:{emissiveStrength:3.5}}}
];
const matId={rust:0,metal:1,stone:2,glass:3,neon:4};
function pushTri(p,n){const base=positions.length/3;positions.push(...p);for(let i=0;i<3;i++)normals.push(...n);indices.push(base,base+1,base+2);}
function quad(a,b,c,d,n){pushTri([...a,...b,...c],n);pushTri([...a,...c,...d],n);}
function box(o){const {x0:a,y0:b,z0:c,x1:d,y1:e,z1:f}=o;
 const p=[ [a,b,c],[d,b,c],[d,e,c],[a,e,c],[a,b,f],[d,b,f],[d,e,f],[a,e,f] ];
 for(const [q,n] of [[ [0,3,2,1],[0,0,-1] ],[[4,5,6,7],[0,0,1]],[[0,4,7,3],[-1,0,0]],[[1,2,6,5],[1,0,0]],[[0,1,5,4],[0,-1,0]],[[3,7,6,2],[0,1,0]]])quad(...q.map(i=>p[i]),n);
}
function cylinder(o){const a=o.a,b=o.b,r=o.r,axis=b.map((v,i)=>v-a[i]),len=Math.hypot(...axis)||1,u=axis.map(v=>v/len),ref=Math.abs(u[1])<.9?[0,1,0]:[1,0,0];let v=[u[1]*ref[2]-u[2]*ref[1],u[2]*ref[0]-u[0]*ref[2],u[0]*ref[1]-u[1]*ref[0]];const vl=Math.hypot(...v)||1;v=v.map(x=>x/vl);const w=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]],N=10;
 for(let j=0;j<N;j++){const t=j*2*Math.PI/N,s=(j+1)*2*Math.PI/N;const pt=(q,k)=>[q[0]+r*(Math.cos(k)*v[0]+Math.sin(k)*w[0]),q[1]+r*(Math.cos(k)*v[1]+Math.sin(k)*w[1]),q[2]+r*(Math.cos(k)*v[2]+Math.sin(k)*w[2])];const n1=[Math.cos(t)*v[0]+Math.sin(t)*w[0],Math.cos(t)*v[1]+Math.sin(t)*w[1],Math.cos(t)*v[2]+Math.sin(t)*w[2]],n2=[Math.cos(s)*v[0]+Math.sin(s)*w[0],Math.cos(s)*v[1]+Math.sin(s)*w[1],Math.cos(s)*v[2]+Math.sin(s)*w[2]];const A=pt(a,t),B=pt(a,s),C=pt(b,s),D=pt(b,t);const base=positions.length/3;positions.push(...A,...B,...C,...D);normals.push(...n1,...n2,...n2,...n1);indices.push(base,base+1,base+2,base,base+2,base+3);}
}
for(let i=0;i<objects.length;i++){const o=objects[i],start=indices.length,vstart=positions.length/3; if(o.kind==='capsule')cylinder(o);else box(o);if(indices.length===start)continue;meshes.push({name:o.assetPartId||`support_part_${i+1}`,range:{vstart,vcount:positions.length/3-vstart,istart:start,icount:indices.length-start,material:matId[o.mat]??1}});nodes.push({name:o.assetPartId||`support_part_${i+1}`,mesh:meshes.length-1,extras:{assetPartId:o.assetPartId||`support_part_${i+1}`,materialId:o.mat,segmentedNeon:o.mat==='neon'||o.mat==='glass'}});}
const aligned=[];let offset=0;function append(buf,target){const pad=(4-offset%4)%4;if(pad){aligned.push(Buffer.alloc(pad));offset+=pad;}const at=offset;aligned.push(buf);offset+=buf.length;return {bufferView:bufferViews.push({buffer:0,byteOffset:at,byteLength:buf.length,...(target?{target}:{})})-1,byteOffset:at};}const bufferViews=[],accessors=[];
const pbuf=Buffer.alloc(positions.length*4);positions.forEach((v,i)=>pbuf.writeFloatLE(v,i*4));const pv=append(pbuf,34962).bufferView;
const nbuf=Buffer.alloc(normals.length*4);normals.forEach((v,i)=>nbuf.writeFloatLE(v,i*4));const nv=append(nbuf,34962).bufferView;
for(const m of meshes){const ix=indices.slice(m.range.istart,m.range.istart+m.range.icount).map(v=>v-m.range.vstart),ibuf=Buffer.alloc(ix.length*4);ix.forEach((v,i)=>ibuf.writeUInt32LE(v,i*4));const iv=append(ibuf,34963).bufferView;const pi=accessors.push({bufferView:pv,byteOffset:m.range.vstart*12,componentType:5126,type:'VEC3',count:m.range.vcount})-1;const ni=accessors.push({bufferView:nv,byteOffset:m.range.vstart*12,componentType:5126,type:'VEC3',count:m.range.vcount})-1;const ii=accessors.push({bufferView:iv,componentType:5125,type:'SCALAR',count:ix.length})-1;m.primitives=[{attributes:{POSITION:pi,NORMAL:ni},indices:ii,material:m.range.material,mode:4}];delete m.range;}
const bin=Buffer.concat(aligned),jsonObj={asset:{version:'2.0',generator:'Moteur3D Energetik segmented neon exporter'},extensionsUsed:['KHR_materials_emissive_strength'],extensionsRequired:['KHR_materials_emissive_strength'],scene:0,scenes:[{nodes:nodes.map((_,i)=>i)}],nodes,meshes,materials,buffers:[{byteLength:bin.length}],bufferViews,accessors};
let json=Buffer.from(JSON.stringify(jsonObj));json=Buffer.concat([json,Buffer.alloc((4-json.length%4)%4,0x20)]);const binary=Buffer.concat([bin,Buffer.alloc((4-bin.length%4)%4)]),total=12+8+json.length+8+binary.length,glb=Buffer.alloc(total);glb.write('glTF');glb.writeUInt32LE(2,4);glb.writeUInt32LE(total,8);glb.writeUInt32LE(json.length,12);glb.writeUInt32LE(0x4e4f534a,16);json.copy(glb,20);let at=20+json.length;glb.writeUInt32LE(binary.length,at);glb.writeUInt32LE(0x004e4942,at+4);binary.copy(glb,at+8);
const glbPath=path.join(root,'assets/pripyat/energetik_roof_neon_sign.glb'),jsonPath=path.join(root,'assets/pripyat/energetik_roof_neon_sign.asset.json');fs.writeFileSync(glbPath,glb);manifest.export={format:'glTF 2.0 GLB',path:'energetik_roof_neon_sign.glb',units:'metres',independentSceneNodes:nodes.length,neonSegments:objects.filter(o=>o.mat==='neon').length,glassSegments:objects.filter(o=>o.mat==='glass'&&o.assetPartId).length,emissiveExtension:'KHR_materials_emissive_strength',source:'energetik_roof_neon_sign.js'};fs.writeFileSync(jsonPath,JSON.stringify(manifest,null,2)+'\n');console.log(`${glbPath}: ${nodes.length} separate nodes, ${manifest.export.neonSegments} emissive sections, ${(glb.length/1048576).toFixed(2)} MiB`);
