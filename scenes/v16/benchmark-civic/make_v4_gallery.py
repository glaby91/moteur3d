#!/usr/bin/env python3
"""Native V17.370 eight-camera final gallery: contact sheet and CSV measurements."""
import argparse,csv,json,math
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

def load(p):
    return json.loads(p.read_text(encoding='utf8'))

def font(sz):
    for name in ('C:/Windows/Fonts/segoeui.ttf','C:/Windows/Fonts/arial.ttf'):
        try:return ImageFont.truetype(name,sz)
        except OSError:pass
    return ImageFont.load_default()

def main(args):
    folder=Path(args.output)
    cameras=load(Path(args.root)/'cameras/v4.json')['cameras']
    budget=load(Path(args.root)/'stats/v4-budget.json')
    tile_w,tile_h=960,540
    caption_h=63
    gap=9
    cols=2
    rows=math.ceil(len(cameras)/cols)
    sheet=Image.new('RGB',(cols*(tile_w+gap)+gap,rows*(tile_h+caption_h+gap)+gap),(17,24,30))
    draw=ImageDraw.Draw(sheet)
    titlefont=font(22);smallfont=font(17)
    metrics=[]
    for i,cam in enumerate(cameras):
        img=folder/(cam['id']+'.png')
        if not img.is_file():raise RuntimeError('Missing native view '+str(img))
        rendered=Image.open(img).convert('RGB')
        if rendered.width!=1366 or rendered.height!=768:
            raise RuntimeError('Wrong native resolution '+str(img))
        reported=list((folder/'raw'/cam['id']).glob('*/summary.json'))
        if len(reported)!=1:raise RuntimeError('Missing native benchmark '+cam['id'])
        content=load(reported[0])
        case=[r for r in content['cases'] if r.get('backend')=='V17_D3D11_Raster'
              and r.get('camera')==cam['id'] and r.get('status')=='ok']
        if len(case)!=1:raise RuntimeError('Invalid GPU result '+cam['id'])
        case=case[0]
        x=gap+(i%cols)*(tile_w+gap)
        y=gap+(i//cols)*(tile_h+caption_h+gap)
        resized=rendered.resize((tile_w,tile_h),Image.Resampling.LANCZOS)
        sheet.paste(resized,(x,y))
        draw.text((x+12,y+tile_h+8),cam['name'],fill=(244,240,230),font=titlefont)
        draw.text((x+12,y+tile_h+37),
                  f"V17.370 WARP réel  |  {int(case.get('submittedTriangles',0)):,} triangles soumis  |  {float(case.get('meanMs',0)):.1f} ms",
                  fill=(182,203,205),font=smallfont)
        metrics.append(dict(camera=cam['id'],caption=cam['name'],
              mean_ms=round(float(case.get('meanMs',0)),2),
              submitted_triangles=int(case.get('submittedTriangles',0)),
              gpu_boxes=int(case.get('gpuBoxInstances',0)),
              gpu_ellipsoids=int(case.get('gpuEllipseInstances',0)),
              gpu_foliage=int(case.get('gpuFoliageInstances',0))))
    sheet.save(folder/'V4_8_vues_reelles.png',optimize=True)
    with (folder/'performance_8_vues.csv').open('w',newline='',encoding='utf-8-sig') as out:
        wr=csv.DictWriter(out,fieldnames=list(metrics[0]))
        wr.writeheader();wr.writerows(metrics)
    report={
      'engine':'CSE WARP V17.370 frozen Windows DX11 WARP',
      'source_triangle_budget':budget['v4_triangles'],
      'source_objects':budget['v4_objects'],
      'note':'Source triangle count differs from frustum-culling/LOD submitted triangles. These are true rendered engine PNGs, not generated design mockups.',
      'views':metrics}
    (folder/'rapport_final_v4.json').write_text(
        json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    print('GALLERY READY',folder/'V4_8_vues_reelles.png')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--root',default='scenes/v16/benchmark-civic')
    p.add_argument('--output',default='results/v4-renders')
    main(p.parse_args())
