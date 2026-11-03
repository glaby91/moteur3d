// Build-time exporter for the scene created by the real V13.14 engine.
// The generated binary contains the engine's actual primitives, smooth vertex
// normals, UVs, SAH BVH, and embedded RGB565 color/normal/roughness mip chains.
const fs = require('fs');
const path = require('path');
const { addGlbCar, readRgb565MipChain } = require('./gltf_scene_import');

function readPbrMipChain(filePath) {
  const b = fs.readFileSync(filePath);
  if (b.toString('ascii', 0, 8) !== 'M3DPBR1\0') throw new Error(`Matériau PBR invalide: ${filePath}`);
  const baseSize = b.readUInt32LE(8), count = b.readUInt32LE(12);
  if (baseSize !== 512 || count !== 7) throw new Error(`Pyramide PBR attendue en 512/7 niveaux: ${filePath}`);
  let offset = 16;
  const maps = { color: [], normal: [], roughness: [] };
  for (let level = 0; level < count; level++) {
    const size = b.readUInt32LE(offset); offset += 4;
    if (size !== (baseSize >> level)) throw new Error(`Niveau PBR incohérent: ${filePath}`);
    const n = size * size, bytes = n * 2;
    for (const channel of ['color', 'normal', 'roughness']) {
      const data = new Uint16Array(n);
      for (let i = 0; i < n; i++) data[i] = b.readUInt16LE(offset + i * 2);
      maps[channel].push({ size, data });
      offset += bytes;
    }
  }
  if (offset !== b.length) throw new Error(`Données en trop dans le matériau PBR: ${filePath}`);
  return maps;
}

const htmlPath = process.argv[2] || path.join(__dirname, 'source_v1314.html');
const outPath = process.argv[3] || path.join(__dirname, 'moteur3d_v1314_scene.bin');
const scenePath = process.argv[4] || path.join(__dirname, 'scene_coastal_v1314.js');
const html = fs.readFileSync(htmlPath, 'utf8');
const sceneSource = fs.readFileSync(scenePath, 'utf8');
if (!sceneSource.includes('function makeScene')) throw new Error('La fonction makeScene() est absente du fichier de scène.');
const match = html.match(/<script>([\s\S]*?)<\/script>/);
if (!match) throw new Error('Script principal absent de V13.14.');
const script = match[1];
const end = script.indexOf('function hitBoundsNear(');
if (end < 0) throw new Error('Le constructeur BVH V13.14 est introuvable.');
const materialStart = script.indexOf('function hash2(');
const materialEnd = script.indexOf('function screenRect(');
if (materialStart < 0 || materialEnd < 0) throw new Error('Les fonctions texture V13.14 sont introuvables.');

// Run the source script in the main V8 context. The isolated vm context is
// dramatically slower for this 1.25M-object scene, especially while building
// its SAH BVH. Only the DOM canvas stub is supplied; no browser is involved.
globalThis.document = { getElementById: () => ({ getContext: () => ({}) }) };
globalThis.__m3dAppendClassicCar = (objects, placement) => {
  const file = path.resolve(__dirname, placement.source);
  globalThis.__m3dImportedCarTriangles = addGlbCar(objects, file, placement);
};
const optionalCar = process.env.M3D_SKIP_IMPORTED_CAR === '1' ? '' : 'globalThis.__m3dAppendClassicCar(objects, M3D_IMPORTED_CAR);';
eval(`${script.slice(0, end)}\n${script.slice(materialStart, materialEnd)}\n${sceneSource}\nmakeScene(); ${optionalCar} buildBVH(); const __mats = [...new Set(objects.map(o => o.mat))]; for (const id of __mats) materials.set(id, makeMips(id, 512)); globalThis.__scene = { objects, bvh, bvhRoot, sun, skyZenith, skyHorizon, pointLights, materials, mats: __mats, reflectivityPercent: M3D_MATERIAL_REFLECTIVITY_PERCENT, transmissionPercent: typeof M3D_MATERIAL_TRANSMISSION_PERCENT === "undefined" ? {} : M3D_MATERIAL_TRANSMISSION_PERCENT, refractiveIndex: typeof M3D_MATERIAL_IOR === "undefined" ? {} : M3D_MATERIAL_IOR, sceneLights: typeof M3D_SCENE_LIGHTS === "undefined" ? [] : M3D_SCENE_LIGHTS };`);
const scene = globalThis.__scene;
const objects = scene.objects;
const bvh = scene.bvh;
const bvhStats = { leaves: 0, trianglePairCandidates: 0, maxLeaf: 0 };
for (const node of bvh) {
  if (node.ids) { bvhStats.leaves++; bvhStats.maxLeaf = Math.max(bvhStats.maxLeaf, node.ids.length); continue; }
  const left = bvh[node.left], right = bvh[node.right];
  if (!left?.ids || !right?.ids || left.ids.length + right.ids.length !== 8) continue;
  if ([...left.ids, ...right.ids].every(id => objects[id].type === 'triangle')) bvhStats.trianglePairCandidates++;
}
console.log(`BVH leaves: ${bvhStats.leaves}; max leaf: ${bvhStats.maxLeaf}; SIMD sibling-pair candidates: ${bvhStats.trianglePairCandidates}`);
const mats = scene.mats;
const orangePbr = readPbrMipChain(path.join(__dirname, 'assets/orange_texturecan/orange_peel_512.m3dpbr'));
scene.materials.set('orange', { levels: orangePbr.color });
scene.materials.set('classicBody', { levels: readRgb565MipChain(path.join(__dirname, 'assets/1970s-coupe/classic_body.rgb565mip')) });
scene.materials.set('classicTire', { levels: readRgb565MipChain(path.join(__dirname, 'assets/1970s-coupe/classic_tire.rgb565mip')) });

function pack565(r, g, b) {
  return ((Math.max(0, Math.min(255, Math.round(r))) >> 3) << 11)
    | ((Math.max(0, Math.min(255, Math.round(g))) >> 2) << 5)
    | (Math.max(0, Math.min(255, Math.round(b))) >> 3);
}
function makeRapturePbr(kind) {
  const size = 512, normal = new Uint16Array(size * size), roughness = new Uint16Array(size * size);
  const hash = (x, y) => { let h = Math.imul((x | 0) ^ 0x9e3779b9, 0x85ebca6b) ^ Math.imul((y | 0) ^ 0xc2b2ae35, 0x27d4eb2f); h = Math.imul(h ^ (h >>> 15), 0x85ebca6b); return (h ^ (h >>> 13)) >>> 0; };
  const height = (x, y) => {
    const n = (hash(x >> 3, y >> 3) / 4294967295 - .5) * (kind === 'raptureStone' ? .035 : .014);
    if (kind === 'raptureStone') return Math.sin(x * .022 + Math.sin(y * .012) * 1.4) * .075 + Math.sin(y * .041 + x * .008) * .035 + n;
    if (kind === 'raptureGlass') return Math.sin(x * .018 + Math.sin(y * .013) * 1.2) * .004 + n * .1;
    return Math.sin(y * .018 + Math.sin(x * .006) * .5) * .018 + n;
  };
  for (let y = 0; y < size; y++) for (let x = 0; x < size; x++) {
    const i = y * size + x, dx = (height(x + 2, y) - height(x - 2, y)) * .85, dy = (height(x, y + 2) - height(x, y - 2)) * .85;
    const inv = 1 / Math.hypot(dx, dy, 1), nx = -dx * inv, ny = -dy * inv, nz = inv;
    normal[i] = pack565(128 + nx * 127, 128 + ny * 127, 128 + nz * 127);
    const r = kind === 'raptureStone' ? 218 : kind === 'raptureGlass' ? 34 : 94, grain = (hash(x >> 3, y >> 3) / 4294967295 - .5) * 14;
    const v = r + grain; roughness[i] = pack565(v, v, v);
  }
  const makeMips = base => {
    const levels = [{ size, data: base }]; let prev = base, prevSize = size;
    while (prevSize > 8) {
      const n = prevSize >> 1, data = new Uint16Array(n * n);
      for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
        const ps = [prev[(y * 2) * prevSize + x * 2], prev[(y * 2) * prevSize + x * 2 + 1], prev[(y * 2 + 1) * prevSize + x * 2], prev[(y * 2 + 1) * prevSize + x * 2 + 1]];
        const ch = k => Math.round(ps.reduce((sum, p) => sum + (k === 0 ? p >> 11 : k === 1 ? (p >> 5) & 63 : p & 31), 0) / 4);
        data[y * n + x] = (ch(0) << 11) | (ch(1) << 5) | ch(2);
      }
      levels.push({ size: n, data }); prev = data; prevSize = n;
    }
    return levels;
  };
  return { normal: makeMips(normal), roughness: makeMips(roughness) };
}

function makePripyatPbr(kind) {
  const size=512, color=new Uint16Array(size*size), normal=new Uint16Array(size*size), roughness=new Uint16Array(size*size);
  const hash=(x,y)=>{let h=Math.imul((x|0)^0x9e3779b9,0x85ebca6b)^Math.imul((y|0)^0xc2b2ae35,0x27d4eb2f);h=Math.imul(h^(h>>>15),0x85ebca6b);return (h^(h>>>13))>>>0;};
  const noise=(x,y,shift,salt=0)=>hash((x>>shift)+salt,(y>>shift)-salt)/2147483647.5-1;
  const heightAt=(x,y)=>{
    const broad=noise(x,y,5), mid=noise(x,y,3), grain=noise(x,y,1);
    if(kind==='stone') return broad*.11+mid*.045+grain*.012;
    if(kind==='asphalt') return broad*.025+mid*.018+grain*.03;
    if(kind==='leaves') return broad*.08+mid*.12+grain*.04;
    return broad*.10+mid*.055+grain*.018;
  };
  const palettes={
    concrete:[145,140,126], stone:[119,113,100], asphalt:[57,59,54],
    brick:[132,91,69], rust:[112,62,39], leaves:[55,76,43]
  };
  const base=palettes[kind]||[128,128,128];
  for(let y=0;y<size;y++) for(let x=0;x<size;x++){
    const i=y*size+x, broad=noise(x,y,5), mid=noise(x,y,3), grain=noise(x,y,1);
    let variation=broad*15+mid*10+grain*5;
    if(kind==='concrete'){
      const wash=Math.max(0,Math.sin(x*.045+noise(x,y,6)*2.2))**10*(.35+.65*Math.max(0,noise(x,y,5)));
      variation-=wash*30;
    } else if(kind==='stone'){
      const gx=(x+((Math.floor(y/72)%2)*36))%96, gy=y%72;
      const seam=(gx<2.4||gy<2.4||gx>94||gy>70)?-34:0;
      const edge=Math.min(gx,96-gx,gy,72-gy);
      variation+=seam+Math.max(0,1-edge/13)*-8;
    } else if(kind==='asphalt'){
      const aggregate=hash(x>>1,y>>1)%100;
      variation+=(aggregate>94?18:aggregate<9?-9:0);
      const crack=(Math.abs(Math.sin(x*.021+Math.sin(y*.016)*2.1))<.012 && noise(x,y,4)>.1)?-38:0;
      variation+=crack;
    } else if(kind==='brick'){
      const row=(y+Math.floor(x/128)*18)%64, col=(x+Math.floor(y/64)%2*64)%128;
      variation+=(row<4||col<4)?-34:Math.abs(noise(x,y,4))*5;
    } else if(kind==='rust'){
      const streak=Math.max(0,Math.sin(x*.12+noise(x,y,5)*2))**14*Math.max(0,Math.sin(y*.018));
      variation-=streak*25;
    } else if(kind==='leaves'){
      variation+=Math.max(0,grain)*12;
    }
    const tint=(k,amount)=>Math.max(0,Math.min(255,base[k]+variation*amount));
    color[i]=pack565(tint(0,1),tint(1,kind==='rust'?.76:kind==='leaves'?.95:.98),tint(2,kind==='rust'?.54:kind==='leaves'?.72:.88));
    const dx=(heightAt(x+1,y)-heightAt(x-1,y))*1.2, dy=(heightAt(x,y+1)-heightAt(x,y-1))*1.2;
    const inv=1/Math.hypot(dx,dy,1);
    normal[i]=pack565(128-dx*inv*127,128-dy*inv*127,128+inv*127);
    const r=(kind==='asphalt'?226:kind==='leaves'?218:kind==='rust'?205:kind==='stone'?218:232)+grain*10;
    roughness[i]=pack565(r,r,r);
  }
  const makeMips=baseMap=>{
    const levels=[{size,data:baseMap}];let prev=baseMap,prevSize=size;
    while(prevSize>8){
      const n=prevSize>>1,data=new Uint16Array(n*n);
      for(let y=0;y<n;y++)for(let x=0;x<n;x++){
        const ps=[prev[(y*2)*prevSize+x*2],prev[(y*2)*prevSize+x*2+1],prev[(y*2+1)*prevSize+x*2],prev[(y*2+1)*prevSize+x*2+1]];
        const ch=k=>Math.round(ps.reduce((sum,p)=>sum+(k===0?p>>11:k===1?(p>>5)&63:p&31),0)/4);
        data[y*n+x]=(ch(0)<<11)|(ch(1)<<5)|ch(2);
      }
      levels.push({size:n,data});prev=data;prevSize=n;
    }
    return levels;
  };
  return {color:makeMips(color),normal:makeMips(normal),roughness:makeMips(roughness)};
}
const generatedPripyatPbr=new Map(mats.filter(name=>['concrete','stone','asphalt','brick','rust','leaves'].includes(name)).map(name=>[name,makePripyatPbr(name)]));
for(const [name,pbr] of generatedPripyatPbr) scene.materials.set(name,{levels:pbr.color});

const generatedPbr = new Map(['raptureStone', 'raptureBronze', 'raptureGlass'].map(name => [name, makeRapturePbr(name)]));
function materialPbr(name) {
  if (name === 'orange') return orangePbr;
  return generatedPripyatPbr.get(name) || generatedPbr.get(name) || null;
}

const types = { triangle: 0, box: 1, roundbox: 2, capsule: 3, cone: 4, ellipse: 5 };
const matIndex = new Map(mats.map((m, i) => [m, i]));
const floatCount = o => o.type === 'triangle' ? 24 : o.type === 'roundbox' ? 7 : o.type === 'capsule' ? 8 : o.type === 'cone' ? 13 : o.type === 'ellipse' ? 6 : 0;
const pbrChainBytes = chain => 4 + (chain ? chain.reduce((n, l) => n + 4 + l.size * l.size * 2, 0) : 0);
const sceneLights = scene.sceneLights || [];
if (sceneLights.length > 256) throw new Error('Une scène ne peut pas définir plus de 256 lumières ponctuelles.');
for (const [i, l] of sceneLights.entries()) {
  if (!Array.isArray(l) || l.length !== 8 || l.some(v => !Number.isFinite(v)) || l[3] <= 0 || l[3] > 10000 || l[4] < 0 || l[4] > 10000 || l.slice(5).some(v => v < 0 || v > 255)) throw new Error(`Lumière ponctuelle invalide à l’index ${i}.`);
}
let totalBytes = 40 + mats.length * 32 + objects.reduce((n, o) => n + 28 + floatCount(o) * 4, 0) + bvh.length * 40 + objects.length * 4
  + mats.reduce((n, id) => { const pbr = materialPbr(id); return n + pbrChainBytes(pbr?.normal) + pbrChainBytes(pbr?.roughness); }, 0)
  + 4 + sceneLights.length * 32;
for (const id of mats) {
  const size = 512;
  for (let n = size; ; n >>= 1) { totalBytes += 4 + n * n * 2; if (n <= 8) break; }
}
const file = Buffer.allocUnsafe(totalBytes);
let offset = 0;
function putU8(v) { file.writeUInt8(v, offset); offset++; }
function putU16(v) { file.writeUInt16LE(v, offset); offset += 2; }
function putU32(v) { file.writeUInt32LE(v >>> 0, offset); offset += 4; }
function putI32(v) { file.writeInt32LE(v, offset); offset += 4; }
function putF32(v) { file.writeFloatLE(v, offset); offset += 4; }
function putF32s(a) { for (const v of a) putF32(v); }
const vecs = (...vs) => vs.flat();

file.fill(0, offset, offset + 8); file.write('M3D1314', offset, 'ascii'); offset += 8;
putU32(4); putU32(objects.length); putU32(bvh.length); putI32(scene.bvhRoot);
putU32(objects.length); putU32(mats.length); putU32(mats.length); putU32(512);
for (const mat of mats) {
  const reflectivity = scene.reflectivityPercent[mat] ?? 0;
  if (!Number.isInteger(reflectivity) || reflectivity < 0 || reflectivity > 100) throw new Error(`Réflectivité invalide pour ${mat}: ${reflectivity}`);
  file.fill(0, offset, offset + 24); file.write(mat, offset, 24, 'utf8'); offset += 24;
  const transmission = scene.transmissionPercent[mat] ?? 0;
  if (!Number.isInteger(transmission) || transmission < 0 || transmission > 100) throw new Error(`Transmission invalide pour ${mat}: ${transmission}`);
  const ior = scene.refractiveIndex[mat] ?? 0;
  const iorMilli = ior ? Math.round(ior * 1000) : 0;
  if (iorMilli && (iorMilli < 1000 || iorMilli > 3000)) throw new Error(`Indice de réfraction invalide pour ${mat}: ${ior}`);
  putU8(reflectivity); putU8(transmission); putU16(iorMilli);
}

for (const o of objects) {
  const ti = types[o.type];
  if (ti === undefined) throw new Error(`Type non exporté: ${o.type}`);
  putU8(ti); putU8(matIndex.get(o.mat)); putU16(0);
  putF32s([...o.min, ...o.max]);
  if (o.type === 'triangle') putF32s(vecs(o.v0, o.v1, o.v2, o.n0, o.n1, o.n2, o.uv0, o.uv1, o.uv2));
  else if (o.type === 'roundbox') putF32s([o.r, ...o.center, ...o.core]);
  else if (o.type === 'capsule') putF32s([...o.a, ...o.b, o.r, o.len2]);
  else if (o.type === 'cone') putF32s([...o.a, ...o.b, o.r0, o.r1, o.k, o.len, o.nx, o.ny, o.nz]);
  else if (o.type === 'ellipse') putF32s([o.cx, o.cy, o.cz, o.rx, o.ry, o.rz]);
}

const order = [];
const packedNodes = bvh.map(node => {
  const start = order.length;
  if (node.ids) order.push(...node.ids);
  return { node, start, count: node.ids ? node.ids.length : 0 };
});
for (const { node, start, count } of packedNodes) {
  putF32s([node.minX, node.minY, node.minZ, node.maxX, node.maxY, node.maxZ]);
  putI32(node.left); putI32(node.right); putI32(start); putI32(count);
}
for (const id of order) putU32(id);

for (const mat of mats) {
  const tex = scene.materials.get(mat);
  putU32(tex.levels.length);
  for (const level of tex.levels) {
    putU32(level.size);
    for (let i = 0; i < level.data.length; i++) putU16(level.data[i]);
  }
}
for (const mat of mats) {
  const pbr = materialPbr(mat);
  for (const chain of [pbr?.normal || null, pbr?.roughness || null]) {
    putU32(chain ? chain.length : 0);
    if (chain) for (const level of chain) {
      putU32(level.size);
      for (let i = 0; i < level.data.length; i++) putU16(level.data[i]);
    }
  }
}
putU32(sceneLights.length);
for (const light of sceneLights) for (const v of light) putF32(v);
if (offset !== totalBytes) throw new Error(`Taille binaire inattendue: ${offset} / ${totalBytes}`);
fs.writeFileSync(outPath, file);
const counts = {};
for (const o of objects) counts[o.type] = (counts[o.type] || 0) + 1;
console.log(JSON.stringify({ source: path.basename(htmlPath), scene: path.basename(scenePath), objects: objects.length, triangles: counts.triangle, bvhNodes: bvh.length, bvhOrder: order.length, lights: sceneLights.length, materials: mats.map(name => ({ name, reflectivityPercent: scene.reflectivityPercent[name] ?? 0, transmissionPercent: scene.transmissionPercent[name] ?? 0, hasNormalMap: !!materialPbr(name)?.normal, hasRoughnessMap: !!materialPbr(name)?.roughness, ior: scene.refractiveIndex[name] ?? null })), importedCarTriangles: globalThis.__m3dImportedCarTriangles || 0, bytes: fs.statSync(outPath).size, counts }, null, 2));
