// Sole scene definition shared by the HTML preview and native exporter (V13.14).
// Geometry helpers such as box(), roundBox(), cone(), capsule() and meshTriangle()
// are provided by source_v1314.html and are available when makeScene() runs.
// Increase tessellation so generated meshes contain about 50% more triangles.
const M3D_SCENE_DETAIL_GAIN = Math.sqrt(1.5);
const m3dSceneDetailSteps = n => Math.max(3, Math.round((n || 1) * M3D_SCENE_DETAIL_GAIN));
function m3dSceneEllipsoid(...args){ args[7]=m3dSceneDetailSteps(args[7] ?? 20); args[8]=m3dSceneDetailSteps(args[8] ?? 14); return ellipsoidMesh(...args); }
function m3dSceneLoft(...args){ args[3]=m3dSceneDetailSteps(args[3] ?? 2); args[4]=m3dSceneDetailSteps(args[4] ?? 2); return loftedSmoothMesh(...args); }
function m3dSceneTorus(...args){ args[6]=m3dSceneDetailSteps(args[6] ?? 64); args[7]=m3dSceneDetailSteps(args[7] ?? 14); return torusWheelMesh(...args); }

function makeScene(){
 objects.length=0;
 // Long coastal road, with the shoreline running along its right side.
 box(-4.85,-.03,-260,4.85,-.005,24,'asphalt');
 for(let z=19;z>-246;z-=5.8)roundBox(-.075,.008,z-.1,.075,.024,z+2.3,.035,'paint');
 // Pale shoulder and sparse, low guardrail toward the water.
 box(4.86,-.012,-180,5.55,.012,24,'sand');
 // Promenade côtière pavée de 3,1 m, séparée de la chaussée par un petit bordurage.
 box(5.02,-.01,-260,8.10,.035,24,'concrete');
 roundBox(4.88,.01,-260,5.05,.13,24,.05,'stone');
 for(let z=22;z>-258;z-=3.6)roundBox(5.08,.037,z,8.04,.043,z+.025,.008,'stone');
 for(let z=20;z>-242;z-=5.5){
  cone([8.22,.02,z],[8.22,.73,z],.055,.045,'metal');
 }
 capsule([8.22,.66,23],[8.22,.66,-242],.045,'metal');
 capsule([8.22,.29,23],[8.22,.29,-242],.025,'metal');
 // Sable clair sur l’estran, puis surface d’océan ondulée à relief doux.
 // Le maillage reste au-dessus du plan d’eau de fond et reçoit la lumière spéculaire.
 const waveCols=Math.round(31*M3D_SCENE_DETAIL_GAIN),waveRows=Math.round(113*M3D_SCENE_DETAIL_GAIN),waveX0=12.0,waveZ0=-260,waveDx=1.45,waveDz=2.5;
 // Houle croisée : longues ondulations, vagues secondaires et crêtes plus raides.
 const wavePhase=(x,z)=>x*.235+z*.043+1.05*Math.sin(z*.026);
 const waveCrest=(x,z)=>Math.sin(wavePhase(x,z)+.42*Math.sin(x*.48-z*.078));
 const waveHeight=(x,z)=>{
  const p=wavePhase(x,z),q=x*.48-z*.078+Math.sin(z*.062)*.55;
  return .58+.34*Math.sin(p)+.12*Math.sin(q)+.06*Math.sin(x*.91+z*.17)+.24*Math.pow(Math.max(0,waveCrest(x,z)),6);
 };
 const waveVertex=(x,z)=>{
  const e=.12,dx=(waveHeight(x+e,z)-waveHeight(x-e,z))/(2*e),dz=(waveHeight(x,z+e)-waveHeight(x,z-e))/(2*e);
  return {p:[x,waveHeight(x,z),z],n:norm(-dx,1,-dz),uv:[x*.34,z*.34],crest:waveCrest(x,z)};
 };
 const waveGrid=[];
 for(let j=0;j<=waveRows;j++){
  const row=[];for(let i=0;i<=waveCols;i++){const z=waveZ0+j*waveDz,x=waveX0+.55*Math.sin(z*.08)+.30*Math.sin(z*.21)+i*waveDx;row.push(waveVertex(x,z));}waveGrid.push(row);
 }
 for(let j=0;j<waveRows;j++)for(let i=0;i<waveCols;i++){
  const a=waveGrid[j][i],b=waveGrid[j][i+1],c=waveGrid[j+1][i+1],d=waveGrid[j+1][i];
  const crest=(a.crest+b.crest+c.crest+d.crest)*.25,height=(a.p[1]+b.p[1]+c.p[1]+d.p[1])*.25;
  const mat=(crest>.88&&height>.94)||(i===0&&crest>.48)?'foam':'sea';
  meshTriangle(a,b,d,mat);meshTriangle(b,c,d,mat);
 }
 // Relief continu à deux plans, avec sommets arrondis et crêtes irrégulières.
 const makeMountainRange=(baseX,width,z0,z1,maxHeight,seed,mat)=>{
  const profiles=[];
  for(let k=0,z=z0-24;z<z1+24;k++,z+=18){
   profiles.push({z,a:.48+hash2(k,seed,41)*.52,w:8+hash2(k,seed+7,83)*7});
  }
  const heightAt=(x,z)=>{
   const t=clamp((x-baseX)/width,0,1),crossSection=Math.pow(Math.max(0,Math.sin(PI*t)),.92);
   let ridge=.08;
   for(const peak of profiles){const d=(z-peak.z)/peak.w;ridge+=peak.a*Math.exp(-.5*d*d);}
   const variation=.94+.045*Math.sin(z*.16+seed)+.025*Math.sin(z*.39+seed*1.7);
   return maxHeight*crossSection*Math.min(1.12,ridge)*variation;
  };
  const nx=Math.round(Math.ceil(width/2.4)*M3D_SCENE_DETAIL_GAIN),nz=Math.round(Math.ceil((z1-z0)/3.5)*M3D_SCENE_DETAIL_GAIN),dx=width/nx,dz=(z1-z0)/nz,grid=[];
  for(let j=0;j<=nz;j++){
   const row=[];for(let i=0;i<=nx;i++){
    const x=baseX+i*dx,z=z0+j*dz,e=.28,y=heightAt(x,z);
    const sx=(heightAt(x+e,z)-heightAt(x-e,z))/(2*e),sz=(heightAt(x,z+e)-heightAt(x,z-e))/(2*e);
    row.push({p:[x,y,z],n:norm(-sx,1,-sz),uv:[x*.08,z*.055]});
   }grid.push(row);
  }
  for(let j=0;j<nz;j++)for(let i=0;i<nx;i++){
   const a=grid[j][i],b=grid[j][i+1],c=grid[j+1][i+1],d=grid[j+1][i];
   meshTriangle(a,d,b,mat);meshTriangle(b,d,c,mat);
  }
 };
 makeMountainRange(55,38,-280,40,11.5,303,'mountain');
 makeMountainRange(76,51,-280,40,17.5,571,'mountainFar');
 for(const [x,z,r] of [[-6,-3,.58],[-6.7,-18,.44],[-5.8,-35,.68],[6.2,-12,.32]])
  m3dSceneEllipsoid(x,.18,z,r,.17,r*.72,'stone',16,10);
 // Rochers de plage dispersés de façon irrégulière, avec tailles et groupes variés.
 for(let i=0;i<76;i++){
  const z=-250+hash2(i,2,37)*270,shore=12+.55*Math.sin(z*.08)+.30*Math.sin(z*.21);
  let x=8.70+hash2(i,1,71)*Math.max(1.0,shore-9.0);
  if(z>-13&&z<-3&&x>8.65&&x<9.30)x=9.45+hash2(i,3,77)*.65;
  const rx=.16+hash2(i,4,37)*.48,rz=.14+hash2(i,5,37)*.46,ry=.10+hash2(i,6,37)*.22;
  m3dSceneEllipsoid(x,ry*.78,z,rx,ry,rz,'stone',10+(i%3)*2,6+(i%2));
  if(i%9===0)m3dSceneEllipsoid(x+rx*.45,ry*.56,z+rz*.32,rx*.48,ry*.65,rz*.58,'stone',10,6);
 }
 // Promenade continue dans cette zone : la dalle en bois surélevée et ses deux bornes basses ont été retirées.
 // Bancs en bois et acier, orientés vers la mer et espacés le long de la promenade.
 const makeBench=(x,z)=>{
  for(let slat=0;slat<4;slat++){
   const sx=x-.31+slat*.18;
   roundBox(sx,.45,z-.86,sx+.11,.51,z+.86,.025,'wood');
  }
  for(const dz of [-.64,.64]){
   capsule([x-.25,.06,z+dz],[x-.25,.44,z+dz],.035,'metal');
   capsule([x+.25,.06,z+dz],[x+.25,.44,z+dz],.035,'metal');
   capsule([x-.36,.44,z+dz],[x-.36,1.08,z+dz],.035,'metal');
   capsule([x-.36,.69,z+dz],[x+.31,.69,z+dz],.032,'metal');
  }
  for(let slat=0;slat<3;slat++){
   const y=.64+slat*.17;
   roundBox(x-.41,y,z-.84,x-.35,y+.075,z+.84,.022,'wood');
  }
 };
 for(let z=-26;z>-248;z-=34)makeBench(6.42,z);
 // Quelques rochers bas et touffes d’oyats pour donner une échelle au rivage.
 for(const [x,y,z,rx,rz] of [[9.15,.10,-2.2,.34,.48],[9.55,.13,-5.2,.48,.34],[9.75,.11,-10.8,.38,.52],[9.10,.09,-13.6,.28,.42],[-6.0,.12,-10.5,.43,.36]])
  m3dSceneEllipsoid(x,y,z,rx,.14,rz,'stone',18,10);
 for(const [x,z] of [[8.90,-6.2],[8.92,-9.0],[9.55,-2.5],[9.45,-13.3],[-5.7,-5.4],[-5.9,-15.2],[-6.0,-25.0]]){
  for(let stem=0;stem<4;stem++){
   const dx=(stem-1.5)*.075,lean=(stem%2?1:-1)*.13,height=.36+(stem%3)*.09;
   capsule([x+dx,.02,z],[x+dx+lean,height,z+(stem-1.5)*.045],.018,stem%2?'leaves':'grass');
  }
 }
 // Palmiers côtiers : troncs légèrement courbés, anneaux d’écorce et palmes pennées.
 function makePalm(baseX,baseZ,scale,lean){
  const trunkHeight=5.5*scale,points=[];
  for(let k=0;k<=8;k++){
   const t=k/8;points.push([baseX+lean*t*t, trunkHeight*t, baseZ+.16*scale*t]);
  }
  for(let k=0;k<8;k++){
   const t0=k/8,t1=(k+1)/8,r0=.205*scale*(1-.58*t0),r1=.205*scale*(1-.58*t1);
   cone(points[k],points[k+1],r0,r1,'bark');
  }
  for(let ring=1;ring<=8;ring++){
   const t=ring/12,p=points[Math.min(7,Math.floor(t*8))],r=.205*scale*(1-.58*t);
   m3dSceneEllipsoid(p[0],trunkHeight*t,p[2],r,.018*scale,r,'wood',12,6);
  }
  const crown=points[8],crownY=trunkHeight;
  // Small central spear and nine radial fronds.
  cone([crown[0],crownY-.08*scale,crown[2]],[crown[0]+lean*.08,crownY+.62*scale,crown[2]],.095*scale,.018*scale,'leaves');
  for(let frond=0;frond<12;frond++){
   const angle=frond*TAU/12+.18,dx=Math.cos(angle),dz=Math.sin(angle),sideX=-dz,sideZ=dx,length=3.05*scale;
   const at=t=>[crown[0]+dx*length*t+lean*.12*t,crownY+.20*scale*Math.sin(PI*t)-.98*scale*t*t,crown[2]+dz*length*t];
   let previous=at(0);
   for(let seg=1;seg<=6;seg++){
    const t=seg/6,next=at(t);cone(previous,next,.055*scale*(1-(seg-1)/7),.016*scale,'wood');previous=next;
   }
   for(let leaf=1;leaf<=9;leaf++){
    const t=leaf/10,start=at(t),blade=(.82-.30*t)*scale,drop=(.10+.30*t)*scale;
    for(const side of [-1,1]){
     const end=[start[0]+side*sideX*blade,start[1]-drop,start[2]+side*sideZ*blade];
     cone(start,end,.043*scale,.005*scale,(leaf+frond)%3===0?'grass':'leaves');
    }
   }
  }
  for(let fruit=0;fruit<4;fruit++){
   const a=fruit*TAU/4;m3dSceneEllipsoid(crown[0]+Math.cos(a)*.13*scale,crownY-.18*scale,crown[2]+Math.sin(a)*.13*scale,.075*scale,.09*scale,.075*scale,'wood',10,8);
  }
 }
 makePalm(9.75,-18,1.0,.24);makePalm(10.25,-36,.88,-.20);makePalm(9.45,-57,.74,.18);
 makePalm(10.2,-82,.72,-.16);makePalm(9.55,-106,.67,.19);makePalm(10.3,-130,.62,-.13);makePalm(9.7,-154,.58,.16);makePalm(10.25,-180,.54,-.16);makePalm(9.6,-207,.49,.14);makePalm(10.3,-234,.45,-.12);
 // Lampadaires LED à tête plate et bras courbe, inspirés de la référence produit.
 for(let z=-12;z>-260;z-=18){
  const x=5.82;
  cone([x,.06,z],[x,4.42,z],.13,.045,'metal');
  roundBox(x-.17,.06,z-.17,x+.17,.17,z+.17,.035,'metal');
  capsule([x,4.20,z],[x,4.48,z],.060,'metal');
  capsule([x,4.48,z],[x-.12,4.67,z],.055,'metal');
  capsule([x-.12,4.67,z],[x-.38,4.77,z],.052,'metal');
  capsule([x-.38,4.77,z],[x-1.12,4.77,z],.050,'metal');
  // Corps aluminium aplati avec diffuseur inférieur.
  roundBox(x-1.28,4.65,z-.18,x-.55,4.80,z+.18,.095,'metal');
  roundBox(x-1.22,4.60,z-.145,x-.62,4.66,z+.145,.060,'lamp');
  roundBox(x-.69,4.67,z-.17,x-.54,4.76,z+.17,.040,'metal');
 }
 // A single small weathered coastal marker keeps the road readable at distance.
 roundBox(-4.62,.12,-24,-4.50,1.42,-23.88,.04,'metal');
 roundBox(-4.83,1.10,-24.04,-4.28,1.48,-23.72,.08,'sign');
 // Le soleil est dessiné une seule fois par skyRGB, dans la direction de la lumière.
 // Pas de sphère 3D ici : elle se décalait du disque du ciel selon la caméra.
 // Hero coupe: isolated in open road so the car, paint and chrome carry the frame.

 // V13.14 — silhouette redessinée d’un seul trait depuis le fastback Manta initial.
 // Long capot, habitacle reculé, pavillon bas, lunette plongeante et épaules arrière larges.
 const tx=-2.05,tz=-13.5;
 const carX=v=>tx-(v-tx)*1.08,carY=v=>Math.max(.02,v*.92-.15),carZ=v=>tz-(v-tz)*1.28;
 const carPoint=p=>[carX(p[0]),carY(p[1]),carZ(p[2])];
 const carLoftedSmoothMesh=(cx,stations,mat,longSteps=2,crossSteps=2)=>{
  const expanded=stations.map(p=>[...p,p[3]]);
  const wheelOffsets=[-1.22,1.18],archRadius=.215,clearance=.02;
  const archSideBottom=(dz,bodyBottom)=>{
   const d=Math.abs(dz),circle=.58+Math.sqrt(Math.max(0,archRadius*archRadius-Math.min(d,archRadius)**2))+clearance;
   const q=clamp((.34-d)/.12,0,1),fade=q*q*(3-2*q);
   return Math.max(bodyBottom,bodyBottom+Math.max(0,circle-bodyBottom)*fade);
  };
  for(const center of wheelOffsets)for(const dz of [-.34,-.30,-.26,-.23,-.20,-.17,-.14,-.11,0,.11,.14,.17,.20,.23,.26,.30,.34]){
   const z=center+dz,coincident=expanded.find(p=>Math.abs(p[0]-z)<1e-7);
   if(coincident){coincident[5]=archSideBottom(dz,coincident[3]);continue;}
   let i=0;while(i<stations.length-2&&stations[i+1][0]<z)i++;
   const a=stations[i],b=stations[i+1],u=clamp((z-a[0])/(b[0]-a[0]),0,1);
   const sample=a.map((v,k)=>k===0?z:v+(b[k]-v)*u);sample.length=5;
   sample.push(archSideBottom(dz,sample[3]));expanded.push(sample);
  }
  expanded.sort((a,b)=>a[0]-b[0]);
  return m3dSceneLoft(carX(cx),expanded.map(p=>[carZ(tz+p[0]),p[1]*1.12,p[2]*1.12,carY(p[3]),carY(p[4]),carY(p[5])]),mat,longSteps,crossSteps);
 };
 const carFlatQuadMesh=(pts,mat,n)=>flatQuadMesh(pts.map(carPoint),mat,n);
 const carCapsule=(a,b,r,mat)=>capsule(carPoint(a),carPoint(b),r,mat);
 const carRoundBox=(x1,y1,z1,x2,y2,z2,r,mat)=>roundBox(Math.min(carX(x1),carX(x2)),Math.min(carY(y1),carY(y2)),Math.min(carZ(z1),carZ(z2)),Math.max(carX(x1),carX(x2)),Math.max(carY(y1),carY(y2)),Math.max(carZ(z1),carZ(z2)),r,mat);
 const carEllipse=(x,y,z,rx,ry,rz,mat)=>ellipse(carX(x),carY(y),carZ(z),rx,ry,rz,mat);
 const bodyStations=[
  [-2.12,.73,.60,.42,.84],[-1.94,.83,.68,.37,.96],[-1.65,.94,.78,.34,1.04],
  [-1.38,.98,.82,.34,1.07],[-1.22,.98,.82,.36,1.07],[-1.02,.94,.79,.37,1.06],
  [-.78,.88,.74,.39,1.05],[-.56,.84,.70,.41,1.03],[-.28,.83,.69,.42,1.03],
  [.02,.84,.70,.43,1.03],[.28,.85,.72,.43,1.02],[.54,.86,.74,.44,1.01],
  [.78,.91,.78,.46,1.00],[1.02,.99,.84,.45,1.00],[1.20,1.03,.88,.43,.99],
  [1.42,1.00,.85,.42,.96],[1.62,.88,.74,.44,.86],[1.70,.70,.58,.46,.78]
 ];
 // Capot, flancs, épaules et coffre : une coque principale, sans doublon de toit.
 carLoftedSmoothMesh(tx,bodyStations,'sportsPaint',34,20);
 const atBodyZ=z=>{
  let i=0;while(i<bodyStations.length-2&&bodyStations[i+1][0]<z)i++;
  const a=bodyStations[i],b=bodyStations[i+1],u=clamp((z-a[0])/(b[0]-a[0]),0,1);
  return a.map((v,k)=>v+(b[k]-v)*u);
 };
 const carSideX=(z,y)=>{
  const [,bw,tw,,top]=atBodyZ(z),outer=bw*.92,shoulder=top-.15,bevel=top-.035;
  if(y<=shoulder)return bw*.97;
  if(y<=bevel)return outer+(tw-outer)*(y-shoulder)/(bevel-shoulder);
  if(y<=top)return tw+(tw*.58-tw)*(y-bevel)/(top-bevel);
  const roofBlend=clamp((y-top)/.60,0,1);
  return bw*.92+(bw*.78-bw*.92)*roofBlend;
 };
 const glassPoint=(side,z,y,offset=1.012)=>[tx+side*carSideX(z,y)*offset,y,tz+z];
 // Pavillon court et effilé, porté par les vitres et raccordé à la ligne de toit.
 const roofSections=[[-.61,.77,.56,1.49,1.60],[-.42,.79,.58,1.51,1.63],
  [-.12,.80,.59,1.52,1.64],[.16,.79,.58,1.50,1.62],[.42,.73,.53,1.47,1.54],
  [.60,.69,.48,1.31,1.39],[.79,.66,.43,1.14,1.22],[.99,.67,.39,.99,1.07],[1.12,.69,.36,.94,1.01]]
  .map(([z,bw,tw,bottom,top])=>[carZ(tz+z),bw*1.08,tw*1.08,carY(bottom),carY(top)]);
 m3dSceneLoft(carX(tx),roofSections,'sportsPaint',24,16);
 // Pare-brise large et lunette de hayon plongeante dans le fastback.
 carFlatQuadMesh([[tx-.66,1.055,tz-1.02],[tx-.55,1.57,tz-.61],
  [tx+.55,1.57,tz-.61],[tx+.66,1.055,tz-1.02]],'carGlass',[0,0,1]);
 const rearGlass=[[tx-.51,1.53,tz+.42],[tx-.68,.99,tz+1.22],
  [tx+.68,.99,tz+1.22],[tx+.51,1.53,tz+.42]];
 carFlatQuadMesh(rearGlass,'carGlass',[0,.82,-.57]);
 for(const side of [-1,1])carCapsule([tx+side*.51,1.53,tz+.42],[tx+side*.68,.99,tz+1.22],.018,'metal');
 carCapsule([tx-.51,1.53,tz+.42],[tx+.51,1.53,tz+.42],.018,'metal');
 carCapsule([tx-.68,.99,tz+1.22],[tx+.68,.99,tz+1.22],.018,'metal');
 for(let line=1;line<6;line++){
  const u=line/6,y=1.53-.54*u,z=tz+.42+.80*u;
  carCapsule([tx-.48+u*.04,y,z],[tx+.48-u*.04,y,z],.002,'metal');
 }
 // Vitrages latéraux construits dans les ouvertures entre la coque et le pavillon.
 for(const side of [-1,1]){
  const A=glassPoint(side,-1.00,.96),AT=glassPoint(side,-.61,1.535);
  const BT=glassPoint(side,.16,1.56),B=glassPoint(side,.16,.96);
  const CT=glassPoint(side,.43,1.49),CR=glassPoint(side,.84,.99);
  carFlatQuadMesh([A,AT,BT,B],'carGlass',[-side,0,0]);
  carFlatQuadMesh([B,BT,CT,CR],'carGlass',[-side,0,0]);
  carCapsule(A,AT,.020,'metal');carCapsule(AT,BT,.018,'metal');
  carCapsule(BT,B,.023,'metal');carCapsule(B,CR,.017,'metal');
  carCapsule(CR,CT,.018,'metal');carCapsule(CT,BT,.020,'metal');
  // Prise d’air latérale compacte, affleurante à la large aile arrière.
  const p0=glassPoint(side,.63,.76,1.008),p1=glassPoint(side,.63,.93,1.008);
  const p2=glassPoint(side,1.08,.91,1.008),p3=glassPoint(side,1.08,.74,1.008);
  carFlatQuadMesh([p0,p1,p2,p3],'windowdark',[-side,0,0]);
  for(let vane=0;vane<4;vane++){
   const y=.77+vane*.038;
   carCapsule([tx+side*carSideX(.68,y)*1.016,y,tz+.68],[tx+side*carSideX(1.02,y)*1.016,y,tz+1.02],.008,'metal');
  }
  // Montant B, tour de vitres et ligne de caisse continue.
  carCapsule([tx+side*.78,1.01,tz-.83],[tx+side*.81,1.04,tz+.43],.010,'windowdark');
  const trim=[-.92,-.65,-.30,.05,.40,.72,1.02,1.34,1.58];
  for(let j=0;j<trim.length-1;j++){
   const a=trim[j],b=trim[j+1];
   carCapsule([tx+side*carSideX(a,.75)*.99,.75,tz+a],[tx+side*carSideX(b,.75)*.99,.75,tz+b],.009,'metal');
  }
  const d0=-.83,d1=-.82,d2=.43,d3=.45;
  carCapsule([tx+side*carSideX(d0,.48)*.99,.48,tz+d0],[tx+side*carSideX(d1,.96)*.99,.96,tz+d1],.006,'windowdark');
  carCapsule([tx+side*carSideX(d1,.96)*.99,.96,tz+d1],[tx+side*carSideX(d2,.96)*.99,.96,tz+d2],.006,'windowdark');
  carCapsule([tx+side*carSideX(d2,.96)*.99,.96,tz+d2],[tx+side*carSideX(d3,.48)*.99,.48,tz+d3],.006,'windowdark');
  const hx=carSideX(.20,.92);
  carRoundBox(Math.min(tx+side*(hx-.024),tx+side*(hx+.008)),.88,tz+.06,
   Math.max(tx+side*(hx-.024),tx+side*(hx+.008)),.91,tz+.34,.016,'metal');
  carCapsule([tx+side*.70,1.30,tz-.83],[tx+side*.91,1.33,tz-.96],.024,'metal');
  carEllipse(tx+side*.92,1.35,tz-.98,.045,.060,.082,'metal');
  // Filet cuivre clair soulignant la ligne de caisse sans faux évents sur l’aile.
  carCapsule([tx+side*carSideX(-.25,.68)*.99,.68,tz-.25],
   [tx+side*carSideX(1.42,.68)*.99,.68,tz+1.42],.009,'paint');
 }
 // Long capot Manta avec deux nervures et extracteurs discrets.
 for(const side of [-1,1]){
  carCapsule([tx+side*.31,.99,tz-1.91],[tx+side*.34,1.02,tz-1.10],.014,'sportsPaint');
  carCapsule([tx+side*.82,.87,tz-1.65],[tx+side*.90,.89,tz-1.17],.014,'metal');
  carRoundBox(Math.min(tx+side*.48,tx+side*.69),.985,tz-1.72,
   Math.max(tx+side*.48,tx+side*.69),1.005,tz-1.28,.025,'windowdark');
  for(let l=0;l<4;l++)carCapsule([tx+side*(.51+l*.045),1.01,tz-1.66],
   [tx+side*(.51+l*.045),1.01,tz-1.34],.006,'metal');
 }
 // Face Manta modernisée : calandre horizontale, quatre optiques LED rondes, lame carbone.
 carRoundBox(tx-.49,.61,tz-2.135,tx+.49,.82,tz-2.12,.035,'windowdark');
 for(let slat=0;slat<8;slat++)carRoundBox(tx-.405,.625+slat*.022,tz-2.145,
  tx+.405,.633+slat*.022,tz-2.14,.003,'metal');
 for(const side of [-1,1])for(const x of [.40,.58]){
  carEllipse(tx+side*x,.735,tz-2.145,.073,.073,.036,'metal');
  carEllipse(tx+side*x,.735,tz-2.178,.055,.055,.026,'led');
 }
 carRoundBox(tx-.88,.44,tz-2.16,tx+.88,.54,tz-2.08,.045,'metal');
 carRoundBox(tx-1.02,.28,tz-2.27,tx+1.02,.36,tz-2.10,.03,'windowdark');
 // Arrière court et musclé, petit bec intégré et feux triples.
 carCapsule([tx-.82,.98,tz+1.16],[tx-.78,.91,tz+1.58],.014,'metal');
 carCapsule([tx+.82,.98,tz+1.16],[tx+.78,.91,tz+1.58],.014,'metal');
 carRoundBox(tx-.84,.57,tz+1.63,tx+.84,.82,tz+1.68,.045,'sportsPaint');
 for(const side of [-1,1]){
  for(let lamp=0;lamp<3;lamp++)carRoundBox(Math.min(tx+side*(.57+lamp*.075),tx+side*(.61+lamp*.075)),.65,tz+1.68,
   Math.max(tx+side*(.57+lamp*.075),tx+side*(.61+lamp*.075)),.78,tz+1.70,.018,'taillight');
  carRoundBox(Math.min(tx+side*.84,tx+side*.90),.42,tz+1.69,
   Math.max(tx+side*.84,tx+side*.90),.54,tz+1.83,.045,'metal');
  carCapsule([tx+side*.40,.36,tz+1.58],[tx+side*.40,.36,tz+1.78],.045,'metal');
  carEllipse(tx+side*.40,.36,tz+1.78,.060,.045,.045,'metal');
 }
 carRoundBox(tx-.22,.60,tz+1.70,tx+.22,.73,tz+1.72,.020,'metal');
 carRoundBox(tx-.17,.635,tz+1.725,tx+.17,.70,tz+1.74,.010,'paint');
 carRoundBox(tx-.82,.43,tz+1.72,tx+.82,.53,tz+1.82,.045,'metal');
 // Pneus à hauteur d’aile, débord léger sur les flancs, jantes forgées à 10 branches.
 for(const side of [-1,1])for(const [z,wheelOffset] of [[tz-1.22,1.05],[tz+1.18,1.07]]){
  const wheelX=tx+side*wheelOffset,faceX=wheelX+side*.074,wy=.58;
  m3dSceneTorus(carX(wheelX),carY(wy),carZ(z),.25,.09,'rubber',96,28);
  carEllipse(faceX-side*.05,wy,z,.018,.184,.184,'metal');
  for(let spoke=0;spoke<10;spoke++){
   const a=spoke*TAU/10;
   carCapsule([faceX,wy,z],[faceX,wy+.222*Math.cos(a),z+.222*Math.sin(a)],.016,'wheelMetal');
   carCapsule([faceX+side*.006,wy+.115*Math.cos(a+.12),z+.115*Math.sin(a+.12)],
    [faceX+side*.006,wy+.205*Math.cos(a+.12),z+.205*Math.sin(a+.12)],.010,'metal');
  }
  for(let arc=0;arc<48;arc++){
   const a=arc*TAU/48,b=(arc+1)*TAU/48;
   carCapsule([faceX,wy+.236*Math.cos(a),z+.236*Math.sin(a)],
    [faceX,wy+.236*Math.cos(b),z+.236*Math.sin(b)],.011,'wheelMetal');
  }
  carEllipse(faceX+side*.008,wy,z,.018,.077,.077,'wheelMetal');
  carEllipse(faceX+side*.014,wy,z,.012,.044,.044,'sportsPaint');
  for(let lug=0;lug<5;lug++){
   const a=lug*TAU/5;
   carEllipse(faceX+side*.014,wy+.061*Math.cos(a),z+.061*Math.sin(a),.008,.009,.009,'metal');
  }
  carRoundBox(Math.min(faceX-side*.055,faceX),.53,z-.11,
   Math.max(faceX-side*.055,faceX),.73,z-.055,.016,'taillight');
 }
 // Habitacle discret derrière les vitres : planche de bord, sièges bas et volant.
 carRoundBox(tx-.57,.94,tz-.76,tx+.57,1.02,tz-.61,.035,'windowdark');
 for(const side of [-1,1]){
  carRoundBox(tx+side*.38,.98,tz-.03,tx+side*.12,1.43,tz+.24,.085,'windowdark');
  carCapsule([tx+side*.20,1.23,tz-.86],[tx+side*.20,1.23,tz-.55],.012,'metal');
 }
 carCapsule([tx-.20,1.25,tz-.74],[tx+.20,1.25,tz-.74],.018,'metal');
 carCapsule([tx+.12,1.15,tz-.74],[tx+.20,1.25,tz-.74],.014,'metal');

 // Deux voitures secondaires, réduites en détail selon la distance : une rouge dépasse,
 // une violette suit dans la voie opposée. La coque reprend la même silhouette fastback.
 function addTrafficCar(cx,cz,scale,paint){
  const X=x=>cx-(x-cx)*1.08*scale,Y=y=>Math.max(.018,(y*.92-.15)*scale),Z=z=>cz-(z-cz)*1.28*scale;
  const P=p=>[X(p[0]),Y(p[1]),Z(p[2])];
  const sideBottom=(z,bottom)=>{
   let value=bottom;
   for(const center of [-1.22,1.18]){
    const d=Math.abs(z-center),circle=.58+Math.sqrt(Math.max(0,.215*.215-Math.min(d,.215)**2))+.02;
    const q=clamp((.34-d)/.12,0,1),fade=q*q*(3-2*q);
    value=Math.max(value,bottom+Math.max(0,circle-bottom)*fade);
   }
   return value;
  };
  const shell=bodyStations.map(([z,bw,tw,bottom,top])=>[Z(cz+z),bw*1.12*scale,tw*1.12*scale,Y(bottom),Y(top),Y(sideBottom(z,bottom))]);
  m3dSceneLoft(cx,shell,paint,19,12);
  const roof=[[ -.61,.77,.56,1.49,1.60],[-.42,.79,.58,1.51,1.63],[-.12,.80,.59,1.52,1.64],
   [.16,.79,.58,1.50,1.62],[.42,.73,.53,1.47,1.54],[.60,.69,.48,1.31,1.39],
   [.79,.66,.43,1.14,1.22],[.99,.67,.39,.99,1.07],[1.12,.69,.36,.94,1.01]]
   .map(([z,bw,tw,bottom,top])=>[Z(cz+z),bw*1.08*scale,tw*1.08*scale,Y(bottom),Y(top)]);
  m3dSceneLoft(cx,roof,paint,15,10);
  flatQuadMesh([P([cx-.66,1.055,cz-1.02]),P([cx-.55,1.57,cz-.61]),P([cx+.55,1.57,cz-.61]),P([cx+.66,1.055,cz-1.02])],'carGlass',[0,0,1]);
  flatQuadMesh([P([cx-.51,1.53,cz+.42]),P([cx-.68,.99,cz+1.22]),P([cx+.68,.99,cz+1.22]),P([cx+.51,1.53,cz+.42])],'carGlass',[0,.82,-.57]);
  for(const side of [-1,1]){
   const a=P([cx+side*-.78,.97,cz-1.0]),b=P([cx+side*-.60,1.53,cz-.58]),c=P([cx+side*.14,1.53,cz-.58]),d=P([cx+side*.14,.97,cz+.15]),e=P([cx+side*.68,.97,cz+1.08]),f=P([cx+side*.50,1.46,cz+.43]);
   flatQuadMesh([a,b,c,d],'carGlass',[-side,0,0]);flatQuadMesh([d,c,f,e],'carGlass',[-side,0,0]);
  }
  const tbox=(x1,y1,z1,x2,y2,z2,r,mat)=>{
   const a=P([cx+x1,y1,cz+z1]),b=P([cx+x2,y2,cz+z2]);
   roundBox(Math.min(a[0],b[0]),Math.min(a[1],b[1]),Math.min(a[2],b[2]),Math.max(a[0],b[0]),Math.max(a[1],b[1]),Math.max(a[2],b[2]),r*scale,mat);
  };
  tbox(-.49,.61,-2.15,.49,.82,-2.12,.035,'windowdark');
  for(let slat=0;slat<6;slat++)tbox(-.39,.635+slat*.026,-2.16,.39,.642+slat*.026,-2.155,.003,'metal');
  for(const side of [-1,1])for(const lx of [.40,.58]){
   const q=P([cx+side*lx,.735,cz-2.16]);ellipse(q[0],q[1],q[2],.066*scale,.066*scale,.035*scale,'led');
  }
  tbox(-.9,.34,-2.18,.9,.44,-2.08,.04,'metal');tbox(-.85,.52,1.60,.85,.73,1.66,.04,paint);
  for(const side of [-1,1]){
   for(let lamp=0;lamp<3;lamp++)tbox(side*(.57+lamp*.075),.62,1.67,side*(.61+lamp*.075),.75,1.70,.014,'taillight');
   for(const [localZ,offset] of [[-1.22,1.05],[1.18,1.07]]){
    const wx=X(cx+side*offset),wy=Y(.58),wz=Z(cz+localZ),face=wx+side*.07*scale;
    m3dSceneTorus(wx,wy,wz,.25*scale,.086*scale,'rubber',48,14);
    ellipse(face,wy,wz,.018*scale,.182*scale,.182*scale,'wheelMetal');
    for(let spoke=0;spoke<8;spoke++){
     const a=spoke*TAU/8;
     capsule([face,wy,wz],[face,wy+.205*scale*Math.cos(a),wz+.205*scale*Math.sin(a)],.012*scale,'wheelMetal');
    }
    ellipse(face+side*.012*scale,wy,wz,.012*scale,.07*scale,.07*scale,paint);
   }
  }
 }
 function addStandardCar(cx,cz,scale,style,paint){
  const dims=style==='compact'?{length:3.9,width:1.78,roofRear:-.72,roofFront:.48,roof:1.39}
   :style==='wagon'?{length:4.85,width:1.88,roofRear:-1.68,roofFront:.52,roof:1.48}
   :{length:4.45,width:1.84,roofRear:-1.05,roofFront:.50,roof:1.43};
  const L=dims.length*scale,W=dims.width*scale,front=cz+L*.5,rear=cz-L*.5,y=v=>v*scale;
  // Carrosserie profilée en sections lissées, avec capot et épaules plus arrondis.
  const body=[
   [rear,.62,.54,.34,.77],[rear+.18*scale,.80,.72,.29,.91],
   [cz-L*.30,.91,.79,.28,.99],[cz-L*.12,.94,.83,.28,1.02],
   [cz+L*.12,.90,.80,.29,1.00],[front-.28*scale,.82,.73,.31,.96],
   [front,.55,.49,.39,.78]
  ].map(([z,bw,tw,bottom,top])=>[z,bw*scale,tw*scale,y(bottom),y(top)]);
  m3dSceneLoft(cx,body,paint,4,5);
  const cabRear=cz+dims.roofRear*scale,cabFront=cz+dims.roofFront*scale;
  const cabin=[
   [cabRear,.42,.34,y(.86),y(1.13)],[cabRear+.16*scale,.57,.49,y(.88),y(1.34)],
   [cz-.10*scale,.59,.52,y(.89),y(dims.roof)],[cabFront-.12*scale,.51,.43,y(.91),y(dims.roof-.02)],
   [cabFront,.35,.29,y(.94),y(1.16)]
  ];
  m3dSceneLoft(cx,cabin,'carGlass',3,5);
  // Bandeau de toit peint et vitrages latéraux intégrés à la silhouette.
  roundBox(cx-W*.27,y(dims.roof-.045),cabRear+.18*scale,cx+W*.27,y(dims.roof+.015),cabFront-.15*scale,.08*scale,paint);
  for(const side of [-1,1]){
   const x0=Math.min(cx+side*W*.34,cx+side*W*.38),x1=Math.max(cx+side*W*.34,cx+side*W*.38);
   roundBox(x0,y(1.01),cabRear+.23*scale,x1,y(1.25),cabFront-.22*scale,.045*scale,'carGlass');
   capsule([cx+side*W*.48,y(.53),cz-L*.12],[cx+side*W*.48,y(.53),cz+L*.18],.010*scale,'metal');
   for(const axle of [-.33,.33]){
    const wz=cz+axle*L,wx=cx+side*W*.47;
    m3dSceneEllipsoid(wx,y(.37),wz,.15*scale,.31*scale,.32*scale,'rubber',18,10);
    m3dSceneEllipsoid(wx+side*.11*scale,y(.37),wz,.032*scale,.17*scale,.17*scale,'wheelMetal',14,8);
   }
   m3dSceneEllipsoid(cx+side*W*.29,y(.69),front-.015*scale,.12*scale,.085*scale,.05*scale,'led',14,9);
   roundBox(Math.min(cx+side*W*.31,cx+side*W*.44),y(.63),rear+.01*scale,
    Math.max(cx+side*W*.31,cx+side*W*.44),y(.82),rear+.06*scale,.035*scale,'taillight');
   roundBox(Math.min(cx+side*W*.51,cx+side*W*.63),y(.90),cz+.18*scale,
    Math.max(cx+side*W*.51,cx+side*W*.63),y(1.04),cz+.34*scale,.045*scale,'metal');
  }
  roundBox(cx-W*.29,y(.43),front,cx+W*.29,y(.55),front+.06*scale,.045*scale,'metal');
  roundBox(cx-W*.29,y(.43),rear-.06*scale,cx+W*.29,y(.55),rear,.045*scale,'metal');
  roundBox(cx-W*.17,y(.68),front+.005*scale,cx+W*.17,y(.81),front+.035*scale,.035*scale,'windowdark');
 }
 addStandardCar(2.15,-35,.95,'sedan','carSilver');
 addStandardCar(-2.0,-48,.94,'compact','carBlue');
 addStandardCar(2.1,-61,.90,'wagon','carGreen');
}

