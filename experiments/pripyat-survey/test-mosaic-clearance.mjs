import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const source=readFileSync(new URL('../2026-10-30-cote-ete-gt/native_v14/scene_pripyat_central.js',import.meta.url),'utf8');
const floor=source.slice(source.indexOf('function floorPlate('),source.indexOf('floorPlate(archLevels.firstSlab,true);'));
assert.ok(floor.length>100,'floor plate function must be found');
assert.ok(floor.includes('box(archFoyer.galleryEastEdgeX,t,-42.2,18,b,-20.1'), 'east gallery must start behind mural wall');
assert.ok(!floor.includes("box(6,t,-35.8,8,b,-26.5"),'mosaic sightline must not be blocked by narrow mezzanine');
assert.ok(!floor.includes("box(-15,t,-42.2,8,b,-35.8"),'full-depth mezzanine must not cross double-height hall');
assert.ok(source.includes('mosaicBottomY:2.0, mosaicTopY:6.30'),'preserve 4.3 m mural');
assert.ok(source.includes("meshTriangle(frescoBR,frescoTR,frescoTL,'energetikFresco')"),'preserve mosaic texture geometry');
console.log('Mosaic hall structural clearance checks passed');

assert.ok(source.includes('m3dCheckEnergetikDatum();'),'architectural datum must be validated');
assert.ok(source.includes('box(-12.4,archLevels.firstSlab,-34.1,8.0,'),'landing must stop before mural wall');
