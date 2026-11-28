import json, pathlib, sys
import numpy as np
from PIL import Image
ref, android, out = map(pathlib.Path,sys.argv[1:])
out.mkdir(parents=True,exist_ok=True)
a=json.loads((ref/"views.json").read_text()); b=json.loads((android/"views.json").read_text())
assert len(a)==len(b)==5, "camera count mismatch"
report=[]
for r,t in zip(a,b):
    assert all(r[k]==t[k] for k in ("id","file","name")), "camera identity mismatch"
    for k in ("eye","target","fov"):
        assert np.allclose(r[k],t[k],rtol=0,atol=1e-5), ("pose mismatch",k,r[k],t[k])
    name=r["file"]
    x=Image.open(ref/name).convert("RGB"); y=Image.open(android/name).convert("RGB")
    assert x.size==y.size==(640,360)
    xx=np.asarray(x,dtype=np.float32); yy=np.asarray(y,dtype=np.float32)
    d=np.abs(xx-yy)
    mae=float(d.mean()); rmse=float(np.sqrt(((xx-yy)**2).mean()))
    item={**r,"mae_8bit":mae,"rmse_8bit":rmse,"pixels_delta_gt_16_pct":float((d.max(axis=2)>16).mean()*100),
          "linux_mean":float(xx.mean()),"android_mean":float(yy.mean())}
    report.append(item)
    panel=Image.new("RGB",(1920,360))
    panel.paste(x,(0,0));panel.paste(y,(640,0))
    panel.paste(Image.fromarray(np.clip(d*4,0,255).astype(np.uint8)),(1280,0))
    panel.save(out/name)
    print(r["id"],"MAE",mae,"RMSE",rmse,"pixel delta >16 %",item["pixels_delta_gt_16_pct"])
(out/"comparison.json").write_text(json.dumps(report,indent=2))
(out/"README.txt").write_text("Left: Linux Lavapipe; middle: Android SwiftShader; right: absolute RGB difference x4. Same scene, camera, 640x360, 1024 texture setting, 14h, MSAA1. Driver differences measured, not automatically certified.\n")
