#!/usr/bin/env python3
"""V17.442: conservative scene fixes with immutable V17.440 baseline.
Changes are authored as NEW JSON assets; all underlying old versions preserved.
"""
import argparse,json
from pathlib import Path

def write(p,value):
 p.parent.mkdir(parents=True,exist_ok=True)
 p.write_text(json.dumps(value,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
def patch(root):
 scene=Path(root)/'benchmark-civic'
 inst=json.loads((scene/'instances/v440-audit.json').read_text(encoding='utf-8'))
 mats=json.loads((scene/'materials/v440-audit.json').read_text(encoding='utf-8'))
 manifest=json.loads((scene/'scene-v440-audit.json').read_text(encoding='utf-8'))
 # ANO-012: eliminate the entire six-metre wooden welcome counter by ID.
 # ANO-010: eliminate exactly three lockers intersecting the first flight
 # plus their matching six individual bodies/handles. No other lockers moved.
 remove={'v17_001539',
         'v17_001790','v17_001791',
         'v17_001792','v17_001793',
         'v17_001794','v17_001795'}
 known={x['id'] for x in inst['instances']}
 assert remove<=known,'Required objects missing: '+repr(remove-known)
 original_count=len(inst['instances'])
 inst['instances']=[x for x in inst['instances'] if x['id'] not in remove]
 assert len(inst['instances'])==original_count-len(remove)
 write(scene/'instances/v442-corrections.json',inst)
 # ANO-011: selectively warm up actual plaster surfaces of the pool/gallery.
 # Do not recolor structural steel, tiles, water, fresco or glass.
 new_colors={'prp_plaster_pale':[.76,.67,.53],
             'v439_ivory_plaster':[.88,.78,.62],
             'v439_gallery_warm':[.73,.67,.55]}
 changed=set()
 for mat in mats['materials']:
  if mat['id'] in new_colors:
   mat['baseColor']=new_colors[mat['id']]
   changed.add(mat['id'])
 assert changed==set(new_colors)
 write(scene/'materials/v442-corrections.json',mats)
 sectors=manifest['sectors'];assert len(sectors)==1
 sector=sectors[0]
 assert sector['instances']==['instances/v440-audit.json']
 assert sector['materials']==['materials/v440-audit.json']
 sector['instances']=['instances/v442-corrections.json']
 sector['materials']=['materials/v442-corrections.json']
 manifest['name']='CSE WARP V17.442 — anomalies de circulation et materiaux'
 write(scene/'scene-v442-corrections.json',manifest)
 print('V17.442 updated scenes: deleted 1 wood counter and 3 lockers plus handles; 3 plaster colors')
 return True
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--source',required=True)
 args=a.parse_args();patch(args.source)
