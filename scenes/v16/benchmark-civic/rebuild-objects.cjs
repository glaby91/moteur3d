// CSE WARP V17 — scene-only geometric reconstruction.
// All meshes use EXISTING GPU-compatible unit_box / unit_sphere / foliage_canopy
// primitives. No new engine instructions, shader, or binary changes.
// Original scene artworks, pool, façade and the oval concrete seat are untouched.
'use strict';

const golden=2.399963229728653;
const rad=Math.PI/180;
function rr(x){return Math.round(x*1000)/1000;}
function sin(x){return Math.sin(x);}
function cos(x){return Math.cos(x);}

function rebuildMonument({B,S,cx,cz}) {
  const root='Place/Monument soviétique fictif', bronze='cse_monument_bronze';
  // Restrained civic allegory: science + youth, not a copied historic statue.
  // Tiered stone base is solid, with a thin readable rim and discreet plaque.
  B(root+'/Socle', 'prp_concrete_raw', cx,1.44,cz,4.35,1.02,3.85);
  B(root+'/Socle', 'prp_terrazzo_floor', cx,1.99,cz,4.12,.18,3.62);
  B(root+'/Socle', 'prp_concrete_panel', cx,2.12,cz,3.63,.15,3.19);
  B(root+'/Socle/Joint', 'prp_steel_rusted', cx,1.74,cz+1.939,3.42,.045,.025);
  S(root+'/Socle/Medaille','cse_neon_gold',cx,1.52,cz+1.965,.35,.35,.11);

  // Version 2 after *real* 1366x768 render inspection:
  // The earlier small ellipsoid chain looked like an accordion. Use few,
  // continuous, smoothly overlapping major volumes for readable sculpture.
  function figure(id,x,z,draped) {
    const p=root+'/Silhouettes/'+id;
    for(const side of [-1,1]){
      const lx=x+side*.23;
      S(p+'/Mollets',bronze,lx,2.80,z+.08,.32,.96,.34);
      S(p+'/Cuisses',bronze,lx,3.42,z-.04,.42,1.00,.41);
      S(p+'/Genoux',bronze,lx,3.08,z+.03,.32,.31,.34);
      S(p+'/Chaussures',bronze,lx,2.30,z+.25,.42,.20,.71);
    }
    S(p+'/Bassin',bronze,x,3.79,z,.86,.64,.69);
    S(p+'/Torse sculpté',bronze,x,4.57,z,.83,1.32,.61);
    S(p+'/Epaules',bronze,x,5.02,z,.99,.32,.64);
    S(p+'/Cou',bronze,x,5.32,z,.26,.33,.29);
    S(p+'/Visage',bronze,x,5.65,z+.045,.49,.73,.47);
    S(p+'/Visage/Nez',bronze,x,5.58,z+.31,.13,.19,.20);
    S(p+'/Visage/Menton',bronze,x,5.35,z+.15,.32,.12,.27);
    if(draped){
      // Single flowing skirt instead of rows of horizontal ribbed beads.
      S(p+'/Tunique',bronze,x,3.74,z,.99,1.13,.76);
      for(const t of [-.34,0,.34])
        S(p+'/Drapé vertical',bronze,x+t,3.59,z+.25,.20,.75,.20);
      S(p+'/Coiffure',bronze,x,5.91,z-.12,.54,.25,.43);
      // Left arm opens gently toward the garden with two smooth tapered
      // long ellipsoids, not seven mechanical beads.
      S(p+'/Bras ouvert',bronze,x-.84,4.91,z,.79,.28,.29);
      S(p+'/Avant-bras ouvert',bronze,x-1.42,4.77,z+.04,.72,.24,.27);
      S(p+'/Main ouverte',bronze,x-1.83,4.74,z+.045,.20,.19,.27);
      S(p+'/Bras bas',bronze,x+.57,4.67,z,.26,.92,.29);
      S(p+'/Main basse',bronze,x+.60,4.23,z+.08,.20,.27,.23);
    }else{
      S(p+'/Veste',bronze,x,4.44,z+.03,.90,1.38,.68);
      for(const offset of [-.32,.32])
        S(p+'/Revers',bronze,x+offset,4.72,z+.33,.16,.54,.14);
      // A readable raised gesture: shoulder, upper arm, forearm, hand.
      S(p+'/Bras haut',bronze,x+.66,5.45,z,.28,.91,.29);
      S(p+'/Avant-bras levé',bronze,x+1.01,6.20,z,.25,1.28,.27);
      S(p+'/Coude',bronze,x+.84,5.78,z,.29,.28,.29);
      S(p+'/Main étoile',bronze,x+1.16,6.75,z,.23,.28,.24);
      S(p+'/Bras bas',bronze,x-.62,4.70,z,.27,.89,.27);
      S(p+'/Main basse',bronze,x-.70,4.27,z+.07,.20,.25,.23);
    }
  }
  figure('Jeunesse',cx-.79,cz+.08,true);
  figure('Science',cx+.76,cz-.10,false);
  // Small five-ray star held above the raised hand; restrained golden paint.
  const sx=cx+1.94,sy=7.10,sz=cz-.10;
  S(root+'/Etoile/Coeur','cse_neon_gold',sx,sy,sz,.27,.27,.16);
  for(let i=0;i<5;i++){
    const theta=(i*72+90)*rad;
    for(let j=0;j<3;j++){
      const t=(j+.5)/3;
      S(root+'/Etoile/Branches','cse_neon_gold',
        sx+.44*t*cos(theta),sy+.44*t*sin(theta),sz,
        .20-.08*t,.20-.08*t,.105);
    }
  }
}

function rebuildGarden({B,S,F,rand,cx,cz}) {
  const root='Place/Butte végétalisée';
  const greens=['cse_leaf_olive','prp_foliage','cse_leaf_gold'];
  const blooms=['cse_flower_red','cse_flower_gold','cse_flower_purple',
                'cse_flower_blue'];
  const top=(u,v)=>.32+1.175*Math.sqrt(Math.max(0,1-u*u-v*v));
  // The physical earth dome remains in generate.cjs; all these clusters
  // follow its actual elliptical surface and clear the central plinth.
  let count=0;
  for(let i=0;i<270;i++){
    const a=i*golden, r=Math.sqrt((i+.5)/270)*.925;
    const u=r*cos(a),v=r*sin(a);
    const x=cx+u*14.5,z=cz+v*8.6;
    if(Math.abs(x-cx)<2.85 && Math.abs(z-cz)<2.55)continue;
    const y=top(u,v);
    const mat=greens[(i+Math.floor(i/9))%greens.length];
    const extent=.43+.19*rand();
    // Low spread-out clumps instead of tall colorful balls.
    F(root+'/Fleurs et feuillages',mat,x,y+.06,z,
      extent*1.5,.20+rand()*.28,extent*1.42);
    if(i%3===0){
      // Asymmetric leaves, small twigs and sparse buds.
      for(let j=0;j<3;j++){
        const b=a+j*2.1,dx=.18*cos(b),dz=.18*sin(b);
        S(root+'/Feuilles',greens[(i+j)%greens.length],
          x+dx,y+.18,z+dz,.36,.12,.19);
      }
    }
    // ~150 living flowers at clear but irregular intervals.
    if(i%2===0 || i%5===0){
      const height=.27+.19*rand(),c=blooms[(i*3+Math.floor(i/17))%blooms.length];
      B(root+'/Tiges','cse_leaf_olive',x,y+height*.50,z,.028,height,.028);
      // Bud points are thin rather than balls; petals radial and flattened.
      for(let j=0;j<5;j++){
        const phi=(j*2*Math.PI/5)+.1*i,pr=.12+.03*rand();
        S(root+'/Petales',c,
          x+pr*cos(phi),y+height+.01,z+pr*sin(phi),
          .18,.07,.15);
      }
      S(root+'/Coeurs','cse_flower_gold',x,y+height+.045,z,.13,.12,.13);
      count++;
    }
  }
  if(count<140)throw Error('Insufficient natural flower geometry');
}

function rebuildTrees({B,S,F,rand,p}) {
  const placement=[
    [-34,12],[-57,18],[-68,22],[-41,33],[-62,41],[-74,51],
    [-55,65],[-67,73],[-43,79],[-59,89],[-74,97],[-48,102],[-65,112],
    [34,12],[59,18],[70,24],[61,37],[75,45],[79,66],
    [82,84],[46,81],[62,92],[73,104],[50,109],[78,60],[67,113]
  ];
  for(let i=0;i<p.trees;i++){
    const birch=i%3===0;
    const near=placement[i];
    const x=near?near[0]+(rand()-.5)*.9:(i%2?-1:1)*(82+rand()*12);
    const z=near?near[1]+(rand()-.5)*.8:-70+(Math.floor(i/2)%44)*5;
    const h=birch?9.5+rand()*2.4:12.1+rand()*4;
    const bark=birch?'cse_birch_bark':'prp_bark_wet';
    const leaf=(i%7===0)?'cse_leaf_gold':(i%3===0?'prp_foliage':'cse_leaf_olive');
    const w=birch?.23:.31;
    // Central shaft is vertical and tapered with short real wooden sections,
    // not simply one box with three spherical leaves.
    B('Nature/Arbres',bark,x,h*.40,z,w,h*.8,w);
    for(let k=0;k<4;k++){
      const yy=h*(.43+k*.125);
      B('Nature/Arbres/Tronc/Noeuds',bark,x,yy,z,
        w*(.90-k*.14),h*.16,w*(.91-k*.12));
    }
    if(birch){
      for(let k=0;k<8;k++){
        const yy=1+k*h*.088;
        B('Nature/Arbres/Ecorce/bouleau','prp_bark_wet',
          x+.14,yy,z,.023,.075,.19);
      }
    }
    // Four strong primary branches preserve existing structural test contract.
    for(let k=0;k<4;k++){
      const a=(i*.23+k*1.5708),yy=h*(.57+.056*(k%3));
      const br=birch?1.25:.77;
      const bx=x+br*cos(a),bz=z+br*sin(a);
      B('Nature/Arbres/Branches',bark,
        x+(bx-x)*.5,yy,bz*.5+z*.5,
        br*1.5,.11+.03*(k%2),.15,rr(a/rad));
      // Fine twigs taper toward different endpoints.
      for(let j=0;j<3;j++){
        const theta=a+(j-1)*.47;
        const dx=(br+.5+j*.25)*cos(theta);
        const dz=(br+.5+j*.25)*sin(theta);
        B('Nature/Arbres/Branches secondaires',bark,
          x+dx*.68,yy+h*.07+j*.10,z+dz*.68,
          .65,.065,.065,rr(theta/rad));
      }
      // Multiple fine canopies, elongated for poplar, irregular for birch.
      for(let j=0;j<4;j++){
        const a2=a+.19*(j-1.5);
        const radius=(birch?1.75:1.06)+.27*(j%2);
        F('Nature/Arbres/Houppier',j%5===0?'prp_foliage':leaf,
          x+radius*cos(a2),h*(.70+.055*(j%3)),z+radius*sin(a2),
          birch?1.70:1.13,birch?2.35:3.1,birch?1.62:1.24);
      }
    }
    // More than five canopy lobes per trunk: two independent core layers.
    for(let j=0;j<6;j++){
      const a=(i+j)*golden;
      const radius=birch?.94:.58;
      F('Nature/Arbres/Houppier',j%6===0?'cse_leaf_gold':leaf,
        x+radius*cos(a),h*(.69+j*.061),z+radius*sin(a),
        birch?1.60:1.2,birch?2.05:3.12,birch?1.54:1.35);
    }
    if(p.detail>8){
      for(let j=0;j<6;j++){
        const a=(i+7*j)*golden;
        F('Nature/Arbres/Houppier',leaf,x+1.35*cos(a),
          h*(.72+.05*(j%3)),z+1.35*sin(a),
          1.1,1.65,1.1);
      }
    }
  }
}

module.exports={rebuildMonument,rebuildGarden,rebuildTrees};
