#!/usr/bin/env python3
"""CSE WARP V7: deterministic non-destructive scene corrections on the real V6 package."""
import argparse,copy,collections,json,math
from pathlib import Path
import numpy as np
from PIL import Image
from shapely.geometry import Point,Polygon,box
from shapely.ops import unary_union
from shapely import constrained_delaunay_triangles

def read(p):return json.loads(Path(p).read_text(encoding="utf-8-sig"))
def write(p,data):
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(json.dumps(data,ensure_ascii=False,separators=(",",":"))+"\n",encoding="utf-8")
def box_inst(name,parent,x,y,z,w,h,d,mat="prp_concrete_raw"):
    return dict(id=name,parent=parent,geometry="unit_box",material=mat,
                position=[round(x,5),round(y,5),round(z,5)],
                size=[round(w,5),round(h,5),round(d,5)],rotationY=0)
def mesh(name,vs,faces,uv=None):
    return dict(id=name,type="triangle_mesh",
       vertices=[[round(float(a),5) for a in v] for v in vs],
       faces=[list(map(int,f)) for f in faces],
       uv=uv if uv is not None else [[0,0] for _ in vs])
def disk(name,r,y=.155,n=64):
    vs=[[0,y,0]]+[[r*math.cos(2*math.pi*i/n),y,r*math.sin(2*math.pi*i/n)] for i in range(n)]
    return mesh(name,vs,[[0,i+1,(i+1)%n+1] for i in range(n)],
                [[.5,.5]]+[[.5+.5*math.cos(2*math.pi*i/n),.5+.5*math.sin(2*math.pi*i/n)] for i in range(n)])
def curb(name,ri,ro,n=64):
    # solid annular concrete edge with closed top and outer vertical walls
    vs=[];faces=[];uv=[]
    for y,r in [(-.14,ri),(.235,ri),(.235,ro),(-.14,ro)]:
        for j in range(n):
            a=2*math.pi*j/n;vs.append([r*math.cos(a),y,r*math.sin(a)]);uv.append([j/n,(y+.14)/.375])
    for k in range(3):
        for j in range(n):
            j2=(j+1)%n;a=k*n+j;b=k*n+j2;c=(k+1)*n+j;d=(k+1)*n+j2
            faces.extend([[a,b,c],[b,d,c]])
    return mesh(name,vs,faces,uv)
def flatmesh(name,polygon,height):
    # Constrained triangulation keeps ALL boundary edges and holes. Ordinary
    # Delaunay + triangle filtering dropped walkable patches in earlier scenes.
    vv=[];ff=[];uv=[];seen={}
    polys=[polygon] if polygon.geom_type=="Polygon" else list(getattr(polygon,"geoms",[]))
    for p in polys:
        if p.is_empty or p.area<1e-6:continue
        for tri in constrained_delaunay_triangles(p).geoms:
            if tri.area<1e-8:continue
            ind=[]
            for x,z in list(tri.exterior.coords)[:3]:
                key=(round(x,5),round(z,5))
                if key not in seen:
                    seen[key]=len(vv);vv.append([x,height,z]);uv.append([round(x*.25,5),round(z*.25,5)])
                ind.append(seen[key])
            ff.append(ind)
    return mesh(name,vv,ff,uv) if ff else None
def pipe(name,a,b,half=.045):
    # Continuous 3D rectangular rail, four sides, 12 triangles, no unsupported posts.
    a=np.array(a,float);b=np.array(b,float);t=(b-a)/np.linalg.norm(b-a)
    u=np.cross(t,[1.,0.,0.]);u/=np.linalg.norm(u)
    v=np.cross(t,u);v/=np.linalg.norm(v)
    vv=[]
    for q in (a,b):
        for sx,sy in [(-1,-1),(1,-1),(1,1),(-1,1)]:
            vv.append((q+half*(sx*u+sy*v)).tolist())
    f=[[0,2,1],[0,3,2],[4,5,6],[4,6,7]]
    for j in range(4):
        k=(j+1)%4;f.extend([[j,k,4+j],[k,4+k,4+j]])
    return mesh(name,vv,f)

def build(root):
    root=Path(root)
    scene=read(root/"scene-v6-monument.json");sector=scene["sectors"][0]
    objs=read(root/"instances/v6-monument.json")["instances"]
    v5geo=read(root/"geometry/revision-v5.json")["meshes"]
    v6geo=read(root/"geometry/monument-v6.json")["meshes"]
    geom={m["id"]:m for m in v5geo+v6geo}
    mats=read(root/"materials/monument-v6.json")
    pbr=read(root/"pbr/monument-v6.json")
    newgeo=[];removed=collections.Counter();report={}
    def remove(predicate):
        nonlocal objs
        old=objs;objs=[o for o in old if not predicate(o)]
        for o in old:
            if predicate(o):removed[o["parent"]]+=1

    # 1. Remove ONLY primitive shrub blobs, not the 26 authentic GLB trees,
    # the eight GLB pot trees or any flowers.
    remove(lambda o:o["parent"] in ("Place/Jardins/Arbustes","Place/Jardins devant entrée"))
    assert sum(removed.values())==22,removed

    # 2. Eight real walk-through gaps, at symmetric points of the existing oval.
    seats=[o for o in objs if o["parent"]=="Place/Banc ovale béton/Assise"]
    assert len(seats)==96
    missing={round(seats[k]["rotationY"],3) for k in range(96) if k%12 in (0,1)}
    remove(lambda o:o["parent"].startswith("Place/Banc ovale béton/")
           and round(o["rotationY"],3) in missing)
    assert len([o for o in objs if o["parent"]=="Place/Banc ovale béton/Assise"])==80
    # A brick strip follows the bench's ellipse; it is broken at each entrance.
    # A single geometric top mesh per segment, not 1 brick = 12 triangles.
    for k in range(96):
        if k%12 in (0,1):continue
        v=[];uv=[]
        for radius in (0,1):
            for q in (k-.05,k+1.05):
                th=2*math.pi*q/96.
                rx=34.10+radius*.48;rz=22.50+radius*.48
                x=rx*math.cos(th);z=72+rz*math.sin(th)
                v.append([x,.165,z]);uv.append([q*.16,radius])
        name="v7_brick_border_%02d"%k
        newgeo.append(mesh(name,v,[[0,1,2],[1,3,2]],uv))
        objs.append(dict(id="inst_"+name,parent="Place/Banc ovale béton/Bordure briques",
           geometry=name,material="prp_brick_red",position=[0,0,0],size=[1,1,1],rotationY=0))

    # 3. Measure each actual trunk footprint in the complete two-mesh GLB.
    # Re-center the trunk, then fit its individual circular pit (20 cm clearance).
    trees=[o for o in objs if o["parent"]=="Nature/Arbres medium-poly GLB"]
    assert len(trees)==52
    pit_shapes=[]
    for k in range(26):
        group=[o for o in trees if o["id"].startswith("rev_tree%d_"%k)]
        assert len(group)==2,(k,len(group))
        position=group[0]["position"][:];sc=group[0]["size"][0]
        th=math.radians(group[0]["rotationY"])
        ct,st=math.cos(th),math.sin(th);pts=[]
        for o in group:
            vv=np.asarray(geom[o["geometry"]]["vertices"],dtype=float)
            vv=vv[vv[:,1]<.10]
            for xx,yy,zz in vv:
                pts.append([sc*(xx*ct+zz*st),sc*(-xx*st+zz*ct)])
        pp=np.asarray(pts)
        offset=np.median(pp,axis=0)
        radius=float(np.max(np.linalg.norm(pp-offset,axis=1))+.27)
        radius=max(.95,min(radius,2.30))
        # Shift BOTH meshes together; their shared trunk center matches the old pit.
        for o in group:
            o["position"][0]=round(position[0]-offset[0],5)
            o["position"][2]=round(position[2]-offset[1],5)
        center=position
        dirt=next(o for o in objs if o["id"]=="rev_fosse_%02d_rev_fosse_terre"%k)
        ring=next(o for o in objs if o["id"]=="rev_fosse_%02d_rev_fosse_anneau_beton"%k)
        dirt_id="v7_pit_dirt_%02d"%k;rim_id="v7_pit_rim_%02d"%k
        newgeo.append(disk(dirt_id,radius,n=64))
        newgeo.append(curb(rim_id,radius,radius+.16,n=64))
        dirt["geometry"]=dirt_id;ring["geometry"]=rim_id
        # Same soil elevation and unchanged 26 original tree coordinates.
        pit_shapes.append(Point(center[0],center[2]).buffer(radius+.16,resolution=32))
    report["pitRadiiM"]=[round(float(math.sqrt(a.area/math.pi)-.16),3) for a in pit_shapes]
    assert max(report["pitRadiiM"])>1.3

    # 4. Retessellate only pavement/ground segments near the oval or tree pits.
    # Constrained triangles prevent missing wedges => no invisible trap holes.
    oval=Point(0,0).buffer(1,resolution=96)
    from shapely.affinity import scale as shp_scale,translate
    oval=translate(shp_scale(oval,34.65,23.05),0,72)
    exclusions=unary_union([oval]+pit_shapes)
    revised=0
    for obj in objs:
        parent=obj["parent"]
        if parent not in ("Place/Dalles V4b alignées","Terrain/Place","Terrain/Trottoirs",
                          "Terrain/Routes","Place/Parvis d’entrée"):
            continue
        if obj["geometry"]=="unit_box":
            x,y,z=obj["position"];w,h,d=obj["size"]
            pg=box(x-w/2,z-d/2,x+w/2,z+d/2);floor=y+h/2
        elif obj["geometry"].startswith("cut_paving_"):
            source=geom[obj["geometry"]]
            # The original large slab bounds are known; for all other geometries
            # reconstruct the original clipped polygon, retaining its prior holes.
            if parent=="Terrain/Place":
                pg=box(-72,-19,72,91.5)
            elif parent=="Place/Parvis d’entrée":
                pg=box(-34,-5.5,34,17)
            else:
                parts=[Polygon([(source["vertices"][i][0],source["vertices"][i][2])
                     for i in f]) for f in source["faces"]]
                pg=unary_union(parts)
            floor=source["vertices"][0][1]
        else:continue
        if parent=="Place/Parvis d’entrée":
            # Full ground repair at upper arrival of exterior monumental staircase.
            target=pg
        elif parent=="Terrain/Place":
            target=pg.difference(unary_union(pit_shapes))
        else:
            if not pg.intersects(exclusions):continue
            target=pg.difference(exclusions if parent=="Place/Dalles V4b alignées"
                                 else unary_union(pit_shapes))
        if target.is_empty:
            obj["parent"]="V7/Removed pavement";obj["size"]=[0,0,0];continue
        if abs(target.area-pg.area)<.00001 and parent!="Place/Parvis d’entrée":continue
        name="v7_cut_paver_%04d"%revised
        m=flatmesh(name,target,floor)
        if m:
            newgeo.append(m);obj["geometry"]=name;obj["position"]=[0,0,0]
            obj["size"]=[1,1,1];revised+=1
    remove(lambda o:o["parent"]=="V7/Removed pavement")
    report["retessellatedPavementSections"]=revised

    # 4b. Restore safe lateral connections from the 55cm-high front parvis
    # to the surrounding plaza. The former 61m staircase ended before the
    # 68m parvis width: visitors near the outside columns could fall 51cm.
    for o in objs:
        if o["parent"]=="Place/Escalier monumental":
            o["size"][0]=68.2 # full width of raised parvis, never 61m
    for side in (-1,1):
        for i in range(4):
            xx=side*(34.55+1.05*i)
            top=.555-.102*(i+1)
            objs.append(box_inst("v7_parvis_side_%s_%02d"%(side,i),
                "Place/Parvis/Transitions laterales sans trou",xx,top-.085,5.75,
                1.14,.17,22.65,"prp_terrazzo_floor"))
    report["frontStairWidthM"]=68.2
    report["lateralParvisTransitions"]=8

    # 5. Stronger hill and authentic flowers conform to its new profile.
    OLD_H=1.2;NEW_H=2.7;BASE=.16
    def oldheight(x,z):
        r=math.hypot(x/13.2,(z-72)/7.8)
        t=max(0,min(1,(r-.12)/.88))
        return BASE+OLD_H*(1-t*t*(3-2*t))
    def newheight(x,z):
        r=math.hypot(x/15.2,(z-72)/9.3)
        t=max(0,min(1,(r-.10)/.90))
        return BASE+NEW_H*(1-t*t*(3-2*t))
    hill=copy.deepcopy(geom["v6_butte_relief_solid"]);hill["id"]="v7_hill_2m70"
    # Recompute surface Y, keep bottom/skirt below the base.
    for v in hill["vertices"]:
        if v[1]>=BASE-.00001:v[1]=round(newheight(v[0],v[2]),5)
    newgeo.append(hill)
    next(o for o in objs if o["id"]=="v6_butte_continue")["geometry"]=hill["id"]
    for o in objs:
        if o["parent"].startswith(("Place/Butte vegetale/","Place/Butte relief 1m20/")):
            if o["geometry"]=="v6_butte_relief_solid":continue
            x,y,z=o["position"];o["position"][1]=round(y+newheight(x,z)-oldheight(x,z),5)
    # Plinth grows 1.2 -> 1.55m; sculpture 5.0 -> 7.1m, preserving source shapes.
    for o in objs:
        if o["id"] in ("v6_socle_beton","v6_inscription_fr_uk"):
            o["position"][1]=round(BASE+NEW_H,5)
            o["size"]=[1.12,1.29,1.10]
        if o["parent"]=="Place/Monument marteau faucille/Sculpture acier rouge":
            o["position"][1]=round(BASE+NEW_H+1.55,5)
            o["size"]=[1.35,1.42,1.35]
    report["hillHeightM"]=NEW_H;report["sculptureHeightM"]=7.10

    # 6. Swimming-pool stair: close to east wall, 2.4m wide, correct risers.
    remove(lambda o:o["parent"].startswith("Interieur/Escalier galerie"))
    def stair(prefix,x,z_start,z_sign,y0,y1,count=25):
        # flight of true contiguous treads; each stair has a visible riser
        step=(y1-y0)/count;span=10.5;dz=span/count
        for i in range(count):
            z=z_start+z_sign*(i+.5)*dz
            y=y0+(i+1)*step
            objs.append(box_inst(prefix+"_t_%02d"%i,prefix+"/Marches",x,y-.075,z,2.4,.15,dz+.025,"prp_terrazzo_floor"))
            objs.append(box_inst(prefix+"_r_%02d"%i,prefix+"/Contremarches",x,y-step*.50,z-z_sign*dz*.43,
                          2.4,step,.055,"prp_concrete_raw"))
        # Two sloped continuous 3D handrails and separate support posts.
        for side in (-1,1):
            xp=x+side*1.17
            a=[xp,y0+1.03,z_start]
            b=[xp,y1+1.03,z_start+z_sign*span]
            name=prefix+"_rail_%s"%("L" if side<0 else "R")
            newgeo.append(pipe(name,a,b,.045))
            objs.append(dict(id=name,parent=prefix+"/Garde-corps continus",geometry=name,
                 material="prp_steel_painted",position=[0,0,0],size=[1,1,1],rotationY=0))
            for i in range(0,count,4):
                z=z_start+z_sign*(i+.5)*dz;y=y0+(i+1)*step
                objs.append(box_inst(prefix+"_post_%s_%02d"%(side,i),prefix+"/Poteaux courts",
                             xp,y+.52,z,.065,1.04,.065,"prp_steel_painted"))
        end=z_start+z_sign*span
        objs.append(box_inst(prefix+"_landing",prefix+"/Palier",x,y1-.07,end+z_sign*.20,2.55,.14,.70,"prp_terrazzo_floor"))
    # V7.426: the two experimental stair flights are removed as requested.
    # Keep the stair() helper for a later relocation away from the pool.
    # Remove stair-width trémies from the first and second galleries,
    # preserving the concrete slabs on both sides.
    remove(lambda o:o["parent"]=="Interieur/Galeries"
       and ((abs(o["position"][1]-4.5)<.1 and o["position"][0]==45)
            or (abs(o["position"][1]-9)<.1 and o["position"][0]==39.5)))
    for level,z0,z1 in [(4.5,-22,-10.5),(9.,-42,-30.5)]:
        # The right side-gallery has bounds x=[28.5,50.5], z=[-54.5,-5.5].
        # Its central 2.8m stairwell cuts only the relevant z-interval.
        # Other slabs already occupy the left, center and rear of this gallery.
        for xx,ww in [(32.0,7.0),(47.2,6.6)]:
            objs.append(box_inst("v7_deck_%d_%s"%(level*10,xx),
                "Interieur/Galeries/Plancher avec tremie",xx,level,-30,ww,.30,49))
        for za,zb in [(-54.5,z0), (z1,-5.5)]:
            if zb>za:
                objs.append(box_inst("v7_deck_seg_%d_%s"%(level*10,za),
                    "Interieur/Galeries/Plancher avec tremie",43,level,(za+zb)/2,8.2,.30,zb-za))
    # Full-height mural atrium: NONE of the three central rear slabs may
    # cross the image, including the 4.5m mezzanine. Side decks are preserved.
    remove(lambda o:o["parent"]=="Interieur/Galeries"
       and o["position"][1]>=4.4 and abs(o["position"][0])<.1
       and o["position"][2]<-43)
    # Remove the now unsupported 57m transverse 1st-floor rail and its posts,
    # not the longitudinal protective rails of the retained side galleries.
    remove(lambda o:o["parent"].startswith("Interieur/Galeries/Garde-corps 1er etage/")
       and abs(o["position"][2]+45.3)<.2 and abs(o["position"][0])<28.1)
    report["muralAtriumCentralSlabs"]=0
    # V7.423: the apparently surviving transverse "gallery" in the exact
    # 07 / v4_piscine view is a separate pool-end 17-bay guardrail at z=-45.6,
    # plus a 50-m freestanding cross-rail at z=-44.64.  The upper concrete
    # gallery slabs had ALREADY been removed in V7.422.  These 35 elements
    # obscured the bottom half of the fresco despite the empty atrium.
    # Remove ONLY the transverse far-end barrier; retain the near-end
    # protection, both longitudinal galleries and all basin concrete walls.
    rear_rail=[o for o in objs
               if o["parent"]=="Interieur/Piscine olympique/Garde-corps"
               and abs(o["position"][2]+45.60)<.01]
    rear_handrail=[o for o in objs
                   if o["parent"]=="Interieur/Piscine olympique/Main courante"
                   and abs(o["position"][2]+44.64)<.01]
    assert len(rear_rail)==34 and len(rear_handrail)==1, (
        "Unexpected pool-end rail geometry; refuse ambiguous deletion",
        len(rear_rail),len(rear_handrail))
    remove(lambda o:
           (o["parent"]=="Interieur/Piscine olympique/Garde-corps"
            and abs(o["position"][2]+45.60)<.01)
           or (o["parent"]=="Interieur/Piscine olympique/Main courante"
               and abs(o["position"][2]+44.64)<.01))
    assert sum(o["parent"]=="Interieur/Piscine olympique/Garde-corps"
               for o in objs)==34
    assert sum(o["parent"]=="Interieur/Piscine olympique/Main courante"
               for o in objs)==1
    report["muralSightlineRemovedPoolEndRailParts"]=len(rear_rail)+len(rear_handrail)
    report["poolEndGuardrailNearEndPreserved"]=34

    # The side gallery itself also projected a *long* foreground rail across
    # the mural in the original camera 07: rail x=28.5, y=5.75, z=-30 ±24.5.
    # Removing only the 35 rear pool-end rail pieces was insufficient.
    # Open 10.4m of the interior RIGHT galleries (levels 4.5, 9 and 13.5)
    # while RETAINING the outer strip at x=38.9..50.5 and the stair at x=43.
    # The former 1st-level rail at x=28.5 and its redundant x=34.5/39.5
    # fragments are replaced by fitted edge rails on the retained strip.
    inboard=[o for o in objs if
        o["parent"] in ("Interieur/Galeries",
                        "Interieur/Galeries/Plancher avec tremie")
        and 20<o["position"][0]<45
        and any(abs(o["position"][1]-h)<.01 for h in (4.5,9.,13.5))
        and o["position"][0]-o["size"][0]/2<38.8]
    assert len(inboard)==6, (
        "Unexpected right gallery geometry; refuse unsupported floor removal",
        len(inboard),[(o["id"],o["position"]) for o in inboard])
    remove(lambda o:o["id"] in {a["id"] for a in inboard})
    older_right_rail=[o for o in objs if
        o["parent"].startswith("Interieur/Galeries/Garde-corps 1er etage/")
        and 28.4<o["position"][0]<40.5]
    assert len(older_right_rail)>=20, "Expected right rail not found"
    old_right_ids={o["id"] for o in older_right_rail}
    remove(lambda o:o["id"] in old_right_ids)
    objs.append(box_inst("v7_gallery_right_third_floor_narrow",
        "Interieur/Galeries/V7 coursive exterieure conservee",
        44.7,13.5,-30,11.6,.30,49,"prp_concrete_raw"))
    # Physically attached 1.10m balustrades on outer strip with openings
    # for the existing (retained) stairs at x=43.
    rail_count=0
    for level,spans in [
        (4.5,[(-54.5,-23.0),(-10.5,-5.5)]),
        (9.0,[(-54.5,-42.25),(-30.25,-5.5)]),
        (13.5,[(-54.5,-5.5)])]:
        for index,(lo,hi) in enumerate(spans):
            length=hi-lo
            if length<=0:continue
            mid=(lo+hi)/2
            prefix="v7_gallery_edge_%s_%d"%(int(level*10),index)
            for j,(height,profile) in enumerate([
                (level+.15+1.10,"Main courante"),
                (level+.15+.56,"Lisse")]):
                objs.append(box_inst(prefix+"_%s"%j,
                    "Interieur/Galeries/V7 Garde-corps recules/"+profile,
                    38.88,height,mid,.075,.075,length,
                    "prp_steel_painted"))
            n=max(2,int(math.ceil(length/2.5))+1)
            for j in range(n):
                zz=lo+length*j/(n-1)
                objs.append(box_inst(prefix+"_post_%02d"%j,
                    "Interieur/Galeries/V7 Garde-corps recules/Montants",
                    38.88,level+.15+.55,zz,.075,1.10,.075,
                    "prp_steel_painted"))
            rail_count+=1
    assert rail_count==5
    report["galleryRightInboardSlabsRemoved"]=len(inboard)
    report["galleryRightOldRailPartsRemoved"]=len(older_right_rail)
    report["galleryRightOuterFootwayPreserved"]=True
    report["galleryRightNewRailSections"]=rail_count
    # The only remaining thin line across the mural in camera 07 was NOT a
    # gallery: two pool fanion cables and 20 dangling red/white cloth blocks
    # (at y=4.16/3.96). Keep the water-lane dividers; remove the overhead
    # pennants so the artwork remains unobstructed at every viewing distance.
    fanions=[o for o in objs if
             o["parent"].startswith("Interieur/Piscine olympique/Fanions/")]
    assert len(fanions)==22, (
        "Unexpected pool bunting count; review before destructive edit",len(fanions))
    remove(lambda o:o["parent"].startswith(
        "Interieur/Piscine olympique/Fanions/"))
    report["hangingMuralOccludingPennantsRemoved"]=len(fanions)




    # V7.426: the original hall/pool staircase (25 treads + 50 loose posts)
    # STILL existed in the V7 screenshot. Remove its 75 visible instances
    # and the detached landing tongue; do not mistake this for the previously
    # removed 27-tread "Interieur/Escalier galerie" legacy flight.
    old_stair=[o for o in objs if
        o["parent"].startswith("Interieur/Hall/Grand escalier")]
    old_landing=[o for o in objs if
        o["parent"]=="Interieur/Hall/Palier raccord escalier"]
    assert len(old_stair)==75 and len(old_landing)==1,(
        "Unexpected obsolete staircase geometry",len(old_stair),len(old_landing))
    remove(lambda o:o["parent"].startswith("Interieur/Hall/Grand escalier")
           or o["parent"]=="Interieur/Hall/Palier raccord escalier")
    # Close the original 6.3m-wide staircase slot in the first-floor frontal
    # mezzanine.  This is part of the actual walkable floor, not a decal.
    objs.append(box_inst("v7_close_obsolete_hall_stairwell",
        "Interieur/Galeries/V7 fermeture tremie ancien escalier",
        -15,4.5,-11.15,6.3,.30,5.1,"prp_concrete_raw"))
    # The experimental east-side V7 staircase is also removed, as requested.
    # Restore the FIRST and SECOND floor as fully continuous 11.6m-wide
    # galleries, not an invisible bridge over the former stair opening.
    old_decks=[o for o in objs if
        o["parent"]=="Interieur/Galeries/Plancher avec tremie"]
    assert len(old_decks)>=4,("Unexpected floor cutouts",len(old_decks))
    remove(lambda o:o["parent"]=="Interieur/Galeries/Plancher avec tremie")
    for level in (4.5,9.0):
        objs.append(box_inst("v7_close_east_stair_floor_%02d"%int(level*10),
            "Interieur/Galeries/V7 coursive exterieure conservee",
            44.7,level,-30,11.6,.30,49,"prp_concrete_raw"))
    # Remove guardrails that had artificial holes for the missing staircase.
    old_edge=[o for o in objs if
        o["parent"].startswith("Interieur/Galeries/V7 Garde-corps recules/")]
    assert len(old_edge)>=20
    remove(lambda o:o["parent"].startswith(
        "Interieur/Galeries/V7 Garde-corps recules/"))
    for level in (4.5,9.0,13.5):
        rootname="Interieur/Galeries/V7 Garde-corps recules"
        for layer,h in (("Main courante",1.25),("Lisse",.71)):
            objs.append(box_inst("v7_complete_east_gallery_%02d_%s"%(
                int(level*10),layer.replace(" ","_")),rootname+"/"+layer,
                38.88,level+h,-30,.075,.075,49,"prp_steel_painted"))
        for k in range(21):
            objs.append(box_inst("v7_complete_east_gallery_%02d_post_%02d"%(
                int(level*10),k),rootname+"/Montants",
                38.88,level+.70,-54.5+k*2.45,.075,1.10,.075,
                "prp_steel_painted"))
    report["obsoletePoolStairInstancesRemoved"]=len(old_stair)+len(old_landing)
    report["experimentalPoolStairInstancesRemaining"]=sum(
        o["parent"].startswith("Interieur/Escalier V7") for o in objs)
    report["closedFormerStairFloorSlabs"]=3
    report["galleryRightNewRailSections"]=3

    # V7.426: all visible "lighting geometry" suspended without a ceiling,
    # beam or wall was obsolete decoration, not one of the actual point lights
    # in lighting.json.  Remove the unsupported floating blocks everywhere
    # in the hall, natatorium and galleries, WITHOUT changing real light
    # definitions or the roof and window frames.
    unmounted_light_groups=(
        "Interieur/Hall/Plafonniers/",
        "Interieur/Piscine olympique/Plafonniers/",
        "Interieur/Piscine olympique/Lumière linéaire",
        "Interieur/Galeries/Sous mezzanine/",
        "Interieur/Hall/Appliques murales",
        "Interieur/Piscine olympique/Appliques galerie")
    floaters=[o for o in objs if o["parent"].startswith(unmounted_light_groups)]
    assert len(floaters)==154,(
        "Unexpected free-standing fixture population",len(floaters))
    remove(lambda o:o["parent"].startswith(unmounted_light_groups))
    # Original two bright isolated white partitions did not form a complete
    # wall or door. They have no overhead/wall mounting; clear the hallway.
    white_posts=[o for o in objs if o["parent"]=="Interieur/Hall/Panneaux"]
    assert len(white_posts)==2
    remove(lambda o:o["parent"]=="Interieur/Hall/Panneaux")
    report["floatingLuminaireMeshesRemoved"]=len(floaters)
    report["isolatedHallLightBlocksRemoved"]=len(white_posts)

    # 7. True maximal-ratio artwork: original 2048x930 pixels -> 4x2 native tiles.
    # Runtime low-memory atlas is limited to <=512px/material, so 8 individual
    # material tiles preserve source detail instead of downsampling it to 512px.
    remove(lambda o:o["parent"].startswith("Interieur/Piscine/Fresque"))
    original=Image.open(root/"textures/energetik_fresco_mosaic.jpg").convert("RGB")
    assert original.size==(2048,930)
    width=39.50;height=width*930/2048
    for iy in range(2):
        for ix in range(4):
            # source file image origin is upper-left; runtime box UV orientation
            # uses the same V orientation as existing raster texture.
            tile=original.crop((ix*512,iy*465,(ix+1)*512,(iy+1)*465))
            path="textures/revision/v7/energetik_tile_%d_%d.png"%(iy,ix)
            out=root/path;out.parent.mkdir(parents=True,exist_ok=True)
            tile.save(out)
            mid="v7_fresco_%d_%d"%(iy,ix)
            mats["materials"].append(dict(id=mid,baseColor=[1,1,1],roughness=.9,
                  metallic=0,transmission=0,ior=1.5,textureSet="tex_"+mid))
            pbr["sets"].append(dict(id="tex_"+mid,material=mid,pattern="concrete",
                  resolution=512,imageFile=path))
            x=(ix-1.5)*width/4
            y=.65+(1.5-iy)*height/2 # center tile, top row is highest Y
            # Custom two-triangle billboard: unit_box UVs repeat textures
            # several times over long walls. Explicit 0..1 UVs prevent it.
            name="v7_fresco_quad_%d_%d"%(iy,ix)
            w=width/4;h=height/2;z=-54.11
            vertices=[[x-w/2,y-h/2,z],[x+w/2,y-h/2,z],
                      [x+w/2,y+h/2,z],[x-w/2,y+h/2,z]]
            uv=[[0,0],[1,0],[1,1],[0,1]]
            newgeo.append(mesh(name,vertices,[[0,1,2],[0,2,3],[2,1,0],[3,2,0]],uv))
            objs.append(dict(id="v7_fresco_%d_%d_inst"%(iy,ix),
               parent="Interieur/Piscine/Fresque murale 2048 native pixels",
               geometry=name,material=mid,position=[0,0,0],
               size=[1,1,1],rotationY=0))
    report["frescoAspectRatio"]=width/height;report["frescoNativePixels"]=[2048,930]
    report["frescoTiles"]=8
    # Preserve the original 3D wall behind it.
    objs.append(box_inst("v7_fresco_wall","Interieur/Piscine/Mur porteur fresque",
                         0,9.62,-54.25,width+.4,18.15,.13,"prp_plaster_pale"))

    # 8. Initial viewpoint: plaza pedestrian, not a remote panorama.
    cams=read(root/"cameras/monument-v6.json")
    cams["cameras"][0].update(position=[0,2.2,100],target=[0,6.1,72],fovDegrees=59)
    cams["cameras"][0]["name"]="01 · Monument proche, départ à pied"
    cams["cameras"][1].update(position=[31,4.7,114],target=[0,5.8,72],fovDegrees=61)
    next(c for c in cams["cameras"] if c["id"]=="v4_piscine").update(
        position=[27.7,2.10,-16.5],target=[0,9.0,-54.14],fovDegrees=73)
    cams["cameras"].append(dict(id="v7_pool_fresco",name="V7 · Fresque piscine panoramique",
        position=[0,3.3,-13],target=[0,9.6,-54.14],fovDegrees=70))
    # All V6 data and the original V5 remain untouched.
    write(root/"cameras/v7.json",cams)
    new_ids=[o["id"] for o in objs]
    assert len(set(new_ids))==len(new_ids),len(new_ids)-len(set(new_ids))
    write(root/"geometry/v7-corrections.json",dict(format="m3d-geometry-v1",units="m",meshes=newgeo))
    write(root/"instances/v7-corrections.json",dict(format="m3d-instances-v1",instances=objs))
    write(root/"materials/v7.json",mats);write(root/"pbr/v7.json",pbr)
    scene=copy.deepcopy(scene)
    scene["name"]="CSE WARP V17.422 / V7 - atrium, escaliers, fosses GLB et monument"
    sec=scene["sectors"][0]
    sec["instances"]=["instances/v7-corrections.json"]
    sec["geometry"]=sec["geometry"]+["geometry/v7-corrections.json"]
    sec["materials"]=["materials/v7.json"];sec["pbr"]=["pbr/v7.json"]
    sec["cameras"]=["cameras/v7.json"]
    write(root/"scene-v7-corrections.json",scene)
    write(root/"scene-v4.json",scene)
    report.update(removedCategories=dict(removed),retainedRealTrees=len(trees),
        remainingBenchSeatSections=80,originalV6Preserved=(root/"scene-v6-monument.json").exists(),
        objects=len(objs),newGeometries=len(newgeo))
    assert len(mats["materials"])<256
    write(root/"reports/v7-corrections.json",report)
    print(json.dumps(report,ensure_ascii=False,indent=2))
if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--root",type=Path,required=True)
    build(parser.parse_args().root)
