'use strict';
// Dependency-free GLB 2.0 writer for the shared façade-bay module.
const fs=require('fs'),path=require('path');
const {buildEnergetikFacadeBay}=require('../assets/pripyat/energetik_facade_bay');
const root=path.resolve(__dirname,'..'),out=path.join(root,'build');fs.mkdirSync(out,{recursive:true});
const boxes=[];
const api={objects:boxes,roundBox:(x0,y0,z0,x1,y1,z1,r,mat)=>boxes.push({x0,y0,z0,x1,y1,z1,r,mat})};
const meta=buildEnergetikFacadeBay(api);
const pos=[],norm=[],uv=[],indices=[];
const addRoundedBox=o=>{
  const min=[o.x0,o.y0,o.z0],max=[o.x1,o.y1,o.z1],r=Math.max(0,Math.min(o.r,(o.x1-o.x0)/2,(o.y1-o.y0)/2,(o.z1-o.z0)/2));
  const faces=[
    {axis:0,sign:1,u:2,v:1},{axis:0,sign:-1,u:1,v:2},
    {axis:1,sign:1,u:0,v:2},{axis:1,sign:-1,u:2,v:0},
    {axis:2,sign:1,u:1,v:0},{axis:2,sign:-1,u:0,v:1}
  ];
  for(const f of faces){const u0=min[f.u],u1=max[f.u],v0=min[f.v],v1=max[f.v];const su=Math.max(1,Math.ceil((u1-u0)*3)),sv=Math.max(1,Math.ceil((v1-v0)*3));
    const base=pos.length/3;
    for(let j=0;j<=sv;j++)for(let i=0;i<=su;i++){
      const p=[0,0,0];p[f.axis]=f.sign>0?max[f.axis]:min[f.axis];p[f.u]=u0+(u1-u0)*i/su;p[f.v]=v0+(v1-v0)*j/sv;
      if(r>0){const q=p.map((v,k)=>Math.max(min[k]+r,Math.min(max[k]-r,v))),d=p.map((v,k)=>v-q[k]),len=Math.hypot(...d);if(len>1e-8)for(let k=0;k<3;k++)p[k]=q[k]+d[k]*r/len;}
      const q=p.map((v,k)=>Math.max(min[k]+r,Math.min(max[k]-r,v))),d=p.map((v,k)=>v-q[k]),len=Math.hypot(...d)||1;
      let n=d.map(v=>v/len);if(len===1&&Math.hypot(...d)<1e-8){n=[0,0,0];n[f.axis]=f.sign}
      pos.push(...p);norm.push(...n);uv.push(p[f.u]/2,p[f.v]/2);
    }
    for(let j=0;j<sv;j++)for(let i=0;i<su;i++){const a=base+j*(su+1)+i,b=a+1,c=a+su+1,d=c+1;indices.push(a,d,b,a,c,d);}
  }
};
boxes.forEach(addRoundedBox);
const binParts=[],bufferViews=[],accessors=[];let offset=0;
function append(buf,target){const pad=(4-offset%4)%4;if(pad){binParts.push(Buffer.alloc(pad));offset+=pad}const start=offset;binParts.push(buf);offset+=buf.length;const idx=bufferViews.push({buffer:0,byteOffset:start,byteLength:buf.length,...(target?{target}:{})})-1;return idx}
function addAccessor(arr,componentType,type,target,min,max){const b=Buffer.alloc(arr.length*(componentType===5125?4:4));for(let i=0;i<arr.length;i++)b.writeFloatLE(arr[i],i*4);const view=append(b,target);const count=arr.length/(type==='VEC3'?3:type==='VEC2'?2:1);accessors.push({bufferView:view,componentType,type,count,...(min?{min}:{}),...(max?{max}:{})});return accessors.length-1}
const pmin=[0,0,0],pmax=[0,0,0];for(let i=0;i<pos.length;i+=3)for(let k=0;k<3;k++){pmin[k]=Math.min(pmin[k],pos[i+k]);pmax[k]=Math.max(pmax[k],pos[i+k]);}
const pa=addAccessor(pos,5126,'VEC3',34962,pmin,pmax),na=addAccessor(norm,5126,'VEC3',34962),ua=addAccessor(uv,5126,'VEC2',34962);
const ib=Buffer.alloc(indices.length*4);indices.forEach((v,i)=>ib.writeUInt32LE(v,i*4));const iv=append(ib,34963);accessors.push({bufferView:iv,componentType:5125,type:'SCALAR',count:indices.length,min:[0],max:[pos.length/3-1]});const ia=accessors.length-1;
const images=['prp_concrete_panel_basecolor.png','prp_concrete_panel_normal.png','prp_concrete_panel_metallic_roughness.png'].map(n=>fs.readFileSync(path.join(root,'assets/pripyat/textures',n)));
const imageViews=images.map(b=>append(b));
const embedded=Buffer.concat(binParts),imageJson=imageViews.map((bufferView)=>({bufferView,mimeType:'image/png'}));
const gltf={asset:{version:'2.0',generator:'Moteur3D Energetik façade asset pipeline'},scene:0,scenes:[{nodes:[0]}],nodes:[{name:'Energetik_Facade_Bay',mesh:0,extras:{assetId:meta.assetId,units:'metres',dimensions:{width:meta.width,height:meta.height,depth:meta.depth},catalogMaterial:meta.materialId,reference:'Energetik photo mapping; geometry authored from dimensions/architectural references'}}],meshes:[{name:'Facade_Bay',primitives:[{attributes:{POSITION:pa,NORMAL:na,TEXCOORD_0:ua},indices:ia,material:0,mode:4}]}],materials:[{name:'prp_concrete_panel',pbrMetallicRoughness:{baseColorTexture:{index:0},metallicRoughnessTexture:{index:2},metallicFactor:0,roughnessFactor:.86},normalTexture:{index:1,scale:.55},doubleSided:false,extras:{catalogId:'prp_concrete_panel',metallic:0,roughnessTarget:[.78,.94],textureLicence:'procedurally generated in project; no documentary photo used as texture'}}],textures:[{source:0,sampler:0},{source:1,sampler:0},{source:2,sampler:0}],images:imageJson,samplers:[{wrapS:10497,wrapT:10497,magFilter:9729,minFilter:9987}],buffers:[{byteLength:embedded.length}],bufferViews,accessors};
let json=Buffer.from(JSON.stringify(gltf));const align=b=>Buffer.concat([b,Buffer.alloc((4-b.length%4)%4)]);json=align(json);const bin=align(embedded);const total=12+8+json.length+8+bin.length;const glb=Buffer.alloc(total);glb.write('glTF',0);glb.writeUInt32LE(2,4);glb.writeUInt32LE(total,8);let o=12;glb.writeUInt32LE(json.length,o);glb.writeUInt32LE(0x4e4f534a,o+4);json.copy(glb,o+8);o+=8+json.length;glb.writeUInt32LE(bin.length,o);glb.writeUInt32LE(0x004e4942,o+4);bin.copy(glb,o+8);const file=process.argv[2]?path.resolve(process.argv[2]):path.join(root,'assets/pripyat/energetik_facade_bay.glb');fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,glb);console.log(`${file}: ${boxes.length} primitives, ${indices.length/3} triangles, ${(glb.length/1048576).toFixed(2)} MiB`);
