// Isolated preview of one mapped Energetik façade bay.
const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({concrete: 2, stone: 3});
const M3D_SCENE_LIGHTS = [];
function makeScene() {
  objects.length = 0;
  M3D_SCENE_LIGHTS.length = 0;
  // neutral paving slab for scale and shadow contact
  box(-8,-0.16,-2.4,8,-0.06,2.4,'stone');
  buildEnergetikFacadeBay({objects,roundBox},0,0,0,{nativeMaterial:'concrete'});
  M3D_SCENE_LIGHTS.push([-3.5,7.4,6.5,12,0.9,255,238,220]);
  M3D_SCENE_LIGHTS.push([4.8,4.5,5.0,7,0.32,195,213,235]);
}
