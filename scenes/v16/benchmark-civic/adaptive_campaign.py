#!/usr/bin/env python3
"""Scene-only visual feedback: 10 native WARP iterations; no C++/engine modifications.
Uses the frozen, prebuilt V17.370 executable and the existing benchmark scene.
Measurements are transparent proxies, NOT an automated artistic review.
"""
import argparse, csv, hashlib, json, math, os, shutil, subprocess, sys
from pathlib import Path
from PIL import Image, ImageFilter, ImageStat

CAMERAS = (("facade", 14), ("passerelle", 14), ("vue_haut", 14),
           ("piscine_soir", 19), ("hall", 19))
MATERIALS = {
    "bronze": "cse_monument_bronze", "leaf": "cse_leaf_olive",
    "green": "prp_foliage", "red": "cse_flower_red",
    "gold": "cse_flower_gold", "bark": "prp_bark_wet",
    "concrete": "prp_concrete_panel", "tile": "prp_tile_ceramic",
    "metal": "prp_steel_painted", "warm": "cse_neon_gold",
    "cream": "cse_banner_cream", "banner": "cse_banner_red",
}
ACTIONS = {
    "monument_surface": ("passerelle", "Statue trop géométrique : volumes sculptés"),
    "monument_anatomy": ("passerelle", "Articulations et drapé insuffisants"),
    "tree_branching": ("facade", "Arbres : embranchements et silhouette pauvres"),
    "tree_leaflets": ("facade", "Feuillage trop uniforme, ajouter des lobes"),
    "flower_stems": ("passerelle", "Fleurs sphériques : vraies tiges et pétales"),
    "flower_groundcover": ("passerelle", "Massif central sans végétation basse"),
    "facade_relief": ("facade", "Relief architectural trop plat"),
    "roof_folds": ("facade", "Banderoles du toit sans plis"),
    "pool_detail": ("piscine_soir", "Bassin : lignes d'eau et finition simplifiées"),
    "hall_detail": ("hall", "Hall : éclairage et habillage peu détaillés"),
    "materials_lighting": ("piscine_soir", "Matériaux ou éclairage trop uniformes"),
}
# Importance comes from the current rendered view, not the pass number.
ZONE = {
    "facade": (0.20, 0.10, 0.80, 0.83),
    "passerelle": (0.27, 0.16, 0.78, 0.94),
    "vue_haut": (0.12, 0.21, 0.90, 0.85),
    "piscine_soir": (0.04, 0.15, 0.97, 0.91),
    "hall": (0.10, 0.12, 0.90, 0.90),
}
def dump(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf8")

def read(path):
    return json.loads(path.read_text(encoding="utf8"))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def add(inst, parent, kind, material, x, y, z, sx, sy, sz, rot=0):
    if min(sx, sy, sz) <= 0:
        raise ValueError("Dimensions non positives")
    inst.append(dict(id="adaptive_%06d" % (len(inst)+1), parent=parent,
                     geometry=kind, material=material,
                     position=[round(x, 3), round(y, 3), round(z, 3)],
                     size=[round(sx, 3), round(sy, 3), round(sz, 3)],
                     rotationY=round(rot, 3)))

def object_stats(inst):
    def count(s): return sum(o["parent"].startswith(s) for o in inst)
    return dict(instances=len(inst),
                monument=count("Place/Monument soviétique fictif"),
                trees=count("Nature/Arbres"),
                flowers=count("Place/Butte végétalisée"),
                facade=count("Architecture/Palais/Facade"),
                banners=count("Architecture/Palais/Banderoles toit"),
                pool=count("Interieur/Piscine olympique"),
                hall=count("Interieur/Hall"))

def image_stats(file, camera):
    im = Image.open(file).convert("RGB")
    w, h = im.size
    roi = ZONE[camera]
    crop = im.crop((int(roi[0]*w), int(roi[1]*h),
                    int(roi[2]*w), int(roi[3]*h)))
    small = crop.resize((128, 72), Image.Resampling.BILINEAR)
    vals = list(small.getdata())
    bright = sum((r+g+b)/3 for r,g,b in vals)/len(vals)
    colorful = sum(max(r,g,b)-min(r,g,b) > 55 for r,g,b in vals)/len(vals)
    green = sum(g > r*1.14 and g > b*1.08 and g > 35 for r,g,b in vals)/len(vals)
    edges = ImageStat.Stat(small.convert("L").filter(ImageFilter.FIND_EDGES)).mean[0]
    contrast = ImageStat.Stat(small.convert("L")).stddev[0]
    return dict(brightness=round(bright,2), edge=round(edges,2),
                contrast=round(contrast,2), colorful=round(colorful,4),
                green=round(green,4), hash=sha(file), width=w, height=h)

def defect_scores(obs, stat, done):
    # Each deficiency is evaluated from the PREVIOUS images plus geometry.
    # Limits are documentary targets, not assertions of human-like AI vision.
    p=obs["passerelle"]; f=obs["facade"]; pool=obs["piscine_soir"]; hall=obs["hall"]
    deficit=lambda target,value: max(0.0, min(1.0, (target-value)/target))
    result={
      "monument_surface": .35+deficit(22,p["edge"])*.5+deficit(95,stat["monument"])*.4,
      "monument_anatomy": .22+deficit(24,p["edge"])*.5,
      "tree_branching": .30+deficit(.03,f["green"])*.45,
      "tree_leaflets": .24+deficit(.055,f["green"])*.48,
      "flower_stems": .29+deficit(.040,p["colorful"])*.28,
      "flower_groundcover": .25+deficit(.055,p["green"])*.30,
      "facade_relief": .12+deficit(21,f["edge"])*.48,
      "roof_folds": .10+deficit(.055,f["colorful"])*.3,
      "pool_detail": .18+deficit(14,pool["edge"])*.40,
      "hall_detail": .12+deficit(16,hall["edge"])*.36,
      "materials_lighting": .12+deficit(30,pool["contrast"])*.44,
    }
    return {k:round(v,4) for k,v in sorted(result.items(),
             key=lambda z:-z[1]) if k not in done}

def repair(data, name):
    inst=data["instances/leger.json"]["instances"]
    mats=data["materials/pripyat-reuse.json"]["materials"]
    lights=data["lighting/day.json"]["lights"]
    before=len(inst)
    B=lambda p,m,x,y,z,a,b,c,r=0:add(inst,p,"unit_box",m,x,y,z,a,b,c,r)
    S=lambda p,m,x,y,z,a,b,c:add(inst,p,"unit_sphere",m,x,y,z,a,b,c)
    F=lambda p,m,x,y,z,a,b,c:add(inst,p,"foliage_canopy",m,x,y,z,a,b,c)
    mat=MATERIALS
    if name == "monument_surface":
        # Two retained sculpted figures around x=36.24 and 37.73, z~=52.
        # New small overlapping parts, not merely stretching the old torso.
        for center,z in ((36.24,52.14),(37.73,51.89)):
            for j in range(8):
                angle=j*math.pi/4
                S("Place/Monument soviétique fictif/Silhouettes/Anatomie",
                  mat["bronze"],center+.39*math.cos(angle),4.55+.07*(j%2),
                  z+.28*math.sin(angle),.25,.67,.23)
            for k in range(6):
                t=k/5
                S("Place/Monument soviétique fictif/Silhouettes/Drapé",
                  mat["bronze"],center-.38+.76*t,3.68-.08*(k%2),
                  z+.31,.19,1.0+.2*(k%2),.25)
    elif name == "monument_anatomy":
        for center,z in ((36.24,52.14),(37.73,51.89)):
            for j in range(7):
                t=j/6
                # Shoulders / wrists and narrower face profile.
                S("Place/Monument soviétique fictif/Silhouettes/Bras et mains",
                  mat["bronze"],center+.6+1.2*t,4.95+.25*t,z,
                  .22,.24,.19)
            for j in range(4):
                S("Place/Monument soviétique fictif/Silhouettes/Visage",
                  mat["bronze"],center,5.64+.095*j,z+.22,
                  .23,.12,.18)
    elif name in ("tree_branching","tree_leaflets"):
        trunks=[o for o in inst if o["parent"]=="Nature/Arbres"]
        for i,o in enumerate(trunks):
            x,y,z=o["position"]; h=o["size"][1]; base=y-h/2
            if name=="tree_branching":
                for j in range(6):
                    angle=(i*.63+j*2.39996)
                    yy=base+h*(.58+.045*(j%5))
                    B("Nature/Arbres/Branches fines",mat["bark"],
                      x+math.cos(angle)*1.05,yy,
                      z+math.sin(angle)*1.05,
                      1.28,.10,.10,math.degrees(angle))
                    F("Nature/Arbres/Feuilles secondaires",mat["leaf"],
                      x+math.cos(angle)*1.55,yy+.45,
                      z+math.sin(angle)*1.45,1.35,2.10,1.35)
            else:
                for j in range(8):
                    angle=(i*.87+j*2.39996)
                    yy=base+h*(.73+.02*(j%4))
                    F("Nature/Arbres/Feuilles fines",mat["green"],
                      x+math.cos(angle)*1.2,yy+.8,
                      z+math.sin(angle)*1.1,.96,1.65,.95)
    elif name in ("flower_stems","flower_groundcover"):
        cx,cz=37,52
        for j in range(180):
            a=j*2.399963; radius=math.sqrt((j+.5)/180)*.85
            x=cx+math.cos(a)*radius*13.3; z=cz+math.sin(a)*radius*7.6
            if abs(x-cx)<2.8 and abs(z-cz)<2.7:continue
            y=.33+1.12*math.sqrt(max(0,1-radius*radius))
            if name=="flower_stems":
                B("Place/Butte végétalisée/Tiges fines",mat["leaf"],
                  x,y+.24,z,.035,.46,.035)
                for k in range(4):
                    aa=k*math.pi/2
                    S("Place/Butte végétalisée/Pétales fins",mat["red"] if j%3 else mat["gold"],
                      x+.13*math.cos(aa),y+.47,z+.13*math.sin(aa),
                      .17,.09,.14)
            else:
                F("Place/Butte végétalisée/Couvre-sol",mat["green"],
                  x,y+.09,z,.57,.27,.48)
    elif name=="facade_relief":
        for side in (-1,1):
            for x0 in (25,32,39,46):
                x=side*x0
                for y in (5.35,9.5,13.65,17.8):
                    B("Architecture/Palais/Facade/Modules de relief",
                      mat["concrete"],x,y,-3.61,1.4,.13,.18)
            for x0 in (26,36,45):
                B("Architecture/Palais/Facade/Joints verticaux",
                  mat["concrete"],side*x0,12.0,-3.68,.055,10.4,.12)
    elif name=="roof_folds":
        for side in (-1,1):
            bx=side*36.5
            for j in range(9):
                x=bx-1.89+j*.47
                B("Architecture/Palais/Banderoles toit/Plis de tissu",
                  mat["banner"] if side<0 else mat["cream"],x,22.52,
                  -3.21+(j%3)*.037,.07,3.7,.08)
    elif name=="pool_detail":
        for side in (-1,1):
            x=side*25.35
            for j in range(25):
                z=-44+j
                B("Interieur/Piscine olympique/Frise carrelage",
                  mat["tile"],x,.19,z,.13,.16,.89)
        for j in range(10):
            z=-43.25+2.5*j
            S("Interieur/Piscine olympique/Plots détail",mat["metal"],
              -26.7,1.14,z,.29,.08,.18)
    elif name=="hall_detail":
        for side in (-1,1):
            for j in range(8):
                B("Interieur/Hall/Encadrements additionnels",mat["concrete"],
                  side*(20+j*1.4),6.8,-17.76,.09,1.8,.15)
        for j in range(15):
            B("Interieur/Hall/Frise basse",mat["tile"],
              -25+j*1.4,2.1,-17.73,1.25,.15,.11)
    elif name=="materials_lighting":
        for m in mats:
            if m["id"]=="cse_monument_bronze":
                m["roughness"]=.48
            elif m["id"]=="cse_banner_red":
                m["roughness"]=.93
        for l in lights:
            if 16.4<l["position"][1]<17.1:
                l["intensity"]=round(l["intensity"]*.94,3)
    else:
        raise ValueError("Unknown repair "+name)
    return len(inst)-before

def validate(data, original):
    objects=data["instances/leger.json"]["instances"]
    materials=data["materials/pripyat-reuse.json"]["materials"]
    valid_ids={m["id"] for m in materials}
    geom={x["id"] for x in data["geometry/benchmark.json"]["meshes"]}
    assert len(objects)<65000 and len(objects)>=len(original)
    assert len({x["id"] for x in objects})==len(objects), "duplicate IDs"
    for o in objects:
        assert o["geometry"] in geom and o["material"] in valid_ids
        assert all(math.isfinite(x) for x in o["position"]+o["size"])
        assert min(o["size"])>0
    assert sum(o["material"]=="cse_personnage_facade" for o in objects)==1
    assert sum(o["material"]=="cse_energetik_fresco" for o in objects)==1
    assert sum(o["parent"]=="Nature/Arbres" for o in objects)==26
    return len(objects)

def render(scene, exe, out, base):
    out.mkdir(parents=True,exist_ok=True)
    pictures={}; obs={}; ms={}
    env=os.environ.copy()
    env.update(M3D_V16_FORCE_WARP="1",M3D_V16_GPU_GI_SAMPLES="0",
               M3D_PRIMARY_RAYS_25="0")
    for cam,hour in CAMERAS:
        dest=out/"runs"/cam
        dest.mkdir(parents=True,exist_ok=True)
        command=[str(exe),str(scene/"scene.json"),str(dest),
                 "--smoke","--camera-id",cam,"--hour",str(hour)]
        cp=subprocess.run(command,env=env,stdout=subprocess.PIPE,
             stderr=subprocess.STDOUT,text=True,timeout=240,check=False)
        if cp.returncode:
            raise RuntimeError("V17 renderer failed for "+cam+": "+cp.stdout[-1800:])
        reports=list(dest.glob("*/summary.json"))
        assert len(reports)==1, "Missing V17 JSON report for "+cam
        summary=read(reports[0])
        cases=[x for x in summary["cases"] if x.get("backend")=="V17_D3D11_Raster"
               and x.get("status")=="ok"]
        assert len(cases)==1, "Missing raster record "+cam
        ms[cam]=float(cases[0]["meanMs"])
        png=reports[0].parent/"images"/("raster_"+cam+"_640x360_depth1.png")
        assert png.exists() and png.stat().st_size>1500, "Invalid PNG "+str(png)
        saved=out/"images"/(cam+".png")
        saved.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(png,saved)
        obs[cam]=image_stats(saved,cam)
        pictures[cam]=saved
    return pictures,obs,ms

def score_delta(name, before, after):
    cam=ACTIONS[name][0]
    a=before[cam]; b=after[cam]
    # Transparent feedback: target higher structural edges only for the
    # anatomy/vegetation/detail actions; lower saturation for restrained design.
    edge=b["edge"]-a["edge"]
    if name in ("tree_branching","tree_leaflets"):
        return (b["green"]-a["green"])*140 + edge*.03
    if name in ("flower_stems","flower_groundcover"):
        return (b["green"]-a["green"])*90 + edge*.04
    if name=="materials_lighting":
        return abs(a["contrast"]-28)-abs(b["contrast"]-28)
    return edge*.08 + (b["contrast"]-a["contrast"])*.015

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--engine",required=True)
    parser.add_argument("--scene",required=True)
    parser.add_argument("--output",required=True)
    parser.add_argument("--passes",type=int,default=10)
    args=parser.parse_args()
    root=Path(args.scene).resolve()
    exe=Path(args.engine).resolve()
    out=Path(args.output).resolve()
    assert exe.is_file() and (root/"scene.json").is_file()
    assert 1<=args.passes<=10
    out.mkdir(parents=True,exist_ok=True)
    fixed=Path(out/"base-scene")
    if fixed.exists():shutil.rmtree(fixed)
    shutil.copytree(root,fixed,ignore=shutil.ignore_patterns("*.png","v344-evening"))
    files=("instances/leger.json","materials/pripyat-reuse.json","lighting/day.json",
           "geometry/benchmark.json")
    original={p:read(fixed/p) for p in files}
    art_files=("textures/energetik_fresco_mosaic.jpg",
               "textures/personnage_facade_mosaique.jpg")
    art_digests=[sha(fixed/p) for p in art_files]
    accepted=fixed
    _,old_obs,old_ms=render(accepted,exe,out/"pass-00",fixed)
    previous=object_stats(original["instances/leger.json"]["instances"])
    results=[dict(pass_num=0,decision="BASELINE",observations=old_obs,
                  render_mean_ms=round(sum(old_ms.values())/5,2),objects=previous)]
    used=[]
    for number in range(1,args.passes+1):
        scores=defect_scores(old_obs,previous,used)
        if not scores: break
        action=next(iter(scores))
        used.append(action)
        candidate=out/("pass-%02d"%number)/"scene"
        shutil.copytree(accepted,candidate)
        data={p:read(candidate/p) for p in files}
        added=repair(data,action)
        validate(data,original["instances/leger.json"]["instances"])
        for path,obj in data.items():dump(candidate/path,obj)
        assert [sha(candidate/p) for p in art_files]==art_digests
        images,new_obs,new_ms=render(candidate,exe,out/("pass-%02d"%number),fixed)
        changed=sum(new_obs[k]["hash"]!=old_obs[k]["hash"] for k,_ in CAMERAS)
        before_time=sum(old_ms.values())/5; after_time=sum(new_ms.values())/5
        gain=score_delta(action,old_obs,new_obs)
        # Reject dramatic regressions and no-op images; allow small proxy
        # fluctuations, report the measured direction rather than claim artistry.
        keep=(changed>=1 and after_time<before_time*1.5
              and all(6<v["brightness"]<249 and v["contrast"]>2
                      for v in new_obs.values())
              and gain>-.75)
        review=dict(pass_num=number,defect=action,explanation=ACTIONS[action][1],
                    evidence=scores[action],object_changes=added,
                    changed_cameras=changed,proxy_delta=round(gain,4),
                    render_mean_ms=round(after_time,2),
                    decision="KEEP" if keep else "ROLLBACK",
                    observations=new_obs,
                    note="Heuristic visual proxies; a human must judge artistic fidelity")
        dump(out/("pass-%02d"%number)/"review.json",review)
        results.append(review)
        if keep:
            accepted=candidate; old_obs=new_obs; old_ms=new_ms
            previous=object_stats(data["instances/leger.json"]["instances"])
        print("PASS %02d/%02d %s %s | changed=%d gain=%+.4f mean=%.1fms"%
              (number,args.passes,action,review["decision"],changed,gain,after_time),
              flush=True)
        dump(out/"progress.json",dict(completed=number,results=results))
    with (out/"passes.csv").open("w",newline="",encoding="utf8") as fp:
        wr=csv.writer(fp);wr.writerow(("pass","issue","decision","changed_cameras",
              "proxy_delta","render_mean_ms","object_changes"))
        for row in results[1:]:
            wr.writerow((row["pass_num"],row["defect"],row["decision"],
                  row["changed_cameras"],row["proxy_delta"],
                  row["render_mean_ms"],row["object_changes"]))
    result=dict(passes_completed=len(results)-1,
      accepted=sum(x.get("decision")=="KEEP" for x in results),
      rolled_back=sum(x.get("decision")=="ROLLBACK" for x in results),
      fixed_engine=str(exe),accepted_scene=str(accepted),
      reference_art_digests=art_digests,
      warning="Automated measurements are not AI design approval. Inspect PNGs.",
      reviews=results)
    dump(out/"result.json",result)
    print("FIN: %d passes, %d kept, %d rollback"%
         (result["passes_completed"],result["accepted"],result["rolled_back"]),
         flush=True)
    assert result["passes_completed"]==args.passes, "Fewer than requested passes"

if __name__=="__main__":
    main()
