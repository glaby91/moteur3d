#!/usr/bin/env node
// Mechanical wheel rig manifest for the future real-time V15/C++ backend.
// The geometry is owned by scene_pripyat_central.js; don't duplicate positions.
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const sceneDir=path.resolve(__dirname,'..');
const scene=fs.readFileSync(path.join(sceneDir,'scene_pripyat_central.js'),'utf8');
const api=vm.runInNewContext(scene+`;({spec:M3D_FERRIS_WHEEL,site:m3dWheelSite,
 pose:m3dWheelPose,expansion:M3D_PRIPYAT_EXPANSION_DATA})`,{}, {timeout:10000});
const seconds=Number(process.env.M3D_WHEEL_TIME_SECONDS||0);
const rpm=Number(process.env.M3D_WHEEL_RPM||1/6);
const site=api.site(), pose=api.pose(seconds,rpm);
const manifest={
  schema:'m3d-pripyat-ferris-wheel-mechanism/v1',
  id:api.spec.id,
  source:'scene_pripyat_central.js',
  geolocation:{latitude:api.spec.gps.lat,longitude:api.spec.gps.lon,
    positionAccuracy:'source ride coordinate; scene transform NOT metrically calibrated',
    projection:{anchorX:api.expansion.anchorX,anchorZ:api.expansion.anchorZ,
      scale:api.expansion.scale,theta:api.expansion.theta}},
  site,
  geometricAssumptions:{
    wheelDiameterMetres:api.spec.diameter,
    axleHeightMetres:api.spec.axleHeight,
    gondolaCount:api.spec.numberOfGondolas,
    nominalRotationSeconds:api.spec.nominalRotationSeconds,
    rotationAxis:[0,0,1]
  },
  parts:[
    {id:'support-fixed',type:'static',parent:null,
      content:['feet','double A-frames','stationary axle','bearings','gear motor','platform']},
    {id:'rotor',type:'dynamic-rigid',parent:'support-fixed',
      origin:pose.hub,axis:[0,0,1],angleRad:pose.phaseRad,
      content:['outer dual hoops','spokes','diagonal braces','hubs','gondola pivot pins']},
    ...pose.gondolas.map(g=>({
      id:g.id,type:'dynamic-gravity-pendulum',parent:'rotor',
      worldPivot:g.pivot,localCounterRotationRad:g.localCounterRotationRad,
      worldPitchRad:g.worldCabinPitchRad,
      content:['hanger brackets','yellow basket','sun canopy']
    }))
  ],
  motion:{seconds,rotationRpm:rpm,wheelAngleRad:pose.phaseRad,
    cabinBehavior:'independent pivot; counter-rotate the rotor to stay upright',
    nominalDirection:'positive radians about +Z',
    application:'EXPORT-TIME POSE ONLY until native renderer supports per-part transforms',
    nativeImplementation:['preserve base meshes and stable object IDs',
      'update rotor transform on CPU/GPU without rebuilding vertices',
      'apply local gondola counter-rotation against rotor phase',
      'refit changed BVH instance bounds, or use a separate dynamic BVH',
      'advance phase from frame delta-time; never rotate support chassis']},
  rights:'Procedural geometry and PBR only; no imported stock images or third-party CAD.'
};
const out=path.join(sceneDir,'scene_pripyat_wheel_mechanism.json');
fs.writeFileSync(out,JSON.stringify(manifest,null,2)+'\n');
console.log('Pripyat articulated wheel manifest:',out,
  'parts:',manifest.parts.length,'seconds:',seconds,'rpm:',rpm);
