// Minimal glTF 2.0/GLB importer for the native scene exporter.
// It imports indexed triangle meshes, node transforms, vertex normals, UV0,
// and maps the bundled coupe's materials to native scene materials.
'use strict';

const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');
const { execFileSync } = require('child_process');
const zlib = require('zlib');

function mat4Mul(a, b) {
  const out = new Array(16).fill(0);
  for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++)
    for (let k = 0; k < 4; k++) out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return out;
}

function localMatrix(node) {
  if (node.matrix) return node.matrix;
  const [x, y, z, w] = node.rotation || [0, 0, 0, 1];
  const [sx, sy, sz] = node.scale || [1, 1, 1];
  const [tx, ty, tz] = node.translation || [0, 0, 0];
  return [
    (1 - 2 * (y * y + z * z)) * sx, (2 * (x * y + z * w)) * sx, (2 * (x * z - y * w)) * sx, 0,
    (2 * (x * y - z * w)) * sy, (1 - 2 * (x * x + z * z)) * sy, (2 * (y * z + x * w)) * sy, 0,
    (2 * (x * z + y * w)) * sz, (2 * (y * z - x * w)) * sz, (1 - 2 * (x * x + y * y)) * sz, 0,
    tx, ty, tz, 1,
  ];
}

function point(m, v) {
  const x = v[0], y = v[1], z = v[2], w = m[3] * x + m[7] * y + m[11] * z + m[15];
  const q = w && w !== 1 ? 1 / w : 1;
  return [(m[0] * x + m[4] * y + m[8] * z + m[12]) * q,
    (m[1] * x + m[5] * y + m[9] * z + m[13]) * q,
    (m[2] * x + m[6] * y + m[10] * z + m[14]) * q];
}

function normal(m, v) {
  // The bundled car uses rigid node transforms. The cofactor form also handles
  // non-uniform node scales without requiring a matrix package.
  const a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6], i = m[10];
  const A = e * i - f * h, B = f * g - d * i, C = d * h - e * g;
  const D = c * h - b * i, E = a * i - c * g, F = b * g - a * h;
  const G = b * f - c * e, H = c * d - a * f, I = a * e - b * d;
  let x = A * v[0] + D * v[1] + G * v[2], y = B * v[0] + E * v[1] + H * v[2], z = C * v[0] + F * v[1] + I * v[2];
  const len = Math.hypot(x, y, z) || 1;
  return [x / len, y / len, z / len];
}

function decodeGlb(filePath) {
  const bytes = fs.readFileSync(filePath);
  if (bytes.toString('ascii', 0, 4) !== 'glTF' || bytes.readUInt32LE(4) !== 2 || bytes.readUInt32LE(8) !== bytes.length)
    throw new Error('Le modèle de voiture n’est pas un GLB 2.0 valide.');
  let offset = 12, json = null, bin = null;
  while (offset < bytes.length) {
    const size = bytes.readUInt32LE(offset), type = bytes.readUInt32LE(offset + 4); offset += 8;
    const chunk = bytes.subarray(offset, offset + size); offset += size;
    if (type === 0x4e4f534a) json = JSON.parse(chunk.toString('utf8').replace(/\0+$/, '').trim());
    else if (type === 0x004e4942) bin = chunk;
  }
  if (!json || !bin || !json.buffers || json.buffers[0].byteLength > bin.length) throw new Error('Le GLB ne contient pas de scène et de buffer binaires.');
  return { json, bin };
}

function accessorReader(json, bin) {
  const components = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT2: 4, MAT3: 9, MAT4: 16 };
  const componentBytes = { 5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4 };
  function read(index) {
    const a = json.accessors[index], view = json.bufferViews[a.bufferView], n = components[a.type], b = componentBytes[a.componentType];
    if (!a || !view || !n || !b) throw new Error('Accessor glTF non pris en charge: ' + index);
    const start = (view.byteOffset || 0) + (a.byteOffset || 0), stride = view.byteStride || n * b;
    const values = new Array(a.count);
    for (let i = 0; i < a.count; i++) {
      const row = new Array(n), p = start + i * stride;
      for (let k = 0; k < n; k++) {
        const q = p + k * b;
        if (a.componentType === 5126) row[k] = bin.readFloatLE(q);
        else if (a.componentType === 5125) row[k] = bin.readUInt32LE(q);
        else if (a.componentType === 5123) row[k] = bin.readUInt16LE(q);
        else if (a.componentType === 5122) row[k] = bin.readInt16LE(q);
        else if (a.componentType === 5121) row[k] = bin.readUInt8(q);
        else row[k] = bin.readInt8(q);
        if (a.normalized && a.componentType !== 5126) {
          const signed = a.componentType === 5120 || a.componentType === 5122;
          const max = b === 1 ? (signed ? 127 : 255) : (signed ? 32767 : 65535);
          row[k] = signed ? Math.max(-1, row[k] / max) : row[k] / max;
        }
      }
      values[i] = n === 1 ? row[0] : row;
    }
    return values;
  }
  return read;
}

function importedMaterial(name) {
  const n = String(name).toLowerCase();
  if (/poplar.*bark|bark.*poplar/i.test(n)) return 'bark';
  if (/broken_bark|deadwood|weathered_wood|aged wood|trim.*wood/i.test(n)) return 'wood';
  if (/leaves.*dry|dry.*leaves/i.test(n)) return 'leavesAutumn';
  if (/leaves|foliage/i.test(n)) return 'leaves';
  if (/vinyl/i.test(n)) return 'seatVinylBrown';
  if (/seat.*steel|painted steel|dark steel/i.test(n)) return 'seatSteel';
  if (/seat.*wood|aged wood/i.test(n)) return 'seatWood';
  if (/concrete|facade|plaster/i.test(n)) return 'concrete';
  if (/rust/i.test(n)) return 'rust';
  if (/neon.*core|core.*neon/i.test(n)) return 'led';
  if (/neon.*tube|tube.*neon/i.test(n)) return 'neon';
  if (/glass|window/i.test(n)) return 'carGlass';
  if (/bronze|aluminium|aluminum|iron|steel|metal|ceramic/i.test(n)) return 'metal';
  if (/body_pbr/i.test(name)) return 'classicBody';
  if (/tire/i.test(name)) return 'classicTire';
  if (/rim|hubcap/i.test(name)) return 'wheelMetal';
  if (/trim/i.test(name)) return 'windowdark';
  if (/window_glass|glass/i.test(name)) return 'carGlass';
  return 'metal';
}

function addGlbCar(objects, filePath, placement, materialResolver=importedMaterial) {
  const { json, bin } = decodeGlb(filePath), read = accessorReader(json, bin);
  const nodes = json.nodes || [], meshes = json.meshes || [], mats = json.materials || [];
  const offset = placement.position || [0, 0, 0], scale = placement.scale || 1;
  let triangles = 0;
  const addMesh = (meshIndex, world) => {
    const mesh = meshes[meshIndex];
    for (const prim of mesh.primitives || []) {
      if ((prim.mode ?? 4) !== 4) throw new Error('Le modèle contient un primitive glTF autre que des triangles.');
      const attrs = prim.attributes || {};
      if (attrs.POSITION == null || attrs.NORMAL == null) throw new Error('Le modèle doit fournir les positions et les normales.');
      const positions = read(attrs.POSITION), normals = read(attrs.NORMAL), uvs = attrs.TEXCOORD_0 == null ? positions.map(() => [0, 0]) : read(attrs.TEXCOORD_0);
      const indices = prim.indices == null ? positions.map((_, i) => i) : read(prim.indices);
      const sourceMat = mats[prim.material] || {}, material = materialResolver(sourceMat.name || '', sourceMat, prim.material, json, filePath);
      for (let k = 0; k + 2 < indices.length; k += 3) {
        const ids = [indices[k], indices[k + 2], indices[k + 1]]; // scene Z is reversed, so preserve front-face winding
        const verts = ids.map(id => {
          const p = point(world, positions[id]), n = normal(world, normals[id]);
          const v = [p[0] * scale + offset[0], p[1] * scale + offset[1], offset[2] - p[2] * scale];
          const nn = [n[0], n[1], -n[2]], nl = Math.hypot(...nn) || 1;
          return { p: v, n: nn.map(x => x / nl), uv: uvs[id] };
        });
        const [a, b, c] = verts, e1 = a.p.map((x, i) => b.p[i] - x), e2 = a.p.map((x, i) => c.p[i] - x);
        const dot = (u, v) => u.reduce((sum, x, i) => sum + x * v[i], 0);
        const d00 = dot(e1, e1), d01 = dot(e1, e2), d11 = dot(e2, e2), den = d00 * d11 - d01 * d01;
        if (den < 1e-14) continue;
        const pad = .0001;
        objects.push({ type: 'triangle', mat: material,
          v0: a.p, v1: b.p, v2: c.p, n0: a.n, n1: b.n, n2: c.n, uv0: a.uv, uv1: b.uv, uv2: c.uv,
          e1, e2, d00, d01, d11, invDen: 1 / den,
          min: [0, 1, 2].map(i => Math.min(a.p[i], b.p[i], c.p[i]) - pad),
          max: [0, 1, 2].map(i => Math.max(a.p[i], b.p[i], c.p[i]) + pad) });
        triangles++;
      }
    }
  };
  const walk = (index, parent) => {
    const node = nodes[index], world = mat4Mul(parent, localMatrix(node));
    if (node.mesh != null) addMesh(node.mesh, world);
    for (const child of node.children || []) walk(child, world);
  };
  const roots = (json.scenes || [])[json.scene || 0]?.nodes || nodes.map((_, i) => i).filter(i => !nodes.some(n => (n.children || []).includes(i)));
  const identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
  for (const root of roots) walk(root, identity);
  if (!triangles) throw new Error('Le GLB ne contient aucun triangle importable.');
  return triangles;
}

function materialTexturePath(json, textureInfo, filePath) {
  if (!textureInfo || textureInfo.index == null) return null;
  const texture = (json.textures || [])[textureInfo.index];
  const image = texture && (json.images || [])[texture.source];
  if (!image) return null;
  if (image.uri && !image.uri.startsWith('data:')) return path.resolve(path.dirname(filePath), decodeURIComponent(image.uri));
  throw new Error(`Image GLB embarquée non prise en charge par le packer natif: ${path.basename(filePath)}`);
}

function readGlbPbrMipChain(filePath) {
  const stored=fs.readFileSync(filePath), b=filePath.endsWith('.gz')?zlib.gunzipSync(stored):stored;
  if(b.toString('ascii',0,8)!=='M3DPBR2\0')throw new Error(`Le packer GLB doit produire des cartes M3DPBR2: ${filePath}`);
  const baseSize=b.readUInt32LE(8),count=b.readUInt32LE(12);
  if(baseSize!==2048||count!==12)throw new Error(`Les cartes GLB natives doivent être en 2048² avec une chaîne complète: ${filePath}`);
  let offset=16;const maps={color:[],normal:[],roughness:[]};
  for(let level=0;level<count;level++){
    const size=b.readUInt32LE(offset);offset+=4;if(size!==(baseSize>>level))throw new Error(`Niveau PBR GLB incohérent: ${filePath}`);
    const n=size*size;
    for(const channel of ['color','normal','roughness']){const bpp=channel==='color'?4:2,data=bpp===4?new Uint32Array(n):new Uint16Array(n);for(let i=0;i<n;i++,offset+=bpp)data[i]=bpp===4?b.readUInt32LE(offset):b.readUInt16LE(offset);maps[channel].push({size,data});}
  }
  if(offset!==b.length)throw new Error(`Données PBR GLB supplémentaires: ${filePath}`);
  return {color:maps.color,normal:maps.normal,roughness:maps.roughness};
}

function importedPbr(json, sourceMat, filePath, registry) {
  const pbr = sourceMat.pbrMetallicRoughness || {};
  const maps = [
    materialTexturePath(json, pbr.baseColorTexture, filePath),
    materialTexturePath(json, sourceMat.normalTexture, filePath),
    materialTexturePath(json, pbr.metallicRoughnessTexture, filePath),
  ];
  if (maps.every(x => !x)) return null;
  const signature = maps.map(x => x || '-').join('\n');
  const id = 'glb' + crypto.createHash('sha1').update(signature).digest('hex').slice(0, 12);
  if (!registry.has(id)) {
    for (const texture of maps) if (texture && !fs.existsSync(texture)) throw new Error(`Texture GLB introuvable: ${texture}`);
    const cacheDir = path.join(os.tmpdir(), 'm3d-glb-pbr');
    const packed = path.join(cacheDir, id + '.m3dpbr.gz');
    if (!fs.existsSync(packed)) {
      const python = process.env.M3D_PYTHON || (process.platform === 'win32' ? 'python' : 'python3');
      const packer = path.join(__dirname, 'tools', 'pack_glb_pbr_material.py');
      execFileSync(python, [packer, ...maps.map(x => x || '-'), packed], { stdio: 'inherit' });
    }
    registry.set(id, readGlbPbrMipChain(packed));
  }
  return id;
}

function addGlbAsset(objects, filePath, placement, registry=new Map()) {
  const { json } = decodeGlb(filePath);
  return addGlbCar(objects, filePath, placement, (name, sourceMat) =>
    importedPbr(json, sourceMat, filePath, registry) || importedMaterial(name));
}

function extendMipChain(levels) {
  while (levels[levels.length - 1].size > 1) {
    const prev=levels[levels.length-1], n=prev.size>>1, data=new Uint16Array(n*n);
    for(let y=0;y<n;y++)for(let x=0;x<n;x++){
      const ps=[prev.data[(y*2)*prev.size+x*2],prev.data[(y*2)*prev.size+x*2+1],prev.data[(y*2+1)*prev.size+x*2],prev.data[(y*2+1)*prev.size+x*2+1]];
      const r=Math.round(ps.reduce((s,p)=>s+(p>>11),0)/4),g=Math.round(ps.reduce((s,p)=>s+((p>>5)&63),0)/4),b=Math.round(ps.reduce((s,p)=>s+(p&31),0)/4);
      data[y*n+x]=(r<<11)|(g<<5)|b;
    }
    levels.push({size:n,data});
  }
  return levels;
}

function readRgb565MipChain(filePath) {
  const bytes = fs.readFileSync(filePath); let offset = 0; const levels = [];
  for (let size = 512; size >= 1 && offset + size * size * 2 <= bytes.length; size >>= 1) {
    const count = size * size;
    if (offset + count * 2 > bytes.length) throw new Error('Chaîne de textures RGB565 tronquée: ' + path.basename(filePath));
    const data = new Uint16Array(count);
    for (let i = 0; i < count; i++, offset += 2) data[i] = bytes.readUInt16LE(offset);
    levels.push({ size, data });
  }
  if (offset !== bytes.length) throw new Error('La chaîne RGB565 contient des données supplémentaires: ' + path.basename(filePath));
  return extendMipChain(levels);
}

module.exports = { addGlbCar, addGlbAsset, importedMaterial, readRgb565MipChain };
