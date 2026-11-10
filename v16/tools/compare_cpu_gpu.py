#!/usr/bin/env python3
"""CSE WARP V16: fixed-camera CPU GI vs GPU WARP image comparator.

Not a beauty score; records objective differences without silently
brightening GPU or claiming full path-tracing equivalence. Python needs
numpy and Pillow. PNG references are renderer output, not committed assets.
"""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


REGIONS = {
    # Normalized image crops for Pavillon 01 camera 01, Full HD.
    # Regions overlap intentionally and measure different major materials.
    "toiture_et_acrotere": (.25, .17, .89, .49),
    "facade_et_vitrages": (.28, .46, .84, .88),
    "terrasse_bois": (.20, .72, .70, .95),
    "bassin": (.23, .84, .57, .995),
    "jardin": (.56, .80, .98, .985),
    "scene_complete": (0., 0., 1., 1.),
}


def metrics(cpu, gpu):
    a=cpu.astype(np.float64)/255.
    b=gpu.astype(np.float64)/255.
    d=a-b
    mae=float(np.abs(d).mean())
    mse=float(np.mean(d*d))
    luminance=np.array([.2126,.7152,.0722])
    lum_a=a@luminance
    lum_b=b@luminance
    return {
        "mae_rgb_255": round(mae*255,4),
        "psnr_db": round(-10*math.log10(max(mse,1e-20)),3),
        "mean_rgb_cpu_255": [round(float(v),2) for v in cpu.mean(axis=(0,1))],
        "mean_rgb_gpu_255": [round(float(v),2) for v in gpu.mean(axis=(0,1))],
        "mean_luminance_cpu": round(float(lum_a.mean()),6),
        "mean_luminance_gpu": round(float(lum_b.mean()),6),
        "luminance_bias_gpu_minus_cpu": round(float((lum_b-lum_a).mean()),6),
        "fraction_rgb_difference_le_15": round(float(
            np.mean(np.max(np.abs(cpu.astype(np.int16)-gpu.astype(np.int16)),axis=2)<=15)
        ),5),
    }


def compare(cpu_file, gpu_file, outdir):
    c=np.asarray(Image.open(cpu_file).convert("RGB"))
    g=np.asarray(Image.open(gpu_file).convert("RGB"))
    if c.shape!=g.shape:
        raise ValueError(f"CPU {c.shape} vs GPU {g.shape}; camera/resolution mismatch")
    height,width,_=c.shape
    report={
        "method":"Direct pixel comparison, same scene/camera. Does NOT establish physical accuracy.",
        "cpu_reference":"CPU GI, 2 bounces, 4 spp in GitHub tests; user reference at 16 spp",
        "gpu_reference":"D3D11 WARP, one primary sample + configured 0..4 cosine-weighted GPU secondary samples; not exact CPU GI",
        "resolution":[width,height],
        "regions":{}
    }
    for name,(x0,y0,x1,y1) in REGIONS.items():
        l=max(0,int(x0*width));r=min(width,int(x1*width))
        t=max(0,int(y0*height));b=min(height,int(y1*height))
        if r<=l or b<=t:raise ValueError("Empty region "+name)
        report["regions"][name]={"rect_px":[l,t,r,b],
                                  **metrics(c[t:b,l:r],g[t:b,l:r])}
    outdir.mkdir(parents=True,exist_ok=True)
    (outdir/"cpu_gpu_comparison.json").write_text(
        json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf8")
    # Contrast-enhanced absolute error for debugging, never substituted
    # for a physically meaningful luminance/exposure comparison.
    diff=np.clip(np.abs(c.astype(np.int16)-g.astype(np.int16))*3,0,255).astype(np.uint8)
    Image.fromarray(diff).save(outdir/"difference_x3.png")
    preview_width=960
    ims=[]
    for original in [c,g,diff]:
        img=Image.fromarray(original)
        img=img.resize((preview_width,preview_width*height//width),Image.Resampling.BILINEAR)
        ims.append(img)
    sheet=Image.new("RGB",(preview_width*3,preview_width*height//width+38),"white")
    d=ImageDraw.Draw(sheet)
    for i,(label,img) in enumerate(zip(("CPU GI (4 spp)","GPU D3D11 WARP","absolute error x3"),ims)):
        d.text((i*preview_width+12,12),label,fill=(10,10,10))
        sheet.paste(img,(i*preview_width,38))
    sheet.save(outdir/"cpu_gpu_triptych.jpg",quality=91)
    print(json.dumps(report["regions"]["scene_complete"],indent=2))
    return report


if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("cpu_png",type=Path)
    ap.add_argument("gpu_png",type=Path)
    ap.add_argument("outdir",type=Path)
    args=ap.parse_args()
    compare(args.cpu_png,args.gpu_png,args.outdir)
