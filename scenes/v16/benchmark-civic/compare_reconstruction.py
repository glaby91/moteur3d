#!/usr/bin/env python3
"""Compare native V17.370 PNGs across the frozen-scene vs rebuilt scene.
The report documents visual differences and WARP timings, without falsely
claiming that a numeric score establishes aesthetic quality.
"""
import csv, json, pathlib, sys
from PIL import Image,ImageChops,ImageStat,ImageDraw,ImageFont

root=pathlib.Path(sys.argv[1])
cameras=["facade","passerelle","vue_haut","piscine_soir","hall"]
rows=[]
pair=[]
canvas=Image.new("RGB",(1280,5*375),(28,29,33))
dr=ImageDraw.Draw(canvas)
for i,cam in enumerate(cameras):
    src=[]
    for variant in ("previous","reconstructed"):
        folder=root/variant/cam
        png=list(folder.glob("**/images/raster_"+cam+"_640x360_depth1.png"))
        rep=list(folder.glob("**/summary.json"))
        if len(png)!=1 or len(rep)!=1:raise RuntimeError("Missing native "+variant+" render "+cam)
        im=Image.open(png[0]).convert("RGB")
        data=json.loads(rep[0].read_text("utf-8"))
        cases=[x for x in data["cases"] if x.get("backend")=="V17_D3D11_Raster" and x.get("status")=="ok"]
        if len(cases)!=1:raise RuntimeError("Invalid raster result for "+cam)
        src.append((im,float(cases[0]["meanMs"])))
    old,new=src
    pixdiff=ImageChops.difference(old[0],new[0])
    # Count meaningful changed RGB pixels (max-channel delta > 5).
    changed=sum(max(rgb)>5 for rgb in pixdiff.getdata())/(640*360)
    def entropy(im):
        hist=im.convert("L").histogram();n=sum(hist)
        import math
        return -sum((x/n)*math.log2(x/n) for x in hist if x)
    rows.append({"camera":cam,"old_mean_ms":round(old[1],2),
                 "new_mean_ms":round(new[1],2),
                 "mean_ratio":round(new[1]/max(.01,old[1]),3),
                 "changed_pixels_pct":round(changed*100,2),
                 "old_entropy":round(entropy(old[0]),3),
                 "new_entropy":round(entropy(new[0]),3)})
    y=i*375
    canvas.paste(old[0],(0,y+15));canvas.paste(new[0],(640,y+15))
    dr.text((10,y),cam+" — ORIGINAL / V17.370",fill=(241,241,243))
    dr.text((653,y),cam+" — SCÈNE RECONSTRUITE / même moteur",fill=(241,241,243))
    pair.append((cam,changed))
root.mkdir(exist_ok=True,parents=True)
with (root/"comparison.csv").open("w",newline="",encoding="utf8") as f:
    wr=csv.DictWriter(f,fieldnames=list(rows[0]));wr.writeheader();wr.writerows(rows)
(root/"comparison.json").write_text(json.dumps({
    "engine":"V17.370 fixed binary / D3D11 WARP",
    "note":"Only the scene was modified; pixel deltas do not establish beauty.",
    "views":rows},indent=2,ensure_ascii=False),encoding="utf8")
canvas.save(root/"comparaison_avant_apres.png")
print(json.dumps(rows,ensure_ascii=False,indent=2))
if not all(row["changed_pixels_pct"]>.2 for row in rows[:3]):
    print("WARNING: one of the key exterior cameras changed very little")
