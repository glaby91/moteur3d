#!/usr/bin/env node
// Export the reusable foyer mosaic plane as a textured glTF 2.0 binary asset.
const fs = require('fs');
const path = require('path');

const here = path.resolve(__dirname, '..');
const imagePath = path.join(here, 'assets/pripyat/textures/energetik_fresco_mosaic.jpg');
const outPath = path.join(here, 'assets/pripyat/energetik_fresco_mosaic.glb');
const width = 9.5;
const height = 4.3;
const image = fs.readFileSync(imagePath);

function align4(buffer) {
  const pad = (4 - (buffer.length % 4)) % 4;
  return pad ? Buffer.concat([buffer, Buffer.alloc(pad)]) : buffer;
}

const positions = Buffer.alloc(4 * 3 * 4);
[[0, 0, 0], [width, 0, 0], [0, height, 0], [width, height, 0]]
  .flat().forEach((v, i) => positions.writeFloatLE(v, i * 4));
const normals = Buffer.alloc(4 * 3 * 4);
for (let i = 0; i < 4; i++) {
  normals.writeFloatLE(0, i * 12);
  normals.writeFloatLE(0, i * 12 + 4);
  normals.writeFloatLE(1, i * 12 + 8);
}
const uvs = Buffer.alloc(4 * 2 * 4);
[[0, 1], [1, 1], [0, 0], [1, 0]].flat().forEach((v, i) => uvs.writeFloatLE(v, i * 4));
const indices = Buffer.from([0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 2, 0]);

let bin = Buffer.alloc(0);
const views = [];
function addView(buffer, target) {
  const offset = bin.length;
  bin = Buffer.concat([bin, align4(buffer)]);
  const view = { buffer: 0, byteOffset: offset, byteLength: buffer.length };
  if (target) view.target = target;
  views.push(view);
  return views.length - 1;
}
const positionView = addView(positions, 34962);
const normalView = addView(normals, 34962);
const uvView = addView(uvs, 34962);
const indexView = addView(indices, 34963);
const imageView = addView(image);

const json = {
  asset: { version: '2.0', generator: 'Moteur3D Energetik fresco exporter' },
  scene: 0,
  scenes: [{ nodes: [0] }],
  nodes: [{ mesh: 0, name: 'EnergetikFrescoMosaic' }],
  meshes: [{ name: 'EnergetikFrescoMosaic', primitives: [{
    attributes: { POSITION: 0, NORMAL: 1, TEXCOORD_0: 2 },
    indices: 3,
    material: 0,
    mode: 4
  }] }],
  materials: [{
    name: 'EnergetikMosaicBright',
    doubleSided: true,
    pbrMetallicRoughness: {
      baseColorTexture: { index: 0 },
      baseColorFactor: [1, 1, 1, 1],
      metallicFactor: 0,
      roughnessFactor: 0.72
    }
  }],
  textures: [{ sampler: 0, source: 0 }],
  samplers: [{ magFilter: 9729, minFilter: 9987, wrapS: 33071, wrapT: 33071 }],
  images: [{ bufferView: imageView, mimeType: 'image/jpeg', name: 'EnergetikFrescoMosaicAlbedo' }],
  accessors: [
    { bufferView: positionView, componentType: 5126, count: 4, type: 'VEC3', min: [0, 0, 0], max: [width, height, 0] },
    { bufferView: normalView, componentType: 5126, count: 4, type: 'VEC3' },
    { bufferView: uvView, componentType: 5126, count: 4, type: 'VEC2' },
    { bufferView: indexView, componentType: 5123, count: 6, type: 'SCALAR' }
  ],
  bufferViews: views,
  buffers: [{ byteLength: bin.length }]
};
const jsonBuffer = align4(Buffer.from(JSON.stringify(json), 'utf8'));
const binBuffer = align4(bin);
const totalLength = 12 + 8 + jsonBuffer.length + 8 + binBuffer.length;
const header = Buffer.alloc(12);
header.writeUInt32LE(0x46546c67, 0);
header.writeUInt32LE(2, 4);
header.writeUInt32LE(totalLength, 8);
const jsonHeader = Buffer.alloc(8);
jsonHeader.writeUInt32LE(jsonBuffer.length, 0);
jsonHeader.writeUInt32LE(0x4e4f534a, 4);
const binHeader = Buffer.alloc(8);
binHeader.writeUInt32LE(binBuffer.length, 0);
binHeader.writeUInt32LE(0x004e4942, 4);
fs.mkdirSync(path.dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, Buffer.concat([header, jsonHeader, jsonBuffer, binHeader, binBuffer]));
console.log(`Wrote ${outPath} (${fs.statSync(outPath).size.toLocaleString()} bytes)`);
