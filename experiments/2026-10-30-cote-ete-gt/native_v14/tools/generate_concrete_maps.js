'use strict';
const fs = require('fs');
const path = require('path');
const sharp = require('sharp');
const zlib = require('zlib');
const root = path.resolve(__dirname, '..');
const tex = path.join(root, 'assets/pripyat/textures');
const source = path.join(tex, 'prp_concrete_panel_source.jpg');
const SIZE = 512;
const pack565 = (r,g,b) => ((Math.round(r) >> 3) << 11) | ((Math.round(g) >> 2) << 5) | (Math.round(b) >> 3);
async function main() {
  const {data: color, info} = await sharp(source).resize(SIZE,SIZE,{fit:'cover'}).removeAlpha().raw().toBuffer({resolveWithObject:true});
  const gray = new Float32Array(SIZE*SIZE), normal = Buffer.alloc(SIZE*SIZE*3), rough = Buffer.alloc(SIZE*SIZE*3);
  for (let i=0;i<SIZE*SIZE;i++) gray[i]=(color[i*3]*.2126+color[i*3+1]*.7152+color[i*3+2]*.0722)/255;
  const at=(x,y)=>gray[((y+SIZE)%SIZE)*SIZE+((x+SIZE)%SIZE)];
  for(let y=0;y<SIZE;y++) for(let x=0;x<SIZE;x++) {
    const i=y*SIZE+x, dx=(at(x+1,y)-at(x-1,y))*0.65, dy=(at(x,y+1)-at(x,y-1))*0.65;
    const nx=-dx/Math.hypot(dx,dy,1), ny=-dy/Math.hypot(dx,dy,1), nz=1/Math.hypot(dx,dy,1);
    normal[i*3]=Math.round(128+nx*127); normal[i*3+1]=Math.round(128+ny*127); normal[i*3+2]=Math.round(128+nz*127);
    const variation=(gray[i]-.5)*14, r=Math.max(0,Math.min(255,Math.round(222+variation)));
    rough[i*3]=rough[i*3+1]=rough[i*3+2]=r;
  }
  const metallicRoughness=Buffer.alloc(SIZE*SIZE*3);
  for(let i=0;i<SIZE*SIZE;i++) metallicRoughness[i*3+1]=rough[i*3];
  const out = {basecolor:color,normal,roughness:rough,metallic_roughness:metallicRoughness};
  for(const [name,raw] of Object.entries(out)) await sharp(raw,{raw:{width:SIZE,height:SIZE,channels:3}}).png().toFile(path.join(tex,`prp_concrete_panel_${name}.png`));
  // Native M3DPBR1: RGB565 color, tangent normal and scalar roughness mip chains.
  const levels=[];
  let prev={color,normal,roughness:rough,size:SIZE};
  while(true){levels.push(prev);if(prev.size===8)break;const size=prev.size>>1,next={size};
    for(const name of ['color','normal','roughness']) { const src=prev[name], dst=Buffer.alloc(size*size*3); for(let y=0;y<size;y++)for(let x=0;x<size;x++)for(let c=0;c<3;c++){const j=((y*2)*prev.size+x*2)*3+c;dst[(y*size+x)*3+c]=Math.round((src[j]+src[j+3]+src[j+prev.size*3]+src[j+prev.size*3+3])/4)} next[name]=dst; }
    prev=next;
  }
  const chunks=[Buffer.from('M3DPBR1\0','ascii')], u32=n=>{const b=Buffer.alloc(4);b.writeUInt32LE(n);return b};
  chunks.push(u32(SIZE),u32(levels.length));
  for(const level of levels){chunks.push(u32(level.size));for(const key of ['color','normal','roughness']){const raw=level[key], packed=Buffer.alloc(level.size*level.size*2);for(let i=0;i<raw.length/3;i++)packed.writeUInt16LE(pack565(raw[i*3],raw[i*3+1],raw[i*3+2]),i*2);chunks.push(packed)}}
  fs.writeFileSync(path.join(tex,'prp_concrete_panel.m3dpbr.gz'),zlib.gzipSync(Buffer.concat(chunks),{level:9}));
  console.log(`Generated ${SIZE}px albedo/normal/roughness and native mip chain (${info.width}x${info.height} source).`);
}
main().catch(e=>{console.error(e);process.exit(1)});
