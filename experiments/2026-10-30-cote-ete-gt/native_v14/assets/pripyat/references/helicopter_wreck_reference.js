'use strict';
// Standalone, UNPLACED helicopter wreck reference asset.
// Identification is provisional: photo alone is insufficient to certify subtype.
// Coordinate convention: nose toward local -Z; Y up. Dimensions are estimates.
// Not imported by scene_pripyat_central.js or any runtime exporter.
function buildHelicopterWreckReference(api, options={}) {
  const {box,capsule,ellipsoidMesh,cone}=api;
  for(const fn of ['box','capsule','ellipsoidMesh','cone'])
    if(typeof api[fn]!=='function') throw new Error('Missing primitive '+fn);
  const scale=options.scale??1;
  if(!Number.isFinite(scale)||scale<=0)throw new Error('Invalid scale');
  const at=(x,y,z)=>[x*scale,y*scale,z*scale];
  const B=(x0,y0,z0,x1,y1,z1,m)=>box(x0*scale,y0*scale,z0*scale,x1*scale,y1*scale,z1*scale,m);
  const C=(a,b,r,m)=>capsule(at(...a),at(...b),r*scale,m);
  const E=(x,y,z,rx,ry,rz,m,s=20,t=12)=>ellipsoidMesh(x*scale,y*scale,z*scale,rx*scale,ry*scale,rz*scale,m,s,t);
  const K=(a,b,r0,r1,m)=>cone(at(...a),at(...b),r0*scale,r1*scale,m);
  // Approximate Mi-24-like tandem cockpit wreck for inspection ONLY.
  // Exposed asymmetrical interior: pale corroded shell, missing cockpit glazing.
  E(0,1.58,-.40,1.13,.90,2.45,'wreckPaint',36,20);
  E(0,1.54,-2.20,.82,.68,1.18,'wreckMetal',28,16);
  E(0,1.72,-3.08,.62,.52,.66,'wreckPaint',24,14);
  // Front and rear cockpit coamings. Do not place opaque glass across openings.
  for(const [zz,rx,ry] of [[-2.64,.66,.63],[-.96,.84,.75]]){
    for(let i=0;i<22;i++){
      const a=i*Math.PI*2/22,b=(i+1)*Math.PI*2/22;
      C([rx*Math.cos(a),1.87+ry*Math.sin(a),zz],
        [rx*Math.cos(b),1.87+ry*Math.sin(b),zz],.038,'wreckRust');
    }
  }
  // Exposed consoles, seats and frames.
  for(const z of [-2.48,-.75]){
    B(-.34,1.0,z-.23,.34,1.21,z+.28,'wreckInterior');
    B(-.34,1.19,z+.22,.34,1.96,z+.36,'wreckInterior');
    B(-.42,1.32,z-.63,.42,1.47,z-.36,'wreckMetal');
    C([-.50,1.05,z-.50],[-.50,1.9,z+.32],.028,'wreckRust');
    C([.50,1.05,z-.50],[.50,1.9,z+.32],.028,'wreckRust');
  }
  // Incomplete canopy ribs / riveted frames, photo-guided silhouette.
  for(const z of [-3.0,-2.7,-2.1,-1.25,-.55]){
    const rx=z< -1.5?.63:.87,ry=z< -1.5?.49:.76;
    for(let i=0;i<12;i++){
      const a=Math.PI*i/12,b=Math.PI*(i+1)/12;
      C([rx*Math.cos(a),1.9+ry*Math.sin(a),z],
        [rx*Math.cos(b),1.9+ry*Math.sin(b),z],.030,'wreckMetal');
    }
  }
  // Rotating-wing mounting, stripped tail boom, and support stubs.
  E(0,2.56,.28,.46,.17,.49,'wreckRust');
  C([0,2.5,.22],[0,3.10,.22],.12,'wreckMetal');
  K([0,1.72,1.28],[0,1.44,6.95],.55,.12,'wreckPaint');
  E(0,1.45,6.9,.18,.20,.48,'wreckRust');
  // Side stub wings and remaining weapon pylons, incomplete by design.
  for(const side of [-1,1]){
    C([side*.88,1.55,.25],[side*2.65,1.35,1.35],.15,'wreckPaint');
    C([side*1.64,1.45,.72],[side*1.73,.70,1.05],.055,'wreckRust');
    C([side*.70,.76,-1.70],[side*.82,.24,-1.55],.050,'wreckMetal');
    C([side*.82,.24,-1.55],[side*.98,.24,.75],.055,'wreckMetal');
    for(let i=0;i<9;i++){
      const z=-2.8+i*.55;
      E(side*(.95+(i%3)*.015),1.22,z,.048,.045,.070,'wreckRust',8,6);
    }
  }
  // Loose fragments and corroded underbody exposed after dismantling.
  B(-.70,.55,-2.92,.70,.66,-2.65,'wreckRust');
  B(-.62,.63,1.00,.62,.71,1.25,'wreckMetal');
  return Object.freeze({
    id:'helicopter-wreck-reference',type:'standalone-reference',placedInPripyat:false,
    estimatedLengthMetres:11.0*scale,uncertainty:'provisional subtype and dimensions',
    materials:['wreckPaint','wreckRust','wreckMetal','wreckInterior'],
    coordinateSystem:'nose toward -Z, Y up'
  });
}
module.exports={buildHelicopterWreckReference};
