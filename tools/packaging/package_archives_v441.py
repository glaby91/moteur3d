#!/usr/bin/env python3
"""Archive assets excluded from clean V17.441 user distribution.
Archives never execute or replace application files. The source is preserved.
"""
import argparse,hashlib,json,shutil
from pathlib import Path

def archive(source,clean,output):
 source=Path(source).resolve();clean=Path(clean).resolve();output=Path(output).resolve()
 if output.exists():shutil.rmtree(output)
 output.mkdir(parents=True)
 skipped={'moteur3d_v17.exe','moteur3d_v16_core.dll',
          'moteur3d_v17_benchmark.exe','moteur3d_v17_benchui.exe',
          'moteur3d_v17_campaign.exe','moteur3d_v17_gibake.exe',
          'moteur3d_v17_gpu_trajectory.exe'}
 dirs=('audio/','shaders/','platforms/','imageformats/','styles/','tls/',
       'generic/','iconengines/','networkinformation/')
 copies=[]
 for p in source.rglob('*'):
  if not p.is_file():continue
  rel=p.relative_to(source).as_posix()
  if rel.split('/')[-1] in skipped or rel.startswith(dirs) or rel.startswith('Qt6'):continue
  if (clean/rel).exists():continue
  dst=output/rel;dst.parent.mkdir(parents=True,exist_ok=True)
  shutil.copy2(p,dst);copies.append(rel)
 (output/'A-LIRE-ARCHIVES.txt').write_text(
   'CSE WARP V17.441 - anciennes ressources et benchmarks non necessaires\n'
   'Ne PAS recopier ces fichiers par-dessus le dossier utilisateur.\n'
   'Le code source et les versions historiques restent aussi sur GitHub.\n'
   'Cette archive permet de consulter les references sans multiplier les EXE\n'
   'dans le dossier de lancement de CSE WARP.\n',encoding='utf-8')
 (output/'INVENTAIRE.json').write_text(json.dumps(
   {'version':'17.441','archivedCount':len(copies),'archivedFiles':copies},
   ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('Archived historical files',len(copies))
 return len(copies)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--source',required=True)
 p.add_argument('--clean',required=True);p.add_argument('--dest',required=True)
 a=p.parse_args();archive(a.source,a.clean,a.dest)
