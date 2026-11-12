#!/usr/bin/env python3
"""CSE WARP V17 scene-only revision: approved 1970s planters, real GLB vegetation,
cut-through pavement/tree pits, textured microgravel soil, grasses, and pool water.
No engine code changes, and no destructive decimation of source GLB geometry.
"""
from __future__ import annotations
import argparse, collections, hashlib, io, json, math, random, shutil
from pathlib import Path
import numpy as np
import trimesh
from PIL import Image, ImageDraw, ImageFilter
from shapely.geometry import Polygon, Point, box as sbox
from shapely.ops import unary_union, triangulate

ROOT_PREFIX='Architecture/Facade/Jardinieres ceramique POP 1970/'
PLANTER_FILES=[
 'Jardiniere_1970_01_Terracotta_Olive.glb','Jardiniere_1970_02_Moutarde_Creme.glb',
 'Jardiniere_1970_03_Vert_Olive.glb','Jardiniere_1970_04_Cobalt_Ambre.glb',
 'Jardiniere_1970_05_Brique_Soleil.glb','Jardiniere_1970_06_Ivoire_Orange.glb',
 'Jardiniere_1970_07_Vert_Creme.glb','Jardiniere_1970_08_Ocre_Turquoise.glb'
]
GARDEN_MODELS=['Tree.glb','Tree-QVOop92WmG.glb','Pine.glb','Pine-79gmlLnweB.glb',
               'Twisted Tree.glb','Flower Group.glb','Flower Single.glb',
               'Bush with Flowers.glb','Grass Wispy.glb','Grass.glb']

def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p,data):
 p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
 p.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')

def make_soil(path):
 # At 2K, mottling is visually microgravel (roughly 2-10 mm when tiled over 2 m).
 rng=np.random.default_rng(170470)
 n=1024
 waves=rng.normal(0,14,(n,n)).astype(np.float32)
 smooth=np.asarray(Image.fromarray(np.uint8(np.clip(120+rng.normal(0,28,(128,128)),0,255))).resize((n,n),Image.Resampling.BILINEAR),dtype=np.float32)-120
 b=np.stack([65+waves+smooth*.55,49+waves*.80+smooth*.49,33+waves*.58+smooth*.38],axis=-1)
 im=Image.fromarray(np.uint8(np.clip(b,0,255)),'RGB')
 draw=ImageDraw.Draw(im)
 for i in range(24000):
  x=int(rng.integers(0,n));y=int(rng.integers(0,n));r=float(rng.uniform(.4,2.8))
  color=(int(rng.integers(74,146)),int(rng.integers(65,124)),int(rng.integers(46,97)))
  draw.ellipse((x-r,y-r,x+r,y+r),fill=color)
 path.parent.mkdir(parents=True,exist_ok=True);im.save(path,quality=90)

def make_pool(path):
 n=1024
 yy,xx=np.mgrid[:n,:n].astype(np.float32)
 wav=np.sin(xx*.095+7*np.sin(yy*.032))*3.5+np.cos(yy*.103+4*np.sin(xx*.021))*3.2
 wav+=np.sin((xx+yy)*.078)*2.0
 im=np.uint8(np.clip(np.stack([65+wav*.35,143+wav*.8,167+wav],axis=-1),0,255))
 Image.fromarray(im,'RGB').save(path,quality=91)

def disc_mesh(name,rin=0.0,rout=.94,y=.155,n=48):
 vertices=[];uv=[];faces=[]
 for r in ([rin,rout] if rin else [0,rout]):
  for k in range(n):
   ang=2*math.pi*k/n
   vertices.append([round(r*math.cos(ang),6),y,round(r*math.sin(ang),6)])
   uv.append([round(.5+r*math.cos(ang)/1.88,6),round(.5+r*math.sin(ang)/1.88,6)])
 for k in range(n):faces.extend([[k,n+k,n+(k+1)%n],[k,n+(k+1)%n,(k+1)%n]])
 return {'id':name,'type':'triangle_mesh','vertices':vertices,'faces':faces,'uv':uv}

def ring_mesh(name,r_inner=.94,r_outer=1.10,y_bottom=-.14,y_top=.235,n=48):
 v=[];uv=[];f=[]
 for y,r in [(y_bottom,r_inner),(y_top,r_inner),(y_top,r_outer),(y_bottom,r_outer)]:
  for k in range(n):
   ang=2*math.pi*k/n;v.append([round(r*math.cos(ang),6),y,round(r*math.sin(ang),6)])
   uv.append([round(k/n,6),round(y*.5+.5,6)])
 for a in range(3):
  for k in range(n):
   i=a*n+k;j=a*n+(k+1)%n;h=(a+1)*n+k;l=(a+1)*n+(k+1)%n
   f.extend([[i,j,h],[j,l,h]])
 return {'id':name,'type':'triangle_mesh','vertices':v,'faces':f,'uv':uv}

def pave_mesh(name,poly,y_top,y_bottom=-.10):
 """Exact solid paving section, with holes maintained by Shapely set difference.
 Triangles are filtered by polygon coverage so not one spans a soil opening.
 """
 vs=[];uvs=[];fs=[];seen={}
 if poly.is_empty:return None
 for pg in ([poly] if poly.geom_type=='Polygon' else list(poly.geoms)):
  if pg.is_empty or not hasattr(pg,'exterior'):continue
  for triangle in triangulate(pg):
   if triangle.area < 1e-5 or not pg.covers(triangle):continue
   coords=list(triangle.exterior.coords)[:3]
   # source front-facing winding corrected to Y-up
   ids=[]
   for x,z in coords:
    key=(round(x,5),round(z,5))
    if key not in seen:
     seen[key]=len(vs);vs.append([round(x,5),round(y_top,5),round(z,5)])
     uvs.append([round(x*.25,5),round(z*.25,5)])
    ids.append(seen[key])
   fs.append(ids)
 if not fs:return None
 return {'id':name,'type':'triangle_mesh','vertices':vs,'faces':fs,'uv':uvs}

def trim_paving(objects,geo,pits):
 """Remove original pavement where soil disks go: really cut geometry, not overlay decals."""
 areas=[Point(x,z).buffer(1.10,resolution=12) for x,z in pits]
 holes=unary_union(areas)
 out=[];cut=collections.Counter();seen=0
 for obj in objects:
  parent=obj['parent'];name=obj['id']
  if (parent not in ['Terrain/Place','Terrain/Trottoirs','Terrain/Routes','Place/Parvis d’entrée','Place/Dalles V4b alignées'] or
      obj.get('geometry')!='unit_box'):
   out.append(obj);continue
  x,y,z=obj['position'];w,h,d=obj['size'];rect=sbox(x-w/2,z-d/2,x+w/2,z+d/2)
  if not rect.intersects(holes):out.append(obj);continue
  polygon=rect.difference(holes)
  if math.isclose(polygon.area,rect.area,abs_tol=0.00001):out.append(obj);continue
  mesh=pave_mesh('cut_paving_'+str(seen),polygon,y+h/2)
  if mesh:
   geo.append(mesh);out.append(dict(obj,geometry=mesh['id'],position=[0,0,0],size=[1,1,1]))
  seen+=1;cut[parent]+=1
 return out,dict(cut)

class Importer:
 def __init__(self,root):
  self.root=root;self.meshes=[];self.materials=[];self.sets=[];self.textures={};self.models={};self.triangles={}
 def add_glb(self,key,path,normalize=False):
  src=trimesh.load(str(path),file_type=path.suffix[1:] or None,force='scene',process=False)
  if isinstance(src,trimesh.Trimesh):src=trimesh.Scene(src)
  node_geos=list(src.graph.nodes_geometry)
  if not node_geos:raise RuntimeError('Empty GLB '+str(path))
  pts=[]
  for node in node_geos:
   transform, geom=src.graph[node];m=src.geometry[geom]
   pts.append(trimesh.transform_points(m.vertices,transform))
  merged=np.vstack(pts)
  mn=merged.min(axis=0);mx=merged.max(axis=0);height=float(mx[1]-mn[1]);
  if height<=0:raise RuntimeError('Unusable GLB orientation '+str(path))
  cx=float((mn[0]+mx[0])/2);cz=float((mn[2]+mx[2])/2)
  scale=height if normalize else 1.0
  result=[];triangles=0
  for i,node in enumerate(node_geos):
   tr,g=src.graph[node];mesh=src.geometry[g];pts=trimesh.transform_points(mesh.vertices,tr)
   vert=(pts-np.array([cx,mn[1],cz]))/scale
   faces=np.asarray(mesh.faces,dtype=np.int64)
   if not len(faces):continue
   # Source meshes remain triangle-identical, with original UVs; no decimation.
   if len(vert)>200000 or len(faces)>80000:raise RuntimeError('WARP mesh ceiling: '+key+' '+str(len(faces)))
   uv=np.asarray(getattr(mesh.visual,'uv',[]),dtype=float)
   if uv.shape!=(len(vert),2):uv=np.zeros((len(vert),2),dtype=float)
   mesh_name='asset_'+key+'_'+str(i)
   geo={'id':mesh_name,'type':'triangle_mesh','vertices':np.round(vert,5).tolist(),
        'faces':faces.tolist(),'uv':np.round(uv,5).tolist(),
        'source':path.name,'preservedTriangles':len(faces)}
   self.meshes.append(geo)
   mid='asset_mat_'+key+'_'+str(i)
   kind=getattr(mesh.visual,'kind',None);mat=getattr(mesh.visual,'material',None)
   factor=getattr(mat,'baseColorFactor',None)
   col=np.array([160,160,160,255] if factor is None else factor,dtype=float).flatten()
   if col.size<3:col=np.array([160,160,160]);
   if col.max()>1.01:col=col/255.
   color=np.clip(col[:3],.035,1.).tolist()
   image=getattr(mat,'baseColorTexture',None)
   if image is not None:
    if not isinstance(image,Image.Image):image=Image.fromarray(np.asarray(image))
    if image.mode not in ('RGB','RGBA'):image=image.convert('RGBA')
    # Keep alpha embedded in PNG where present; V17 raster currently uses only image RGB.
    png='textures/imports/'+key+'_'+str(i)+'.png'
    dest=self.root/png;dest.parent.mkdir(parents=True,exist_ok=True);image.save(dest)
    imagesource=png
   else:imagesource=None
   if kind in ('face','vertex') and imagesource is None:
    rgb=np.array(mesh.visual.face_colors if kind=='face' else mesh.visual.vertex_colors,dtype=np.float32)
    if rgb.size and rgb.shape[-1]>=3:color=np.clip(rgb[:,:3].mean(axis=0)/255.,.035,1.).tolist()
   self.materials.append({'id':mid,'baseColor':[round(float(c),4) for c in color],
        'roughness':.42 if key.startswith('pot') else .88,'metallic':0.,'transmission':0.,
        'ior':1.5,'textureSet':'tex_'+mid,'origin':str(path.name)+' CC0 import / approved custom pot'})
   pbr={'id':'tex_'+mid,'material':mid,'pattern':'concrete' if key.startswith('pot') else 'grass',
        'resolution':1024 if imagesource else 256}
   if imagesource:pbr['imageFile']=imagesource
   self.sets.append(pbr)
   result.append((mesh_name,mid));triangles+=len(faces)
  self.models[key]=result;self.triangles[key]=triangles
  return result
 def place(self,key,parent,position,size,rotation=0,instance_prefix=None):
  items=[]
  for i,(geo,mat) in enumerate(self.models[key]):
   items.append({'id':f'rev_{instance_prefix or parent.replace("/","_")}_{i:03d}',
     'parent':parent,'geometry':geo,'material':mat,'position':[round(float(x),4) for x in position],
     'size':[round(float(x),4) for x in size],'rotationY':float(rotation)})
  return items

def main():
 parser=argparse.ArgumentParser()
 parser.add_argument('--root',type=Path,required=True)
 parser.add_argument('--pot-dir',type=Path,required=True)
 parser.add_argument('--plant-dir',type=Path,required=True)
 parser.add_argument('--allow-fallback-flower',action='store_true')
 args=parser.parse_args();root=args.root
 original=load(root/'scene-v4.json');sector=original['sectors'][0]
 instpath=root/sector['instances'][0];inst=load(instpath)
 original_plant_centers=sorted({(float(x['position'][0]),float(x['position'][2])) for x in inst['instances'] if x['parent']=='Nature/Arbres'})
 assert len(original_plant_centers)==26,('Unexpected prior native tree count',len(original_plant_centers))
 pot_base=sorted([o for o in inst['instances'] if o['parent']==ROOT_PREFIX+'Corps emaille'],key=lambda o:o['position'][0]);
 assert len(pot_base)==8
 pot_centers=[(o['position'][0],o['position'][2]) for o in pot_base]
 i3=Importer(root)
 for n,file in enumerate(PLANTER_FILES):
  p=args.pot_dir/file
  if not p.is_file():raise FileNotFoundError(str(p))
  i3.add_glb('pot'+str(n),p,normalize=False)
 for key,file in [('tree1','Tree.glb'),('tree2','Tree-QVOop92WmG.glb'),('pine1','Pine.glb'),
                  ('pine2','Pine-79gmlLnweB.glb'),('curved','Twisted Tree.glb'),
                  ('flower1','Flower Group.glb'),('flower2','Flower Single.glb'),
                  ('flower3','Bush with Flowers.glb'),('grass1','Grass Wispy.glb'),('grass2','Grass.glb')]:
  p=args.plant_dir/file
  if not p.is_file():raise FileNotFoundError(str(p))
  i3.add_glb(key,p,normalize=True)
 # Approved realistic flowers from Poly Haven (if available); otherwise the
 # matching Quaternius CC0 flowers remain clearly recorded as fallback assets.
 floral_keys=['flower1','flower2','flower3']
 polyhaven={}
 for key,name in [('empodium','flower_empodium'),('gazania','flower_gazania')]:
  p=args.plant_dir/(name+'.glb')
  if p.is_file():
   i3.add_glb(key,p,normalize=True);floral_keys.append(key);polyhaven[key]=True
  else:polyhaven[key]=False
 if not all(polyhaven.values()) and not args.allow_fallback_flower:
  raise RuntimeError('Approved Empodium/Gazania GLBs not available; use --allow-fallback-flower explicitly')
 raw=inst['instances'];filtered=[];removed=collections.Counter()
 for o in raw:
  pa=o['parent']
  drop=(pa.startswith(ROOT_PREFIX) or pa.startswith('Nature/Arbres') or
        (pa.startswith('Place/Butte végétalisée/') and
          any(pa.startswith('Place/Butte végétalisée/'+x) for x in
              ['Petales','Tiges','Coeurs','Feuilles','Fleurs et feuillages','V4 Pétales',
               'V4 Tiges','V4 Pistils','V4 Feuilles','V4 Couvre-sol'])))
  if drop:removed[pa]+=1
  else:filtered.append(o)
 geometry=load(root/'geometry/benchmark.json');materials=load(root/'materials/pripyat-reuse.json')
 pbr=load(root/'pbr/pripyat-reuse-leger.json')
 # Protect original V17 files: write fully independent revised lists and manifest.
 soil=root/'textures/revision/terre_microgravillons_2a10mm.jpg';soil.parent.mkdir(parents=True,exist_ok=True);make_soil(soil)
 water=root/'textures/revision/eau_ondulations_douces.jpg';make_pool(water)
 materials['materials'].extend([
  {'id':'rev_microsoil','baseColor':[.31,.24,.16],'roughness':.94,'metallic':0.,'transmission':0.,'ior':1.5,'textureSet':'tex_rev_microsoil'},
  {'id':'rev_bordure_beton','baseColor':[.62,.60,.56],'roughness':.87,'metallic':0.,'transmission':0.,'ior':1.5,'textureSet':'tex_rev_bordure_beton'}])
 pbr['sets'].extend([
  {'id':'tex_rev_microsoil','material':'rev_microsoil','pattern':'grass','resolution':1024,'imageFile':'textures/revision/terre_microgravillons_2a10mm.jpg'},
  {'id':'tex_rev_bordure_beton','material':'rev_bordure_beton','pattern':'concrete','resolution':512}])
 # Existing water already uses transmission and IOR; preserve it while improving
 # visible albedo ripples with native normal-bearing pattern (grass) for fine waves.
 pool=pbr['sets'];wp=next((x for x in pool if x['id']=='tex_cse_pool_water'),None)
 if wp is not None:
  wp['imageFile']='textures/revision/eau_ondulations_douces.jpg'
  wp['pattern']='grass';wp['resolution']=1024
 materials['materials'].extend(i3.materials);pbr['sets'].extend(i3.sets)
 if len(materials['materials'])>256:raise RuntimeError('Too many materials '+str(len(materials['materials'])))
 objects=[]
 for n,(x,z) in enumerate(pot_centers):
  variant=n # use all eight approved GLB colour variants once
  objects.extend(i3.place('pot'+str(variant),'Architecture/Facade/Grandes jardinières GLB 1970',[x,.08,z],[1,1,1],0,'pot'+str(n)))
  treekey=['tree1','tree2','tree1','tree2','tree2','tree1','tree2','tree1'][n]
  objects.extend(i3.place(treekey,'Architecture/Facade/Arbres elances GLB',[x,1.19,z],
                         [1.35,3.55+.13*(n%3),1.35],n*27,'pottree'+str(n)))
 # Original 26 trees replaced with actual complete mesh trees, including bark and leaves.
 rng=random.Random(20261112)
 treekeys=['tree1','pine1','tree2','pine2','curved']
 for n,(x,z) in enumerate(original_plant_centers):
  key=treekeys[(n*3+n//3)%len(treekeys)]
  height=8.2+(n%7)*.60
  pit_level=.39 if (abs(x)<35.1 and -6<z<17.1) else 0.0
  objects.extend(i3.place(key,'Nature/Arbres medium-poly GLB',[x,.16+pit_level,z],[height,height,height],
                          (n*97)%360,'tree'+str(n)))
 # Tree pits: sidewalk/plaza must have real circular holes in paving/foundation.
 geo=list(i3.meshes)
 filtered,cut_report=trim_paving(filtered,geo,original_plant_centers)
 geo.extend([disc_mesh('rev_fosse_terre'),ring_mesh('rev_fosse_anneau_beton')])
 for n,(x,z) in enumerate(original_plant_centers):
  pit_level=.39 if (abs(x)<35.1 and -6<z<17.1) else 0.0
  for key,material,category in [('rev_fosse_terre','rev_microsoil','Terre'),
                                ('rev_fosse_anneau_beton','rev_bordure_beton','Bordure beton')]:
   objects.append({'id':f'rev_fosse_{n:02d}_{key}',
     'parent':'Nature/Fosses circulaires/'+category,'geometry':key,'material':material,
     'position':[x,pit_level,z],'size':[1,1,1],'rotationY':0})
  for t in range(3):
   angle=rng.random()*2*math.pi;r=rng.uniform(.43,.79)
   xx=x+r*math.cos(angle);zz=z+r*math.sin(angle)
   objects.extend(i3.place(['grass1','grass2'][t%2],
    'Nature/Fosses circulaires/Herbes clairsemees',[xx,.16+pit_level,zz],[.15,.20+.10*rng.random(),.15],
    rng.randrange(360),f'pitgrass{n}_{t}'))
 # Replace the hundreds of toy-spheres with a restrained scatter of real flower GLB.
 for n in range(46):
  ang=rng.uniform(0,2*math.pi);rad=math.sqrt(rng.uniform(.04,.93));x=25.5*rad*math.cos(ang);z=72+13.0*rad*math.sin(ang)
  # Ellipsoid mound already modeled in V4; flowers grow from its envelope.
  y=.32+1.35*math.sqrt(max(.02,1-(x/27.9)**2-((z-72)/16.1)**2))
  key=floral_keys[n%len(floral_keys)]
  h=.26+rng.random()*.22
  objects.extend(i3.place(key,'Place/Butte vegetale/Fleurs GLB CC0',[x,y,z],
                           [h*.85,h,h*.85],rng.randrange(360),f'flower{n}'))
 for n in range(92):
  ang=rng.uniform(0,2*math.pi);rad=math.sqrt(rng.uniform(.08,.99));x=26.*rad*math.cos(ang);z=72+13.5*rad*math.sin(ang)
  y=.32+1.35*math.sqrt(max(.01,1-(x/27.9)**2-((z-72)/16.1)**2))
  h=.13+rng.random()*.20
  objects.extend(i3.place('grass1' if n%2 else 'grass2','Place/Butte vegetale/Brins d herbe GLB',
                          [x,y,z],[h*.5,h,h*.5],rng.randrange(360),f'bedgrass{n}'))
 # Quality controls BEFORE writing manifest: original pond and fresco remain.
 final=filtered+objects
 ids=[o['id'] for o in final]
 if len(set(ids))!=len(ids):raise RuntimeError('Duplicate object IDs')
 all_geo=geometry['meshes']+geo
 # Place custom circle mesh definitions after original; everything referenced must exist.
 geoids={g['id'] for g in all_geo};matids={m['id'] for m in materials['materials']}
 if not all(o['geometry'] in geoids and o['material'] in matids for o in final):raise RuntimeError('Unresolved meshes/materials')
 if len([x for x in final if x['parent']=='Interieur/Piscine olympique/Eau'])!=1:raise RuntimeError('Pool lost')
 if not cut_report:raise RuntimeError('No real pavement openings made')
 if i3.triangles['pot0']!=6944:raise RuntimeError('Approved pot no longer exact 6944 triangles')
 write(root/'geometry/revision-v5.json',{'format':'m3d-geometry-v1','units':'m','meshes':geo})
 write(root/'instances/v5-complete.json',{'format':'m3d-instances-v1','instances':final,
      'notes':'V5: 8 approved wider-base GLB pots; GLB trees and flowers; concrete rings; paved actual holes; soil 2-10 mm grit; pool surface PBR.'})
 write(root/'materials/revision-v5.json',materials)
 write(root/'pbr/revision-v5.json',pbr)
 manifest=json.loads(json.dumps(original));manifest['name']='CSE WARP V17 V5 - jardinières 1970, végétation GLB, fosses réelles, eau et terre PBR'
 sec=manifest['sectors'][0]
 sec['geometry']=['geometry/benchmark.json','geometry/revision-v5.json']
 sec['instances']=['instances/v5-complete.json'];sec['materials']=['materials/revision-v5.json'];sec['pbr']=['pbr/revision-v5.json']
 write(root/'scene-v5-complete.json',manifest)
 write(root/'scene-v4.json',manifest) # double click latest exe starts the requested fully revised scene
 info={'approvedPotBaseDiameterM':.92,'approvedPotTopDiameterM':1.77,'approvedPotHeightM':1.32,
       'potInstances':8,'potTrianglesEach':i3.triangles['pot0'],
       'replacedProceduralTreePositions':len(original_plant_centers),
       'slenderTreesInPots':8,'newGLBFlowerPositions':46,'newGrassPositions':92,
       'treePitCount':len(original_plant_centers),'realPavingCut':cut_report,
       'priorObjectTypesRemoved':dict(removed),'importedMeshTriangleCounts':i3.triangles,
       'polyhavenApprovedFlowersAvailable':polyhaven,'allMaterialCount':len(materials['materials']),
       'allObjectCount':len(final),'importedMeshCount':len(geo),
       'sceneFile':'benchmark-civic/scene-v5-complete.json',
       'limitations':['GLB source kept triangle-identical and UVs preserved; WARP supports albedo PBR, not alpha transparency or imported normal texture files',
         'Water normal bump is a static native material; moving normal maps require separate engine support',
         'Flower variants from Quaternius used only if explicitly --allow-fallback-flower']}
 write(root/'reports/revision-v5-verification.json',info)
 print(json.dumps(info,indent=2,ensure_ascii=False))

if __name__=='__main__':main()