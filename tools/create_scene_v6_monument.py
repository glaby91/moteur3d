#!/usr/bin/env python3
"""CSE WARP V17.420/V5 -> V6 monument, independent scene-only upgrade.

Uses the *actual* existing V5 data, and keeps all old V5 files unchanged.
Builds a continuous visibly raised hill with conforming flowers/grasses,
original stylized red hammer/sickle sculpture, plinth with exact FR+UA inscription.
No CPU/GPU engine code changes. A purchased TurboSquid asset is NOT represented
as included: the new monument is original geometric modeling of the design.
"""
from __future__ import annotations
import argparse, collections, copy, json, math, random
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from shapely.geometry import Polygon
from shapely.ops import triangulate

CX, CZ = 0.0, 72.0
BASE_Y = 0.16
HILL_H = 1.20
PLINTH_H = 1.20
SCULPT_H = 5.00
SCULPT_BOTTOM = BASE_Y + HILL_H + PLINTH_H
FR = ['LE COMMUNISME EST TOMBÉ.', 'L’UKRAINE EST DEBOUT.']
UA = ['КОМУНІЗМ УПАВ.', 'УКРАЇНА ЖИВЕ.']


def get(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def save(path, obj):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, ensure_ascii=False, separators=(',', ':')) + '\n', encoding='utf-8')


def bed_elevation(x, z):
    # 20 m x 10.8 m planted central dome INSIDE existing ~56x32 m oval.
    # Plateau top supports the 5 m hammer+sickle without floating in midair.
    rr = math.hypot((x-CX)/13.2, (z-CZ)/7.8)
    t = max(0.0, min(1.0, (rr-0.12)/(1.0-0.12)))
    smooth = t*t*(3.0-2.0*t)
    return BASE_Y + HILL_H*(1.0 - smooth)


def register_mesh(meshes, name, vertices, faces, uv=None, extra=None):
    v=np.asarray(vertices,dtype=float)
    f=np.asarray(faces,dtype=int)
    if v.ndim!=2 or v.shape[1]!=3 or f.ndim!=2 or f.shape[1]!=3 or not len(f):
        raise ValueError('Invalid mesh '+name)
    if np.min(f)<0 or np.max(f)>=len(v) or not np.isfinite(v).all():
        raise ValueError('Invalid mesh indices/coordinates '+name)
    m={'id':name,'type':'triangle_mesh','vertices':v.round(6).tolist(),
       'faces':f.tolist(),
       'uv':(np.asarray(uv,dtype=float).round(6).tolist() if uv is not None else [[0.,0.] for _ in v])}
    if extra:m.update(extra)
    meshes.append(m)
    return m


def make_butte(meshes):
    """Continuous Y-up planted surface, center +1.2m, fully grounded at edges."""
    rings=24; segments=112
    verts=[[CX,bed_elevation(CX,CZ),CZ]]
    tex=[[0.5,0.5]]
    for ir in range(1,rings+1):
        r=ir/rings
        for i in range(segments):
            angle=2*math.pi*i/segments
            x=CX+28.25*r*math.cos(angle)
            z=CZ+16.35*r*math.sin(angle)
            y=bed_elevation(x,z)
            verts.append([x,y,z]);tex.append([(x+30)/3,(z-72+20)/3])
    faces=[]
    for i in range(segments):
        a=1+i;b=1+(i+1)%segments
        # As +X, +Z advances outward the upward face is (center, b, a).
        faces.append([0,b,a])
    for ir in range(rings-1):
        a0=1+ir*segments
        b0=a0+segments
        for i in range(segments):
            j=(i+1)%segments
            faces.extend([[a0+i,a0+j,b0+i], [a0+j,b0+j,b0+i]])
    # Add down-facing base and vertical skirt for robust ray intersection.
    skirtfirst=len(verts)
    for i in range(segments):
        t=2*math.pi*i/segments
        verts.append([CX+28.25*math.cos(t), BASE_Y-.08, CZ+16.35*math.sin(t)])
        tex.append([0.,0.])
    for i in range(segments):
        j=(i+1)%segments
        upper=1+(rings-1)*segments
        faces.extend([[upper+i,skirtfirst+j,skirtfirst+i],
                      [upper+i,upper+j,skirtfirst+j]])
    # Bottom polygon fan.
    base_i=len(verts);verts.append([CX, BASE_Y-.08, CZ]);tex.append([0.,0.])
    for i in range(segments):faces.append([base_i,skirtfirst+i,skirtfirst+(i+1)%segments])
    # Keep double-sided top for WARP versions with different winding/cull state.
    topcount=segments+2*(rings-1)*segments
    faces.extend([[c,b,a] for a,b,c in faces[:topcount]])
    return register_mesh(meshes,'v6_butte_relief_solid',verts,faces,tex,
                         {'note':'Continuous raised original mound, 1.20m high; soil mesh, not decal'})


def extrude_shape(meshes, name, coords, thickness=.25):
    shape=Polygon(coords)
    if not shape.is_valid:shape=shape.buffer(0)
    if shape.is_empty or shape.geom_type!='Polygon':raise RuntimeError('Invalid extrusion polygon '+name)
    vv=[];uv=[];ff=[]
    def addface(a,b,c):
        idx=len(vv)
        for xyz in (a,b,c):
            vv.append(list(xyz));uv.append([(xyz[0]+2.6)/5.2, xyz[1]/5.0])
        ff.append([idx,idx+1,idx+2])
    for tri in triangulate(shape):
        if not shape.covers(tri):continue
        c=list(tri.exterior.coords)[:3]
        pts=[(p[0],p[1]) for p in c]
        for z,reverse in [(thickness/2,False),(-thickness/2,True)]:
            coords3=[(p[0],p[1],z) for p in pts]
            if reverse:coords3=coords3[::-1]
            addface(*coords3)
    outline=list(shape.exterior.coords)
    for p,q in zip(outline,outline[1:]):
        a=(p[0],p[1],thickness/2);b=(q[0],q[1],thickness/2)
        c=(p[0],p[1],-thickness/2);d=(q[0],q[1],-thickness/2)
        addface(a,b,c);addface(b,d,c)
    if not ff:raise RuntimeError('Empty geometry '+name)
    return register_mesh(meshes,name,vv,ff,uv)


def cubic(t,p0,p1,p2,p3):
    u=1-t
    return np.array(p0)*u**3+3*np.array(p1)*t*u*u+3*np.array(p2)*t*t*u+np.array(p3)*t**3


def sickle_polygon():
    # Original shape referencing the chosen Soviet monument silhouette, not paid asset.
    controls=[(-1.76,.88),(1.70,.10),(2.86,3.68),(.42,5.00)]
    left=[];right=[]
    for i in range(69):
        t=i/68
        p=cubic(t,*controls)
        t0=max(0,t-.003);t1=min(1,t+.003)
        tangent=cubic(t1,*controls)-cubic(t0,*controls)
        tangent=tangent/max(1e-8,float(np.linalg.norm(tangent)))
        norm=np.array([-tangent[1],tangent[0]])
        width=.46*(1-t)**.70 + .012
        left.append(tuple(p+norm*width/2));right.append(tuple(p-norm*width/2))
    return left+right[::-1]


def tilted_rect(p,q,width):
    p=np.array(p);q=np.array(q);normal=np.array([-(q-p)[1],(q-p)[0]])
    normal=normal/np.linalg.norm(normal)*width*.5
    return [tuple(p+normal),tuple(q+normal),tuple(q-normal),tuple(p-normal)]


def create_monument(meshes):
    elements=[]
    elements.append(extrude_shape(meshes,'v6_sickle_original',sickle_polygon(),.25))
    elements.append(extrude_shape(meshes,'v6_hammer_handle',
                                  tilted_rect((1.03,.26),(-.83,3.72),.44),.31))
    # Hammer head: conspicuous industrial, beveled ends reminiscent of 1970s sculpture.
    elements.append(extrude_shape(meshes,'v6_hammer_head',
                 [(-1.76,3.71),(-1.41,4.33),(-.52,3.79),(-.84,3.25)],.39))
    # Small diagonal impact bevel, still an independent actual mesh.
    elements.append(extrude_shape(meshes,'v6_hammer_bevel',
                 [(-1.76,3.71),(-1.55,3.94),(-1.16,3.66),(-1.37,3.45)],.415))
    return elements


def create_plinth(meshes):
    # Cuboid with actual triangles, 5.10 x 1.20 x 2.80 meters.
    w,h,d=5.10,PLINTH_H,2.80
    xs=(-w/2,w/2); ys=(0,h); zs=(-d/2,d/2)
    v=[[x,y,z] for y in ys for z in zs for x in xs]
    # order 0 (-x,0,-z),1 (+x,0,-z),2 (-x,0,+z),3 (+x,0,+z), +4 top
    quad=[[0,2,3,1],[4,5,7,6],[0,1,5,4],[2,6,7,3],[0,4,6,2],[1,3,7,5]]
    f=[]
    for a,b,c,d0 in quad:f.extend([[a,b,c],[a,c,d0]])
    return register_mesh(meshes,'v6_plinth_510_120_280',v,f,
                         [[0,0] for _ in v])


def make_textures(root):
    dest=root/'textures/revision/monument';dest.mkdir(parents=True,exist_ok=True)
    W,H=2048,512
    rng=np.random.default_rng(19701986)
    coarse=rng.normal(0,7,(128,512)).astype(np.float32)
    coarse=np.asarray(Image.fromarray(np.uint8(np.clip(coarse+128,0,255)))
                      .resize((W,H),Image.Resampling.BILINEAR),dtype=float)-128
    fine=rng.normal(0,5,(H,W))
    v=np.clip(192+coarse*.72+fine,0,255).astype(np.uint8)
    col=np.stack([v+1,v,v-5],axis=-1).clip(0,255).astype('uint8')
    plaque=Image.fromarray(col,'RGB')
    dr=ImageDraw.Draw(plaque)
    dr.rectangle((22,17,W-22,H-17),outline=(132,126,117),width=4)
    dr.line((250,289,W-250,289), fill=(112,103,92),width=3)
    # Font selection works both locally (Linux) and in GitHub Windows runners.
    candidates=[
        '/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',
        '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
        'C:/Windows/Fonts/arialbd.ttf',
        'C:/Windows/Fonts/segoeuib.ttf',
        'C:/Windows/Fonts/arial.ttf',
    ]
    fr_font=next((p for p in candidates if Path(p).is_file()),None)
    if fr_font is None:raise RuntimeError('No font containing Ukrainian glyphs available')
    ua_font=fr_font
    for i,line in enumerate(FR+UA):
        target_w=1930 if i<2 else 1540
        fsize=116 if i<2 else 94
        while True:
            ft=ImageFont.truetype(fr_font if i<2 else ua_font,fsize)
            bb=dr.textbbox((0,0),line,font=ft,stroke_width=0)
            if bb[2]-bb[0]<=target_w or fsize<=35:break
            fsize-=2
        center_y=[90,197,358,440][i]
        x=(W-(bb[2]-bb[0]))/2 - bb[0]
        y=center_y-(bb[3]-bb[1])/2-bb[1]
        # Carved-look offset shadow/highlight on stone without font mesh bloat.
        dr.text((x+2,y+3),line,font=ft,fill=(231,221,205))
        dr.text((x-1,y-1),line,font=ft,fill=(49,44,39))
    filename='inscription_fr_uk_gravure_2048.png'
    plaque.save(dest/filename,optimize=True)
    # Mild worn red steel (same albedo texture on all sculpture components).
    n=512
    noise=rng.normal(0,11,(n,n))
    big=np.asarray(Image.fromarray(np.uint8(np.clip(128+rng.normal(0,30,(32,32)),0,255)))
                   .resize((n,n),Image.Resampling.BILINEAR),dtype=float)-128
    c=np.stack([153+noise*.70+big*.38,39+noise*.27+big*.20,31+noise*.24+big*.18],axis=-1)
    Image.fromarray(np.uint8(np.clip(c,0,255)),'RGB').save(dest/'acier_rouge_patine.png')
    return 'textures/revision/monument/'+filename,'textures/revision/monument/acier_rouge_patine.png'


def make_plaque(meshes):
    # Front face faces +Z, toward the original v4_monument camera at z=106.
    w,h,z=4.74,.99,2.80/2+.014
    y0=.11
    verts=[[-w/2,y0,z],[w/2,y0,z],[w/2,y0+h,z],[-w/2,y0+h,z]]
    uv=[[0,0],[1,0],[1,1],[0,1]]  # WARP texture V orientation verified against real DX11 output
    faces=[[0,1,2],[0,2,3],[2,1,0],[3,2,0]]  # double-sided
    return register_mesh(meshes,'v6_inscription_front_texture',verts,faces,uv)


def main(root):
    root=Path(root)
    manifest=get(root/'scene-v5-complete.json')
    sec=manifest['sectors'][0]
    prior=get(root/'instances/v5-complete.json')['instances']
    oldmaterials=get(root/'materials/revision-v5.json')
    oldpbr=get(root/'pbr/revision-v5.json')
    for path in sec['geometry']+sec['instances']+sec['materials']+sec['pbr']:
        if not (root/path).is_file():raise FileNotFoundError(path)
    oldmon=[x for x in prior if x['parent'].startswith('Place/Monument soviétique fictif')]
    oldhill=[x for x in prior if x['parent']=='Place/Butte végétalisée' or x['parent'].startswith('Place/Butte végétalisée/V4b')]
    if len(oldmon)<50 or len(oldhill)<100:raise RuntimeError('Expected source V5 monument and hill not found')
    objs=[]
    relocated=collections.Counter()
    for original in prior:
        pa=original['parent']
        if pa.startswith('Place/Monument soviétique fictif') or pa=='Place/Butte végétalisée' or pa.startswith('Place/Butte végétalisée/V4b'):
            continue
        o=copy.deepcopy(original)
        if pa.startswith('Place/Butte vegetale/'):
            x,_,z=o['position']
            # GLB vegetation in V5 was normalized with its lowest vertex at y=0.
            o['position'][1]=round(bed_elevation(x,z)+0.014,5)
            relocated[pa]+=1
        objs.append(o)
    meshes=[]
    make_butte(meshes)
    create_plinth(meshes)
    make_plaque(meshes)
    create_monument(meshes)
    insc,iron=make_textures(root)
    materials=copy.deepcopy(oldmaterials);pbr=copy.deepcopy(oldpbr)
    mat=[
      ('v6_plinth_concrete',[.70,.68,.62],.91,0,'concrete',None),
      ('v6_carved_inscription',[.78,.75,.70],.91,0,'concrete',insc),
      ('v6_red_metal',[.57,.12,.08],.53,.60,'metal',iron),
    ]
    for key,color,rough,metal,pattern,image in mat:
        materials['materials'].append({'id':key,'baseColor':color,'roughness':rough,
             'metallic':metal,'transmission':0,'ior':1.5,'textureSet':'tex_'+key})
        setting={'id':'tex_'+key,'material':key,'pattern':pattern,'resolution':2048 if image==insc else 512}
        if image:setting['imageFile']=image
        pbr['sets'].append(setting)
    # Even flower/grasses positions follow geometric hill; preserve them all.
    def inst(id,parent,geo,mat,pos,size=(1.,1.,1.),ang=0.):
        return {'id':id,'parent':parent,'geometry':geo,'material':mat,'position':list(pos),'size':list(size),'rotationY':ang}
    objs.extend([
        inst('v6_butte_continue','Place/Butte relief 1m20','v6_butte_relief_solid','rev_microsoil',[0,0,0]),
        inst('v6_socle_beton','Place/Monument marteau faucille/Socle beton','v6_plinth_510_120_280',
             'v6_plinth_concrete',[CX,BASE_Y+HILL_H,CZ]),
        inst('v6_inscription_fr_uk','Place/Monument marteau faucille/Inscription contemporaine',
             'v6_inscription_front_texture','v6_carved_inscription',[CX,BASE_Y+HILL_H,CZ]),
    ])
    for name in ('v6_sickle_original','v6_hammer_handle','v6_hammer_head','v6_hammer_bevel'):
        objs.append(inst('inst_'+name,'Place/Monument marteau faucille/Sculpture acier rouge',
                  name,'v6_red_metal',[CX,SCULPT_BOTTOM,CZ]))
    # Existing medium-poly GLB grasses; no fresh GLB downloads, no geometry simplification.
    rng=random.Random(17041986)
    existinggeo={m['id'] for m in get(root/'geometry/revision-v5.json')['meshes']}
    existingmat={m['id'] for m in materials['materials']}
    for n in range(188):
        th=rng.random()*2*math.pi
        r=math.sqrt(rng.uniform(.14,1.0))
        x=23.0*r*math.cos(th);z=CZ+12.0*r*math.sin(th)
        # Keep the inscription and central pedestal unobstructed.
        if (abs(x)<3.0 and 73<z<84):continue
        h=rng.uniform(.42,.92)
        mesh,mtl=('asset_grass1_0','asset_mat_grass1_0') if n%6==0 else ('asset_grass2_0','asset_mat_grass2_0')
        if mesh not in existinggeo or mtl not in existingmat:raise RuntimeError('Missing retained grass GLB')
        objs.append(inst('v6_herbe_haute_%03d'%n,'Place/Butte relief 1m20/Herbes hautes GLB',mesh,mtl,
                         [round(x,4),round(bed_elevation(x,z)+.016,4),round(z,4)],
                         [round(h*.34,4),round(h,4),round(h*.34,4)],rng.randrange(360)))
    # Denser planting without extra unique source GLBs or material duplication.
    # Existing preserved flower2 meshes total about 285 triangles / flower.
    # Wispy grass2 meshes total 155 triangles / tuft: inexpensive cover.
    extra_cover=0
    for n in range(700):
        th=rng.uniform(0,2*math.pi)
        radius=math.sqrt(rng.uniform(.015,.99))
        x=27.3*radius*math.cos(th);z=CZ+15.2*radius*math.sin(th)
        if abs(x)<3 and abs(z-CZ)<2.7:continue
        h=rng.uniform(.25,.44);w=rng.uniform(.32,.48)
        objs.append(inst('v6_couvre_sol_%03d'%n,'Place/Butte relief 1m20/Couvre-sol GLB dense',
                         'asset_grass2_0','asset_mat_grass2_0',
                         [round(x,4),round(bed_elevation(x,z)+.017,4),round(z,4)],
                         [round(w,4),round(h,4),round(w,4)],rng.randrange(360)))
        extra_cover+=1
    extra_flowers=0
    for n in range(260):
        th=rng.uniform(0,2*math.pi)
        radius=math.sqrt(rng.uniform(.12,.96))
        x=24.8*radius*math.cos(th);z=CZ+13.9*radius*math.sin(th)
        if abs(x)<3.6 and abs(z-CZ)<3:continue
        h=rng.uniform(.39,.66);w=h*rng.uniform(.72,.89)
        key='flower1' if n%5==0 else 'flower2'
        for i,(mesh,mat) in enumerate([('asset_flower1_0','asset_mat_flower1_0'),
                                      ('asset_flower1_1','asset_mat_flower1_1')] if key=='flower1' else
                                      [('asset_flower2_0','asset_mat_flower2_0'),
                                       ('asset_flower2_1','asset_mat_flower2_1')]):
            if mesh not in existinggeo or mat not in existingmat:raise RuntimeError('Missing CC0 flower mesh')
            objs.append(inst(f'v6_fleur_dense_{n:03d}_{i}',
                 'Place/Butte relief 1m20/Fleurs GLB compactes',mesh,mat,
                 [round(x,4),round(bed_elevation(x,z)+.012,4),round(z,4)],
                 [round(w,4),round(h,4),round(w,4)],rng.randrange(360)))
        extra_flowers+=1
    ids=[o['id'] for o in objs]
    if len(set(ids))!=len(ids):raise RuntimeError('Duplicate object ID')
    nmat=len(materials['materials'])
    if nmat>256:raise RuntimeError('Material overflow '+str(nmat))
    allgeo={x['id'] for x in get(root/'geometry/benchmark.json')['meshes']}
    allgeo.update(x['id'] for x in get(root/'geometry/revision-v5.json')['meshes'])
    allgeo.update(x['id'] for x in meshes)
    assert all(o['geometry'] in allgeo and o['material'] in existingmat for o in objs)
    # Preserve pool, 8 widened approved pot GLBs, 26 tree pits, 26 trees and flower field.
    assert len([o for o in objs if o['parent']=='Architecture/Facade/Grandes jardinières GLB 1970'])>=8
    assert sum(o['parent']=='Nature/Fosses circulaires/Bordure beton' for o in objs)==26
    assert any(o['parent']=='Interieur/Piscine olympique/Eau' for o in objs)
    assert len(oldmon)>100 and len(oldhill)>100
    save(root/'geometry/monument-v6.json',{'format':'m3d-geometry-v1','units':'m','meshes':meshes})
    save(root/'instances/v6-monument.json',{'format':'m3d-instances-v1','instances':objs})
    save(root/'materials/monument-v6.json',materials)
    save(root/'pbr/monument-v6.json',pbr)
    new=copy.deepcopy(manifest)
    new['name']='CSE WARP V17 V6 - butte réelle, monument marteau-faucille 5m, socle gravé FR-UA'
    sector=new['sectors'][0]
    sector['geometry']=sec['geometry']+['geometry/monument-v6.json']
    sector['instances']=['instances/v6-monument.json']
    sector['materials']=['materials/monument-v6.json']
    sector['pbr']=['pbr/monument-v6.json']
    # Additional intended views without touching original saved camera file.
    cameras=copy.deepcopy(get(root/'cameras/v4.json'))
    cameras['cameras'].extend([
      {'id':'v6_monument_facade','name':'V6 - Monument depuis parvis du palais','position':[0,3.1,32.0],
       'target':[0,4.10,72],'fovDegrees':43},
      {'id':'v6_monument_front','name':'V6 - Monument et inscription depuis place','position':[3.0,4.5,96],
       'target':[0,3.9,72],'fovDegrees':51},
      {'id':'v6_monument_contrechamp','name':'V6 - Depuis monument vers palais','position':[-7.0,3.8,66],
       'target':[0,6.0,-12],'fovDegrees':59},
    ])
    save(root/'cameras/monument-v6.json',cameras)
    sector['cameras']=['cameras/monument-v6.json']
    save(root/'scene-v6-monument.json',new)
    save(root/'scene-v4.json',new)  # default EXE loads approved scene
    # Note: leave scene-v5-complete.json and its instances entirely unchanged.
    report={
       'version':'V17.420/V6',
       'monument':'original geometric model inspired by reference; not commercial TurboSquid asset',
       'sculptureHeightM':SCULPT_H,'sculptureBottomYM':SCULPT_BOTTOM,
       'moundHeightAboveBaseM':HILL_H,'plinthHeightM':PLINTH_H,
       'totalHeightAbovePlazaM':HILL_H+PLINTH_H+SCULPT_H,
       'inscription':FR+UA,'frontPanelFacesTextured':4,
       'removedOldSculptureInstances':len(oldmon),'removedOldHillPieces':len(oldhill),
       'flowersAndGroundcoverResnappedToNewMound':dict(relocated),
       'tallGrassClumps':sum(o['parent']=='Place/Butte relief 1m20/Herbes hautes GLB' for o in objs),
       'extraDenseGroundcoverClumps':extra_cover,'extraFlowerLocations':extra_flowers,
       'newGeometries':{x['id']:len(x['faces']) for x in meshes},
       'newMaterialsCount':nmat,'newInstancesCount':len(objs),
       'preservedPotPositions':len(set((round(o['position'][0],3),round(o['position'][2],3)) for o in objs if o['parent']=='Architecture/Facade/Grandes jardinières GLB 1970')),
       'preservedTreePits':sum(o['parent']=='Nature/Fosses circulaires/Bordure beton' for o in objs),
       'sourceSceneUnchanged':'scene-v5-complete.json',
       'deliveryScene':'scene-v6-monument.json'}
    save(root/'reports/v6-monument-validation.json',report)
    print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,required=True)
    args=ap.parse_args();main(args.root)