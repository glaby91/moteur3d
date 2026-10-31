// Build-time exporter for the scene created by the real V13.14 engine.
// The generated binary contains the engine's actual primitives, smooth vertex
// normals, UVs, SAH BVH, and RGB565 mip chains. No scene is regenerated here.
const fs = require('fs');
const path = require('path');

const htmlPath = process.argv[2] || path.join(__dirname, 'source_v1314.html');
const outPath = process.argv[3] || path.join(__dirname, 'moteur3d_v1314_scene.bin');
const html = fs.readFileSync(htmlPath, 'utf8');
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
eval(`${script.slice(0, end)}\n${script.slice(materialStart, materialEnd)}\nmakeScene(); buildBVH(); const __mats = [...new Set(objects.map(o => o.mat))]; for (const id of __mats) materials.set(id, makeMips(id, 512)); globalThis.__scene = { objects, bvh, bvhRoot, sun, skyZenith, skyHorizon, pointLights, materials, mats: __mats };`);
const scene = globalThis.__scene;
const objects = scene.objects;
const bvh = scene.bvh;
const mats = scene.mats;

const types = { triangle: 0, box: 1, roundbox: 2, capsule: 3, cone: 4, ellipse: 5 };
const matIndex = new Map(mats.map((m, i) => [m, i]));
const floatCount = o => o.type === 'triangle' ? 24 : o.type === 'roundbox' ? 7 : o.type === 'capsule' ? 8 : o.type === 'cone' ? 13 : o.type === 'ellipse' ? 6 : 0;
let totalBytes = 40 + mats.length * 28 + objects.reduce((n, o) => n + 28 + floatCount(o) * 4, 0) + bvh.length * 40 + objects.length * 4;
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

file.write('M3D1314', offset, 'ascii'); offset += 8;
putU32(1); putU32(objects.length); putU32(bvh.length); putI32(scene.bvhRoot);
putU32(objects.length); putU32(mats.length); putU32(mats.length); putU32(512);
for (const mat of mats) { file.fill(0, offset, offset + 24); file.write(mat, offset, 24, 'utf8'); offset += 24; }

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
if (offset !== totalBytes) throw new Error(`Taille binaire inattendue: ${offset} / ${totalBytes}`);
fs.writeFileSync(outPath, file);
const counts = {};
for (const o of objects) counts[o.type] = (counts[o.type] || 0) + 1;
console.log(JSON.stringify({ source: path.basename(htmlPath), objects: objects.length, triangles: counts.triangle, bvhNodes: bvh.length, bvhOrder: order.length, materials: mats, bytes: fs.statSync(outPath).size, counts }, null, 2));
