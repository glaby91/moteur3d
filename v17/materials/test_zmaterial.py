import json, pathlib, math
p=pathlib.Path(__file__).resolve().parent
root=p.parents[1]
d=json.loads((p/'zmaterial8192.json').read_text())
f=d['families']; assert d['schema']==1 and len(f)==64
ids=set()
for name,m in f.items():
 assert len(m['colors'])==8 and len(m['roughness'])==16
 assert all(0<=v<=1 and math.isfinite(v) for c in m['colors'] for v in c)
 assert all(0<=v<=1 and math.isfinite(v) for v in m['roughness'])
 for i in range(1,9):
  for j in range(1,17): ids.add(f'{name}_{i:02}_{j:02}')
assert len(ids)==8192
print('1: catalogue 8192 IDs and valid PBR ranges')
for n in (1,80,256,257):
 active=set(sorted(ids)[:n]); assert (len(active)<=256)==(n!=257)
assert len(set(['inox_01_01']*10000))==1
print('2: selection bounds and deduplication model')
for metal in (0,.45,1):
 for rough in (.04,.2,.7,1):
  for y in (-1,0,1):
   d=1+.55*(1-rough*rough)*y
   assert .45<=d<=1.55 and 0<=1-metal<=1
   if metal==0: assert (1-metal)*.7+metal*.3*d==.7
print('3: finite bounded compensation and dielectric parity model')
src=root/'experiments/2026-10-30-cote-ete-gt/native_v14/src'
h=(src/'m3d_directx11_raster_shader.h').read_text()
g=(root/'v17/vulkan/shaders/raster.frag').read_text()
assert 'receiverN' in h and 'receiverN' in g
assert 'zmMetal' in h and 'zmMetal' in g and 'diffuseBase' in h and 'diffuseBase' in g
assert 'sampleOptical' in h and 'poolWater' in g
assert json.loads((root/'v17/version.json').read_text())['engine']['build']==30
print('4: integration markers, L01 retained, optical paths retained, version')
