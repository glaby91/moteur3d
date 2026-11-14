#!/usr/bin/env python3
"""Reconstruct the approved independent V17.493 rooftop sign geometry as GLB.
Original model: 112 objects, 15,780 triangles. Python trimesh, shapely >=2.1,
opencv-python-headless, pillow, numpy. The scene is NOT modified by this script.
"""
import numpy as np, math, cv2, trimesh, argparse
from PIL import ImageFont, Image, ImageDraw
from shapely.geometry import Polygon, MultiPolygon
from shapely import constrained_delaunay_triangles
from trimesh.visual.material import PBRMaterial
from trimesh.visual import TextureVisuals
from pathlib import Path
parts=[]
def mat(name,col,emissive=(0,0,0),metal=0,rough=.55):
    def rgb(v):return [int(max(0,min(255,c))) for c in v]+[255]
    return PBRMaterial(name=name,baseColorFactor=rgb(col),metallicFactor=metal,roughnessFactor=rough,emissiveFactor=[float(v) for v in emissive],doubleSided=True)
metal=mat('Charpente acier noir vieilli',(37,45,52),metal=.72)
edge=mat('Liserés acier inoxydable',(117,123,129),metal=.86)
red=mat('Etoile acier rouge rubis',(177,29,20),(.20,.006,.006),metal=.18)
redLight=mat('Néon rouge étoile',(255,55,33),(1,.08,.03))
warm=mat('Lettrage néon blanc chaud',(255,224,164),(1,.74,.36),metal=.08)
white=mat('Tube nageur blanc',(255,248,217),(.98,.92,.79))
cyan=mat('Néon eau cyan',(15,206,255),(.03,.68,1))
blue=mat('Néon eau bleu',(35,113,255),(.07,.21,1))
midcyan=mat('Néon eau turquoise',(28,237,232),(.12,.90,.78))
bronze=mat('Boîtier lettres acier chromé',(88,82,70),metal=.87,rough=.28)
back=mat('Panneau arrière sombre',(28,39,50),metal=.50,rough=.7)
def add(m,name,material):
    m.visual=TextureVisuals(material=material)
    m.metadata={'name':name}
    parts.append((name,m))
def box(name,ext,loc,material):
    m=trimesh.creation.box(extents=ext);m.apply_translation(loc);add(m,name,material)
def cylinder(name,start,end,r,material,sections=8):
    a=np.asarray(start,float);b=np.asarray(end,float)
    if np.linalg.norm(b-a)<1e-8:return
    m=trimesh.creation.cylinder(radius=r,height=float(np.linalg.norm(b-a)),sections=sections)
    m.apply_transform(trimesh.geometry.align_vectors([0,0,1],b-a))
    m.apply_translation((a+b)*.5);add(m,name,material)
def tube(name,points,r,material,sides=8):
    p=np.array(points,float);verts=[];faces=[]
    for i,pos in enumerate(p):
        tangent=(p[min(i+1,len(p)-1)]-p[max(i-1,0)])
        tangent/=np.linalg.norm(tangent)
        ref=np.array([0,1.,0])
        side=np.cross(tangent,ref);side/=np.linalg.norm(side)
        up=np.cross(tangent,side);up/=np.linalg.norm(up)
        for j in range(sides):
            ang=2*math.pi*j/sides
            verts.append(pos+r*(math.cos(ang)*side+math.sin(ang)*up))
        if i:
            for j in range(sides):
                a=(i-1)*sides+j;b=(i-1)*sides+(j+1)%sides;c=i*sides+j;d=i*sides+(j+1)%sides
                faces.extend(((a,b,c),(b,d,c)))
    for j in range(1,sides-1):
        faces.append((0,j+1,j))
        faces.append(((len(p)-1)*sides,(len(p)-1)*sides+j,(len(p)-1)*sides+j+1))
    add(trimesh.Trimesh(vertices=verts,faces=faces,process=False),name,material)
def polyextrude(name,polygon,depth,center_y,material):
    if not polygon.is_valid:polygon=polygon.buffer(0)
    if polygon.is_empty:return
    if isinstance(polygon,MultiPolygon):
        for n,poly in enumerate(polygon.geoms):polyextrude(name+str(n),poly,depth,center_y,material)
        return
    y0=center_y-depth/2;y1=center_y+depth/2
    verts=[];faces=[]
    for tri in constrained_delaunay_triangles(polygon).geoms:
        if not polygon.covers(tri.representative_point()):continue
        ring=list(tri.exterior.coords)[:3]
        ix=len(verts)
        verts.extend([(x,y0,z) for x,z in ring]);verts.extend([(x,y1,z) for x,z in ring])
        faces.extend(((ix+0,ix+2,ix+1),(ix+3,ix+4,ix+5)))
    rings=[polygon.exterior]+list(polygon.interiors)
    for ring in rings:
        coords=list(ring.coords)
        for a,b in zip(coords,coords[1:]):
            ix=len(verts);verts.extend(((a[0],y0,a[1]),(b[0],y0,b[1]),(a[0],y1,a[1]),(b[0],y1,b[1])))
            faces.extend(((ix,ix+1,ix+2),(ix+1,ix+3,ix+2)))
    if not verts:return
    add(trimesh.Trimesh(vertices=np.array(verts),faces=np.array(faces),process=False),name,material)

def create(filename):
    global parts
    parts=[]
    # x: 16m width, y +/- depth (front negative y), z vertically upward.
    box('Lisse basse structure 16m',(15.8,.055,.055),(0,.28,.08),metal)
    box('Lisse haute triangulation',(15.5,.05,.05),(0,.40,2.58),metal)
    for x in np.linspace(-7.6,7.6,13):
        cylinder(f'Montant_vertical_{x:.2f}',(x,.35,.12),(x,.35,2.60),.025,metal,6)
        cylinder(f'Jambe_de_force_{x:.2f}',(x,.35,2.18),(x,.92,.07),.019,metal,6)
    for x in np.linspace(-7.35,7.25,12):
        cylinder(f'Croisillon_{x:.2f}',(x,.36,.21),(x+1.05,.36,2.50),.012,metal,6)
    for x in np.linspace(-7.45,7.45,8):
        box(f'Platine_toit_{x:.2f}',(.22,.46,.055),(x,.48,.02),metal)
        for dx in (-.075,.075):
            cylinder(f'Ancrage_toit_{x:.2f}_{dx}',(x+dx,.60,.04),(x+dx,.60,.16),.01,edge,6)
    n=5;cx=-6.67;cz=1.88;ro=.69;ri=.31
    coords=[]
    for i in range(2*n):
        a=math.pi/2+i*math.pi/n
        rad=ro if i%2==0 else ri
        coords.append((cx+rad*math.cos(a),cz+rad*math.sin(a)))
    star=Polygon(coords)
    polyextrude('Etoile soviétique rouge 3D',star,.18,-.19,red)
    for i in range(2*n):
        a,b=coords[i],coords[(i+1)%(2*n)]
        tube(f'Contour_rouge_{i}',[(a[0],-.32,a[1]),(b[0],-.32,b[1])],.032,redLight,8)
    TEXT='БАССЕЙН'
    # DejaVuSans Bold is present on the Windows Actions runner via installation.
    import os
    font_path=os.environ.get('CSE_WARP_FONT','/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf')
    font=ImageFont.truetype(font_path,320)
    sizes=[]
    for char in TEXT:
        b=font.getbbox(char);sizes.append((char,font.getlength(char)))
    scale=9.65/sum(w for _,w in sizes)
    x=-5.53
    for gi,(glyph,advance) in enumerate(sizes):
        img=Image.new('L',(400,420));d=ImageDraw.Draw(img)
        d.text((15,-8),glyph,font=font,fill=255,stroke_width=0)
        pix=np.array(img)
        contours,hier=cv2.findContours(pix,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)
        shapes=[]
        if hier is not None:
            hier=hier[0]
            for j,c in enumerate(contours):
                if hier[j][3]!=-1:continue
                outer=cv2.approxPolyDP(c,0.85,True).reshape(-1,2)
                if len(outer)<3:continue
                holes=[]
                child=hier[j][2]
                while child!=-1:
                    co=cv2.approxPolyDP(contours[child],.75,True).reshape(-1,2)
                    if len(co)>=3:holes.append([(v[0],v[1]) for v in co])
                    child=hier[child][0]
                p=Polygon(outer,holes)
                if not p.is_valid:p=p.buffer(0)
                if not p.is_empty:shapes.append(p)
        glyphWidth=advance*scale
        ypixels=np.where(pix>0)[0]
        if not len(ypixels):continue
        ztop=2.36;ytop=min(ypixels)
        for j,p in enumerate(shapes):
            ex=[(x+(xx-15)*scale,ztop-(yy-ytop)*scale) for xx,yy in p.exterior.coords]
            holes=[[(x+(xx-15)*scale,ztop-(yy-ytop)*scale) for xx,yy in ring.coords] for ring in p.interiors]
            shape=Polygon(ex,holes)
            polyextrude(f'Boitier_lettre_{gi}_{j}_{glyph}',shape,.135,-.14,bronze)
            polyextrude(f'Face_neon_lettre_{gi}_{j}_{glyph}',shape.buffer(-.014),.026,-.232,warm)
        x+=glyphWidth
    for k,(zmat,clr,off) in enumerate([(0.88,blue,0.0),(1.02,cyan,.15),(1.16,midcyan,.33)]):
        pts=[]
        for t in np.linspace(0,1,100):
            xx=3.95+3.75*t;zz=zmat+.12*math.sin(t*4.4*math.pi+.8)+.12*t
            pts.append((xx,-.32-k*.018,zz))
        tube(f'Vague_lumineuse_{k+1}',pts,.046,clr,9)
        tube(f'Gaine_vague_{k+1}',[(xx,.05,zz) for xx,_,zz in pts],.044,metal,7)
    head=trimesh.creation.icosphere(subdivisions=2,radius=.125)
    head.apply_translation((7.0,-.32,2.20));add(head,'Nageur_tête_lumineuse',white)
    tube('Nageur_dos_bras',[(6.75,-.30,2.02),(6.96,-.30,1.92),(7.22,-.30,1.75),(7.42,-.30,1.65)],.078,white)
    tube('Nageur_bras_levé',[(6.94,-.32,1.94),(6.73,-.32,2.23),(6.48,-.32,2.29),(6.28,-.32,2.12)],.063,white)
    tube('Nageur_silhouette_avec_vague',[(6.39,-.32,1.58),(6.68,-.32,1.66),(7.02,-.32,1.57),(7.38,-.32,1.58)],.05,white)
    for x in (-7.13,-6.13,-5.4,-3.7,-2.0,-.2,1.5,3.3,4.7,5.8,7.2):
        cylinder(f'Suspente_{x:.1f}',(x,.37,.1),(x,.37,2.43),.016,metal,6)
    scene=trimesh.Scene()
    for name,mesh in parts:scene.add_geometry(mesh,geom_name=name,node_name=name)
    output=Path(filename);output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(scene.export(file_type='glb'))
    print('Created',output,'parts',len(parts),'triangles',sum(len(m.faces) for _,m in parts))
    assert len(parts)==112 and 14000<=sum(len(m.faces) for _,m in parts)<=17000

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--out',required=True)
    args=ap.parse_args()
    create(args.out)
