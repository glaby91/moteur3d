'use strict';

// Segmented rooftop neon lettering for the Palace of Culture Energetik.
// Coordinates are metres. The sign faces +Z; every short glass-tube section
// is its own native capsule and receives a stable id in the returned manifest.
const LETTER_STROKES = Object.freeze({
  'Э': [
    [[0.94,1.43],[0.45,1.43],[0.20,1.20],[0.16,0.94],[0.34,0.75],[0.16,0.55],[0.20,0.27],[0.45,0.06],[0.94,0.06]],
    [[0.36,0.75],[0.95,0.75]]
  ],
  'Н': [ [[0.12,0.06],[0.12,1.43]], [[0.98,0.06],[0.98,1.43]], [[0.12,0.75],[0.98,0.75]] ],
  'Е': [ [[0.15,0.06],[0.15,1.43]], [[0.15,1.43],[1.00,1.43]], [[0.15,0.75],[0.88,0.75]], [[0.15,0.06],[1.00,0.06]] ],
  'Р': [ [[0.14,0.06],[0.14,1.43],[0.82,1.43],[1.00,1.25],[1.00,0.93],[0.82,0.76],[0.14,0.76]] ],
  'Г': [ [[0.15,0.06],[0.15,1.43],[1.02,1.43]] ],
  'Т': [ [[0.08,1.43],[1.04,1.43]], [[0.56,1.43],[0.56,0.06]] ],
  'И': [ [[0.13,0.06],[0.13,1.43]], [[0.13,0.06],[0.99,1.43]], [[0.99,1.43],[0.99,0.06]] ],
  'К': [ [[0.14,0.06],[0.14,1.43]], [[0.14,0.76],[0.98,1.43]], [[0.14,0.76],[0.98,0.06]] ]
});

function buildEnergetikRoofNeonSign(api, x=0, y=0, z=0, options={}) {
  const {box, roundBox, capsule}=api;
  const scale=Number.isFinite(options.scale)?options.scale:1;
  const title=options.title||'ЭНЕРГЕТИК';
  const letterW=1.22*scale, letterH=1.62*scale, gap=.20*scale;
  const sectionLength=options.sectionLength||.245*scale;
  const neonRadius=options.neonRadius||.026*scale;
  const outerRadius=options.outerRadius||.044*scale;
  const frontZ=z+.38*scale;
  const originY=y+.23*scale;
  const totalW=title.length*letterW+(title.length-1)*gap;
  const left=x-totalW*.5;
  const objectStart=Array.isArray(api.objects)?api.objects.length:null;
  const parts=[];
  const register=(id,kind,first,count,extra={})=>parts.push({id,kind,firstObject:first,objectCount:count,...extra});
  const addTube=(a,b,r,mat,id,letter)=>{
    const first=Array.isArray(api.objects)?api.objects.length:null;
    capsule(a,b,r,mat);
    if (first!==null && api.objects[first]) api.objects[first].assetPartId=id;
    return {id,letter,firstObject:first,objectCount:first===null?null:api.objects.length-first,radius:r/scale};
  };
  const line=(points,mat,rad,id,letter,split=true)=>{
    const segments=[];
    for(let k=0;k<points.length-1;k++){
      const a=points[k],b=points[k+1];
      const len=Math.hypot(b[0]-a[0],b[1]-a[1],b[2]-a[2]);
      const n=split?Math.max(1,Math.ceil(len/sectionLength)):1;
      for(let i=0;i<n;i++){
        const t0=i/n+(i?0.018:0),t1=(i+1)/n-(i<n-1?0.018:0);
        if(t1<=t0)continue;
        const p0=a.map((v,j)=>v+(b[j]-v)*t0),p1=a.map((v,j)=>v+(b[j]-v)*t1);
        const id0=`${id}_s${k+1}_${String(i+1).padStart(2,'0')}`;
        segments.push(addTube(p0,p1,rad,mat,id0,letter));
      }
    }
    return segments;
  };

  // Steel frame, rear mounting rails, triangulated stays and insulated feed box.
  const frameStart=Array.isArray(api.objects)?api.objects.length:null;
  // Open truss, not a solid backing board: the neon stays legible through the frame.
  capsule([x-totalW*.5-.18*scale,y+.10*scale,z-.24*scale],[x+totalW*.5+.18*scale,y+.10*scale,z-.24*scale],.065*scale,'rust');
  capsule([x-totalW*.5-.18*scale,y+1.94*scale,z-.24*scale],[x+totalW*.5+.18*scale,y+1.94*scale,z-.24*scale],.065*scale,'rust');
  capsule([x-totalW*.5-.14*scale,y+.10*scale,z+.24*scale],[x+totalW*.5+.14*scale,y+.10*scale,z+.24*scale],.025*scale,'metal');
  capsule([x-totalW*.5-.14*scale,y+1.88*scale,z+.24*scale],[x+totalW*.5+.14*scale,y+1.88*scale,z+.24*scale],.025*scale,'metal');
  for(const sx of [-1,1]){
    const px=x+sx*(totalW*.5+.08*scale);
    capsule([px,y+.10*scale,z-.24*scale],[px,y+1.92*scale,z-.24*scale],.045*scale,'rust');
    capsule([px,y+.10*scale,z-.24*scale],[px-sx*.38*scale,y+1.92*scale,z-.24*scale],.032*scale,'metal');
  }
  for(let i=1;i<5;i++){
    const px=x-totalW*.5+totalW*i/5;
    capsule([px,y+.18*scale,z-.23*scale],[px,y+1.86*scale,z-.23*scale],.026*scale,'rust');
    capsule([px-.36*scale,y+.18*scale,z-.23*scale],[px+.36*scale,y+1.86*scale,z-.23*scale],.018*scale,'metal');
  }
  // Twelve individual ceramic insulators and two transformer housings.
  for(let i=0;i<12;i++){
    const px=x-totalW*.5+totalW*(i+.5)/12;
    capsule([px,y+.10*scale,z-.15*scale],[px,y+.29*scale,z-.15*scale],.055*scale,'stone');
    capsule([px,y+.29*scale,z-.15*scale],[px,y+.39*scale,z-.15*scale],.034*scale,'metal');
  }
  for(const sx of [-1,1]){
    const tx=x+sx*(totalW*.5+.30*scale);
    roundBox(tx-.20*scale,y+.37*scale,z-.40*scale,tx+.20*scale,y+.70*scale,z-.12*scale,.05*scale,'rust');
    for(let j=0;j<4;j++)box(tx-.14*scale+j*.09*scale,y+.44*scale,z-.405*scale,tx-.10*scale+j*.09*scale,y+.63*scale,z-.39*scale,'metal');
  }
  register('support_frame','steel_frame',frameStart,frameStart===null?null:api.objects.length-frameStart,{materialIds:['prp_steel_rusted','prp_steel_painted']});

  const letterRecords=[];
  for(let index=0;index<title.length;index++){
    const ch=title[index],strokes=LETTER_STROKES[ch];
    if(!strokes)throw new Error(`Unsupported neon glyph: ${ch}`);
    const lx=left+index*(letterW+gap), letterStart=Array.isArray(api.objects)?api.objects.length:null;
    const tubeSegments=[];
    for(let strokeIndex=0;strokeIndex<strokes.length;strokeIndex++){
      const p=strokes[strokeIndex].map(([u,v])=>[lx+u*letterW,originY+v*letterH,frontZ]);
      // Secondary outer glass and bright inner core are separately segmented.
      tubeSegments.push(...line(p,'glass',outerRadius,`glyph_${index+1}_${ch}_tube${strokeIndex+1}_glass`,ch));
      tubeSegments.push(...line(p,'neon',neonRadius,`glyph_${index+1}_${ch}_tube${strokeIndex+1}_core`,ch));
      // Visible steel clips hold the tube at regular intervals along the trace.
      for(let k=0;k<p.length-1;k++){
        const a=p[k],b=p[k+1],len=Math.hypot(b[0]-a[0],b[1]-a[1]);
        const count=Math.floor(len/(.34*scale));
        for(let q=1;q<=count;q++){
          const t=q/(count+1),cx=a[0]+(b[0]-a[0])*t,cy=a[1]+(b[1]-a[1])*t;
          capsule([cx,cy,z+.14*scale],[cx,cy,frontZ-.025*scale],.012*scale,'metal');
        }
      }
    }
    const letterCount=letterStart===null?null:api.objects.length-letterStart;
    letterRecords.push({id:`letter_${index+1}_${ch}`,glyph:ch,firstObject:letterStart,objectCount:letterCount,tubeSegments});
    register(`letter_${index+1}_${ch}`,'neon_letter',letterStart,letterCount,{tubeSegmentCount:tubeSegments.length});
  }

  // Insulated feeder wire leaves the lettering in separate circuits; the
  // metadata keeps each of the nine letters addressable for later lighting control.
  const wireStart=Array.isArray(api.objects)?api.objects.length:null;
  for(let i=0;i<title.length;i++){
    const cx=left+i*(letterW+gap)+letterW*.5;
    capsule([cx,originY-.07*scale,z+.24*scale],[cx,originY-.07*scale,frontZ],.012*scale,'metal');
  }
  register('segmented_feeder_wiring','wiring',wireStart,wireStart===null?null:api.objects.length-wireStart);
  const objectEnd=Array.isArray(api.objects)?api.objects.length:null;
  return {
    schema:'m3d-asset-instance-v1',asset:'energetik_roof_neon_sign',title,
    units:'metres',origin:[x,y,z],scale,dimensions:{width:totalW+.82*scale,height:2.02*scale,depth:.82*scale},
    face:'+Z',objectRange:objectStart===null?null:[objectStart,objectEnd],objectCount:objectStart===null?null:objectEnd-objectStart,
    electricalSegmentation:{independentCircuits:title.length,letters:letterRecords,parts},
    lighting:{nativeMaterial:'neon',lightPositions:letterRecords.map((_,i)=>[left+i*(letterW+gap)+letterW*.5,originY+.76*letterH,frontZ+.18*scale])}
  };
}

module.exports={buildEnergetikRoofNeonSign,LETTER_STROKES};
