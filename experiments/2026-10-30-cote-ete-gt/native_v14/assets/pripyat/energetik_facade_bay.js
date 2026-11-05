'use strict';

// One reusable Energetik façade bay. Dimensions are metres; front faces +Z.
// The blockout keeps the main proportions of the reference: open central bay,
// broad pilasters, heavy lintel, stepped cornice and projecting plinth.
const MATERIAL = 'prp_concrete_panel';
const NATIVE_MATERIAL = 'concrete';
function buildEnergetikFacadeBay(api, x=0, y=0, z=0, options={}) {
  const start=Array.isArray(api.objects)?api.objects.length:null;
  const scale=Number.isFinite(options.scale)?options.scale:1;
  const X=v=>x+v*scale, Y=v=>y+v*scale, Z=v=>z+v*scale;
  const B=(x0,y0,z0,x1,y1,z1,r=.02)=>api.roundBox(X(x0),Y(y0),Z(z0),X(x1),Y(y1),Z(z1),r*scale,options.nativeMaterial||NATIVE_MATERIAL);
  // Main open bay: 6.8 m span, approximately 9.4 m high.
  // The opening stays empty to match the missing glazing in the reference.
  B(-3.40,0,-.38,3.40,.72,.38,.035);                 // projecting plinth
  B(-3.34,.66,-.30,-2.78,8.36,.30,.055);            // left pilaster
  B(2.78,.66,-.30,3.34,8.36,.30,.055);              // right pilaster
  B(-3.38,8.18,-.40,3.38,8.68,.40,.035);            // broad lintel
  // Recessed vertical returns give the open bay visible depth.
  B(-2.80,.80,-.37,-2.63,8.18,.15,.018);
  B(2.63,.80,-.37,2.80,8.18,.15,.018);
  B(-2.80,.80,-.37,-2.63,8.18,-.16,.018);
  B(2.63,.80,-.37,2.80,8.18,-.16,.018);
  // Pilaster bases and stepped capitals follow the façade profile.
  for(const px of [-3.06,3.06]){
    B(px-.42,.68,-.44,px+.42,1.06,.44,.025);
    B(px-.31,1.06,-.34,px+.31,1.18,.34,.018);
    B(px-.36,7.98,-.39,px+.36,8.20,.39,.02);
    B(px-.52,8.20,-.46,px+.52,8.42,.46,.028);
    B(px-.60,8.42,-.50,px+.60,8.54,.50,.018);
  }
  // Four cornice courses reproduce the stepped, projecting silhouette.
  B(-3.4,8.68,-.48,3.4,8.86,.48,.025);
  B(-3.4,8.86,-.55,3.4,9.10,.55,.035);
  B(-3.4,9.10,-.49,3.4,9.28,.49,.02);
  B(-3.25,9.28,-.42,3.25,9.38,.42,.02);
  // Narrow raised arrises frame the broad, flat face of each pilaster.
  // These replace the old protruding corner chips, which read as loose blocks.
  B(-3.30,1.28,.295,-3.25,7.76,.345,.012);
  B(-2.87,1.28,.295,-2.82,7.76,.345,.012);
  B(2.82,1.28,.295,2.87,7.76,.345,.012);
  B(3.25,1.28,.295,3.30,7.76,.345,.012);
  const count=api.objects&&start!==null?api.objects.length-start:null;
  return {assetId:'energetik_facade_bay',units:'metres',width:6.8,height:9.38,depth:1.1,objectCount:count,materialId:MATERIAL,front:'+Z',openBay:true};
}
module.exports={buildEnergetikFacadeBay,MATERIAL,NATIVE_MATERIAL};
