// Rapture scene study — V13.14 scene-only source.
// Detailed Art Deco street, dome, bathysphere station and street furniture.
// No characters. Geometry is deliberately assembled from modeled scene parts,
// not a low-poly/blockout city. Edit this file to revise the scene.

const M3D_MATERIAL_REFLECTIVITY_PERCENT = Object.freeze({
  raptureGlass: 0, sea: 45, puddle: 38, metal: 48, neon: 12,
  windowlit: 4, lamp: 2, raptureStone: 4, raptureBronze: 24, concrete: 3, brick: 2
});
// Thin glazing transmits straight-through rays; Fresnel-like reflections are
// controlled independently by the reflectivity table above.
const M3D_MATERIAL_TRANSMISSION_PERCENT = Object.freeze({ raptureGlass: 82 });
const M3D_MATERIAL_IOR = Object.freeze({ raptureGlass: 1.52 });
const M3D_SCENE_LIGHTS = [];

function makeScene() {
  objects.length = 0;

  // --- Street bed, sidewalks, drains and tiled borders --------------------
  box(-30, -.42, -78, 30, -.06, 18, 'puddle');
  box(-30, -.08, -78, 30, -.035, 18, 'concrete');
  // Broad wet-stone avenue around a narrow central water runnel, as in the
  // bathysphere approach images; keep paving above the engine's asphalt ground.
  box(-14,.012,-78,-1.20,.11,18,'raptureStone');
  box(1.20,.012,-78,14,.11,18,'raptureStone');
  for(let z=-76;z<17;z+=2.4){
    roundBox(-8.0,.112,z,-1.34,.142,z+.045,.018,'metal');
    roundBox(1.34,.112,z,8.0,.142,z+.045,.018,'metal');
    for(const x of [-12.5,-10.2,-8,-5.8,-3.6,-1.8,1.8,3.6,5.8,8,10.2,12.5])
      roundBox(x,.112,z+.32,x+.035,.138,z+2.08,.014,'concrete');
  }
  for(const x of [-1.18,1.18])
    roundBox(x-.10,.09,-78,x+.10,.25,18,.07,'raptureBronze');
  // Two curved-edged side promenades with raised bronze curb details.
  for (const side of [-1, 1]) {
    const x0 = side < 0 ? -29.5 : 14.8, x1 = side < 0 ? -14.8 : 29.5;
    box(x0, -.015, -78, x1, .12, 18, 'raptureStone');
    box(side < 0 ? -14.95 : 14.8, .09, -78, side < 0 ? -14.8 : 14.95, .26, 18, 'metal');
    for (let z = -76; z < 17; z += 2.6) {
      // Narrow brass joints and carved drainage channels break up the paving.
      roundBox(x0 + .18, .125, z, x1 - .18, .145, z + .035, .012, 'metal');
      for (let x = x0 + .55; x < x1 - .4; x += 2.4)
        roundBox(x, .13, z + .48, x + .045, .16, z + 1.98, .018, 'concrete');
    }
    for (let z = -72; z < 16; z += 8.0) {
      roundBox(side < 0 ? -9.03 : 8.82, .26, z, side < 0 ? -8.86 : 8.99, .34, z + 7.5, .055, 'metal');
    }
  }
  // Central runnel and repeated grates; shallow water is represented by a
  // textured reflecting surface with modeled joints and drain slots.
  box(-1.15, -.025, -58, 1.15, .006, 18, 'sea');
  for (let z = -76; z < 17; z += 1.8) {
    roundBox(-.95, .012, z, .95, .035, z + .055, .02, 'metal');
    for (let x = -.72; x <= .72; x += .24)
      roundBox(x, .014, z + .24, x + .035, .026, z + 1.45, .012, 'raptureStone');
  }

  // --- Reusable facade craft ---------------------------------------------
  function faceTriangle(x, a, b, c, material, normalX) {
    const n = [normalX, 0, 0];
    const v = p => ({p, n, uv:[p[2] * .08, p[1] * .08]});
    meshTriangle(v([x,a[0],a[1]]), v([x,b[0],b[1]]), v([x,c[0],c[1]]), material);
  }
  // Raised fan relief above doors/windows, with individual radiating ribs.
  function fanRelief(x, y, z, width, height, normalX, material='metal') {
    const center=[y + .04, z], radius=width*.48, tips=[];
    for (let i=0; i<=16; i++) {
      const a=Math.PI*i/16;
      tips.push([y + .04 + Math.sin(a)*radius, z + Math.cos(a)*radius]);
    }
    for (let i=0; i<16; i++) faceTriangle(x, center, tips[i], tips[i+1], material, normalX);
    // Fine raised rays sit slightly proud of the fan plate.
    for (let i=1; i<16; i+=2) {
      const a=Math.PI*i/16, end=[y+.04+Math.sin(a)*radius, z+Math.cos(a)*radius];
      capsule([x,center[0],center[1]],[x,end[0],end[1]],.025,material);
    }
    roundBox(x-.055,y+.02,z-width*.50,x+.055,y+.13,z+width*.50,.035,'raptureBronze');
  }
  function archedWindow(faceX, y, z, width, height, normalX, lit=false) {
    const frame='metal', pane=lit?'windowlit':'raptureGlass', depth=.30;
    // Deep reveal, sill and lintel; pane is inset behind the bronze frame.
    roundBox(faceX-depth,y,z-width*.5,faceX+.035,y+height,z+width*.5,.09,'raptureStone');
    roundBox(faceX-.005,y+.10,z-width*.5+.10,faceX+.08,y+height-.18,z+width*.5-.10,.035,pane);
    for (const dz of [-width*.5,width*.5])
      roundBox(faceX-.12,y-.02,z+dz-.085,faceX+.13,y+height-.10,z+dz+.085,.055,frame);
    roundBox(faceX-.14,y-.08,z-width*.58,faceX+.18,y+.09,z+width*.58,.045,'raptureBronze');
    // Multi-segment arched head and a stepped keystone.
    const spring=y+height-.40, rad=width*.5;
    for (let k=0;k<12;k++) {
      const a0=Math.PI*k/12, a1=Math.PI*(k+1)/12;
      const p0=[faceX,spring+Math.sin(a0)*rad,z+Math.cos(a0)*rad];
      const p1=[faceX,spring+Math.sin(a1)*rad,z+Math.cos(a1)*rad];
      capsule(p0,p1,.065,frame);
    }
    roundBox(faceX-.17,spring+rad-.08,z-.16,faceX+.17,spring+rad+.16,z+.16,.05,'raptureBronze');
    // Mullions are actual slender metal pieces, not painted-on stripes.
    roundBox(faceX-.045,y+.08,z-.045,faceX+.09,y+height-.12,z+.045,.022,frame);
    roundBox(faceX-.045,y+height*.53,z-width*.42,faceX+.09,y+height*.53+.045,z+width*.42,.018,frame);
    fanRelief(faceX+.10,spring+rad+.08,z,width*1.08,.58,normalX,'metal');
  }
  function pilaster(faceX, y0, y1, z, normalX, width=.36) {
    roundBox(faceX-.22,y0,z-width*.5,faceX+.12,y1,z+width*.5,.105,'raptureStone');
    roundBox(faceX-.29,y0+.22,z-width*.62,faceX+.20,y0+.38,z+width*.62,.075,'metal');
    roundBox(faceX-.29,y1-.40,z-width*.62,faceX+.20,y1-.22,z+width*.62,.075,'metal');
    // Fluting: six vertical inlaid rods on each pier.
    for (let i=0;i<6;i++) {
      const zz=z-width*.39+i*width*.156;
      capsule([faceX+.135,y0+.45,zz],[faceX+.135,y1-.48,zz],.022,'raptureBronze');
    }
    const capY=y1-.18;
    roundBox(faceX-.34,capY,z-width*.72,faceX+.26,capY+.13,z+width*.72,.055,'metal');
    // stepped capital with a small geometric crown
    roundBox(faceX-.27,capY+.13,z-width*.58,faceX+.18,capY+.33,z+width*.58,.07,'raptureStone');
    fanRelief(faceX+.19,capY+.24,z,width*.92,.35,normalX,'metal');
  }
  function cornice(faceX, y, z0, z1) {
    for (let band=0;band<5;band++) {
      const d=.12*band, yy=y-band*.14;
      roundBox(faceX-d,yy,z0,faceX+.25+d,yy+.10,z1,.035,band%2?'raptureBronze':'metal');
    }
  }
  function facade(side, z0, z1, floors, height, style=0, setback=0) {
    const faceX=side<0?-14.15-setback:14.15+setback, normalX=side<0?1:-1;
    const backX=side<0?faceX-.75:faceX+.75;
    const material='raptureStone';
    const bayWidth=3.15, first=z0+1.05, openings=[];
    for (let z=first;z<z1-1;z+=bayWidth) for (let floor=0;floor<floors;floor++) {
      const y=2.20+floor*3.35, width=floor===0?1.54:1.30, winH=floor===0?2.18:2.24;
      openings.push({y0:y+.10,y1:y+winH-.18,z0:z-width*.5+.10,z1:z+width*.5-.10});
    }
    // Tile the wall volume around the actual window apertures. A pane now
    // transmits the scene behind it instead of blending over a solid wall.
    const ys=[0,height],zs=[z0,z1];
    for(const o of openings){ys.push(o.y0,o.y1);zs.push(o.z0,o.z1);}
    const uniq=a=>[...new Set(a)].sort((x,y)=>x-y);
    const yCuts=uniq(ys),zCuts=uniq(zs);
    for(let yi=0;yi<yCuts.length-1;yi++)for(let zi=0;zi<zCuts.length-1;zi++){
      const cy=(yCuts[yi]+yCuts[yi+1])*.5,cz=(zCuts[zi]+zCuts[zi+1])*.5;
      if(openings.some(o=>cy>o.y0&&cy<o.y1&&cz>o.z0&&cz<o.z1))continue;
      box(Math.min(faceX,backX),yCuts[yi],zCuts[zi],Math.max(faceX,backX),yCuts[yi+1],zCuts[zi+1],material);
    }
    // Continuous rusticated base with individually modeled courses.
    for (let course=0;course<5;course++)
      roundBox(faceX-.18,course*.42,z0,faceX+.17,course*.42+.09,z1,.035,course%2?'metal':'raptureBronze');
    for (let z=first;z<z1-1;z+=bayWidth) {
      for (let floor=0;floor<floors;floor++) {
        const y=2.20+floor*3.35;
        const width=(floor===0?1.54:1.30), winH=(floor===0?2.18:2.24);
        archedWindow(faceX+.04,y,z,width,winH,normalX,((Math.floor(z/bayWidth)+floor+style)%3!==0));
        if (floor===0) {
          // Door surround and fan pediment at street level.
          roundBox(faceX-.24,.43,z-.81,faceX+.18,2.38,z+.81,.11,'metal');
          roundBox(faceX-.06,.58,z-.62,faceX+.09,2.22,z+.62,.07,'windowdark');
          fanRelief(faceX+.20,2.03,z,1.48,.82,normalX,'raptureBronze');
          roundBox(faceX-.32,2.57,z-.95,faceX+.23,2.73,z+.95,.07,'metal');
        }
      }
      pilaster(faceX+.08,1.25,height-.34,z-.98,normalX,.36);
      // Panel between bays: recessed plaque with layered border and relief.
      const plaqueY=height-.92, plaqueZ=z+bayWidth*.5;
      roundBox(faceX-.16,plaqueY-.58,plaqueZ-.58,faceX+.13,plaqueY+.56,plaqueZ+.58,.12,'raptureBronze');
      roundBox(faceX-.21,plaqueY-.44,plaqueZ-.44,faceX+.17,plaqueY+.42,plaqueZ+.44,.09,'metal');
      fanRelief(faceX+.19,plaqueY-.24,plaqueZ,.72,.63,normalX,'raptureStone');
    }
    // Story bands, ledges and a dentilled cornice cap.
    for (let floor=0;floor<=floors;floor++) {
      const y=2.02+floor*3.35;
      cornice(faceX+.03,y,z0,z1);
      if (floor<floors) {
        for (let z=z0+1.2;z<z1;z+=bayWidth)
          roundBox(faceX-.23,y+.34,z-.16,faceX+.20,y+.46,z+.16,.045,'metal');
      }
    }
    cornice(faceX+.12,height-.15,z0,z1);
    // Parapet crest: small repeated bronze uprights and finials.
    for (let z=z0+.55;z<z1;z+=1.1) {
      roundBox(faceX-.08,height+.02,z-.07,faceX+.20,height+.66,z+.07,.045,'metal');
      cone([faceX+.06,height+.66,z],[faceX+.06,height+.94,z],.11,.005,'raptureBronze');
    }
    return {faceX,normalX};
  }

  // --- Main avenue blocks and richly modeled skyline ---------------------
  facade(-1,-14,14,3,14.5,0,9.0);
  facade(1,-14,14,3,14.5,1);
  // Low quay blocks stop early; the harbor and skyline stay visible beyond.
  facade(-1,-29,-14,4,18.5,1,9.0);
  facade(1,-29,-14,4,18.5,0);
  // Corner towers use stepped, retreating tiers with repeated vertical ribs.
  function tower(cx, z, base, width, height, floors, variant) {
    const x0=cx-width*.5, x1=cx+width*.5, z0=z-width*.38, z1=z+width*.38;
    const mat='raptureStone';
    const floorH=height/floors;
    for(let f=0;f<floors;f++) {
      const y=base+f*floorH;
      const setback=Math.max(0,f-Math.floor(floors*.52))*.12;
      const fx0=x0+setback,fx1=x1-setback,fz0=z0+setback*.62,fz1=z1-setback*.62;
      box(fx0,y,fz0,fx1,y+floorH,fz1,mat);
      // Deep belt courses with alternating bronze and stone.
      roundBox(fx0-.17,y,fz0-.17,fx1+.17,y+.13,fz1+.17,.065,'metal');
      roundBox(fx0-.10,y+floorH-.16,fz0-.10,fx1+.10,y+floorH-.02,fz1+.10,.05,'raptureBronze');
      const floorWidth=fx1-fx0,floorDepth=fz1-fz0;
      const count=Math.max(3,Math.floor(floorWidth/1.48));
      for(let ix=0;ix<count;ix++) for(let iz=0;iz<3;iz++) {
        const wx=fx0+.60+ix*(floorWidth-1.2)/count, wz=fz0+.52+iz*(floorDepth-1.04)/3;
        const w=Math.min(.86,(floorWidth-1.2)/count*.62), h=Math.min(1.34,floorH*.55);
        // Four-sided molded window surround, inset pane and cross muntins.
        roundBox(wx-.12,y+.37,wz-.12,wx+w+.12,y+.37+h+.24,wz+.12,.055,'metal');
        roundBox(wx,y+.48,wz-.045,wx+w,y+.37+h+.13,wz+.045,.025,(f+ix+variant)%2===0?'windowlit':'raptureGlass');
        roundBox(wx+w*.5-.035,y+.48,wz-.065,wx+w*.5+.035,y+.37+h+.13,wz+.065,.018,'raptureBronze');
        roundBox(wx,y+.37+h*.5,wz-.065,wx+w,y+.37+h*.5+.035,wz+.065,.018,'raptureBronze');
        fanRelief(fx1+.04,y+.37+h+.21,wz+w*.5,w+.35,.34,1,'metal');
      }
      // Corner piers with fluting and small inset medallions.
      for(const [px,pz] of [[fx0,fz0],[fx0,fz1],[fx1,fz0],[fx1,fz1]]) {
        roundBox(px-.18,y+.18,pz-.18,px+.18,y+floorH-.18,pz+.18,.09,'raptureBronze');
        for(let k=0;k<4;k++) {
          const a=k*Math.PI/2, xx=px+Math.cos(a)*.14, zz=pz+Math.sin(a)*.14;
          capsule([xx,y+.28,zz],[xx,y+floorH-.28,zz],.026,'metal');
        }
      }
    }
    // Crown steps, recessed attic windows and an Art Deco pinnacle.
    const topInset=Math.max(0,floors-1-Math.floor(floors*.52))*.12;
    let tx0=x0+topInset,tx1=x1-topInset,tz0=z0+topInset*.62,tz1=z1-topInset*.62,ty=base+height;
    for(let step=0;step<5;step++) {
      const inset=.28+step*.08, hh=.38+step*.05;
      roundBox(tx0,ty,tz0,tx1,ty+hh,tz1,.07,step%2?'raptureBronze':'metal');
      ty+=hh;tx0+=inset*.48;tx1-=inset*.48;tz0+=inset*.48;tz1-=inset*.48;
    }
    const spireH=height*.16;
    cone([cx,ty,z],[cx,ty+spireH,z],width*.22,.055,'metal');
    cone([cx,ty+spireH,z],[cx,ty+spireH+height*.10,z],.16,.006,'raptureBronze');
    for(let k=0;k<8;k++) {
      const a=k*Math.PI/4;
      capsule([cx,ty+height*.05,z],[cx+Math.cos(a)*width*.35,ty+height*.025,z+Math.sin(a)*width*.35],.045,'metal');
    }
  }
  tower(-16,-29,0,8.5,32,12,0);
  tower(17,-32,0,9.2,38,14,1);
  tower(-30,-39,0,9.6,28,11,1);
  tower(31,-42,0,10.0,34,13,0);
  tower(-12,-48,0,8.8,43,16,0);
  tower(13,-51,0,9.0,47,17,1);
  tower(-39,-58,0,10.2,36,14,1);
  tower(40,-61,0,10.8,40,15,0);
  tower(-25,-67,0,9.4,52,19,0);
  tower(25,-70,0,10.0,49,18,1);
  tower(-8,-76,0,8.0,58,21,1);
  tower(9,-78,0,8.6,61,22,0);
  tower(0,-83,0,6.2,64,23,0);
  // Open water remains visible between the staggered downtown towers.
  // Leave the horizon open; a vertical sea backdrop would cut across the
  // glass dome and erase the skyline at the renderer's usual camera angles.

  // --- Monumental entrance portals and cross-street bridges --------------
  function bridge(z, y, width, height) {
    box(-width*.5,y,z-.42,width*.5,y+.45,z+.42,'raptureStone');
    roundBox(-width*.5-.18,y+.45,z-.58,width*.5+.18,y+.74,z+.58,.13,'metal');
    // Three-layer balustrade with individually modeled balusters.
    for(const side of [-1,1]) {
      const edge=side*(width*.5-.25);
      capsule([edge,y+.8,z],[edge,y+1.65,z],.085,'metal');
      capsule([edge,y+1.05,z-.28],[edge,y+1.05,z+.28],.052,'raptureBronze');
      for(let k=0;k<24;k++) {
        const bx=-width*.5+.45+k*(width-.9)/23;
        cone([bx,y+.78,z],[bx,y+1.40,z],.048,.027,'metal');
        roundBox(bx-.065,y+1.38,z-.07,bx+.065,y+1.53,z+.07,.045,'raptureBronze');
      }
      capsule([-width*.5,y+1.56,z+side*.3],[width*.5,y+1.56,z+side*.3],.055,'metal');
    }
    // Pier capitals below the span: layered brackets and hanging acanthus tips.
    for(const x of [-width*.5+1.1,width*.5-1.1]) {
      roundBox(x-.60,y-1.25,z-.72,x+.60,y+.08,z+.72,.16,'raptureStone');
      for(let k=0;k<5;k++) {
        const xx=x-.4+k*.2;
        cone([xx,y-.10,z-.38],[xx,y-1.18,z-.38],.13,.025,'raptureBronze');
      }
      roundBox(x-.9,y-.18,z-.92,x+.9,y+.08,z+.92,.12,'metal');
    }
  }
  bridge(-20,8.0,24,2.1);
  bridge(-43,12.0,28,2.4);
  bridge(-67,16.0,34,2.8);
  // Tall entry arch across the boulevard, built from fluted piers and rings.
  const portalZ=-7.2;
  for(const x of [-7.5,7.5]) {
    for(let k=0;k<8;k++) {
      const zz=portalZ-.20+k*.055;
      cone([x,.35,zz],[x,9.15,zz],.64,.48,k%2?'raptureBronze':'raptureStone');
    }
    roundBox(x-1.0,8.75,portalZ-.45,x+1.0,9.10,portalZ+.40,.26,'metal');
    roundBox(x-.78,9.10,portalZ-.30,x+.78,9.75,portalZ+.22,.17,'raptureStone');
    fanRelief(x<0?x+1.0:x-1.0,9.22,portalZ-.02,1.34,.55,x<0?1:-1,'raptureBronze');
  }
  // Curved crown across portal made of closely segmented modeled voussoirs.
  for(let i=0;i<30;i++) {
    const a0=Math.PI*i/30,a1=Math.PI*(i+1)/30;
    const p0=[Math.sin(a0)*7.5,9.0+Math.cos(a0)*3.2,portalZ];
    const p1=[Math.sin(a1)*7.5,9.0+Math.cos(a1)*3.2,portalZ];
    capsule(p0,p1,.22,i%3?'metal':'raptureBronze');
  }
  // Portal keystone medallion with concentric rings and fan rays.
  const medX=0,medY=12.25,medZ=portalZ;
  for(let ring=0;ring<3;ring++) {
    const r=.94-ring*.19;
    for(let i=0;i<40;i++) {
      const a0=i*Math.PI*2/40,a1=(i+1)*Math.PI*2/40;
      capsule([Math.sin(a0)*r,medY+Math.cos(a0)*r,medZ],
              [Math.sin(a1)*r,medY+Math.cos(a1)*r,medZ],.045,ring%2?'raptureBronze':'metal');
    }
  }
  for(let i=0;i<24;i++) {
    const a=i*Math.PI*2/24;
    cone([Math.sin(a)*.98,medY+Math.cos(a)*.98,medZ],
         [Math.sin(a)*1.42,medY+Math.cos(a)*1.42,medZ],.06,.012,'metal');
  }

  // City identifier: raised block-letter neon mounted above the skyline span.
  // The word is modeled as luminous strokes so it remains legible in this renderer.
  const glyphs={
    R:[[[0,0],[0,1]],[[0,1],[.78,1]],[[.78,1],[.78,.56]],[[.78,.56],[0,.56]],[[.12,.56],[.88,0]]],
    A:[[[0,0],[.18,1]],[[.18,1],[.70,1]],[[.70,1],[.9,0]],[[.1,.48],[.80,.48]]],
    P:[[[0,0],[0,1]],[[0,1],[.82,1]],[[.82,1],[.82,.56]],[[.82,.56],[0,.56]]],
    T:[[[0,1],[.92,1]],[[.46,1],[.46,0]]],
    U:[[[0,1],[0,.12]],[[0,.12],[.16,0]],[[.16,0],[.72,0]],[[.72,0],[.88,.12]],[[.88,.12],[.88,1]]],
    E:[[[.82,1],[0,1]],[[0,1],[0,0]],[[0,.52],[.70,.52]],[[0,0],[.82,0]]]
  };
  const word='RAPTURE', glyphW=1.22, advance=1.63, signZ=-38.0, signY=15.5;
  const total=(word.length-1)*advance+glyphW, startX=-total*.5;
  roundBox(startX-.55,signY-.42,signZ-.28,startX+total+.55,signY+1.75,signZ+.32,.18,'raptureBronze');
  roundBox(startX-.37,signY-.25,signZ-.12,startX+total+.37,signY+1.57,signZ+.22,.10,'sign');
  for(let index=0;index<word.length;index++) {
    const letter=word[index], gx=startX+index*advance;
    for(const [[x0,y0],[x1,y1]] of glyphs[letter]) {
      const a=[gx+x0*glyphW,signY+y0*1.45,signZ+.55];
      const b=[gx+x1*glyphW,signY+y1*1.45,signZ+.55];
      // Bronze channel sits behind the luminous tube so the letters stay clear.
      capsule([a[0],a[1],signZ+.42],[b[0],b[1],signZ+.42],.105,'metal');
      capsule(a,b,.072,'neon');
    }
  }

  // --- Spherical bathysphere terminal, dock and approach stairs -----------
  // Set the bathysphere back between the avenue bridges so it reads as a
  // station destination rather than an oversized object blocking the entrance.
  const bx=0,by=4.4,bz=-35,br=3.7,platformY=by-br-.10;
  ellipsoidMesh(bx,by,bz,br,br,br,'wheelMetal',64,48);
  ellipsoidMesh(bx,by,bz,br*.94,br*.94,br*.94,'metal',64,48);
  // Shell rings, longitudinal ribs, access hatch, bolts and viewport bezels.
  for(let latitude=-3;latitude<=3;latitude++) {
    const theta=(latitude/4)*Math.PI*.5,rr=br*Math.cos(theta),yy=by+br*Math.sin(theta);
    for(let i=0;i<64;i++) {
      const a0=i*Math.PI*2/64,a1=(i+1)*Math.PI*2/64;
      capsule([bx+Math.cos(a0)*rr,yy,bz+Math.sin(a0)*rr],
              [bx+Math.cos(a1)*rr,yy,bz+Math.sin(a1)*rr],.060,'raptureBronze');
    }
  }
  for(let i=0;i<16;i++) {
    const a=i*Math.PI*2/16, pts=[];
    for(let j=0;j<=18;j++) {
      const phi=-Math.PI*.48+j*Math.PI*.96/18;
      pts.push([bx+Math.cos(a)*br*Math.cos(phi),by+br*Math.sin(phi),bz+Math.sin(a)*br*Math.cos(phi)]);
    }
    for(let j=0;j<pts.length-1;j++) capsule(pts[j],pts[j+1],.043,'metal');
    const p=pts[9];
    ellipsoidMesh(p[0],p[1],p[2],.085,.085,.085,'raptureBronze',12,8);
  }
  // Prominent forward porthole, beveled circular rings and radial bolts.
  const portY=by+.15,portZ=bz-br-.06;
  for(const r of [.98,.78,.61]) {
    for(let i=0;i<48;i++) {
      const a0=i*Math.PI*2/48,a1=(i+1)*Math.PI*2/48;
      capsule([Math.cos(a0)*r,portY+Math.sin(a0)*r,portZ],
              [Math.cos(a1)*r,portY+Math.sin(a1)*r,portZ],.075,r>.8?'wheelMetal':'metal');
    }
  }
  ellipsoidMesh(0,portY,portZ+.04,.57,.57,.08,'raptureGlass',40,28);
  for(let i=0;i<16;i++) {
    const a=i*Math.PI*2/16;
    ellipsoidMesh(Math.cos(a)*.91,portY+Math.sin(a)*.91,portZ-.06,.075,.075,.055,'led',10,8);
  }
  // Side portholes and hatch hardware.
  for(const side of [-1,1]) {
    const x=side*(br*.76);
    for(const r of [.62,.47]) {
      for(let i=0;i<36;i++) {
        const a0=i*Math.PI*2/36,a1=(i+1)*Math.PI*2/36;
        capsule([x,by+Math.cos(a0)*r,bz+Math.sin(a0)*r],
                [x,by+Math.cos(a1)*r,bz+Math.sin(a1)*r],.055,'metal');
      }
    }
    ellipsoidMesh(x,by,bz,.42,.42,.42,'raptureGlass',32,22);
    for(let k=0;k<8;k++) {
      const a=k*Math.PI/4;
      ellipsoidMesh(x*1.03,by+Math.cos(a)*.56,bz+Math.sin(a)*.56,.06,.06,.06,'raptureBronze',8,6);
    }
    capsule([side*3.03,by-1.05,bz],[side*3.03,by+1.05,bz],.055,'metal');
  }
  // Docking platform, curved supports, gangway, stairs, rails and chains.
  roundBox(-7.4,platformY-.36,bz-1.55,7.4,platformY-.08,bz+1.55,.30,'raptureStone');
  roundBox(-7.0,platformY-.08,bz-1.44,7.0,platformY+.18,bz+1.44,.20,'metal');
  for(const x of [-5.8,-3.9,3.9,5.8]) {
    capsule([x,.18,bz],[x,platformY-.20,bz],.24,'metal');
    roundBox(x-.44,platformY-.34,bz-.44,x+.44,platformY-.18,bz+.44,.14,'raptureBronze');
    roundBox(x-.22,platformY-.88,bz-.25,x+.22,platformY-.34,bz+.25,.10,'raptureStone');
  }
  // Detailed gangway to the forward hatch.
  for(let i=0;i<12;i++) {
    const z=bz-1.62-i*.16,y=platformY+.12+i*(by-platformY-.75)/12;
    roundBox(-1.35,y,z,1.35,y+.15,z+.20,.06,i%2?'raptureStone':'metal');
  }
  for(const side of [-1,1]) {
    for(let i=0;i<11;i++) {
      const z=bz-1.56-i*.16,y=platformY+.48+i*(by-platformY-.75)/11,x=side*1.58;
      capsule([x,y-.32,z],[x,y+.38,z],.040,'metal');
    }
    capsule([side*1.58,platformY+.55,bz-1.5],[side*1.58,by-.45,bz-3.36],.060,'raptureBronze');
    capsule([side*1.58,platformY+.15,bz-1.5],[side*1.58,platformY+.15,bz-3.36],.045,'metal');
  }
  // Bathysphere mooring cables and pulley blocks.
  for(const side of [-1,1]) {
    capsule([side*6.0,by+1.8,bz],[side*6.0,by+9.0,bz],.095,'metal');
    ellipsoidMesh(side*6.0,by+9.1,bz,.48,.58,.48,'raptureBronze',20,14);
    for(let k=0;k<8;k++) {
      const y=by+1.88+k*.88;
      roundBox(Math.min(side*6.14,side*5.78),y,bz-.19,
               Math.max(side*6.14,side*5.78),y+.12,bz+.19,.05,'wheelMetal');
    }
  }

  // --- Dome framework: layered ribs and glass-panel seams ----------------
  const domeR=43, domeBase=5.0, domePeak=22.0;
  const domeY=x=>domeBase+(domePeak-domeBase)*Math.sqrt(Math.max(0,1-(x/domeR)**2));
  for(let z=-78;z<=12;z+=9.0) {
    // Transverse ribs use many short curved segments to follow the dome.
    for(let i=0;i<24;i++) {
      const x0=-domeR+2*domeR*i/24,x1=-domeR+2*domeR*(i+1)/24;
      capsule([x0,domeY(x0),z],[x1,domeY(x1),z],.19,'wheelMetal');
    }
    // Raised brass rib head and riveted collars at each pane junction.
    for(let i=0;i<=24;i+=2) {
      const x=-domeR+2*domeR*i/24,y=domeY(x);
      ellipsoidMesh(x,y,z,.22,.18,.22,'raptureBronze',10,8);
    }
  }
  // Actual transparent panes span the rib bays. The ray tracer now sees the
  // city and ocean through this continuous curved roof instead of a frame-only arch.
  const domePoint=(x,z)=>[x,domeY(x)-.18,z];
  const domeNormal=x=>{const slope=-(domePeak-domeBase)*x/(domeR*domeR*Math.sqrt(Math.max(.02,1-(x/domeR)**2)));const n=[-slope,1,0],l=Math.hypot(...n);return n.map(v=>v/l);};
  for(let ix=0;ix<24;ix++)for(let iz=0;iz<10;iz++){
    const x0=-domeR+2*domeR*ix/24,x1=-domeR+2*domeR*(ix+1)/24,z0=-78+90*iz/10,z1=-78+90*(iz+1)/10;
    const a={p:domePoint(x0,z0),n:domeNormal(x0),uv:[ix/24,iz/10]},b={p:domePoint(x1,z0),n:domeNormal(x1),uv:[(ix+1)/24,iz/10]},c={p:domePoint(x1,z1),n:domeNormal(x1),uv:[(ix+1)/24,(iz+1)/10]},d={p:domePoint(x0,z1),n:domeNormal(x0),uv:[ix/24,(iz+1)/10]};
    meshTriangle(a,b,c,'raptureGlass');meshTriangle(a,c,d,'raptureGlass');
  }
  // Longitudinal rails keep the glazed dome legible through its full length.
  for(let i=-4;i<=4;i++) {
    const x=i*10.0;
    for(let seg=0;seg<20;seg++) {
      const z0=-78+90*seg/20,z1=-78+90*(seg+1)/20;
      const taper=Math.sqrt(Math.max(.05,1-(x/domeR)**2));
      const y0=domeBase+(domePeak-domeBase)*taper*(.985-.015*Math.cos((z0+39)*.035));
      const y1=domeBase+(domePeak-domeBase)*taper*(.985-.015*Math.cos((z1+39)*.035));
      capsule([x,y0,z0],[x,y1,z1],i===0?.085:.065,i===0?'raptureBronze':'wheelMetal');
    }
  }
  // Dome end ring with segmented gasket and anchor plates.
  for(const endZ of [-78,12]) {
    for(let i=0;i<64;i++) {
      const a0=i*Math.PI*2/64,a1=(i+1)*Math.PI*2/64;
      const x0=Math.cos(a0)*domeR,y0=domeBase+(domePeak-domeBase)*Math.sin(a0);
      const x1=Math.cos(a1)*domeR,y1=domeBase+(domePeak-domeBase)*Math.sin(a1);
      capsule([x0,y0,endZ],[x1,y1,endZ],.24,'raptureBronze');
    }
  }

  // --- Ornate lamp posts, plaques, benches and planted stone boxes --------
  function decoLamp(x,z) {
    // Fluted octagonal column with stepped base, collars and lantern cage.
    for(let k=0;k<7;k++) {
      const y=k*.55;
      cone([x,y,z],[x,y+.56,z],.24-k*.018,.22-k*.018,k%2?'raptureBronze':'metal');
    }
    for(const y of [.16,.62,2.54,3.37])
      roundBox(x-.38,y,z-.38,x+.38,y+.15,z+.38,.13,'raptureBronze');
    for(let i=0;i<8;i++) {
      const a=i*Math.PI/4,dx=Math.cos(a),dz=Math.sin(a);
      capsule([x+dx*.18,.78,z+dz*.18],[x+dx*.18,3.34,z+dz*.18],.028,'wheelMetal');
    }
    roundBox(x-.51,3.40,z-.51,x+.51,3.58,z+.51,.12,'metal');
    // Lantern housing and individual vertical cage bars.
    roundBox(x-.34,3.58,z-.34,x+.34,4.48,z+.34,.15,'raptureGlass');
    for(let i=0;i<8;i++) {
      const a=i*Math.PI/4,dx=Math.cos(a),dz=Math.sin(a);
      capsule([x+dx*.35,3.55,z+dz*.35],[x+dx*.35,4.51,z+dz*.35],.035,'metal');
    }
    roundBox(x-.46,3.50,z-.46,x+.46,3.65,z+.46,.13,'raptureBronze');
    roundBox(x-.44,4.45,z-.44,x+.44,4.58,z+.44,.12,'raptureBronze');
    cone([x,4.58,z],[x,5.18,z],.45,.05,'metal');
    ellipsoidMesh(x,5.25,z,.10,.11,.10,'lamp',12,8);
  }
  let lampPair=0;
  for(let z=-9;z>-75;z-=11.0) {
    decoLamp(-7.15,z); decoLamp(7.15,z-4.2);
    if((lampPair++%2)===0){
      M3D_SCENE_LIGHTS.push([-7.15,4.9,z,24,3.2,255,184,126]);
      M3D_SCENE_LIGHTS.push([7.15,4.9,z-4.2,24,3.2,255,184,126]);
    }
  }
  // Wall sconces continue the warm lighting up the street, so the avenue does
  // not fall into darkness between the tall freestanding lamps.
  function wallSconce(side,z,y=4.25) {
    const wallX=side<0?-14.05:14.05, towardStreet=side<0?1:-1, x=wallX+towardStreet*.24;
    const boxX=(x0,y0,z0,x1,y1,z1,r,mat)=>roundBox(Math.min(x0,x1),y0,z0,Math.max(x0,x1),y1,z1,r,mat);
    boxX(x-towardStreet*.23,y-.42,z-.34,x+towardStreet*.18,y+.42,z+.34,.12,'raptureBronze');
    boxX(x-towardStreet*.20,y-.30,z-.24,x+towardStreet*.22,y+.30,z+.24,.08,'metal');
    capsule([x+towardStreet*.18,y-.10,z],[x+towardStreet*.63,y+.12,z],.075,'raptureBronze');
    capsule([x+towardStreet*.63,y+.12,z],[x+towardStreet*.67,y+.44,z],.045,'metal');
    boxX(x+towardStreet*.47,y+.34,z-.28,x+towardStreet*.87,y+.96,z+.28,.14,'windowlit');
    for(const dz of [-.25,.25])
      capsule([x+towardStreet*.68,y+.32,z+dz],[x+towardStreet*.68,y+.98,z+dz],.035,'raptureBronze');
    boxX(x+towardStreet*.43,y+.26,z-.34,x+towardStreet*.93,y+.38,z+.34,.09,'metal');
    boxX(x+towardStreet*.43,y+.91,z-.34,x+towardStreet*.93,y+1.03,z+.34,.09,'raptureBronze');
    cone([x+towardStreet*.68,y+1.03,z],[x+towardStreet*.68,y+1.30,z],.25,.025,'metal');
    M3D_SCENE_LIGHTS.push([x+towardStreet*1.05,y+.62,z,16,2.1,255,190,137]);
  }
  for(const side of [-1,1]) for(let z=-12;z>-76;z-=12) wallSconce(side,z,4.15+(Math.abs(z/12)%2)*2.4);

  // Oversized suspended chandeliers give the covered avenue a warm, readable
  // ceiling rhythm. Their geometry is built from rings, arms, chains and bulbs.
  function chandelier(z,y=11.8,radius=1.55) {
    capsule([0,20.1,z],[0,y+1.65,z],.045,'raptureBronze');
    for(const rr of [radius,radius*.72]) for(let i=0;i<40;i++) {
      const a0=i*Math.PI*2/40,a1=(i+1)*Math.PI*2/40;
      capsule([Math.cos(a0)*rr,y+1.18, z+Math.sin(a0)*rr],
              [Math.cos(a1)*rr,y+1.18, z+Math.sin(a1)*rr],.055,'raptureBronze');
    }
    ellipsoidMesh(0,y+1.18,z,.32,.38,.32,'metal',20,14);
    for(let i=0;i<8;i++) {
      const a=i*Math.PI/4,cx=Math.cos(a),cz=Math.sin(a);
      capsule([cx*.22,y+1.18,z+cz*.22],[cx*radius,y+.72,z+cz*radius],.075,'metal');
      capsule([cx*radius,y+.72,z+cz*radius],[cx*radius,y+.28,z+cz*radius],.045,'raptureBronze');
      ellipsoidMesh(cx*radius,y+.05,z+cz*radius,.20,.28,.20,'windowlit',14,10);
      ellipsoidMesh(cx*radius,y-.20,z+cz*radius,.13,.13,.13,'led',10,8);
    }
    cone([0,y+1.52,z],[0,y+2.02,z],.52,.025,'raptureBronze');
    ellipsoidMesh(0,y-.12,z,.36,.42,.36,'lamp',16,12);
    M3D_SCENE_LIGHTS.push([0,y-.10,z,21,4.1,255,194,142]);
  }
  chandelier(-25,11.2,1.75); chandelier(-46,12.4,1.55);
  chandelier(-66,13.5,1.42); chandelier(-8,10.7,1.35);

  // Ornate guardrails frame the central reflecting runnel and catch the light.
  for(const side of [-1,1]) {
    const x=side*1.34;
    capsule([x,.46,-76],[x,.46,15],.065,'raptureBronze');
    capsule([x,.96,-76],[x,.96,15],.050,'metal');
    capsule([x,1.34,-76],[x,1.34,15],.065,'raptureBronze');
    for(let z=-76;z<=15;z+=2.0) {
      capsule([x,.20,z],[x,1.34,z],.042,'metal');
      ellipsoidMesh(x,1.43,z,.11,.10,.11,'raptureBronze',10,8);
    }
    for(let z=-72;z<=12;z+=14) {
      roundBox(x-.12,1.48,z-.12,x+.12,1.72,z+.12,.045,'metal');
      ellipsoidMesh(x,1.84,z,.09,.13,.09,'led',10,8);
      M3D_SCENE_LIGHTS.push([x,1.85,z,8,0.9,115,215,232]);
    }
  }

  // Small Art Deco wayfinding kiosks furnish the outer promenades. Each has
  // a lit glass map case, layered brass crown, fluted sides and step plinth.
  function kiosk(x,z,side) {
    const toward=side<0?1:-1, face=x+toward*.34;
    const boxX=(x0,y0,z0,x1,y1,z1,r,mat)=>roundBox(Math.min(x0,x1),y0,z0,Math.max(x0,x1),y1,z1,r,mat);
    roundBox(x-.90,.12,z-.66,x+.90,.42,z+.66,.17,'raptureBronze');
    roundBox(x-.76,.42,z-.54,x+.76,2.62,z+.54,.13,'raptureStone');
    for(const dx of [-.68,.68]) {
      roundBox(x+dx-.10,.52,z-.61,x+dx+.10,2.47,z+.61,.07,'metal');
      for(let k=0;k<5;k++) capsule([x+dx-.035,.72,z-.44+k*.22],[x+dx-.035,2.30,z-.44+k*.22],.022,'raptureBronze');
    }
    roundBox(face-.06,.68,z-.40,face+.09,2.23,z+.40,.08,'raptureGlass');
    boxX(face+toward*.10,.82,z-.31,face+toward*.16,2.05,z+.31,.035,'sign');
    // Schematic street-map lines, intentionally geometric rather than text.
    for(let k=0;k<5;k++) {
      const yy=1.00+k*.18, offset=(k%2)*.09;
      capsule([face+toward*.18,yy,z-.22+offset],[face+toward*.18,yy+.10,z+.08],.024,k===2?'neon':'windowlit');
    }
    roundBox(x-1.02,2.53,z-.76,x+1.02,2.78,z+.76,.13,'metal');
    roundBox(x-.84,2.78,z-.61,x+.84,3.13,z+.61,.16,'raptureBronze');
    for(let k=0;k<5;k++) {
      const zz=z-.48+k*.24;
      roundBox(x-.72,2.86,zz-.035,x+.72,3.04,zz+.035,.025,k%2?'metal':'neon');
    }
    cone([x,3.13,z],[x,3.62,z],.54,.03,'raptureBronze');
    ellipsoidMesh(x,3.74,z,.13,.15,.13,'led',12,8);
    M3D_SCENE_LIGHTS.push([x,2.10,z,10,1.4,117,218,239]);
  }
  for(const [x,z,side] of [[-19,-15,-1],[19,-28,1],[-19,-43,-1],[19,-56,1],[-19,-70,-1],[19,-74,1]]) kiosk(x,z,side);

  // Exposed pressure pipes and valve wheels break up the long blank wall runs.
  for(const side of [-1,1]) for(let z=-18;z>-76;z-=14) {
    const x=side<0?-13.72:13.72;
    for(const dx of [-.25,.25]) {
      capsule([x+dx,.52,z],[x+dx,9.8,z],.105,'wheelMetal');
      for(const y of [1.15,3.4,5.8,8.2])
        roundBox(x+dx-.18,y,z-.24,x+dx+.18,y+.15,z+.24,.055,'raptureBronze');
    }
    capsule([x-.25,1.15,z],[x+.25,1.15,z],.12,'metal');
    ellipsoidMesh(x,1.15,z,.48,.48,.13,'raptureBronze',20,12);
    ellipsoidMesh(x,1.15,z,.18,.18,.17,'metal',12,8);
  }

  // Planters with stylized kelp and coral make the open pavement feel inhabited
  // by the sea without introducing any characters into the scene.
  for(const [x,z,variant] of [[-22,-12,0],[22,-18,1],[-22,-33,1],[22,-41,0],[-22,-54,0],[22,-67,1]]) {
    roundBox(x-1.05,.10,z-.82,x+1.05,.44,z+.82,.18,'raptureBronze');
    roundBox(x-.90,.40,z-.67,x+.90,.68,z+.67,.16,'raptureStone');
    ellipsoidMesh(x,.72,z,.82,.20,.57,'sea',24,12);
    for(let k=0;k<7;k++) {
      const a=k*Math.PI*2/7,px=x+Math.cos(a)*(.18+(k%3)*.12),pz=z+Math.sin(a)*(.18+(k%3)*.12);
      const h=1.1+(k%4)*.33;
      capsule([px,.78,pz],[px+Math.cos(a)*.18,.78+h*.55,pz+Math.sin(a)*.18],.045,'leaves');
      capsule([px+Math.cos(a)*.18,.78+h*.55,pz+Math.sin(a)*.18],
              [px+Math.cos(a+variant*.4)*.36,.78+h,pz+Math.sin(a+variant*.4)*.36],.032,'leaves');
    }
    for(let k=0;k<3;k++) {
      const px=x+(k-1)*.30,pz=z+.12*(k%2?1:-1);
      cone([px,.70,pz],[px,.70+.62+(k%2)*.26,pz],.18,.035,k%2?'neon':'raptureBronze');
    }
  }
  function bench(x,z,side) {
    const dir=side<0?1:-1;
    const orderedBox=(xa,ya,za,xb,yb,zb,r,mat)=>roundBox(
      Math.min(xa,xb),Math.min(ya,yb),Math.min(za,zb),
      Math.max(xa,xb),Math.max(ya,yb),Math.max(za,zb),r,mat);
    // Cast iron end frames, curled feet and individually modeled oak slats.
    for(const dz of [-.72,.72]) {
      capsule([x,.12,z+dz],[x,1.12,z+dz],.07,'metal');
      capsule([x,.14,z+dz],[x+dir*.55,.14,z+dz],.075,'raptureBronze');
      capsule([x,1.00,z+dz],[x+dir*.43,1.36,z+dz],.06,'metal');
      ellipsoidMesh(x,1.14,z+dz,.14,.12,.12,'raptureBronze',12,8);
    }
    for(let i=0;i<6;i++) {
      const yy=.56+i*.115;
      orderedBox(x+dir*(i<4?.12:.41),yy,z-.94,x+dir*(i<4?.20:.48),yy+.084,z+.94,.035,'wood');
    }
    for(const dz of [-.50,-.25,0,.25,.50])
      orderedBox(x+dir*.24,1.24,z+dz-.045,x+dir*.56,1.31,z+dz+.045,.035,'metal');
  }
  bench(-6.25,-16,-1); bench(6.25,-24,1); bench(-6.25,-50,-1); bench(6.25,-69,1);
  // Public information plaques with layered frame and raised deco fan emblem.
  for(const [x,z,side] of [[-13.82,-13,1],[13.82,-37,-1],[-13.82,-58,1],[13.82,-76,-1]]) {
    const fx=x+side*.09,nx=side;
    roundBox(fx-.12,2.6,z-.80,fx+.16,4.8,z+.80,.10,'raptureBronze');
    roundBox(fx-.17,2.78,z-.66,fx+.19,4.62,z+.66,.08,'metal');
    roundBox(fx-.20,2.94,z-.53,fx+.20,4.48,z+.53,.08,'sign');
    fanRelief(fx+.22,3.2,z,.84,.78,nx,'neon');
    for(let k=0;k<4;k++)
      ellipsoidMesh(fx+.23,2.99+k*.46,z-.44,.055,.055,.055,'led',8,6);
  }
  // Large raised inscription panels use colored geometry, with no legible
  // brand marks so the scene remains original and engine-friendly.
  for(const side of [-1,1]) {
    const fx=side<0?-13.80:13.80,nx=side<0?1:-1,z=-5.5;
    roundBox(fx-.12,6.85,z-2.15,fx+.20,8.28,z+2.15,.16,'raptureBronze');
    roundBox(fx-.17,6.98,z-2.00,fx+.24,8.15,z+2.00,.12,'sign');
    fanRelief(fx+.27,7.08,z,2.25,.90,nx,'neon');
    for(let k=0;k<7;k++) {
      const yy=7.13+k*.145;
      roundBox(fx+.26,yy,z-1.38,fx+.33,yy+.045,z+1.38,.018,k%2?'lamp':'metal');
    }
  }

  // --- Aquatic silhouettes beyond the dome -------------------------------
  // A few distant fish meshes establish the outside water; no humanoid figures.
  function fish(x,y,z,s) {
    const n=[0,0,1], v=(p,uv)=>({p,n,uv});
    const body0=v([x-s*.72,y,z],[0,0]),body1=v([x+s*.52,y+s*.20,z],[1,0]);
    const body2=v([x+s*.70,y,z],[1,.5]),body3=v([x+s*.52,y-s*.20,z],[1,1]);
    const body4=v([x-s*.72,y,z],[0,1]);
    meshTriangle(body0,body1,body2,'raptureGlass'); meshTriangle(body0,body2,body3,'raptureGlass');
    meshTriangle(body0,body3,body4,'raptureGlass');
    const t1=v([x-s*.88,y,z],[0,0]),t2=v([x-s*1.18,y+s*.23,z],[0,1]),t3=v([x-s*1.18,y-s*.23,z],[1,1]);
    meshTriangle(t1,t2,t3,'raptureGlass');
  }
  for(const [x,y,z,s] of [[-12,22,-65,1.15],[9,25,-74,1.55],[17,20,-47,.92],[-18,18,-31,.78],[3,27,-88,1.2]]) fish(x,y,z,s);

  // Scattered drain covers, mooring rings and subtle debris add foreground scale.
  for(const [x,z] of [[-3,-11],[3,-11],[-4,-31],[4,-31],[-5,-55],[5,-55]]) {
    for(let i=0;i<32;i++) {
      const a=i*Math.PI*2/32;
      capsule([x+Math.cos(a)*.42,.035,z+Math.sin(a)*.42],
              [x+Math.cos(a+Math.PI/16)*.42,.035,z+Math.sin(a+Math.PI/16)*.42],.028,'metal');
    }
    ellipsoidMesh(x,.03,z,.10,.035,.10,'raptureBronze',12,8);
  }
}
