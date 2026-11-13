#!/usr/bin/env python3
"""V17.439: approved warm material palette and polygonal entrance floor mosaic.
Patches ONLY portable V8 scene resources, preserving historical artwork,
all meshes/lighting/audio and the validated four pool ladders.
"""
import argparse, copy, hashlib, json, math
from pathlib import Path

F = "Architecture/Palais/"
P = "Interieur/Piscine olympique/"
G = "Interieur/Galeries/"

def load(p):
    return json.loads(Path(p).read_text(encoding="utf-8-sig"))

def save(p, data):
    p = Path(p); p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n', encoding='utf-8')

def box(name,parent,xyz,size,mat):
    return dict(id=name,parent=parent,geometry="unit_box",material=mat,
                position=[round(float(v),5) for v in xyz],
                size=[round(float(v),5) for v in size],rotationY=0)

def add_material(materials,pbr,mid,rgb,roughness,pattern='concrete',image=None):
    assert not any(m['id']==mid for m in materials['materials']),mid
    materials['materials'].append(dict(id=mid,baseColor=rgb,roughness=roughness,
                 metallic=0.,transmission=0.,ior=1.5,textureSet='tex_'+mid))
    t=dict(id='tex_'+mid,material=mid,pattern=pattern,resolution=1024 if image else 512)
    if image:t['imageFile']=image
    pbr['sets'].append(t)

def build(root):
    root=Path(root)
    scene_path=root/'scene-v7-corrections.json'
    scene=load(scene_path)
    sec=scene['sectors'][0]
    instances_path=root/sec['instances'][0]
    materials_path=root/sec['materials'][0]
    pbr_path=root/sec['pbr'][0]
    objects=load(instances_path)['instances']
    mats=load(materials_path);pbr=load(pbr_path)
    original=len(objects)
    ids={o['id'] for o in objects}
    def add(o):
        assert o['id'] not in ids,o['id']
        objects.append(o);ids.add(o['id'])
    def strip(pred):
        nonlocal objects
        before=len(objects)
        objects=[o for o in objects if not pred(o)]
        return before-len(objects)
    fresco=root/'textures/energetik_fresco_mosaic.jpg'
    # Locate the real V8 historical image even if the texture is in revision folders.
    fresco_candidates=list(root.glob('textures/**/*energetik*fresco*.jpg'))
    fresco_signatures={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in fresco_candidates if p.is_file()}
    assert any('Fresque' in o['parent'] for o in objects),"Real historical fresco missing"
    assert any('v8_' in o['id'] for o in objects),"Expected latest V8 scene, not a demo build"
    art=root/'textures/revision/v439/cse_parvis_mosaique.png'
    assert art.is_file() and art.read_bytes()[:8]==b'\x89PNG\r\n\x1a\n',"Missing generated floor mosaic PNG"

    colors=[
        ('v439_ivory_plaster',[.88,.84,.77],.91,'plaster',None),
        ('v439_waxed_concrete',[.57,.56,.53],.50,'concrete',None),
        ('v439_gallery_sandstone',[.77,.72,.65],.65,'concrete',None),
        ('v439_gallery_warm',[.68,.66,.61],.70,'concrete',None),
        ('v439_nonslip_grey',[.43,.43,.40],.90,'ceramic',None),
        ('v439_coping_beige',[.74,.70,.63],.81,'ceramic',None),
        ('v439_terra_tile',[.71,.41,.32],.83,'ceramic',None),
        ('v439_ochre_tile',[.79,.65,.39],.84,'ceramic',None),
        ('v439_olive_tile',[.42,.46,.39],.86,'ceramic',None),
        ('v439_entry_art',[1.,1.,1.],.87,'image','textures/revision/v439/cse_parvis_mosaique.png')]
    for mid,rgb,rough,pat,img in colors:add_material(mats,pbr,mid,rgb,rough,pat,img)
    report=dict(version='V17.439',baseline='V8',orig=original,materials=len(colors),
                frescoHash=fresco_signatures,corrections={})

    # Warm grey waxed concrete on dry structural floors. Blue water retains
    # the original ceramic design; surrounding vertical walls do not repeat blue.
    for o in objects:
        par=o['parent']
        if par==F+'Fondations' and o['geometry']=='unit_box':
            o['material']='v439_waxed_concrete'
        if par=='Interieur/Hall/Panneaux':
            o['material']='v439_ivory_plaster'
        if par.startswith('Interieur/Vestiaires') and '/Murs' in par:
            o['material']='v439_ivory_plaster'
        if par==P+'Parois':
            o['material']='v439_coping_beige'

    # Add one surface finish PER actual gallery deck, without filling
    # the three real V8 staircase shafts. Never create a giant floor overlay.
    decks=[o for o in objects if o['parent']=='Interieur/Galeries'
          and o['geometry']=='unit_box' and o['position'][1] in (4.5,9,13.5)]
    decks += [o for o in objects if o['parent']==G+'Escaliers lateraux/Planchers decoupes'
              and o['geometry']=='unit_box']
    for k,o in enumerate(decks):
        x,y,z=o['position'];w,h,d=o['size']
        if w<.50 or d<.5:continue
        mat='v439_gallery_sandstone' if y<7 else ('v439_gallery_warm' if y<11 else 'v439_waxed_concrete')
        add(box(f'v439_floor_deck_{k}',G+'Sols des niveaux', [x,y+h/2+.014,z],
                [max(.1,w-.08),.025,max(.1,d-.08)],mat))
    report['corrections']['finishedUpperDecks']=len(decks)

    # Ivory wall lining, positioned inside wall planes. DO NOT cover rear
    # fresco at x around zero, nor the external facade portrait artwork.
    for side in (-1,1):
        add(box(f'v439_ivory_side_{side}',P+'Murs interieur enduit ivoire',
                [side*49.72,9.5,-32],[.05,17.4,42.0],'v439_ivory_plaster'))
        add(box(f'v439_ivory_rear_{side}',P+'Murs interieur enduit ivoire',
                [side*27.5,9.5,-54.20],[43.0,17.4,.05],'v439_ivory_plaster'))
    report['corrections']['ivoryWallPanels']=4

    # Dark nonslip floor on all FOUR wet strips; separate warm beige coping.
    for side in (-1,1):
        add(box(f'v439_nonslip_side_{side}',P+'Plages antiderapantes',
                [side*27.40,.568,-32],[4.45,.043,25.6],'v439_nonslip_grey'))
        add(box(f'v439_coping_side_{side}',P+'Margelles antiderapantes',
                [side*25.42,.596,-32],[.45,.06,25.0],'v439_coping_beige'))
    for end,z in enumerate((-46.,-18.)):
        add(box(f'v439_nonslip_end_{end}',P+'Plages antiderapantes',
                [0,.568,z],[49.6,.043,2.5],'v439_nonslip_grey'))
        add(box(f'v439_coping_end_{end}',P+'Margelles antiderapantes',
                [0,.596,(-44.66 if end==0 else -19.34)],[49.6,.06,.38],'v439_coping_beige'))
    report['corrections']['nonslipStrips']=4

    # Front parvis used to project behind the glazed facade. Stop slab just
    # outside the -4.035 glass datum, insert a walkable entrance threshold.
    parvis=[o for o in objects if o['parent'].startswith('Place/Parvis d')]
    for o in parvis:
        if o['geometry']=='unit_box':
            x,y,z=o['position']; w,h,d=o['size']
            end=z+d/2
            new_start=-3.70
            if z-d/2<new_start and end>new_start:
                o['size'][2]=round(end-new_start,5)
                o['position'][2]=round((end+new_start)/2,5)
    add(box('v439_entry_threshold','Interieur/Hall/Seuil principal',
            [0,.556,-3.82],[18.4,.08,.68],'v439_waxed_concrete'))

    # TRUE trapezoid with UVs: no stretched rectangular slab and no detached
    # horizontal bars. Mosaic opens from 10.6 m at the entrance to 19.6 m
    # toward the square, flush with the existing parvis paving.
    vertices=[[-5.3,.574,-3.4],[5.3,.574,-3.4],
              [9.8,.574,16.25],[-9.8,.574,16.25]]
    mesh=dict(id='v439_entry_mosaic_trapezoid',type='triangle_mesh',
              vertices=vertices,faces=[[0,2,1],[0,3,2],[1,2,0],[2,3,0]],
              uv=[[0,0],[1,0],[1,1],[0,1]])
    # Avoid duplicates in originals; this is a new dedicated mesh file.
    geo_path='geometry/revision-v439-entry-design.json'
    save(root/geo_path,dict(format='m3d-geometry-v1',units='m',meshes=[mesh]))
    add(dict(id='v439_entry_mosaic_art',parent='Place/Parvis/Mosaique monumentale',
             geometry=mesh['id'],material='v439_entry_art',position=[0,0,0],
             size=[1,1,1],rotationY=0))
    sec['geometry']=sec['geometry']+[geo_path]
    report['corrections']['floorArt']='trapezoidal UV mesh + original tessera PNG'

    # Clear the actual 20m central entrance (existing V8 framing generated
    # steel mullions even in the geometrically open door). Restrict deletion
    # to GROUND-floor glass/frame entries in the public doorway only.
    def in_entrance(o):
        par=o.get('parent','')
        x,y,z=o.get('position',[100,100,100])
        if y>=4.65 or abs(x)>10.8 or not(-4.4<=z<=-3.6):return False
        return (par in (F+'Mur-rideau V8/Montants raccordes',
                       F+'Mur-rideau V8/Traverses raccordees',
                       F+'Chassis',F+'Verriere')
                and o.get('geometry')=='unit_box')
    report['corrections']['doorFramesRemoved']=strip(in_entrance)
    # Existing V8 upper lintel is maintained. Door opening remains structural.

    # To avoid speakers embedded in glass, split only glass panels actually
    # crossing concrete-mounted horn positions; keep remaining transparent panes.
    speaker_x=(-30.,-12.,12.,30.)
    split_count=0
    for o in list(objects):
        if o['parent']!=F+'Verriere' or o['geometry']!='unit_box':continue
        x,y,z=o['position'];w,h,d=o['size']
        if y>4.6 or z> -3.6 or z< -4.5:continue
        segs=[(x-w/2,x+w/2)]
        for sx in speaker_x:
            remaining=[]
            for lo,hi in segs:
                if lo<sx+.48 and hi>sx-.48:
                    if sx-.48-lo>.16:remaining.append((lo,sx-.48))
                    if hi-(sx+.48)>.16:remaining.append((sx+.48,hi))
                else:remaining.append((lo,hi))
            segs=remaining
        if len(segs)==1 and abs(segs[0][1]-segs[0][0]-w)<.01:continue
        objects.remove(o)
        for index,(lo,hi) in enumerate(segs):
            add(box(f'v439_glazing_segment_{split_count}_{index}',
                    o['parent'],[(lo+hi)/2,y,z],[hi-lo,h,d],o['material']))
        split_count+=1
    report['corrections']['glassPanelsAdjustedForSpeakers']=split_count

    # Remove posts intersecting tree pits (never delete the trees themselves).
    trees=[o['position'] for o in objects if o['parent']=='Nature/Arbres']
    def conflicts_tree(o):
        if o['parent']!='Place/Garde corps' or o['geometry']!='unit_box':return False
        x,y,z=o['position']
        return any((x-t[0])**2+(z-t[2])**2<2.0**2 for t in trees)
    report['corrections']['postsRemovedFromTreePits']=strip(conflicts_tree)

    # Save new resources without replacing any original V8 JSON files.
    ips='instances/v439-design.json';mps='materials/v439-design.json';pps='pbr/v439-design.json'
    save(root/ips,dict(format='m3d-instances-v1',instances=objects))
    save(root/mps,mats);save(root/pps,pbr)
    sec['instances']=[ips];sec['materials']=[mps];sec['pbr']=[pps]
    scene['name']='CSE WARP V17.439 — parvis mosaïque, couleurs minérales et accès'
    save(root/'scene-v439-design.json',scene)
    save(root/'scene-v7-corrections.json',scene) # portable launcher points here
    assert len({o['id'] for o in objects})==len(objects)
    assert any('Fresque' in o['parent'] for o in objects)
    assert all(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==v
               for rel,v in fresco_signatures.items() for p in [root/rel])
    report['total']=len(objects)
    save(root/'reports/v439-design.json',report)
    print(json.dumps(report,indent=2,ensure_ascii=False))
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--root',required=True,type=Path)
    build(parser.parse_args().root)
