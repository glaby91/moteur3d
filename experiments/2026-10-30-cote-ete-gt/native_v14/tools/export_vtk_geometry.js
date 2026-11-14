// Exact scene primitive extraction: calls the real Energetik chair asset, not a proxy.
const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..'),objects=[];
const buildEnergetikCinemaSeat=require(path.join(root,'assets/pripyat/energetik_cinema_seat.js'));
const emit=(type,args)=>objects.push({type,args});
const ctx={objects,console,Math,
 box:(...a)=>emit('box',a),
 roundBox:(...a)=>emit('roundBox',a),
 capsule:(...a)=>emit('capsule',a),
 ellipse:(...a)=>emit('ellipsoid',a),
 ellipsoidMesh:(...a)=>emit('ellipsoid',a),
 buildEnergetikCinemaSeat};
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(path.join(root,'scene_pripyat_central.js'),'utf8')+'\nmakeScene();',ctx,{timeout:30000});
const output=process.argv[2]||path.join(root,'vtk_scene_geometry.json');
fs.writeFileSync(output,JSON.stringify({schema:'m3d-vtk-primitive-v2',source:'scene_pripyat_central.js',chairAsset:'assets/pripyat/energetik_cinema_seat.js',objects}));
console.log('Exported '+objects.length+' original scene primitives to '+output);
