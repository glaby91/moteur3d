#!/usr/bin/env node
// V17: deterministic ten-pass native image-feedback campaign.
// Each pass starts from the latest ACCEPTED scene, modifies one design aspect,
// renders five fixed cameras using the real V17 DX11 WARP raster backend,
// measures image/latency deltas, and accepts or rolls back. No AI renders.
'use strict';
const fs=require('node:fs');
const path=require('node:path');
const cp=require('node:child_process');
const crypto=require('node:crypto');
const zlib=require('node:zlib');
const arguments_=process.argv.slice(2);
const opt=(flag,defaultValue)=>{
  const i=arguments_.indexOf(flag);
  return i<0?defaultValue:arguments_[i+1];
};
const source=path.resolve(opt('--scene','scenes/v16/benchmark-civic'));
const binary=path.resolve(opt('--binary','build-v17/Release/moteur3d_v17_benchmark.exe'));
const output=path.resolve(opt('--output','build-v17/total-simulation/palais-10-pass'));
const files=['scene.json','geometry/benchmark.json',
  'instances/leger.json','materials/pripyat-reuse.json',
  'pbr/pripyat-reuse-leger.json','cameras/views.json',
  'lighting/day.json','environment/site.json',
  'textures/energetik_fresco_mosaic.jpg',
  'textures/personnage_facade_mosaique.jpg'];
const cameras=[
  {id:'facade',hour:14},{id:'passerelle',hour:14},
  {id:'vue_haut',hour:14},{id:'piscine_soir',hour:19},
  {id:'hall',hour:19}
];
const objectives=[
 'BASE: unchanged V17.368',
 '01 monument: slimmer figures, smaller heads and emblem',
 '02 trees: taller narrower poplar and birch canopies',
 '03 landscaping: scaled petals, ground-cover and shrubs',
 '04 facade: more discreet cornices and pilasters',
 '05 rooftop: tauter banners and thinner neon strokes',
 '06 pool: smaller flags and refined rail thickness',
 '07 hall: slimmer artwork frames and luminaires',
 '08 PBR: lower saturation and higher natural roughness',
 '09 lighting: gentler indoor fixtures and warm roof accents',
 '10 finish: reduce roof clutter and tune remaining details'
];
const readJson=file=>JSON.parse(fs.readFileSync(file,'utf8'));
const writeJson=(file,obj)=>fs.writeFileSync(file,JSON.stringify(obj,null,2)+'\n','utf8');
const clone=obj=>JSON.parse(JSON.stringify(obj));
const round=x=>Math.round(x*1000)/1000;
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const force=(yes,message)=>{if(!yes)throw new Error(message);};
const shared=Object.fromEntries(files.filter(x=>x.endsWith('.json')).map(x=>
  [x,readJson(path.join(source,x))]));
force(fs.existsSync(binary),'Missing Windows V17 benchmark binary: '+binary);
for(const file of files)force(fs.existsSync(path.join(source,file)),
  'Missing benchmark scene resource: '+file);
fs.mkdirSync(output,{recursive:true});
const originalArt=files.filter(x=>x.endsWith('.jpg')).map(x=>sha(path.join(source,x)));
const stateFromShared=()=>clone(shared);
const prefix=(o,s)=>o.parent.startsWith(s);
function applyPass(state,n){
  const objects=state['instances/leger.json'].instances;
  const materials=state['materials/pripyat-reuse.json'].materials;
  const lights=state['lighting/day.json'].lights;
  let edits=0;
  function changeObjects(test,edit){
    for(const o of objects)if(test(o)){edit(o);edits++;}
  }
  function sizes(o,kx,ky,kz){o.size=[round(o.size[0]*kx),round(o.size[1]*ky),round(o.size[2]*kz)];}
  if(n===1){
    changeObjects(o=>prefix(o,'Place/Monument soviétique fictif/Silhouettes')&&
      /Torse|Epaules|Bassin|Taille/.test(o.parent),o=>sizes(o,.86,1.07,.86));
    changeObjects(o=>prefix(o,'Place/Monument soviétique fictif/Silhouettes')&&
      /Tete|Cheveux/.test(o.parent),o=>sizes(o,.82,.86,.86));
    changeObjects(o=>prefix(o,'Place/Monument soviétique fictif/Etoile'),o=>sizes(o,.76,.76,.80));
  }else if(n===2){
    changeObjects(o=>prefix(o,'Nature/Arbres/Houppier'),o=>{
      sizes(o,.79,1.11,.81);o.position[1]=round(o.position[1]+.23);
    });
    changeObjects(o=>o.parent==='Nature/Arbres',o=>sizes(o,.94,1.03,.94));
  }else if(n===3){
    changeObjects(o=>prefix(o,'Place/Butte végétalisée/Petales'),o=>sizes(o,.78,.83,.78));
    changeObjects(o=>prefix(o,'Place/Butte végétalisée/Fleurs et feuillages')&&
      /^cse_flower_/.test(o.material),o=>sizes(o,.84,.83,.84));
    changeObjects(o=>prefix(o,'Place/Jardins/Arbustes'),o=>sizes(o,1.04,.84,1.04));
  }else if(n===4){
    changeObjects(o=>prefix(o,'Architecture/Palais/Facade/Corniches'),o=>sizes(o,1,.70,.80));
    changeObjects(o=>prefix(o,'Architecture/Palais/Facade/Pilastres'),o=>sizes(o,.85,1,.82));
    changeObjects(o=>prefix(o,'Architecture/Palais/Facade/Soubassement'),o=>sizes(o,.89,.79,.80));
  }else if(n===5){
    changeObjects(o=>prefix(o,'Architecture/Palais/Banderoles toit/Tissu'),o=>sizes(o,.97,.88,.76));
    changeObjects(o=>prefix(o,'Architecture/Palais/Banderoles toit/Insigne'),o=>sizes(o,.81,.81,.81));
    changeObjects(o=>prefix(o,'Architecture/Palais/Enseigne lumineuse toit/Neon rouge'),o=>sizes(o,1,.76,.76));
    changeObjects(o=>prefix(o,'Architecture/Palais/Enseigne lumineuse toit/Ampoules'),o=>sizes(o,.75,.75,.75));
  }else if(n===6){
    changeObjects(o=>prefix(o,'Interieur/Piscine olympique/Fanions/Tissu'),o=>sizes(o,.86,.76,.85));
    changeObjects(o=>prefix(o,'Interieur/Piscine olympique/Garde-corps'),o=>sizes(o,.82,1,.82));
    changeObjects(o=>prefix(o,'Interieur/Piscine olympique/Echelles/Barres'),o=>sizes(o,.85,1,.85));
  }else if(n===7){
    changeObjects(o=>prefix(o,'Interieur/Hall/Fresque/Cadre'),o=>sizes(o,.88,.88,.90));
    changeObjects(o=>prefix(o,'Interieur/Hall/Plafonniers/Chassis'),o=>sizes(o,1,.75,.86));
    changeObjects(o=>prefix(o,'Interieur/Hall/Plafonniers/Diffuseur'),o=>sizes(o,1,.80,.80));
  }else if(n===8){
    const overrides={
      cse_flower_red:{color:[.48,.23,.20],rough:.92},
      cse_flower_gold:{color:[.65,.48,.25],rough:.92},
      cse_flower_blue:{color:[.34,.42,.51],rough:.92},
      cse_flower_purple:{color:[.43,.34,.47],rough:.92},
      cse_monument_bronze:{color:[.34,.29,.24],rough:.54},
      cse_banner_red:{color:[.47,.17,.15],rough:.94}
    };
    for(const m of materials){
      const cfg=overrides[m.id];
      if(cfg){m.baseColor=cfg.color;m.roughness=cfg.rough;edits++;}
    }
  }else if(n===9){
    for(const l of lights){
      if(l.position[1]>16.4&&l.position[1]<17.1){
        l.intensity=round(l.intensity*.92);edits++;
      }else if(l.position[1]===24){
        l.intensity=round(l.intensity*.88);edits++;
      }else if(l.position[1]>7&&l.position[1]<8){
        l.intensity=round(l.intensity*.95);edits++;
      }
    }
  }else if(n===10){
    changeObjects(o=>prefix(o,'Architecture/Palais/Toiture techniques'),o=>sizes(o,.95,.78,.88));
    changeObjects(o=>prefix(o,'Place/Banc ovale béton/Assise'),o=>sizes(o,1,.90,1));
    changeObjects(o=>prefix(o,'Place/Monument soviétique fictif/Socle/Insigne'),o=>sizes(o,.86,.86,.86));
    changeObjects(o=>prefix(o,'Interieur/Piscine olympique/Plots'),o=>sizes(o,.94,.98,.94));
  }
  force(edits>0,'Pass '+n+' changed no scene elements');
  return edits;
}
function check(state){
  const ins=state['instances/leger.json'].instances;
  const mats=state['materials/pripyat-reuse.json'].materials;
  const lights=state['lighting/day.json'].lights;
  const exact=p=>ins.filter(x=>x.parent===p);
  const names=new Set(ins.map(x=>x.id));
  force(mats.length===39&&lights.length===171,'Materials or light rig changed unexpectedly');
  force(names.size===ins.length,'Duplicate object ids');
  force(ins.length>=3000&&ins.length<=5000,'Bad instance total');
  force(ins.every(o=>o.size.every(x=>x>0&&Number.isFinite(x))&&
    o.position.every(Number.isFinite)),'Invalid instance size or position');
  force(exact('Nature/Arbres').length===26,'Civic trees disappeared');
  force(exact('Place/Banc ovale béton/Assise').length===96,'Oval bench changed');
  force(exact('Interieur/Piscine olympique/Fanions/Tissu').length===20,'Pool flags missing');
  force(exact('Architecture/Palais/Banderoles toit/Tissu').length===10,'Roof fabric missing');
  force(ins.filter(x=>x.material==='cse_personnage_facade').length===1&&
    ins.filter(x=>x.material==='cse_energetik_fresco').length===1,
    'Existing historic/supplied artworks lost');
  force(exact('Interieur/Piscine olympique/Fond')[0].size[0]===50&&
    exact('Interieur/Piscine olympique/Fond')[0].size[2]===25,
    'Olympic pool dimensions changed');
  force(state['cameras/views.json'].cameras.length===11,'Camera set changed');
  return ins.length;
}
function saveVariant(state,directory){
  for(const file of files){
    const dest=path.join(directory,file);
    fs.mkdirSync(path.dirname(dest),{recursive:true});
    if(file.endsWith('.jpg'))fs.copyFileSync(path.join(source,file),dest);
    else writeJson(dest,state[file]);
  }
  for(let i=0;i<originalArt.length;i++)
    force(sha(path.join(directory,files.filter(x=>x.endsWith('.jpg'))[i]))===originalArt[i],
      'Artwork bytes changed');
}
function imageStats(pngPath){
  const data=fs.readFileSync(pngPath);
  force(data.length>1200&&data.subarray(1,4).toString()==='PNG','Empty/corrupt PNG '+pngPath);
  let pos=8,width=0,height=0,depth=0,kind=0,palette=null;
  const idat=[];
  while(pos+8<=data.length){
    const n=data.readUInt32BE(pos),name=data.toString('ascii',pos+4,pos+8);
    const chunk=data.subarray(pos+8,pos+8+n);pos+=12+n;
    if(name==='IHDR'){width=chunk.readUInt32BE(0);height=chunk.readUInt32BE(4);depth=chunk[8];kind=chunk[9];}
    if(name==='IDAT')idat.push(chunk);
    if(name==='PLTE')palette=chunk;
    if(name==='IEND')break;
  }
  force(depth===8&&width>0&&height>0&&[0,2,3,4,6].includes(kind),
    'Unsupported PNG format (need 8-bit PNG)');
  const bpp={0:1,2:3,3:1,4:2,6:4}[kind],stride=width*bpp;
  const raw=zlib.inflateSync(Buffer.concat(idat));
  const row=Buffer.alloc(stride),last=Buffer.alloc(stride);
  let offset=0,count=0,sum=0,sum2=0,saturation=0,edge=0,edges=0,prevValue=0;
  for(let y=0;y<height;y++){
    const filter=raw[offset++];
    for(let i=0;i<stride;i++){
      const u=raw[offset++],a=i>=bpp?row[i-bpp]:0,b=last[i],c=i>=bpp?last[i-bpp]:0;
      let v;
      if(filter===0)v=u;
      else if(filter===1)v=u+a;
      else if(filter===2)v=u+b;
      else if(filter===3)v=u+Math.floor((a+b)/2);
      else if(filter===4){
        const p=a+b-c,da=Math.abs(p-a),db=Math.abs(p-b),dc=Math.abs(p-c);
        v=u+(da<=db&&da<=dc?a:db<=dc?b:c);
      }else throw Error('Unsupported PNG row filter '+filter);
      row[i]=v&255;
    }
    if(y%8===0){
      prevValue=0;
      for(let x=0;x<width;x+=8){
        let r,g,b;
        const off=x*bpp;
        if(kind===2||kind===6){r=row[off];g=row[off+1];b=row[off+2];}
        else if(kind===3){const i=3*row[off];r=palette[i];g=palette[i+1];b=palette[i+2];}
        else{r=g=b=row[off];}
        const l=(.2126*r+.7152*g+.0722*b);
        sum+=l;sum2+=l*l;count++;
        const max=Math.max(r,g,b),min=Math.min(r,g,b);
        saturation+=max-min;
        if(x){edge+=Math.abs(l-prevValue);edges++;}
        prevValue=l;
      }
    }
    row.copy(last);
  }
  const avg=sum/count;
  return {brightness:round(avg),contrast:round(Math.sqrt(Math.max(0,sum2/count-avg*avg))),
    saturation:round(saturation/count),horizontalEdge:round(edge/Math.max(1,edges)),
    bytes:data.length,sha256:sha(pngPath),width,height};
}
function runCamera(scene,destination,camera){
  fs.mkdirSync(destination,{recursive:true});
  const args=[path.join(scene,'scene.json'),destination,'--smoke',
    '--camera-id',camera.id,'--hour',String(camera.hour)];
  const done=cp.spawnSync(binary,args,{
    encoding:'utf8',maxBuffer:6*1024*1024,timeout:180000,
    env:{...process.env,M3D_V16_FORCE_WARP:'1',M3D_PRIMARY_RAYS_25:'0'}
  });
  force(!done.error&&done.status===0,
    camera.id+' native V17 render failed: '+String(done.error||done.stderr||done.stdout).slice(-900));
  const folders=fs.readdirSync(destination,{withFileTypes:true})
    .filter(x=>x.isDirectory()).map(x=>path.join(destination,x.name));
  force(folders.length===1,'Expected one native V17 output directory for '+camera.id);
  const dir=folders[0],summary=readJson(path.join(dir,'summary.json'));
  force(summary.failed===0&&summary.successful>0,
    'Native renderer reports failed cases for '+camera.id);
  const image=path.join(dir,'images','raster_'+camera.id+'_640x360_depth1.png');
  force(fs.existsSync(image),'No native WARP image: '+image);
  const stat=imageStats(image);
  const rec=summary.cases.find(x=>x.backend==='V17_D3D11_Raster');
  force(rec&&rec.status==='ok','Raster benchmark missing for '+camera.id);
  const target=path.join(path.dirname(scene),'images',camera.id+'.png');
  fs.mkdirSync(path.dirname(target),{recursive:true});
  fs.copyFileSync(image,target);
  return {camera:camera.id,hour:camera.hour,file:path.relative(output,target).replace(/\\/g,'/'),
    meanMs:round(Number(rec.meanMs)),...stat};
}
function csvEscape(v){const t=String(v??'');return /[,"\n]/.test(t)?'"'+t.replace(/"/g,'""')+'"':t;}
const rows=[],outcomes=[];
let accepted=stateFromShared(),previous=null,approvedCount=0;
for(let pass=0;pass<=10;pass++){
  const code='pass-'+String(pass).padStart(2,'0');
  const directory=path.join(output,code),scene=path.join(directory,'scene');
  const candidate=clone(accepted);
  const changes=pass===0?0:applyPass(candidate,pass);
  const objects=check(candidate);
  saveVariant(candidate,scene);
  console.log('PASS '+pass+'/10 '+objectives[pass]+' | '+changes+' edits | '+objects+' objects');
  const images=[];
  for(const cam of cameras){
    const result=runCamera(scene,path.join(directory,'raw',cam.id),cam);
    images.push(result);
    console.log('  '+cam.id+' '+result.meanMs+' ms '+result.brightness+
      ' luma '+result.contrast+' contrast');
  }
  const meanMs=round(images.reduce((a,v)=>a+v.meanMs,0)/images.length);
  const baseline=pass===0?images:previous.images;
  const diff=images.filter((x,i)=>x.sha256!==baseline[i].sha256).length;
  const bad=images.some(x=>x.brightness<=4||x.brightness>=251||x.contrast<=2||
    x.bytes<2000||x.meanMs<=0);
  const slow=pass>0&&meanMs>previous.meanMs*2.5;
  const accept=pass===0||(!bad&&!slow&&diff>=1);
  const reason=pass===0?'baseline':
    bad?'image sanity check failed':slow?'render slowdown >2.5x':
    diff===0?'no changed visual pixels':'passed deterministic image and performance checks';
  if(accept){
    accepted=candidate;
    previous={images,meanMs,code};
    if(pass>0)approvedCount++;
  }
  outcomes.push({pass,code,objective:objectives[pass],edits:changes,
    instances:objects,meanMs,changedImages:diff,decision:accept?'KEEP':'ROLLBACK',
    reason,images});
  rows.push([pass,objectives[pass],changes,objects,meanMs,diff,
    accept?'KEEP':'ROLLBACK',reason]);
  writeJson(path.join(output,'campaign-progress.json'),{
    sourceCommit:process.env.GITHUB_SHA||null,passesCompleted:pass,
    approved:approvedCount,results:outcomes});
  console.log('  '+(accept?'KEEP':'ROLLBACK')+' — '+reason);
}
const csv=[['pass','objective','object_edits','instances','mean_ms','changed_cameras',
  'decision','reason'],...rows].map(r=>r.map(csvEscape).join(',')).join('\n')+'\n';
fs.writeFileSync(path.join(output,'passes.csv'),csv,'utf8');
writeJson(path.join(output,'campaign-summary.json'),{
  nativeBackend:'V17_D3D11_Raster / Windows WARP',passesRequested:10,
  passesRendered:10,baselineRendered:true,passesKept:approvedCount,
  passesRolledBack:10-approvedCount,finalAcceptedScene:previous.code,
  note:'Image and performance checks are automated safety proxies, not artistic taste ratings. Inspect the five PNGs for each pass.',
  results:outcomes});
fs.writeFileSync(path.join(output,'README.txt'),
  'Native engine V17 iterative scene review; five fixed cameras at 14h/19h.\n'+
  'Open pass-XX/images/*.png for the 11 x 5 before/after comparisons.\n'+
  'See passes.csv, campaign-summary.json and each pass-XX/scene/scene.json.\n'+
  'KEEP means structural/render/image sanity accepted, not human-reviewed artistry.\n');
console.log('Ten real native passes completed; '+approvedCount+' kept, '+
  (10-approvedCount)+' rejected. Results '+output);
