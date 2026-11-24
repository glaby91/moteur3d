#!/usr/bin/env python3
"""Assemble a clean CSE WARP V17.441-V17.443 release package.

Never modify the source portable. Copy only the transitive dependencies of the
selected scene manifest, fresh matching-version EXEs and Qt runtime.
Fail closed on a missing resource or a second main engine executable.
"""
import argparse,hashlib,json,shutil
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

SCENE='benchmark-civic'
SYSTEM_FILES=('moteur3d_v17.exe','moteur3d_v16_core.dll','cse_warp_v17.ico',
              'cse_warp_v17_bench.ico','D3Dcompiler_47.dll','opengl32sw.dll')
TOOLS={
 'moteur3d_v17_benchui.exe':'outils/benchmark/moteur3d_v17_benchui.exe',
 'moteur3d_v17_benchmark.exe':'outils/benchmark/moteur3d_v17_benchmark.exe',
 'moteur3d_v17_campaign.exe':'outils/benchmark/moteur3d_v17_campaign.exe',
 'moteur3d_v17_gpu_trajectory.exe':'outils/benchmark/moteur3d_v17_gpu_trajectory.exe',
 'moteur3d_v17_gibake.exe':'outils/gi/moteur3d_v17_gibake.exe'
}
QT_PLUGIN_DIRS=('platforms','imageformats','styles','tls','generic',
                'iconengines','networkinformation')


@dataclass(frozen=True)
class Release:
    scene_manifest: str
    scene_name: str
    scene_source: str
    source_architecture: str


RELEASES={
 441: Release(
     'scene-v440-audit.json',
     'CSE WARP V17.441 — Palais, mosaïque et accès',
     'V8 corrigée V17.440',
     'V8 validated through V17.440'),
 442: Release(
     'scene-v442-corrections.json',
     'CSE WARP V17.442 — Palais, mosaïque et corrections',
     'V8 avec corrections V17.442',
     'V8 geometry plus V17.442 conservative corrections'),
 443: Release(
     'scene-v442-corrections.json',
     'CSE WARP V17.443 — Palais, mosaïque et corrections',
     'V8 avec corrections V17.442',
     'V8 geometry plus V17.443 conservative corrections')
}


def jsonread(p):
 return json.loads(p.read_text(encoding='utf-8-sig'))


def checked_copy(src, target):
 if not src.is_file():raise FileNotFoundError(src)
 target.parent.mkdir(parents=True,exist_ok=True)
 shutil.copy2(src,target)


def clean_relative(s):
 p=PurePosixPath(s)
 if p.is_absolute() or '..' in p.parts or not p.parts:raise ValueError('unsafe resource: '+s)
 return p.as_posix()


def build(source,dest,revision,build_number):
 try:
  release=RELEASES[build_number]
 except KeyError as exc:
  raise ValueError(f'Unsupported clean package build: {build_number}') from exc
 source=Path(source).resolve(); dest=Path(dest).resolve()
 if source==dest or source in dest.parents or dest in source.parents:
  raise ValueError('source and output must be independent')
 if dest.exists():shutil.rmtree(dest)
 dest.mkdir(parents=True)
 # Validate all compiled binaries are supplied by CI, never silently re-use
 # an old 17.417 or 17.437 benchmark executable from the base portable.
 for f in SYSTEM_FILES: checked_copy(source/f,dest/f)
 for dll in sorted(source.glob('Qt6*.dll')):checked_copy(dll,dest/dll.name)
 assert len(list(dest.glob('Qt6*.dll')))>=3,'Qt deployment incomplete'
 for folder in QT_PLUGIN_DIRS:
  src=source/folder
  if src.is_dir():shutil.copytree(src,dest/folder)
 for f,output in TOOLS.items():checked_copy(source/f,dest/output)
 for folder in ('audio','shaders'):
  shutil.copytree(source/folder,dest/folder)
 assert (dest/'audio/pripyat_radio.wav').is_file()
 # Copy only the resources referenced by this release's source manifest.
 scene_root=source/SCENE
 manifest=jsonread(scene_root/release.scene_manifest)
 assert len(manifest['sectors'])==1
 manifest['name']=release.scene_name
 selected=set()
 for sector in manifest['sectors']:
  for field in ('geometry','instances','materials','pbr','lighting','cameras','environment'):
   for rel in sector.get(field,[]):selected.add(clean_relative(rel))
  for rel in sector.get('pbr',[]):
   pbr=jsonread(scene_root/clean_relative(rel))
   for tex in pbr.get('sets',[]):
    image=tex.get('imageFile')
    if image:selected.add(clean_relative(image))
 assert len(selected)>=85,('Unexpected missing scene dependencies',len(selected))
 for relative in sorted(selected):
  checked_copy(scene_root/relative,dest/SCENE/relative)
 (dest/SCENE/'scene.json').write_text(
     json.dumps(manifest,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
 # Do not rename benchmark-civic: pool-ladder logic currently identifies it
 # by scene path. Historical manifests remain in the source repository.
 (dest/'BUILD_NUMBER.txt').write_text(str(build_number)+'\n',encoding='ascii')
 (dest/'REVISION.txt').write_text(revision.strip()+'\n',encoding='ascii')
 (dest/'LANCER_CSE_WARP.bat').write_text(
     '@echo off\r\ncd /d "%~dp0"\r\nstart "" "%~dp0moteur3d_v17.exe" "%~dp0benchmark-civic\\scene.json"\r\n',encoding='ascii')
 (dest/'LANCER_BENCHMARK.bat').write_text(
     '@echo off\r\nset "PATH=%~dp0;%PATH%"\r\nset "QT_PLUGIN_PATH=%~dp0"\r\ncd /d "%~dp0"\r\n'
     'start "" "%~dp0outils\\benchmark\\moteur3d_v17_benchui.exe" "%~dp0benchmark-civic\\scene.json"\r\n',
     encoding='ascii')
 (dest/'README.txt').write_text(
    f'CSE WARP Beta X V17.{build_number} - package utilisateur propre\n'
    'Demarrer avec LANCER_CSE_WARP.bat ou moteur3d_v17.exe.\n'
    f'Scene unique : benchmark-civic/scene.json ({release.scene_source}).\n'
    'Benchmarks : LANCER_BENCHMARK.bat ; archives et anciennes scenes exclues.\n'
    'La DLL moteur3d_v16_core.dll est le backend necessaire a V17, pas une ancienne application.\n'
    'Les donnees historiques sont preservees dans le depot GitHub et les anciens artefacts CI.\n'
    'Tests sur Intel HD 620 a verifier sur la machine cible.\n',
    encoding='utf-8')
 entries={}
 for p in sorted(dest.rglob('*')):
  if p.is_file():
   rel=p.relative_to(dest).as_posix()
   entries[rel]=dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
 version=f'17.{build_number}'
 record={'product':'CSE WARP Beta X','version':version,
         'sourceArchitecture':release.source_architecture,'commit':revision,
         'scene':'benchmark-civic/scene.json',
         'totalFiles':len(entries),'sceneResources':len(selected),
         'files':entries}
 (dest/'RELEASE-MANIFEST.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 assert set(p.name for p in dest.glob('*.exe'))=={'moteur3d_v17.exe'}
 assert not (dest/'benchmark-civic/scene-v4.json').exists()
 assert not list(dest.rglob('BASELINE-*.exe'))
 assert not list(dest.rglob('*.ps1'))
 assert len(TOOLS)==len(list(dest.rglob('moteur3d_v17_*.exe')))
 assert (dest/SCENE/'textures/energetik_fresco_mosaic.jpg').exists()
 print(json.dumps({'version':version,'files':len(entries)+1,
                   'sceneResources':len(selected),
                   'totalBytes':sum(v['bytes'] for v in entries.values()),
                   'onlyMainExe':[p.name for p in dest.glob('*.exe')]},indent=2))
 return dest


if __name__=='__main__':
 a=argparse.ArgumentParser()
 a.add_argument('--build-number',required=True,type=int,choices=RELEASES)
 a.add_argument('--source',required=True)
 a.add_argument('--dest',required=True)
 a.add_argument('--revision',required=True)
 p=a.parse_args()
 build(p.source,p.dest,p.revision,p.build_number)
