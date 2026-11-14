'use strict';
// Cheap public chair for the Pripyat square. Dimensions are metres; faces -Z.
// Reusable geometry with restrained differences for common abandonment states.
module.exports = function buildPripyatOutdoorChair(api, x=0, y=0, z=0, options={}) {
  const angle=Number.isFinite(options.angle)?options.angle:0;
  const scale=Number.isFinite(options.scale)?options.scale:1;
  const variant=options.variant||'worn_intact';
  const missing=variant==='missing_seat_slat'?'seat':variant==='missing_back_slat'?'back':null;
  const c=Math.cos(angle),s=Math.sin(angle);
  const P=(a,b,d)=>[x+scale*(a*c+d*s),y+scale*b,z+scale*(-a*s+d*c)];
  const start=api.objects?.length??null;
  const frameStart=start;
  const tube=(a,b,r,mat='seatSupportPaint')=>api.capsule(P(...a),P(...b),r*scale,mat);
  const board=(a,b,d,w,h,dep,r,mat='seatWood')=>{
    const q=[P(a,b,d),P(a+w,b+h,d+dep)];
    api.roundBox(Math.min(q[0][0],q[1][0]),Math.min(q[0][1],q[1][1]),Math.min(q[0][2],q[1][2]),Math.max(q[0][0],q[1][0]),Math.max(q[0][1],q[1][1]),Math.max(q[0][2],q[1][2]),r*scale,mat);
  };
  // Two welded side frames: narrow bent tube, plain feet and short arm rails.
  for(const side of [-1,1]){
    const a=side*.235;
    tube([a,.045,.205],[a,.445,.205],.018);
    tube([a,.445,.205],[a,.835,.265],.018);
    tube([a,.045,.205],[a,.045,-.245],.018);
    tube([a,.045,-.245],[a,.445,-.245],.018);
    tube([a,.445,-.245],[a,.445,.205],.018);
    tube([a,.445,-.245],[a,.445,.075],.019,'seatSteel');
    tube([a,.445,.075],[a,.445,.205],.019);
    // Small flattened end caps and selective paint loss at joints.
    api.ellipse(...P(a,.048,.205),.021*scale,.012*scale,.021*scale,'rust');
    if(side===1) tube([a,.405,.205],[a,.405,.245],.008,'rust');
  }
  const frameCount=start===null?null:api.objects.length-frameStart;
  // Three separate rough-cut seat slats and four simple back boards.
  const slatStart=start===null?null:api.objects.length;
  for(let i=0;i<3;i++){
    if(missing==='seat'&&i===1)continue;
    board(-.207,.425,-.214+i*.137,.414,.016,.095,.008,'seatWood');
  }
  for(let i=0;i<4;i++){
    if(missing==='back'&&i===2)continue;
    const yy=.505+i*.078, zz=.105+i*.047;
    board(-.207,yy,zz,.414,.060,.026,.009,'seatWood');
  }
  // A broken middle seat board is represented by a loose splintered remnant.
  if(variant==='broken_slat'){
    tube([-.19,.438,-.055],[-.035,.421,-.045],.014,'rust');
  }
  const count=start===null?null:api.objects.length-start;
  const slatCount=start===null?null:api.objects.length-slatStart;
  return {schema:'m3d-asset-instance-v1',asset:'pripyat-cheap-outdoor-chair',variant,
    origin:[x,y,z],rotationY:angle,scale,dimensionsMetres:[.50,.85,.54],
    objectRange:start===null?null:[start,start+count],objectCount:count,
    components:[{id:'painted_tube_frame',objectCount:frameCount,materialIds:['prp_steel_painted','prp_steel_rusted']},
      {id:'weathered_wood_slats',objectCount:slatCount,materialIds:['prp_wood_weathered']}],
    damageState:variant};
};
