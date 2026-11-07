// Build-time exporter for the scene created by the real V13.14 engine.
// The generated binary contains the engine's actual primitives, smooth vertex
// normals, UVs, SAH BVH, and embedded RGB565 color/normal/roughness mip chains.
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { addGlbCar, addGlbAsset, readRgb565MipChain } = require('./gltf_scene_import');
globalThis.buildEnergetikCinemaSeat = require('./assets/pripyat/energetik_cinema_seat.js');
globalThis.buildEnergetikFacadeBay = require('./assets/pripyat/energetik_facade_bay.js').buildEnergetikFacadeBay;
globalThis.buildEnergetikRoofNeonSign = require('./assets/pripyat/energetik_roof_neon_sign.js').buildEnergetikRoofNeonSign;
globalThis.buildPripyatOutdoorChair = require('./assets/pripyat/pripyat_outdoor_chair.js');

function extendMipChain(levels, mode='color') {
  while (levels[levels.length - 1].size > 1) {
    const prev = levels[levels.length - 1], n = prev.size >> 1, data = mode==='color'&&prev.data instanceof Uint32Array?new Uint32Array(n*n):new Uint16Array(n * n);
    for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
      const ps = [prev.data[(y*2)*prev.size+x*2], prev.data[(y*2)*prev.size+x*2+1], prev.data[(y*2+1)*prev.size+x*2], prev.data[(y*2+1)*prev.size+x*2+1]];
      let r=0,g=0,b=0;
      if(mode==='color'&&prev.data instanceof Uint32Array){for(const p of ps){r+=(p>>16)&255;g+=(p>>8)&255;b+=p&255;}r/=4;g/=4;b/=4;data[y*n+x]=(Math.round(r)<<16)|(Math.round(g)<<8)|Math.round(b);continue;}
      if (mode === 'normal') {
        for (const p of ps) { r += ((p>>11)&31)*255/31/127.5-1; g += ((p>>5)&63)*255/63/127.5-1; b += (p&31)*255/31/127.5-1; }
        const len=Math.hypot(r,g,b)||1; r=128+(r/len)*127; g=128+(g/len)*127; b=128+(b/len)*127;
      } else {
        for (const p of ps) { r += ((p>>11)&31)*255/31; g += ((p>>5)&63)*255/63; b += (p&31)*255/31; }
        r/=4;g/=4;b/=4;
      }
      data[y*n+x]=((Math.max(0,Math.min(255,Math.round(r)))>>3)<<11)|((Math.max(0,Math.min(255,Math.round(g)))>>2)<<5)|(Math.max(0,Math.min(255,Math.round(b)))>>3);
    }
    levels.push({size:n,data});
  }
  return levels;
}

function readPbrMipChain(filePath) {
  const stored = fs.readFileSync(filePath);
  const b = filePath.endsWith('.gz') ? zlib.gunzipSync(stored) : stored;
  const format=b.toString('ascii',0,8);
  if (format !== 'M3DPBR1\0' && format !== 'M3DPBR2\0') throw new Error(`Matériau PBR invalide: ${filePath}`);
  const colorBytes=format==='M3DPBR2\0'?4:2;
  const baseSize = b.readUInt32LE(8), count = b.readUInt32LE(12);
  if (![512, 1024, 2048].includes(baseSize) || ![Math.log2(baseSize / 8) + 1, Math.log2(baseSize) + 1].includes(count)) throw new Error(`Pyramide PBR invalide (512 à 2048 px, chaîne vers 8 ou 1): ${filePath}`);
  let offset = 16;
  const maps = { color: [], normal: [], roughness: [] };
  for (let level = 0; level < count; level++) {
    const size = b.readUInt32LE(offset); offset += 4;
    if (size !== (baseSize >> level)) throw new Error(`Niveau PBR incohérent: ${filePath}`);
    const n = size * size;
    for (const channel of ['color', 'normal', 'roughness']) {
      const bpp=channel==='color'?colorBytes:2;
      const data = bpp===4?new Uint32Array(n):new Uint16Array(n);
      for (let i = 0; i < n; i++) data[i] = bpp===4?b.readUInt32LE(offset+i*bpp):b.readUInt16LE(offset+i*bpp);
      maps[channel].push({ size, data });
      offset += n*bpp;
    }
  }
  for (const [channel, chain] of Object.entries(maps)) extendMipChain(chain, channel);
  if (offset !== b.length) throw new Error(`Données en trop dans le matériau PBR: ${filePath}`);
  return maps;
}

const sceneBuildTextureSize = Math.max(512, Math.min(2048, Number(process.env.M3D_TEXTURE_SIZE) || 1024));
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
globalThis.__m3dAppendPripyatCatalog = (objects) => {
  const objectRoot = process.env.M3D_PRIPYAT_OBJECTS_DIR;
  if (!objectRoot) throw new Error('M3D_PRIPYAT_OBJECTS_DIR doit pointer vers le dossier assets/objects du pack GLB Pripyat.');
  globalThis.__m3dPripyatPbr = globalThis.__m3dPripyatPbr || new Map();
  const trees = [
    [-52,-11],[-48,29],[-39,58],[-27,58],[-13,59],[5,59],[20,59],[38,57],[51,49],[53,28],
    [52,7],[48,-11],[-52,49],[-48,2],[-44,-12],[-34,-10],[34,-11],[43,-11],[-56,68],[53,67]
  ];
  let triangles = 0;
  const append = (file, placement) => { triangles += addGlbAsset(objects, path.join(objectRoot, file), placement, globalThis.__m3dPripyatPbr); };
  for (let i=0;i<20;i++) append(`poplar_${String(i+1).padStart(2,'0')}.glb`, { position:[trees[i][0],0,trees[i][1]], scale:1 });
  append('poplar_mature.glb', { position:[0,0,74], scale:1 });
  append('public_park_bench.glb', { position:[-7,0,17], scale:1 });
  append('soviet_street_lamp.glb', { position:[7,0,17], scale:1 });
  append('energetik_cinema_seat_row.glb', { position:[-2,0,48], scale:1 });
  append('energetik_facade_bay.glb', { position:[59,0,20], scale:1 });
  append('energetik_roof_neon_sign.glb', { position:[59,8,20], scale:1 });
  globalThis.__m3dImportedPripyatTriangles = triangles;
};
const optionalCar = process.env.M3D_SKIP_IMPORTED_CAR === '1' ? '' : 'globalThis.__m3dAppendClassicCar(objects, M3D_IMPORTED_CAR);';
const optionalPripyatCatalog = process.env.M3D_IMPORT_PRIPYAT_GLB === '1' && path.basename(scenePath) === 'scene_pripyat_central.js' ? 'globalThis.__m3dAppendPripyatCatalog(objects);' : '';
eval(`${script.slice(0, end)}\n${script.slice(materialStart, materialEnd)}\n${sceneSource}\nmakeScene(); ${optionalCar} ${optionalPripyatCatalog} buildBVH(); const __mats = [...new Set(objects.map(o => o.mat))]; for (const id of __mats) materials.set(id, makeMips(id, id.startsWith('gym') ? 512 : sceneBuildTextureSize)); globalThis.__scene = { objects, bvh, bvhRoot, sun, skyZenith, skyHorizon, pointLights, materials, mats: __mats, reflectivityPercent: M3D_MATERIAL_REFLECTIVITY_PERCENT, transmissionPercent: typeof M3D_MATERIAL_TRANSMISSION_PERCENT === "undefined" ? {} : M3D_MATERIAL_TRANSMISSION_PERCENT, refractiveIndex: typeof M3D_MATERIAL_IOR === "undefined" ? {} : M3D_MATERIAL_IOR, sceneLights: typeof M3D_SCENE_LIGHTS === "undefined" ? [] : M3D_SCENE_LIGHTS };`);
const scene = globalThis.__scene;
const energetikFrescoPbr = readPbrMipChain(path.join(__dirname, 'assets/pripyat/textures/energetik_fresco_mosaic.m3dpbr.gz'));
scene.materials.set('energetikFresco', { levels: energetikFrescoPbr.color });
const importedPripyatPbr = globalThis.__m3dPripyatPbr || new Map();
for (const [name, pbr] of importedPripyatPbr) scene.materials.set(name, { levels:pbr.color });
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
const facadePbr = process.env.M3D_FACADE_PBR === '1' ? readPbrMipChain(path.join(__dirname, 'assets/pripyat/textures/prp_concrete_panel.m3dpbr.gz')) : null;
if (facadePbr) scene.materials.set('concrete', { levels: facadePbr.color });

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
    while (prevSize > 1) {
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
  const size=kind.startsWith('gym') && kind!=='gymFloor' ? 1024 : Number(process.env.M3D_TEXTURE_SIZE || 1024);
  if (![512,1024,2048].includes(size)) throw new Error(`Taille de texture M3D_TEXTURE_SIZE invalide: ${size} (512, 1024 ou 2048 attendus).`);
  const isLeaf=kind==='leaves'||kind==='leavesAutumn';
  const color=new Uint16Array(size*size), normal=new Uint16Array(size*size), roughness=new Uint16Array(size*size);
  const hash=(x,y)=>{let h=Math.imul((x|0)^0x9e3779b9,0x85ebca6b)^Math.imul((y|0)^0xc2b2ae35,0x27d4eb2f);h=Math.imul(h^(h>>>15),0x85ebca6b);return (h^(h>>>13))>>>0;};
  const noise=(x,y,shift,salt=0)=>hash((x>>shift)+salt,(y>>shift)-salt)/2147483647.5-1;
  const heightAt=(x,y)=>{
    const broad=noise(x,y,5), mid=noise(x,y,3), grain=noise(x,y,1);
    if(kind==='gymFloor') return Math.sin(y*.075)*.003+Math.sin(y*.37)*.0009+broad*.055+mid*.025+grain*.010;
    if(kind==='gymWall') return broad*.16+mid*.10+grain*.027;
    if(kind==='gymConcrete') return broad*.11+mid*.065+grain*.025;
    if(kind==='gymMetal'||kind==='gymRed'||kind==='gymBlue') return broad*.045+mid*.025+grain*.012;
    if(kind==='stone') return broad*.11+mid*.045+grain*.012;
    if(kind==='asphalt') return broad*.025+mid*.018+grain*.03;
    if(isLeaf) return broad*.08+mid*.12+grain*.04;
    if(kind==='plaster') return broad*.20+mid*.11+grain*.035;
    if(kind==='mural') return broad*.13+mid*.09+grain*.035;
    if(kind==='volcanicTuff') return broad*.18+mid*.12+grain*.055;
    if(kind==='seatSteel') return Math.sin(y*12+Math.sin(x*.15)*.45)*.0013+Math.sin(y*29)*.00035;
    if(kind==='seatPreviewFloor') return Math.sin(x*.13)*.0012+Math.cos(y*.11)*.0012+noise(x,y,9)*.0005;
    if(kind==='seatWood') return Math.sin(y*.035+noise(x,y,8)*.25)*.020+Math.sin(y*.09)*.006+Math.sin(x*.021+noise(x,y,9)*.25)*.002;
    if(kind==='seatVinylBrown') return broad*.050+mid*.020+Math.sin(y*.045+Math.sin(x*.009)*1.8)*.009+grain*.002;
    if(kind==='seatSupportPaint') return broad*.025+mid*.016+grain*.006;
    if(kind==='seatFabricClean'||kind==='seatTrim') return Math.sin(x*Math.PI/5)*.003+Math.cos(y*Math.PI/5)*.003;
    if(kind==='wood'||kind==='bark') return broad*.07+Math.sin(y*.08+noise(x,y,5))* .035+grain*.018;
    if(kind==='seatFabric'||kind==='curtain') return broad*.045+mid*.035+grain*.018;
    if(kind==='cinemaScreen') return broad*.07+mid*.035+grain*.012;
    if(kind==='debris') return broad*.18+mid*.10+grain*.07;
    if(kind==='grass') return broad*.11+mid*.14+grain*.05;
    return broad*.10+mid*.055+grain*.018;
  };
  const palettes={
    gymFloor:[119,159,170], gymWall:[143,159,151], gymConcrete:[135,131,118],
    gymMetal:[100,78,61], gymLetter:[228,223,207], gymRed:[151,59,53], gymBlue:[36,113,164],
    concrete:[145,140,126], stone:[119,113,100], asphalt:[57,59,54],
    plaster:[166,159,143], mural:[179,151,112], volcanicTuff:[145,96,79],
    brick:[132,91,69], rust:[112,62,39], leaves:[65,91,48], leavesAutumn:[126,83,41],
    wood:[91,67,45], bark:[70,55,38], debris:[104,97,83], grass:[88,99,54],
    seatFabric:[76,43,41], seatFabricClean:[109,37,44], seatVinylBrown:[136,96,72], seatSupportPaint:[91,145,126], seatWood:[110,73,46], seatSteel:[78,84,89], seatPreviewFloor:[132,129,120], seatTrim:[84,31,38], curtain:[69,43,42], cinemaScreen:[119,117,108]
  };
  const base=palettes[kind]||[128,128,128];
  for(let y=0;y<size;y++) for(let x=0;x<size;x++){
    const i=y*size+x, broad=noise(x,y,5), mid=noise(x,y,3), grain=noise(x,y,1);
    let variation=(kind==='seatFabricClean'||kind==='seatWood'||kind==='seatSteel'||kind==='seatPreviewFloor'||kind==='seatTrim')?0:kind==='seatVinylBrown'?broad*10+mid*6+grain*1.2:broad*15+mid*10+grain*5;
    if(kind==='gymFloor'){
      // Parallel parquet planks, weathered worn paint and inlaid sports court.
      const seam=Math.min(y%17,17-y%17),woodKnots=Math.max(0,noise(x,y,3));
      variation+=seam<1.05?-31:Math.sin(x*.15+noise(x,y,5))*4;
      variation-=Math.max(0,woodKnots-.62)*27;
    } else if(kind==='gymWall'){
      const damage=noise(x,y,5)*.58+noise(x,y,2)*.29+grain*.13;
      variation-=Math.max(0,(damage-.22))*46;
      variation-=Math.max(0,Math.sin(x*.027+noise(x,y,5)))*14;
    } else if(kind==='gymConcrete'){
      variation-=Math.max(0,noise(x,y,5)*.63+noise(x,y,3)*.37)*31;
    } else if(kind==='gymMetal'){
      variation-=Math.max(0,Math.sin(x*.05+noise(x,y,5))*Math.sin(y*.023))*29;
    } else if(kind==='concrete'){
      const wash=Math.max(0,Math.sin(x*.045+noise(x,y,6)*2.2))**10*(.35+.65*Math.max(0,noise(x,y,5)));
      const runoff=Math.max(0,Math.sin(x*.018+noise(x,y,5)*1.6))**12*(.35+.65*Math.max(0,noise(x,y,4)));
      variation-=wash*27+runoff*19;
      variation+=hash(x>>1,y>>1)%100<9?-7:0;
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
    } else if(isLeaf){
      variation+=Math.max(0,grain)*12;
    } else if(kind==='volcanicTuff'){
      const bw=166,bh=118,row=Math.floor(y/bh),offset=(row&1)*83;
      const bx=(x+offset)%bw,by=y%bh;
      const blockX=Math.floor((x+offset)/bw), blockY=row;
      const edge=Math.min(bx,bw-bx,by,bh-by);
      const joint=(edge<3.2+Math.abs(noise(x,y,5))*2)?-34:edge<8?-8:0;
      const blockTone=(hash(blockX,blockY)%19)-9;
      const porous=(hash(x>>2,y>>2)%100<11)?-10:0;
      const spallField=noise(x,y,4)*.58+noise(x,y,2)*.30+grain*.12;
      const spall=spallField>.69&&edge<15;
      variation+=joint+blockTone+porous;
      if(spall) variation-=25+Math.max(0,mid)*12;
      if(hash(blockX+91,blockY-33)%100<18&&bx>bw-13&&edge<12) variation-=18;
    } else if(kind==='plaster'){
      const damp=Math.max(0,Math.sin(x*.031+noise(x,y,6)*1.8))*Math.max(0,Math.sin(y*.017));
      const peelField=noise(x,y,4)*.58+noise(x,y,2)*.30+grain*.12;
      const peel=Math.max(0,(peelField-.39)/.61);
      variation-=damp*27+peel*(18+Math.max(0,mid)*24);
    } else if(kind==='mural'){
      variation+=Math.sin(x*.025)*7-Math.max(0,Math.sin(y*.019+noise(x,y,5)))*12;
      const peelField=noise(x,y,4)*.58+noise(x,y,2)*.30+grain*.12;
      variation-=Math.max(0,(peelField-.49)/.51)*(22+Math.max(0,mid)*25);
    } else if(kind==='seatSteel'){
      const brush=Math.sin(y*12+Math.sin(x*.15)*.45);
      variation+=brush*3.1+Math.sin(y*29)*1.1+Math.sin(y*2.3+x*.09)*.7;
    } else if(kind==='seatTrim'){
      const wx=2*Math.PI*(x%10)/10, wy=2*Math.PI*(y%10)/10;
      variation+=Math.cos(wx)*1.2+Math.cos(wy)*1.0;

    } else if(kind==='seatPreviewFloor'){
      variation+=Math.sin(x*.13)*1.8+Math.cos(y*.11)*1.5+Math.sin((x+y)*.07)*.8+noise(x,y,9)*.4;
    } else if(kind==='seatWood'){
      const grainLine=Math.sin(y*.035+noise(x,y,8)*.25);
      variation+=grainLine*4.2+noise(x,y,9)*1.1+Math.sin(y*.12)*.8;
      if(Math.abs(grainLine)>.97) variation-=4.0;
    } else if(kind==='wood'||kind==='bark'){
      const grainLine=Math.abs(Math.sin(y*.08+noise(x,y,5)*1.2));
      variation-=Math.max(0,.25-grainLine)*24;
      variation+=noise(x,y,6)*9;
    } else if(kind==='seatFabric'){
      const worn=Math.max(0,noise(x,y,4)*.65+noise(x,y,2)*.35-.48);
      variation+=worn*18;
      if(hash(x>>2,y>>2)%100<2) variation-=15;
    } else if(kind==='seatFabricClean'){
      // Fine warp-and-weft upholstery weave, distinct from the darker piping.
      const wx=2*Math.PI*(x%10)/10, wy=2*Math.PI*(y%10)/10;
      const interlace=((Math.floor(x/10)+Math.floor(y/10))&1)?-1:1;
      variation+=Math.cos(wx)*2.4+Math.cos(wy)*2.0+interlace*.8;
    } else if(kind==='seatVinylBrown'){
      const wornField=noise(x,y,6)*.52+noise(x,y,3)*.32+grain*.16;
      const worn=Math.max(0,(wornField-.12)/.72);
      const crease=Math.pow(Math.max(0,Math.sin(y*.045+Math.sin(x*.009)*1.8)),12);
      const cloud=noise(x,y,7)*.55+noise(x,y,5)*.30+noise(x,y,3)*.15;
      variation+=cloud*8+Math.sin(x*.075+noise(x,y,4))*1.5+Math.cos(y*.055+noise(x,y,4))*1.0+worn*13-crease*7;
    } else if(kind==='seatSupportPaint'){
      const brush=Math.sin(y*.11+Math.sin(x*.035)*.5);
      variation+=brush*4.0+noise(x,y,3)*3.0;
    } else if(kind==='curtain'){
      const fold=Math.abs(Math.sin(x*.11+noise(x,y,5)*.7));
      variation-=Math.max(0,.24-fold)*17;
      variation-=Math.max(0,noise(x,y,4)*.65+noise(x,y,2)*.35-.57)*24;
    } else if(kind==='cinemaScreen'){
      variation-=Math.max(0,noise(x,y,4)*.60+noise(x,y,2)*.40-.53)*28;
      variation+=Math.sin(x*.018)*3;
    } else if(kind==='debris'){
      const chip=hash(x>>3,y>>3)%100;
      variation+=chip<16?15:chip>86?-20:0;
    } else if(kind==='grass'||isLeaf){
      const cluster=Math.max(0,Math.sin(x*.035+noise(x,y,5))*Math.sin(y*.027+noise(x,y,4)));
      variation+=cluster*18+Math.max(0,grain)*8;
    }
    let pigment=base;
    if(kind==='volcanicTuff'){
      const spallField=noise(x,y,4)*.58+noise(x,y,2)*.30+grain*.12;
      if(spallField>.69&&Math.min((x%166),166-(x%166),(y%118),118-(y%118))<15) pigment=[119,114,104];
      else if(hash(x>>4,y>>4)%100<5) pigment=[168,115,90];
    }
    if(kind==='seatVinylBrown'){
      const wear=noise(x,y,6)*.52+noise(x,y,3)*.32+noise(x,y,1)*.16;
      if(wear>.37) pigment=[158,112,80];
      else if(wear<-.49) pigment=[98,66,49];
      else if(noise(x,y,5)>.50) pigment=[146,101,73];
    }
    if(kind==='mural'){
      const region=(Math.sin(x*.011+Math.sin(y*.006)*1.7+noise(x,y,5)*.8)+1)*.5;
      pigment=region<.22?[39,132,159]:region<.46?[55,103,164]:region<.69?[196,91,51]:[205,178,112];
      const peelField=noise(x,y,4)*.58+noise(x,y,2)*.30+grain*.12;
      if(peelField>.68)pigment=[139,132,115];
    }
    if(kind==='gymFloor'){
      // The atlas is mapped *once* to the floor mesh, not tiled per board.
      const U=x/(size-1),V=y/(size-1),X=(U-.5)*39.6,Z=(V-.5)*25.6;
      const aX=Math.abs(X),aZ=Math.abs(Z);
      const red=aX>12 && aZ<8.3;
      const yellow=aZ>9.5 && aZ<11.7;
      pigment=red?[148,85,83]:yellow?[172,155,105]:[119,161,177];
      // Sparse exposed wood under the worn sports paint; paint scratches.
      const exposed=noise(x,y,5)*.55+noise(x,y,2)*.31+grain*.14;
      if(exposed>.41) pigment=[135,116,91];
      else if(exposed<-.58) pigment=[80,114,124];
      const near=(d,w)=>Math.abs(d)<w;
      const courtOutline=(near(aX-18.0,.075)&&aZ<=10.15) ||
                         (near(aZ-10.0,.075)&&aX<=18.2);
      const centreLine=near(X,.061)&&aZ<=10;
      const centreCircle=near(Math.hypot(X,Z)-1.8,.070);
      // The two semicircular basketball key boundaries use white paint.
      const basketArc=near(Math.hypot(aX-15.5,Z)-4.85,.075)&&aX<16.4;
      const basketKey=near(aZ-2.45,.064)&&aX>13.5&&aX<18.0;
      const handArc=near(Math.hypot(aX-18.0,Z)-6.0,.095)&&aX<18.1;
      const handDash=near(Math.hypot(aX-18.0,Z)-9.0,.082)&&aX<18.1&&Math.floor(Math.atan2(Z,aX-18)*4)%2===0;
      let ink=null;
      if(courtOutline||centreLine||centreCircle||basketArc||basketKey)ink=[215,211,195];
      if(handArc||handDash)ink=[202,177,92];
      // Paint survives irregularly, even in lines.
      if(ink&&exposed<.38)pigment=ink;
      const split=Math.abs((y%17)-8.5);
      if(split>7.9) pigment=pigment.map(c=>Math.max(0,c-35));
    }
    if(kind==='gymWall'){
      const peel=noise(x,y,6)*.57+noise(x,y,3)*.31+grain*.12;
      if(peel>.32)pigment=peel>.53?[117,111,99]:[191,184,157];
      if(peel<-.54)pigment=[88,110,109];
    }
    if(kind==='gymConcrete'){
      const stain=noise(x,y,6)*.56+noise(x,y,3)*.34+grain*.10;
      if(stain>.37)pigment=[88,86,77];
      else if(stain<-.56)pigment=[172,159,132];
    }
    if(kind==='gymMetal'){
      const oxide=noise(x,y,6)*.58+noise(x,y,3)*.31+grain*.11;
      if(oxide>.27)pigment=[141,79,48];
      else if(oxide<-.45)pigment=[70,70,66];
    }
    const tint=(k,amount)=>Math.max(0,Math.min(255,pigment[k]+variation*amount));
    color[i]=pack565(tint(0,1),tint(1,kind==='rust'?.76:isLeaf||kind==='grass'?.95:.98),tint(2,kind==='rust'?.54:isLeaf||kind==='grass'?.72:kind==='wood'||kind==='bark'?.72:.88));
    const dx=(heightAt(x+1,y)-heightAt(x-1,y))*1.2, dy=(heightAt(x,y+1)-heightAt(x,y-1))*1.2;
    const inv=1/Math.hypot(dx,dy,1);
    normal[i]=pack565(128-dx*inv*127,128-dy*inv*127,128+inv*127);
    const roughBase=kind==='gymFloor'?187:kind==='gymWall'?221:kind==='gymConcrete'?216:kind==='gymMetal'?198:kind==='gymLetter'?174:kind==='gymRed'||kind==='gymBlue'?175:kind==='seatSteel'?170:kind==='seatSupportPaint'?176:kind==='seatVinylBrown'?96:kind==='seatPreviewFloor'?236:kind==='seatWood'?145:kind==='seatTrim'?232:kind==='seatFabricClean'?238:kind==='asphalt'?232:isLeaf||kind==='grass'?224:kind==='rust'?218:kind==='stone'?222:kind==='wood'||kind==='bark'?214:kind==='debris'?236:kind==='plaster'||kind==='mural'?228:kind==='volcanicTuff'?238:kind==='seatFabric'||kind==='curtain'?244:kind==='cinemaScreen'?228:232;
    const r=kind==='seatSteel'?169+Math.sin(y*12)*15+Math.sin(y*29)*4:kind==='seatWood'?145+Math.sin(y*.07)*7+grain*2:kind==='seatFabricClean'?238+Math.cos(x*Math.PI/5)*4:kind==='seatVinylBrown'?96+noise(x,y,4)*24+Math.abs(grain)*10:kind==='seatSupportPaint'?176+noise(x,y,3)*12:kind==='seatTrim'?232+Math.sin(y*Math.PI/5)*3:roughBase+grain*10+Math.abs(mid)*4;
    roughness[i]=pack565(r,r,r);
  }
  const makeMips=baseMap=>{
    const levels=[{size,data:baseMap}];let prev=baseMap,prevSize=size;
    while(prevSize>1){
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
const generatedPripyatPbr=new Map(mats.filter(name=>['concrete','plaster','mural','volcanicTuff','stone','asphalt','brick','rust','leaves','leavesAutumn','grass','wood','bark','debris','seatFabric','seatFabricClean','seatVinylBrown','seatSupportPaint','seatWood','seatSteel','seatPreviewFloor','seatTrim','curtain','cinemaScreen','gymFloor','gymWall','gymConcrete','gymMetal','gymLetter','gymRed','gymBlue'].includes(name)).map(name=>[name,makePripyatPbr(name)]));
for(const [name,pbr] of generatedPripyatPbr) scene.materials.set(name,{levels:pbr.color});

const generatedPbr = new Map(['raptureStone', 'raptureBronze', 'raptureGlass'].map(name => [name, makeRapturePbr(name)]));
function materialPbr(name) {
  if (importedPripyatPbr.has(name)) return importedPripyatPbr.get(name);
  if (name === 'concrete' && facadePbr) return facadePbr;
  if (name === 'orange') return orangePbr;
  return generatedPripyatPbr.get(name) || generatedPbr.get(name) || null;
}

const types = { triangle: 0, box: 1, roundbox: 2, capsule: 3, cone: 4, ellipse: 5 };
const matIndex = new Map(mats.map((m, i) => [m, i]));
const floatCount = o => o.type === 'triangle' ? 24 : o.type === 'roundbox' ? 7 : o.type === 'capsule' ? 8 : o.type === 'cone' ? 13 : o.type === 'ellipse' ? 6 : 0;
const pbrChainBytes = chain => 4 + (chain ? chain.reduce((n, l) => n + 4 + l.data.length*l.data.BYTES_PER_ELEMENT, 0) : 0);
const sceneLights = scene.sceneLights || [];
if (sceneLights.length > 256) throw new Error('Une scène ne peut pas définir plus de 256 lumières ponctuelles.');
for (const [i, l] of sceneLights.entries()) {
  if (!Array.isArray(l) || l.length !== 8 || l.some(v => !Number.isFinite(v)) || l[3] <= 0 || l[3] > 10000 || l[4] < 0 || l[4] > 10000 || l.slice(5).some(v => v < 0 || v > 255)) throw new Error(`Lumière ponctuelle invalide à l’index ${i}.`);
}
const textureSize = Math.max(512, ...mats.map(id => scene.materials.get(id)?.levels?.[0]?.size || 512));
// Each material stores a 24-byte name plus four bytes of optical metadata.
let totalBytes = 40 + mats.length * 28 + objects.reduce((n, o) => n + 28 + floatCount(o) * 4, 0) + bvh.length * 40 + objects.length * 4
  + mats.reduce((n, id) => { const pbr = materialPbr(id); return n + pbrChainBytes(pbr?.normal) + pbrChainBytes(pbr?.roughness); }, 0)
  + 4 + sceneLights.length * 32;
for (const id of mats) { const tex=scene.materials.get(id); totalBytes += 4 + tex.levels.reduce((n,l)=>n+4+l.data.length*4,0); }
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
putU32(5); putU32(objects.length); putU32(bvh.length); putI32(scene.bvhRoot);
putU32(objects.length); putU32(mats.length); putU32(mats.length); putU32(textureSize);
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
    for (let i = 0; i < level.data.length; i++) {
      if(level.data instanceof Uint32Array)putU32(level.data[i]);
      else{const p=level.data[i],r=Math.round(((p>>11)&31)*255/31),g=Math.round(((p>>5)&63)*255/63),b=Math.round((p&31)*255/31);putU32((r<<16)|(g<<8)|b);}
    }
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
console.log(JSON.stringify({ source: path.basename(htmlPath), scene: path.basename(scenePath), objects: objects.length, triangles: counts.triangle, bvhNodes: bvh.length, bvhOrder: order.length, lights: sceneLights.length, materials: mats.map(name => ({ name, reflectivityPercent: scene.reflectivityPercent[name] ?? 0, transmissionPercent: scene.transmissionPercent[name] ?? 0, hasNormalMap: !!materialPbr(name)?.normal, hasRoughnessMap: !!materialPbr(name)?.roughness, ior: scene.refractiveIndex[name] ?? null })), importedCarTriangles: globalThis.__m3dImportedCarTriangles || 0, importedPripyatTriangles: globalThis.__m3dImportedPripyatTriangles || 0, bytes: fs.statSync(outPath).size, counts }, null, 2));
