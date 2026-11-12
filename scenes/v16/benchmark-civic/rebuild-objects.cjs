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

  // V3: slender, asymmetric anatomical silhouettes; no ribbed bead chains.
  // All components are real existing ellipsoid meshes, never engine changes.
  // The front-facing ensemble must read as two figures, not two bronze tubes.
  function figure(id,x,z,draped) {
    const p=root+'/Silhouettes/'+id;
    const turn=draped?-.095:.085;
    const faceX=x+turn;
    for(const side of [-1,1]){
      const foot=x+side*.255;
      // Long legs in three continuous transitions, with staggered knees
      // and anatomically smaller ankles. No chunky spherical kneecaps.
      const stride=draped?side*.085:side*.045;
      S(p+'/Mollets',bronze,foot+stride,2.77,z+(side>0?.12:-.08),
        .245,.90,.27);
      S(p+'/Cuisses',bronze,foot+stride*.35,3.44,z-.035,
        .315,1.06,.335);
      S(p+'/Genoux',bronze,foot+stride*.7,3.06,z+.035,
        .26,.23,.28);
      S(p+'/Chaussures',bronze,foot+stride,2.30,z+.19,
        .33,.17,.58);
    }
    // Proportional pelvis, tapered upper torso and narrower shoulders.
    S(p+'/Bassin',bronze,x,3.78,z,.70,.54,.59);
    S(p+'/Torse sculpté',bronze,x+turn*.55,4.53,z-.02,.65,1.23,.48);
    S(p+'/Poitrine',bronze,x+turn*.8,4.82,z+.035,.72,.49,.53);
    S(p+'/Epaules',bronze,x+turn,5.00,z-.035,.80,.26,.51);
    S(p+'/Cou',bronze,faceX,5.28,z-.03,.19,.28,.22);
    // Smaller head with a visible brow, chin and thin profile.
    S(p+'/Visage',bronze,faceX,5.60,z+.015,.375,.57,.345);
    S(p+'/Visage/Front',bronze,faceX,5.81,z+.12,.33,.15,.265);
    S(p+'/Visage/Nez',bronze,faceX,5.57,z+.235,.085,.17,.145);
    S(p+'/Visage/Menton',bronze,faceX,5.37,z+.115,.24,.115,.215);
    for(const side of [-1,1])
      S(p+'/Visage/Joues',bronze,faceX+side*.13,5.60,z+.19,
        .13,.145,.115);
    if(draped) {
      // A narrower tunic with a three-layer skirt, rather than an oval barrel.
      S(p+'/Tunique',bronze,x,3.76,z+.005,.78,1.10,.58);
      for(const offset of [-.24,.02,.25])
        S(p+'/Drapé vertical',bronze,x+offset,3.46,z+.28,
          .12,.72,.11);
      S(p+'/Ceinture',bronze,x,4.14,z+.09,.52,.18,.43);
      S(p+'/Coiffure',bronze,faceX,5.81,z-.13,.415,.22,.34);
      // Deliberate lower arm angle; fewer long continuous segments.
      S(p+'/Bras ouvert',bronze,x-.58,4.90,z,
        .28,.66,.245);
      S(p+'/Avant-bras ouvert',bronze,x-1.03,4.69,z+.05,
        .60,.215,.23);
      S(p+'/Main ouverte',bronze,x-1.40,4.63,z+.06,
        .16,.14,.19);
      S(p+'/Bras bas',bronze,x+.53,4.67,z-.02,
        .20,.72,.22);
      S(p+'/Main basse',bronze,x+.56,4.27,z+.05,
        .145,.21,.16);
    } else {
      // Close-cut jacket: one readable body, light lapel relief only.
      S(p+'/Veste',bronze,x+turn*.55,4.51,z+.005,.67,1.16,.53);
      for(const offset of [-.22,.22])
        S(p+'/Revers',bronze,x+offset+turn,4.75,z+.255,
          .10,.40,.09);
      // Gesture follows shoulder -> elbow -> wrist without hard separation.
      S(p+'/Bras haut',bronze,x+.51,5.30,z-.015,
        .225,.71,.23);
      S(p+'/Coude',bronze,x+.74,5.77,z-.015,
        .21,.22,.22);
      S(p+'/Avant-bras levé',bronze,x+.91,6.20,z-.015,
        .195,.95,.21);
      S(p+'/Main étoile',bronze,x+1.00,6.65,z-.015,
        .165,.24,.18);
      S(p+'/Bras bas',bronze,x-.52,4.69,z,
        .225,.75,.24);
      S(p+'/Main basse',bronze,x-.56,4.28,z+.04,
        .16,.21,.17);
    }
  }
  figure('Jeunesse',cx-.79,cz+.08,true);
  figure('Science',cx+.76,cz-.10,false);
  // Small five-ray star held above the raised hand; restrained golden paint.
  const sx=cx+1.76,sy=7.01,sz=cz-.10;
  S(root+'/Etoile/Coeur','cse_neon_gold',sx,sy,sz,.27,.27,.16);
  for(let i=0;i<5;i++){
    const theta=(i*72+90)*rad;
    for(let j=0;j<3;j++){
      const t=(j+.5)/3;
      S(root+'/Etoile/Branches','cse_neon_gold',
        sx+.31*t*cos(theta),sy+.31*t*sin(theta),sz,
        .14-.06*t,.14-.06*t,.08);
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
