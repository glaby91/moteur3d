#!/usr/bin/env python3
"""Eight authentic hollow 1970s flared ceramic planter GLB models with embedded UV textures.
Coordinates are glTF-native Y up, dimensions in meters. Does not touch the WARP repo.
"""
from __future__ import annotations
import json, math, random, io, zipfile
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
import trimesh
from trimesh.visual.texture import TextureVisuals
from trimesh.visual.material import PBRMaterial
from scipy.interpolate import PchipInterpolator

OUT=Path('/mnt/data/jardinieres_70s_base_elargie_glb')
OUT.mkdir(parents=True,exist_ok=True)
N=56
HEIGHT=1.32
RIM_R=0.885

PAL=[
 ('01_Terracotta_Olive', (174,69,39), (238,210,160), (67,93,60), (212,151,49)),
 ('02_Moutarde_Creme', (196,131,38), (242,225,178), (96,106,60), (151,69,50)),
 ('03_Vert_Olive', (84,108,65), (231,217,166), (166,72,45), (199,151,52)),
 ('04_Cobalt_Ambre', (53,91,144), (242,223,179), (184,93,47), (221,163,49)),
 ('05_Brique_Soleil', (160,59,49), (243,219,177), (190,143,47), (78,100,67)),
 ('06_Ivoire_Orange', (208,177,122), (245,226,177), (187,74,43), (85,111,73)),
 ('07_Vert_Creme', (78,112,86), (242,221,172), (210,131,39), (165,65,42)),
 ('08_Ocre_Turquoise', (169,110,50), (240,219,167), (44,107,114), (194,75,47)),
]

def make_texture(kind, palettes, seed):
    base, cream, accent, gold = palettes
    W,H=2048,1024
    rng=np.random.default_rng(seed)
    img=np.zeros((H,W,3),dtype=np.uint8)
    # subtle matte glaze variation / baked kiln flecks
    Y=np.arange(H)[:,None]
    X=np.arange(W)[None,:]
    n=rng.normal(0,2.5,(H,W)) + 1.5*np.sin(X*.054+np.cos(Y*.017)*3)+0.9*np.sin(Y*.16)
    for k,c in enumerate(base):
        img[:,:,k]=np.clip(c+n,0,255).astype('uint8')
    im=Image.fromarray(img,'RGB')
    d=ImageDraw.Draw(im)
    def yp(h):return round((1-h/HEIGHT)*H)
    def stripe(a,b,c):
        top,bottom=sorted([yp(a),yp(b)])
        d.rectangle((0,top,W,bottom),fill=tuple(c))
    # bold clean geometric POP banding, designed for substantial 1970s planters
    stripe(.015,.15,tuple(int(c*.62) for c in base))
    stripe(.175,.215,gold)
    stripe(.22,.252,cream)
    stripe(.34,.38,cream)
    stripe(.43,.49,accent)
    stripe(.51,.55,tuple(int(c*.90) for c in base))
    stripe(.58,1.005,cream)
    stripe(1.015,1.07,accent)
    stripe(1.09,1.18,gold)
    stripe(1.19,1.233,cream)
    stripe(1.238,1.272,accent)
    # Repeating bold chevron / fan elements with large negative space
    count=12
    for i in range(count):
        xm=(i+.5)*W/count
        ww=.44*W/count
        vertices=[(xm,yp(.968)),(xm+ww,yp(.76)),(xm,yp(.615)),(xm-ww,yp(.76))]
        d.polygon(vertices, fill=base)
        # gold point running down through the central motif
        vertices2=[(xm,yp(.88)),(xm+ww*.36,yp(.755)),(xm,yp(.65)),(xm-ww*.36,yp(.755))]
        d.polygon(vertices2, fill=gold)
        d.ellipse((xm-ww*.10,yp(.84),xm+ww*.10,yp(.80)),fill=accent)
    # diagonal interlocking chevrons across main lower band
    for i in range(count*2+1):
        xm=(i+.3)*W/(count*2)
        ww=W/(count*2)*.42
        d.polygon([(xm-ww,yp(.55)),(xm,yp(.44)),(xm+ww,yp(.55)),(xm,yp(.58))],fill=gold)
    # stamped repeating dots near lip
    for i in range(48):
        xm=(i+.5)*W/48
        rw=3.2
        d.ellipse((xm-rw,yp(1.164)-rw,xm+rw,yp(1.164)+rw),fill=cream)
    # horizontal darker pinstripes over the glaze
    for h in [.152,.276,.399,.501,.571,1.085,1.236]:
        y=yp(h)
        color=tuple(max(0,int(c*.7)) for c in accent)
        d.rectangle((0,y-2,W,y+2),fill=color)
    return im

# A profile curve which is intentionally much more flared than a typical flowerpot.
outer_points=[(0.0,.460),(.04,.463),(.125,.471),(.225,.491),(.34,.531),(.46,.590),(.58,.650),(.72,.715),(.87,.774),(1.02,.823),(1.16,.865),(1.26,.882)]
interp=PchipInterpolator([p[0] for p in outer_points],[p[1] for p in outer_points])
outer_profile=[(float(h),float(interp(h))) for h in np.linspace(0,1.26,30)]
# Curved glazed lip connects the outer shell and the inner wall, physically open at the top.
lip_profile=[(1.271,.885),(1.295,.88),(1.315,.868),(1.320,.848),(1.316,.827),(1.297,.815),(1.273,.806)]
# Interior follows the outer shell with a visible wall thickness; closed inside, open at mouth.
inner_points=[(.18,.405),(.27,.450),(.40,.505),(.56,.599),(.75,.676),(.94,.744),(1.12,.796),(1.273,.806)]
inner_interp=PchipInterpolator([p[0] for p in inner_points],[p[1] for p in inner_points])
inner_profile=[(float(h),float(inner_interp(h))) for h in np.linspace(1.273,.18,19)]

def lathe(name,profile,material=None,texture=None,radial=N,angle_offset=0.0,reverse=False):
    vertices=[];uv=[];faces=[]
    for j,(h,r) in enumerate(profile):
        for k in range(radial+1):
            t=2*math.pi*k/radial+angle_offset
            vertices.append([r*math.cos(t),h,r*math.sin(t)])
            uv.append([k/radial,1-h/HEIGHT])
    for j in range(len(profile)-1):
        for k in range(radial):
            a=j*(radial+1)+k
            b=a+1
            c=(j+1)*(radial+1)+k
            e=c+1
            # Determine normal separately; make smooth surface with UV seam
            if not reverse:faces.extend([[a,c,b],[b,c,e]])
            else:faces.extend([[a,b,c],[b,e,c]])
    m=trimesh.Trimesh(vertices=np.asarray(vertices),faces=np.asarray(faces),process=False)
    if texture is not None:
        m.visual=TextureVisuals(uv=np.asarray(uv),material=PBRMaterial(
            name=material or name,baseColorTexture=texture,metallicFactor=.0,roughnessFactor=.31,
            alphaMode='OPAQUE',doubleSided=True))
    elif material:
        m.visual=TextureVisuals(material=PBRMaterial(name=name,baseColorFactor=material,
            metallicFactor=.0,roughnessFactor=.31,doubleSided=True))
    return m


def cap(name,r,height,material,uv_flip=False,n=N):
    verts=[[0.,height,0.]];faces=[]
    for k in range(n):
        t=2*np.pi*k/n
        verts.append([r*np.cos(t),height,r*np.sin(t)])
    for k in range(n):
        faces.append([0,1+k,1+(k+1)%n])
    m=trimesh.Trimesh(vertices=verts,faces=faces,process=False)
    m.visual=TextureVisuals(material=PBRMaterial(name=name,baseColorFactor=material,metallicFactor=0.,roughnessFactor=.65,doubleSided=True))
    return m

def make_soil_texture(seed):
    W=H=1024
    rng=np.random.default_rng(seed+300)
    noise=rng.normal(0,12,(H,W,1))
    coarse=rng.normal(0,1,(64,64))
    coarse=np.asarray(Image.fromarray(np.uint8(np.clip(128+coarse*30,0,255))).resize((W,H),Image.Resampling.BILINEAR)).astype(float)[:,:,None]-128
    col=np.array([66,49,31])[None,None,:]+noise+coarse*.32
    arr=np.uint8(np.clip(col,0,255)); im=Image.fromarray(arr,'RGB')
    d=ImageDraw.Draw(im)
    for _ in range(11000):
        x=int(rng.integers(0,W));y=int(rng.integers(0,H));rad=float(rng.uniform(.3,2.7))
        val=int(rng.integers(56,120));c=(val,int(val*.83),int(val*.65))
        d.ellipse((x-rad,y-rad,x+rad,y+rad), fill=c)
    return im

def soil_disc(image):
    # surface at y=1.13 m, inside the pot with 12cm of open rim visible
    height=1.131
    r=float(inner_interp(height))-.008
    rings=6
    v=[];uv=[];f=[]
    for k in range(rings+1):
        rr=r*k/rings
        for j in range(N+1):
            a=2*np.pi*j/N
            # sub-centimeter roughness, small enough not to perforate the shell
            dh=.004*np.sin(13*a+5*k)+.003*np.cos(7*a-3*k)
            v.append([rr*np.cos(a),height+dh,rr*np.sin(a)])
            uv.append([.5+.46*k/rings*np.cos(a),.5+.46*k/rings*np.sin(a)])
    for k in range(rings):
        for j in range(N):
            a=k*(N+1)+j;b=a+1;c=a+N+1;d=c+1
            f.extend([[a,b,c],[b,d,c]])
    mesh=trimesh.Trimesh(vertices=v,faces=f,process=False)
    mesh.visual=TextureVisuals(uv=uv,material=PBRMaterial(name='Terre_vegetale_granuleuse',baseColorTexture=image,metallicFactor=0.,roughnessFactor=.98,doubleSided=True))
    return mesh

def build(label,colors,variant_idx):
    scene=trimesh.Scene()
    texture=make_texture(label,colors,2026+variant_idx)
    ext=lathe('Ceramique_emaillée_motif_70s',outer_profile,material='Ceramique_POP_70s',texture=texture)
    inner=lathe('Paroi_interieure_ceramique',inner_profile,material=(*colors[1],255),reverse=True)
    lip=lathe('Rebord_roule_epais',lip_profile,material=(*colors[1],255))
    # Polished, fine decorative seam at outer upper shoulder
    thin_band=lathe('Anneau_decoratif_epaule',[(1.237,.872),(1.244,.877),(1.251,.879)],material=(*colors[2],255))
    bottom=cap('Sous_face_ceramique',.460,.0,(*colors[0],255))
    inner_bottom=cap('Fond_interieur_ferme',float(inner_interp(.18)),.18,(*colors[1],255))
    earth=soil_disc(make_soil_texture(variant_idx))
    for id,m in [('Corps_exterieur',ext),('Paroi_interieure',inner),('Rebord_arrondi',lip),('Liseré',thin_band),('Fond_exterieur',bottom),('Fond_interieur',inner_bottom),('Terre_plantee',earth)]:
        scene.add_geometry(m,geom_name=id,node_name=id)
    filename=f'Jardiniere_1970_{label}.glb'
    path=OUT/filename
    path.write_bytes(scene.export(file_type='glb'))
    return {'fichier':filename,'triangles':int(sum(len(g.faces) for g in scene.geometry.values())),
            'maillages':len(scene.geometry),'dimensions_m':[1.77,1.32,1.77], 'diametre_base_m':0.92,
            'taille_octets':path.stat().st_size}

if __name__=='__main__':
    manifest=[build(label,colors,i) for i,(label,*c) in enumerate(PAL) for colors in [tuple(c)]]
    (OUT/'MANIFESTE.json').write_text(json.dumps({'objet':'Jardinières évasées POP 1970 avec base élargie 92 cm pour façade WARP','unites':'metres','axe_vertical':'Y','contenu':'8 modèles individuels, céramique texturée UV, ouverture réelle, fond de terre, aucune végétation','variantes':manifest},ensure_ascii=False,indent=2),encoding='utf-8')
    zipdest=Path('/mnt/data/CSE-WARP-JARDINIERES-BASE-ELARGIE-1970-8-GLB.zip')
    with zipfile.ZipFile(zipdest,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(OUT.glob('*')):z.write(p,arcname=p.name)
    print(json.dumps(manifest,indent=2,ensure_ascii=False));print('ZIP',zipdest,zipdest.stat().st_size)