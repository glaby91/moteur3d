#!/usr/bin/env python3
"""Summarize real CSE WARP V17.443 DX11 WARP CLI benchmarks.
Never infer the user's Intel HD620 performance from software WARP samples.
"""
import csv,json,statistics,sys
from pathlib import Path
root=Path(sys.argv[1])
runs=[]
for setting in ('MSAA1_ANISO1','MSAA2_ANISO8','MSAA4_ANISO16'):
    for repeat in (1,2):
        folder=root/f'{setting}_R{repeat}'
        summaries=list(folder.rglob('summary.json'))
        if len(summaries)!=1:
            raise RuntimeError(f'Expected one summary.json in {folder}, got {len(summaries)}')
        report=json.loads(summaries[0].read_text(encoding='utf-8-sig'))
        for item in report.get('cases',[]):
            if not isinstance(item,dict):continue
            if item.get('backend') not in ('DirectX11 Raster','DirectX11 raster','DX11 Raster','GPU DirectX11 raster') and 'raster' not in str(item.get('backend','')).lower():
                continue
            if item.get('status')!='ok':continue
            entry={'configuration':setting,'repeat':repeat,'source':str(summaries[0].relative_to(root)),
                   'backend':item.get('backend'),'camera':item.get('camera'),
                   'width':item.get('width'),'height':item.get('height'),
                   'frames':item.get('frames'),'meanMs':item.get('meanMs'),
                   'medianMs':item.get('medianMs'),'p95Ms':item.get('p95Ms'),
                   'p99Ms':item.get('p99Ms'),'estimatedFps':item.get('estimatedFps'),
                   'gpuDrawMs':item.get('gpuDrawMs'),'gpuTransferMs':item.get('gpuTransferMs'),
                   'cpuSubmitMs':item.get('cpuSubmitMs'),'cpuMapWaitMs':item.get('cpuMapWaitMs'),
                   'cpuPixelConvertMs':item.get('cpuPixelConvertMs'),
                   'gpuTimingValid':item.get('gpuTimingValid'),
                   'submittedTriangles':item.get('submittedTriangles'),
                   'gpuDrawCalls':item.get('gpuDrawCalls'),'readbackDepth':item.get('readbackDepth'),
                   'status':item.get('status')}
            runs.append(entry)
if len(runs)!=6:raise RuntimeError(f'Expected 6 real GPU raster samples, got {len(runs)}: {runs}')
with (root/'measures.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.DictWriter(f,fieldnames=runs[0].keys());w.writeheader();w.writerows(runs)
groups=[]
for setting in ('MSAA1_ANISO1','MSAA2_ANISO8','MSAA4_ANISO16'):
    rows=[x for x in runs if x['configuration']==setting]
    if len(rows)!=2:raise RuntimeError('Expected 2 repetitions for '+setting)
    means=[float(x['meanMs']) for x in rows]
    groups.append({'configuration':setting,'repetitions':2,
                   'framesPerRepetition':[x['frames'] for x in rows],
                   'meanFrameMs':statistics.mean(means),
                   'minFrameMs':min(means),'maxFrameMs':max(means),
                   'estimatedFpsByMeanMs':1000/statistics.mean(means),
                   'averageGpuDrawMs':statistics.mean(float(x['gpuDrawMs'] or 0) for x in rows),
                   'gpuTimingValidInBoth':all(x['gpuTimingValid'] for x in rows)})
result={'title':'CSE WARP V17.443 - real DirectX 11 WARP software raster benchmark',
        'warning':'Hosted CI virtual CPU and D3D11 WARP SOFTWARE adapter, not an Intel HD 620 hardware performance benchmark.',
        'scene':'V17.443 packaged benchmark-civic/scene.json, underlying V17.442 corrections',
        'resolution':'1366x768','readbackDepth':1,
        'configurations':groups,'rawRuns':runs}
(root/'comparison.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(groups,indent=2))
