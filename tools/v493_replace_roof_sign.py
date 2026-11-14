#!/usr/bin/env python3
"""Replace only the existing V17.493 rooftop sign, using approved GLB geometry.
Never rebuild the scene from older revisions; preserves 186 materials and every
non-sign instance. Adds real triangle meshes for CSE WARP V16/V17 loader.
"""
import argparse, hashlib, json, shutil
from pathlib import Path
import numpy as np
import trimesh

PREFIX='Architecture/Palais/Enseigne lumineuse toit/'
NEW_PREFIX='Architecture/Complexe sportif/Enseigne toiture БАССЕЙН/'
ID='v493_bassein_'
# Old roof sign spans roughly x=-15..15, y=20.9..25.3, z=-3.72..-2.82.
ORIGIN=(0.0,21.06,-3.23)
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def save(p,obj):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(obj,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--package',required=True,type=Path)
    parser.add_argument('--glb',required=True,type=Path)
    parser.add_argument('--allow-unversioned',action='store_true')
    a=parser.parse_args()
    root=a.package.resolve(); folder=root/'benchmark-civic'
    if not (folder/'scene.json').exists():raise SystemExit('V493 scene.json absent')
    version=root/'BUILD_NUMBER.txt'
    if not a.allow_unversioned:
        manifest=root/'RELEASE-MANIFEST.json'
        ver=('493' in version.read_text(errors='replace') if version.is_file() else False)
        if manifest.is_file():ver=ver or '17.493' in manifest.read_text(errors='replace')
        if not ver:raise SystemExit('Package not verified V17.493; refusing replacement')
    sc=read(folder/'scene.json')
    if sc.get('format')!='m3d-scene-v16' or len(sc.get('sectors',[]))!=1:raise SystemExit('Unsupported scene')
    sec=sc['sectors'][0]
    for key in ('materials','pbr','instances'):
        if len(sec[key])!=1:raise SystemExit('Unexpected '+key+' references')
    ip=folder/sec['instances'][0];mp=folder/sec['materials'][0];pp=folder/sec['pbr'][0]
    instances=read(ip);materials=read(mp);pbr=read(pp)
    old=instances['instances']
    if any(i.get('id','').startswith(ID) for i in old):raise SystemExit('Already installed')
    remove=[i for i in old if i.get('parent','').startswith(PREFIX)]
    if len(remove)<50 or len(remove)>500:raise SystemExit(f'Unexpected old sign count: {len(remove)}')
    if not any('Lettres' in i['parent'] for i in remove) or not any('Panneau' in i['parent'] for i in remove):
        raise SystemExit('Roof sign members not found')
    for item in remove:
        x,y,z=item['position']
        if not (-20<x<20 and 19<y<27 and -5<z<-1):
            raise SystemExit('Old sign member outside expected roof bounds: '+item['id'])
    source=trimesh.load(a.glb,force='scene')
    if len(source.geometry)!=112 or not 14000<=sum(len(m.faces) for m in source.geometry.values())<=17000:
        raise SystemExit('This is not the approved 112-object GLB')
    bounds=source.bounds
    if not(15.8<=bounds[1,0]-bounds[0,0]<=16.5 and 2.5<=bounds[1,2]-bounds[0,2]<=2.8):
        raise SystemExit('GLB physical scale incorrect')
    newgeos=[];newinst=[];newmats=[];newsets=[];matmap={}
    # The existing V493 renderer recognizes these precise historic material
    # IDs as self-lit, with emissive GPU parameters. Reusing them avoids
    # rebuilding the renderer or silently losing the GLB's neon glow.
    emissive={
        'Néon rouge étoile':'cse_neon_red',
        'Lettrage néon blanc chaud':'cse_neon_gold',
        'Tube nageur blanc':'cse_pool_strip',
        'Néon eau cyan':'cse_neon_cyan',
        'Néon eau bleu':'cse_neon_cyan',
        'Néon eau turquoise':'cse_neon_cyan'
    }
    available={m['id'] for m in materials['materials']}
    required=set(emissive.values())
    if not required.issubset(available):
        raise SystemExit('Real scene lacks expected emissive materials: '+str(required-available))
    for idx,node in enumerate(source.graph.nodes_geometry):
        transform,gid=source.graph[node]
        mesh=source.geometry[gid].copy()
        mesh.apply_transform(transform)
        material=mesh.visual.material
        key=getattr(material,'name','unspecified')
        if key not in matmap:
            if key in emissive:
                matmap[key]=emissive[key]
            else:
                mi=len(newmats);mid=f'{ID}mat_{mi:02d}';tid=f'{ID}tex_{mi:02d}'
                c=np.asarray(getattr(material,'baseColorFactor',[180,180,180,255]),dtype=float)[:3]/255.
                matmap[key]=mid
                newmats.append({'id':mid,'baseColor':np.round(c,4).tolist(),
                    'roughness':round(float(getattr(material,'roughnessFactor',.55) or .55),4),
                    'metallic':round(float(getattr(material,'metallicFactor',0) or 0),4),
                    'transmission':0,'ior':1.5,'textureSet':tid,'sourceGLBMaterial':key})
                newsets.append({'id':tid,'material':mid,'pattern':'solid','resolution':64})
        verts=np.asarray(mesh.vertices,dtype=float)
        if len(verts)>200000 or len(mesh.faces)>80000:raise SystemExit('Mesh exceeds engine limits')
        # glTF x,y,z -> engine x,z,y. Axis swap inverts winding.
        v=np.column_stack([verts[:,0],verts[:,2],verts[:,1]])
        faces=np.asarray(mesh.faces,dtype=int)[:,[0,2,1]]
        g=f'{ID}mesh_{idx:03d}'
        newgeos.append({'id':g,'type':'triangle_mesh',
                        'vertices':np.round(v,5).tolist(),'faces':faces.tolist()})
        newinst.append({'id':f'{ID}obj_{idx:03d}',
                        'parent':NEW_PREFIX+str(node),'geometry':g,'material':matmap[key],
                        'position':list(ORIGIN),'size':[1,1,1],'rotationY':0})
    if len(materials['materials'])+len(newmats)>255:raise SystemExit('Scene material index overflow')
    newgeometry='geometry/v493_bassein_roof_sign.json'
    if newgeometry in sec['geometry']:raise SystemExit('Already patched')
    # Backup the exact V493 sources before writing, for faithful rollback.
    for p in (folder/'scene.json',ip,mp,pp):
        backup=p.with_suffix(p.suffix+'.pre-bassein-v493.bak')
        if backup.exists():raise SystemExit('Backup already exists: '+str(backup))
    for p in (folder/'scene.json',ip,mp,pp):
        shutil.copy2(p,p.with_suffix(p.suffix+'.pre-bassein-v493.bak'))
    sec['geometry'].append(newgeometry)
    instances['instances']=[i for i in old if not i.get('parent','').startswith(PREFIX)]+newinst
    materials['materials'].extend(newmats)
    pbr['sets'].extend(newsets)
    sc['name']=sc.get('name','')+' — enseigne toiture БАССЕЙН'
    save(folder/newgeometry,{'format':'m3d-geometry-v16','units':'m','meshes':newgeos})
    save(folder/'scene.json',sc);save(ip,instances);save(mp,materials);save(pp,pbr)
    shutil.copy2(a.glb,folder/'CSE_WARP_BASSEIN_toiture_v493.glb')
    info={'base':'CSE WARP V17.493','removedSignObjects':len(remove),
          'addedSignObjects':len(newinst),'addedMaterials':len(newmats),
          'addedTriangles':sum(len(m['faces']) for m in newgeos),
          'originalUnchangedObjects':len(old)-len(remove),
          'glbSHA256':hashlib.sha256(a.glb.read_bytes()).hexdigest(),
          'mountWorldOriginXYZ':ORIGIN,'note':'No facade mosaic, pool, plants or other objects touched'}
    save(folder/'V493_BASSEIN_SIGN_INSTALL.json',info)
    print(json.dumps(info,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
