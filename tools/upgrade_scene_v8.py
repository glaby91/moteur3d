#!/usr/bin/env python3
"""CSE WARP V17.430 -- nine scoped scene repairs on untouched V17.429 data.

Read V17.428 scene as distributed inside the tested V17.429 portable package.
Write independent V8 scene resources. Original V7 files / fresco pixels stay intact.
Never modify the user-approved large pool mosaic.
"""
import argparse, collections, copy, hashlib, json, math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

P='Interieur/Piscine olympique/'
G='Interieur/Galeries/'
F='Architecture/Palais/'

def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p,data):
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
def box(name,parent,xyz,size,mat):
    return dict(id=name,parent=parent,geometry='unit_box',material=mat,
      position=[round(float(v),5) for v in xyz],
      size=[round(float(v),5) for v in size],rotationY=0)
def mesh(name,vs,faces,uv):
    return dict(id=name,type='triangle_mesh',vertices=[[round(float(v),5) for v in p] for p in vs],
      faces=faces,uv=uv)
def instance(name,parent,geometry,material):
    return dict(id='inst_'+name,parent=parent,geometry=geometry,material=material,
        position=[0,0,0],size=[1,1,1],rotationY=0)
def quad(name,vs,uv):
    return mesh(name,vs,[[0,1,2],[0,2,3],[2,1,0],[3,2,0]],uv)
def tube(name,path,radius=.0675,sides=20):
    """Continuous round stainless pipe, not hundreds of overlapping spheres."""
    vertices=[];uv=[];faces=[]
    for i,p in enumerate(path):
        a=path[max(0,i-1)];b=path[min(len(path)-1,i+1)]
        tx,ty,tz=[b[k]-a[k] for k in range(3)]
        t=math.sqrt(tx*tx+ty*ty+tz*tz) or 1
        tx/=t;ty/=t;tz/=t
        ax,ay,az=(0,0,1) if abs(tz)<.88 else (0,1,0)
        ux,uy,uz=ty*az-tz*ay,tz*ax-tx*az,tx*ay-ty*ax
        d=math.sqrt(ux*ux+uy*uy+uz*uz) or 1
        ux/=d;uy/=d;uz/=d
        vx,vy,vz=ty*uz-tz*uy,tz*ux-tx*uz,tx*uy-ty*ux
        for j in range(sides):
            angle=2*math.pi*j/sides
            ca=math.cos(angle);sa=math.sin(angle)
            vertices.append([p[0]+radius*(ca*ux+sa*vx),p[1]+radius*(ca*uy+sa*vy),p[2]+radius*(ca*uz+sa*vz)])
            uv.append([i/max(1,len(path)-1),j/sides])
            if i:
                prev=(i-1)*sides+j;other=(i-1)*sides+(j+1)%sides
                cur=i*sides+j;nxt=i*sides+(j+1)%sides
                faces.extend([[prev,other,cur],[other,nxt,cur]])
    return mesh(name,vertices,faces,uv)

def add_material(mats,pbr,id,colour,rough,metal=0,trans=0,pattern='metal',image=None,res=512):
    assert not any(x['id']==id for x in mats['materials']),id
    mats['materials'].append(dict(id=id,baseColor=colour,roughness=rough,
       metallic=metal,transmission=trans,ior=1.333 if trans else 1.5,
       textureSet='tex_'+id))
    t=dict(id='tex_'+id,material=id,pattern=pattern,resolution=res)
    if image:t['imageFile']=image
    pbr['sets'].append(t)

def make_mosaic(root):
    """Real ceramic tesserae in image file, without recompressing the mural."""
    image=Image.new('RGB',(1024,1024),(180,203,211));d=ImageDraw.Draw(image)
    swatches=[(180,212,219),(188,217,224),(171,206,217),(193,221,228),
             (176,206,217),(206,230,234),(164,198,210)]
    for y in range(0,1024,24):
        for x in range(0,1024,24):
            col=swatches[((x//24)*31+(y//24)*17+(x//24)*(y//24))%len(swatches)]
            d.rectangle([x+2,y+2,min(1023,x+22),min(1023,y+22)],fill=col)
            d.line([x+3,y+3,min(1023,x+21),y+3],fill=(215,235,237))
    path='textures/revision/v8/pool_mosaic_ceramic_1024.png'
    (root/path).parent.mkdir(parents=True,exist_ok=True)
    image.save(root/path)
    return path

def make_digits(root):
    paths=[]
    for n in range(1,11):
        im=Image.new('RGB',(256,256),(17,49,79));d=ImageDraw.Draw(im)
        d.rectangle([9,9,246,246],outline=(200,223,223),width=9)
        txt=str(n)
        font=None
        for face in ('C:/Windows/Fonts/arialbd.ttf','DejaVuSans-Bold.ttf'):
            try:font=ImageFont.truetype(face,162);break
            except OSError:pass
        if font is None:raise RuntimeError('No font for lane numbers')
        bb=d.textbbox((0,0),txt,font=font)
        d.text(((256-(bb[2]-bb[0]))/2-bb[0],(256-(bb[3]-bb[1]))/2-bb[1]),
               txt,font=font,fill=(240,242,234))
        path=f'textures/revision/v8/lane_{n:02d}_256.png'
        (root/path).parent.mkdir(parents=True,exist_ok=True)
        im.save(root/path);paths.append(path)
    return paths

def build(root):
    root=Path(root);base=root/'scene-v7-corrections.json'
    scene=load(base);objects=load(root/'instances/v7-corrections.json')['instances']
    mats=load(root/'materials/v7.json');pbr=load(root/'pbr/v7.json')
    lights=load(root/'lighting/v7-evening.json');cameras=load(root/'cameras/v7.json')
    geometry=[];report={'source':'V17.429 / scene V17.428','fixes':{}}
    before_fresco=[copy.deepcopy(o) for o in objects if 'Fresque murale 2048' in o['parent']]
    original_art=(root/'textures/energetik_fresco_mosaic.jpg').read_bytes()
    source_scene_bytes=base.read_bytes()
    assert len(before_fresco)==8
    ids=set()
    for o in objects:
        assert o['id'] not in ids,o['id'];ids.add(o['id'])
    def remove(pred,label,expected=None):
        nonlocal objects
        removed=[o for o in objects if pred(o)]
        if expected is not None:assert len(removed)==expected,(label,len(removed),expected)
        objects=[o for o in objects if not pred(o)]
        report['fixes'][label]=len(removed)
        return removed
    def add(o):
        assert o['id'] not in ids,o['id']
        ids.add(o['id']);objects.append(o)

    # 1 — span that was still left open across the former 6.3 m hall stair.
    for label,h,thick in [('Main courante',5.75,.075),('Lisse',5.18,.055)]:
        add(box('v8_former_hall_stair_closure_'+label.replace(' ',''),
            G+'Garde-corps continus apres trémie/'+label,
            [-15,h,-13.7],[6.35,thick,.085],'prp_steel_painted'))
    for i,x in enumerate((-18.15,-16.1,-14.1,-12.0,-11.85)):
        add(box(f'v8_hall_old_stair_post_{i}',G+'Garde-corps continus apres trémie/Montants',
            [x,5.18,-13.7],[.09,1.14,.09],'prp_steel_painted'))
    report['fixes']['1_guardrail_gap_closed_m']=6.35

    # 2 — three unpaired suspended concrete joists under skylight.
    remove(lambda o:o['parent']==P+'Poutres plafond' and o['position'][0] in (-15,-5,15),
           '2_unsupported_concrete_roof_joists_removed',3)
    assert sum(o['parent']==P+'Poutres plafond' for o in objects)==3

    # 3 — actual three-floor stair shaft on FAR LEFT wing, away from pool and
    # mural; each new upper-floor cut is positioned over its own flight.
    slabs=[o for o in objects if o['parent']=='Interieur/Galeries' and o['geometry']=='unit_box'
           and abs(o['position'][0]+39.5)<.01 and o['position'][1] in (4.5,9.0,13.5)
           and o['size']==[22,.3,49]]
    assert len(slabs)==3,[(x['id'],x['position']) for x in slabs]
    remove(lambda o:o['id'] in {s['id'] for s in slabs},'3_left_gallery_slabs_for_structural_cutouts',3)
    floor_specs=[(4.5,-45.,-43.9,-24.1), (9.,-40.,-43.9,-24.1),
                 (13.5,-35.,-43.9,-24.1)]
    for floor_y,cx,z0,z1 in floor_specs:
        left,right=cx-1.55,cx+1.55
        parts=[(-50.5,left,-54.5,-5.5),
               (right,-28.5,-54.5,-5.5),
               (left,right,-54.5,z0),
               (left,right,z1,-5.5)]
        for j,(x0,x1,a,b) in enumerate(parts):
            assert x1>x0 and b>a
            add(box(f'v8_side_gallery_floor_{int(floor_y*10)}_{j}',
                G+'Escaliers lateraux/Planchers decoupes',
                [(x0+x1)/2,floor_y,(a+b)/2],[x1-x0,.30,b-a],'prp_concrete_raw'))
        # rail along BOTH actual shaft edges, with 2.5m landing clearance.
        rail_z0,rail_z1=(z0+2.3,z1) if floor_y in (4.5,13.5) else (z0,z1-2.3)
        for side in (-1,1):
            xx=cx+side*1.55
            for label,dy,hh in [('Main courante',1.25,.065),('Lisse',.70,.055)]:
                add(box(f'v8_floor_void_rail_{int(floor_y*10)}_{side}_{label.replace(" ","")}',
                  G+'Escaliers lateraux/Protection trémies/'+label,
                  [xx,floor_y+dy,(rail_z0+rail_z1)/2],[.075,hh,rail_z1-rail_z0],
                  'prp_steel_painted'))
            for j in range(8):
                zz=rail_z0+(rail_z1-rail_z0)*j/7
                add(box(f'v8_void_post_{int(floor_y*10)}_{side}_{j}',
                  G+'Escaliers lateraux/Protection trémies/Montants',
                  [xx,floor_y+.7,zz],[.08,1.10,.08],'prp_steel_painted'))
    flights=[(-45,0.55,4.5,-24.2,-43.8,20),
             (-40,4.5,9.,-43.8,-24.2,22),
             (-35,9.,13.5,-24.2,-43.8,22)]
    for fi,(x,start,end,z_start,z_end,n) in enumerate(flights):
        climb=(end-start)/n;run=(z_end-z_start)/n
        assert climb<=.34 and abs(run)>.5
        for k in range(n):
            # Connected flight of genuine walkable surfaces (not a visual ramp).
            z=z_start+(k+.5)*run
            y=start+(k+1)*climb-.07
            add(box(f'v8_stair_{fi}_step_{k:02d}',
                G+'Escaliers lateraux/Marches',
                [x,y,z],[2.85,.14,abs(run)+.025],'prp_concrete_raw'))
            if k%3==0 or k==n-1:
                for side in (-1,1):
                    add(box(f'v8_stair_{fi}_upright_{k:02d}_{side}',
                      G+'Escaliers lateraux/Garde-corps/Montants',
                      [x+side*1.48,start+(k+1)*climb+.61,z],
                      [.075,1.12,.075],'prp_steel_painted'))
        # continuous sloping two-rail protection, implemented with genuine
        # triangular prism mesh; still fully owned by this one flight.
        for side in (-1,1):
            X=x+side*1.48
            for j,offset in enumerate((.70,1.20)):
                p0=[X,start+offset,z_start];p1=[X,end+offset,z_end]
                # two-sided fence ribbon, 9cm wide and sloping.
                vs=[[X-.05,p0[1]-.05,p0[2]], [X+.05,p0[1]-.05,p0[2]],
                    [X+.05,p1[1]+.05,p1[2]], [X-.05,p1[1]+.05,p1[2]]]
                nm=f'v8_stair_{fi}_handrail_{side}_{j}'
                geometry.append(quad(nm,vs,[[0,0],[1,0],[1,1],[0,1]]))
                add(instance(nm,G+'Escaliers lateraux/Garde-corps/Barres continues',nm,'prp_steel_painted'))
        # Each top landing is a concrete bridge towards the NEXT flight.
        landingz=z_end
        x2=x+5 if fi<2 else x
        add(box(f'v8_stair_{fi}_landing',G+'Escaliers lateraux/Paliers raccordes',
          [(x+x2)/2,end,landingz],[abs(x2-x)+3.4,.30,2.4],'prp_concrete_raw'))
    report['fixes']['3_floor_levels_reached']=[4.5,9.0,13.5]
    report['fixes']['3_stair_treads']=sum(f[-1] for f in flights)
    report['fixes']['3_shaft_openings']=3

    # 4 — remove duplicated offset frame pairs and replace with true common
    # mullions at glass-joint midpoints; 1970s dark metal distinct from concrete.
    add_material(mats,pbr,'v8_mullion_dark_steel',[.18,.22,.255],.28,.82,
                 pattern='metal')
    old_framing_prefix=F+'Mur-rideau V7/'
    remove(lambda o:o['parent'] in (
       old_framing_prefix+'Montants',old_framing_prefix+'Traverses',
       old_framing_prefix+'Montants hauts',old_framing_prefix+'Traverses hautes'),
       '4_misaligned_double_glazing_frames_removed',466)
    # Existing centre-decorative members are now visually real dark steel.
    for o in objects:
        if o['parent']==F+'Chassis':o['material']='v8_mullion_dark_steel'
    x0=-44.;pitch=3.35
    for lv in range(4):
        y=2.6+lv*4.15
        for seam in range(-1,27):
            x=x0+(seam+.5)*pitch
            add(box(f'v8_front_mullion_{lv}_{seam}',
                F+'Mur-rideau V8/Montants raccordes',
                [x,y,-4.035],[.32,3.93,.28],'v8_mullion_dark_steel'))
        for k in range(27):
            x=x0+k*pitch
            # glass edges and metal bottom sills are continuous except the
            # three real ground-level door apertures.
            doorway=lv==0 and (abs(x)<7 or abs(x+27.25)<.01 or abs(x-26.35)<.01)
            add(box(f'v8_front_transom_upper_{lv}_{k}',
                F+'Mur-rideau V8/Traverses raccordees',
                [x,y+1.90,-4.035],[3.35,.24,.28],'v8_mullion_dark_steel'))
            if not doorway:
                add(box(f'v8_front_transom_lower_{lv}_{k}',
                    F+'Mur-rideau V8/Traverses raccordees',
                    [x,y-1.90,-4.035],[3.35,.24,.28],'v8_mullion_dark_steel'))
    for o in objects:
        if o['parent']==F+'Mur-rideau V7/Portes latérales ouvertes':
            o['material']='v8_mullion_dark_steel'
        if o['parent']==F+'Verriere haute/Chassis':
            o['material']='v8_mullion_dark_steel'
    # Upper clerestory glazing is retained; all 22 are enclosed on the same
    # axial steel grid and capped under the existing roof edge.
    for k in range(27):
        x=x0+k*pitch
        if abs(x)<9:continue
        for side in (-1,1):
            add(box(f'v8_clerestory_mullion_{k}_{side}',
                 F+'Mur-rideau V8/Montants bandeau haut',
                 [x+side*1.675,18.02,-4.035],[.30,2.1,.28],
                 'v8_mullion_dark_steel'))
        add(box(f'v8_clerestory_transom_{k}',F+'Mur-rideau V8/Traverse bandeau haut',
                [x,19.13,-4.035],[3.35,.22,.28],'v8_mullion_dark_steel'))
    report['fixes']['4_new_frame_material']='v8_mullion_dark_steel'

    # 5 — move actual bottom surface from 1.66m to 2.10m depth, extend
    # ceramic retaining walls, and retain 10 submerged longitudinal lane marks.
    mosaic_file=make_mosaic(root)
    add_material(mats,pbr,'v8_pool_mosaic_pale',[.84,.93,.95],.30,0,0,
                 pattern='porcelain',image=mosaic_file,res=1024)
    add_material(mats,pbr,'v8_pool_mosaic_ink',[.07,.28,.42],.29,0,0,
                 pattern='porcelain')
    water=next(o for o in objects if o['parent']==P+'Eau')
    floor=next(o for o in objects if o['parent']==P+'Fond')
    floor['position'][1]=-1.92
    floor['material']='v8_pool_mosaic_pale'
    for o in objects:
        if o['parent']==P+'Parois':
            o['position'][1]=-.67;o['size'][1]=2.54
            o['material']='v8_pool_mosaic_pale'
        if o['parent']==P+'Marquage fond':
            o['position'][1]=-1.776;o['material']='v8_pool_mosaic_ink'
        if o['parent']==P+'Bandes carrelage fond':
            o['position'][1]=-1.77;o['material']='v8_pool_mosaic_pale'
        if o['parent']==P+'Frise ligne eau':
            o['material']='v8_pool_mosaic_pale'
    # UV-scaled ceramic in real units, rather than one stretched 1024px tile.
    mosaicgeo='v8_pool_floor_tesserae_uv'
    vertices=[[-24.98,-1.803,-44.48],[24.98,-1.803,-44.48],
              [24.98,-1.803,-19.52],[-24.98,-1.803,-19.52]]
    geometry.append(quad(mosaicgeo,vertices,[[0,0],[49.96,0],[49.96,24.96],[0,24.96]]))
    add(instance(mosaicgeo,P+'Mosaïque céramique/fond bassin',mosaicgeo,'v8_pool_mosaic_pale'))
    # Transparent blue water remains, but make it less milky than V17.428.
    wm=next(m for m in mats['materials'] if m['id']==water['material'])
    wm['baseColor']=[.20,.61,.69];wm['roughness']=.055;wm['transmission']=.96
    report['fixes']['5_pool_depth_metres']=2.10

    # The previously repeated coloured spheres looked like random objects in
    # the pool. Replace the 171 literal ball primitives with nine continuous
    # 50m lane ropes; native source fresco not modified.
    remove(lambda o:o['parent']==P+'Lignes d’eau','5_stray_pool_buoy_spheres_removed',171)
    add_material(mats,pbr,'v8_lane_rope_navy',[.085,.20,.36],.32,.0,0,'metal')
    add_material(mats,pbr,'v8_lane_rope_white',[.84,.89,.90],.31,.0,0,'metal')
    for k,z in enumerate(-42.+2.5*i for i in range(9)):
        nm=f'v8_lane_rope_{k}'
        geometry.append(tube(nm,[[-24.2+.70*j,.225,z] for j in range(70)],.034,12))
        add(instance(nm,P+'Lignes d’eau/cordes continues',nm,
              'v8_lane_rope_navy' if k%2 else 'v8_lane_rope_white'))
    report['fixes']['5_real_lane_ropes']=9

    # 5 + 7 — the 10 starting blocks belong on ONE end of the 50m pool.
    # Remove the ten competing blocks at the opposite end; two access ladders
    # remain there, moved out of the middle of the swimming lane endpoints.
    blocks=remove(lambda o:o['parent']==P+'Plots' and o['position'][0]>0,
                  '7_starting_blocks_at_ladder_end_removed',10)
    remaining_blocks=[o for o in objects if o['parent']==P+'Plots']
    assert len(remaining_blocks)==10
    add_material(mats,pbr,'v8_plot_mosaic',[.72,.79,.79],.55,0,0,
                 pattern='porcelain',image=mosaic_file,res=1024)
    add_material(mats,pbr,'v8_plot_antislip',[.16,.22,.23],.76,.13,0,'metal')
    numbers=make_digits(root)
    for i,path in enumerate(numbers,1):
        add_material(mats,pbr,f'v8_swim_number_{i:02d}',[1,1,1],.65,0,0,
                     pattern='porcelain',image=path,res=256)
    for n,o in enumerate(sorted(remaining_blocks,key=lambda o:o['position'][2]),1):
        x,y,z=o['position'];o['position']=[-26.05,.86,z]
        o['size']=[.90,.47,.82];o['material']='v8_plot_mosaic'
        # Anti-slip platform is a separate top plate, not slick glazed tiles.
        add(box(f'v8_start_platform_{n:02d}',P+'Plots/Plateaux antidérapants',
            [-26.05,1.12,z],[.92,.045,.84],'v8_plot_antislip'))
        # Pool-facing numbered plaque with imageFile native 256px.
        xface=-25.578
        vs=[[xface,.78,z-.34],[xface,.78,z+.34],
            [xface,1.04,z+.34],[xface,1.04,z-.34]]
        nm=f'v8_start_plot_number_{n:02d}'
        geometry.append(quad(nm,vs,[[0,0],[1,0],[1,1],[0,1]]))
        add(instance(nm,P+'Plots/Numéros des 10 couloirs',nm,f'v8_swim_number_{n:02d}'))
    report['fixes']['6_numbered_starting_blocks']=10

    # 5 + 7 — two authentic ladders at OPPOSITE END, clear of starting plots.
    # Delete the two left/end ladders and their segmented handrails; replace
    # retained 268 sphere samples with four TRUE smooth pipe meshes.
    ladders_parent=P+'Echelles/'
    remove(lambda o:o['parent'].startswith(ladders_parent)
           and o['position'][0]<0,
           '7_ladders_removed_from_starting_end',286)
    old_pipe_samples=remove(lambda o:o['parent']==ladders_parent+'Mains courantes continues',
           '5_ribbed_stainless_ball_segments_removed',268)
    add_material(mats,pbr,'v8_inox_brushed_316L',[.80,.84,.87],.22,.94,0,'metal')
    new_ladder_rails=0
    for base_z in (-41.5,-22.5):
        nz=-44.05 if base_z==-41.5 else -19.95
        dz=nz-base_z
        for orig_z in (base_z-.36,base_z+.36):
            seq=sorted((o for o in old_pipe_samples if
                abs(o['position'][2]-orig_z)<.01),key=lambda o:o['id'])
            assert len(seq)==67,(orig_z,len(seq))
            pts=[[o['position'][0],o['position'][1],o['position'][2]+dz] for o in seq]
            name=f'v8_smooth_inox_ladder_rail_{int(abs(base_z)*10)}_{int((orig_z-base_z)*100)}'
            geometry.append(tube(name,pts,.068,24))
            add(instance(name,P+'Echelles/Inox 316L tubulaire continu',name,
                         'v8_inox_brushed_316L'))
            new_ladder_rails+=1
        for o in objects:
            if (o['parent'].startswith(ladders_parent) and o['position'][0]>0 and
               abs(o['position'][2]-base_z)<.9):
                o['position'][2]=round(o['position'][2]+dz,5)
                o['material']='v8_inox_brushed_316L'
                if o['parent']==ladders_parent+'Montants immerges':
                    o['position'][1]=-.60;o['size'][1]=2.42
        add(box(f'v8_ladder_additional_step_{int(abs(base_z)*10)}',
            P+'Echelles/Marches immergees',
            [24.6,-1.44,nz],[.17,.075,.72],'v8_inox_brushed_316L'))
    assert new_ladder_rails==4
    report['fixes']['5_smooth_brushed_inox_rails']=4
    report['fixes']['5_ladders_remaining']=2

    # 8 — strictly wooden poolside benches and their feet, not swim blocks.
    remove(lambda o:o['parent'] in (P+'Bancs latéraux',P+'Bancs latéraux/Pieds'),
           '8_wooden_pool_bench_parts_removed',18)

    # V17.430 correction validated against actual 640x360 DX11 images:
    # These 203 structural-looking boxes were all on TOP of the glazed roof,
    # seen through the transparent canopy as hundreds of dark dummy lights.
    # They are NOT the 18 true structural glazing members or the three
    # thin antenna/roof supports. Remove only the repeated 1x.6x2.4 boxes.
    remove(lambda o:o['parent']==F+'Toiture techniques'
           and o['geometry']=='unit_box'
           and o['material']=='prp_roof_metal'
           and o['size']==[1,.6,2.4],
           '9_fake_overhead_roof_blocks_removed',203)

    # 9 — remove ghostly repeated roof blobs by using clear skylight material.
    # Keep the 18 true metal glass supports. Keep real evening lighting, fewer
    # physically attached diffusers (six not dozens of floating fake slabs).
    add_material(mats,pbr,'v8_clear_roof_glass',[.89,.93,.96],.075,.0,.92,
                 pattern='glass')
    glass=next(o for o in objects if o['parent']==F+'Verriere haute')
    glass['material']='v8_clear_roof_glass'
    add_material(mats,pbr,'v8_neutral_swim_light',[.83,.89,.94],.3,.05,0,'metal')
    # Existing pool lights are positional sources without visible fixtures.
    pool_lights=[o for o in lights['lights'] if o['position'][1]==16.0]
    assert len(pool_lights)==12
    lights['lights']=[o for o in lights['lights'] if o not in pool_lights]
    for x in (-25,5,25):
        for z in (-38,-26):
            # The retained structural joists are at these exact x locations.
            assert any(o['parent']==P+'Poutres plafond'
                and abs(o['position'][0]-x)<.01 for o in objects)
            add(box(f'v8_pool_real_ceiling_luminaire_{x}_{z}',
                P+'Plafond/Luminaires fonctionnels fixes aux poutres',
                [x,17.32,z],[.31,.14,3.1],'v8_neutral_swim_light'))
            lights['lights'].append(dict(position=[x,16.75,z],radius=21,
                 intensity=2.9,color=[248,250,255]))
    report['fixes']['9_actual_ceiling_luminaires']=6
    report['fixes']['9_pool_light_sources']=6

    # Cameras for repeatable screenshots independent of user view selection.
    cameras['cameras'] += [
        dict(id='v8_three_level_stairs',name='V8 · Trois niveaux accessibles',
            position=[-45,2.3,-28],target=[-45,4.15,-38],fovDegrees=70),
        dict(id='v8_pool_blocks',name='V8 · Plots numérotés sans échelles',
            position=[-22,2.5,-14.5],target=[-25.5,1,-32],fovDegrees=67),
        dict(id='v8_ladder_stainless',name='V8 · Echelle inox et eau',
            position=[29,1.9,-42.0],target=[24.45,-.25,-44.05],fovDegrees=66),
        dict(id='v8_skylight_lighting',name='V8 · Verrière et luminaires',
            position=[0,3,-35],target=[5,18.5,-32],fovDegrees=71),
        dict(id='v8_glazing_raccords',name='V8 · Menuiseries métalliques',
            position=[-28,4.5,16],target=[-27,6,-4.03],fovDegrees=68)]

    cameras['cameras'] += [
        dict(id='v8_stair_first_landing',name='V8 · Palier du premier étage',
             position=[-44,4.85,-43.5],target=[-40,7,-31],fovDegrees=69),
        dict(id='v8_stair_second_landing',name='V8 · Palier du deuxième étage',
             position=[-40,9.35,-24.2],target=[-35,12,-36],fovDegrees=69)]

    # Preservation assertions. Never retessellate or resample the artwork.
    assert before_fresco==[o for o in objects if 'Fresque murale 2048' in o['parent']]
    assert original_art==(root/'textures/energetik_fresco_mosaic.jpg').read_bytes()
    assert len({o['id'] for o in objects})==len(objects)
    assert len(mats['materials'])<256
    assert len([o for o in objects if o['parent']==P+'Plots'])==10
    assert len([o for o in objects if o['parent']==P+'Echelles/Marches immergees'])==12
    assert not any(o['parent']==P+'Bancs latéraux' or o['parent']==P+'Bancs latéraux/Pieds' for o in objects)
    assert len([o for o in objects if o['parent']==G+'Escaliers lateraux/Marches'])==64
    assert len([o for o in objects if o['parent']==P+'Plafond/Luminaires fonctionnels fixes aux poutres'])==6
    assert report['fixes']['9_actual_ceiling_luminaires']==report['fixes']['9_pool_light_sources']
    # Save V17.428 baseline scenes under another filename, intact by byte.
    (root/'scene-v7-baseline-untouched.json').write_bytes(source_scene_bytes)
    write(root/'geometry/v8-nine-anomalies.json',dict(format='m3d-geometry-v1',units='m',meshes=geometry))
    write(root/'instances/v8-nine-anomalies.json',dict(format='m3d-instances-v1',instances=objects))
    write(root/'materials/v8-nine-anomalies.json',mats)
    write(root/'pbr/v8-nine-anomalies.json',pbr)
    write(root/'lighting/v8-nine-anomalies.json',lights)
    write(root/'cameras/v8-nine-anomalies.json',cameras)
    newscene=copy.deepcopy(scene)
    newscene['name']='CSE WARP V17.430 / V8 — neuf corrections du palais et de la piscine'
    sec=newscene['sectors'][0]
    sec['instances']=['instances/v8-nine-anomalies.json']
    sec['geometry']=sec['geometry']+['geometry/v8-nine-anomalies.json']
    sec['materials']=['materials/v8-nine-anomalies.json']
    sec['pbr']=['pbr/v8-nine-anomalies.json']
    sec['lighting']=['lighting/v8-nine-anomalies.json']
    sec['cameras']=['cameras/v8-nine-anomalies.json']
    write(root/'scene-v8-nine-anomalies.json',newscene)
    # The existing portable launcher hardcodes this path; change the pointer
    # only, never overwrite any original artwork or geometry asset.
    write(root/'scene-v7-corrections.json',newscene)
    report['frescoOriginalSha256']=hashlib.sha256(original_art).hexdigest()
    report['frescoMeshesPreserved']=len(before_fresco)
    report['objects']=len(objects);report['newGeometries']=len(geometry)
    report['materials']=len(mats['materials']);report['lights']=len(lights['lights'])
    write(root/'reports/v8-nine-anomalies.json',report)
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--root',required=True,type=Path)
    build(ap.parse_args().root)