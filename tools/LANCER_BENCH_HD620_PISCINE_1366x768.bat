@echo off
setlocal EnableExtensions
cd /d "%~dp0"
set "M3D_V16_FORCE_WARP=0"
set "M3D_V17_EXPERIMENT_Z_PREPASS=0"
set "M3D_V17_EXPERIMENT_LIGHT_AABB_CULL=0"
set "ROOT=RESULTATS_BENCH_HD620_1366x768"
if not exist "moteur3d_v17_benchmark.exe" (
  echo ERREUR: moteur3d_v17_benchmark.exe absent.
  pause
  exit /b 1
)
if not exist "benchmark-civic\scene.json" (
  echo ERREUR: la scene de la V17.443 est absente.
  pause
  exit /b 1
)
echo CSE WARP 17.444 — DX11 GPU HARDWARE - BASSE RESOLUTION PERIPHERIQUE
echo Scene approuvee V17.443 ; camera v4_piscine ; 1366x768 ; MSAA 1
echo 3 passages BASELINE et MULTIRES - meme resolution et meme vue.
echo.
for %%N in (1 2 3) do (
  echo ========== PASSAGE %%N / 3 ==========
  set "M3D_V17_PERIPHERAL_MULTIREZ=0"
  echo [Reference] %%N
  "moteur3d_v17_benchmark.exe" "benchmark-civic\scene.json" "%ROOT%\reference-%%N" --quick --camera-id v4_piscine --hour 19 --raster-msaa 1
  if errorlevel 1 goto :failed
  set "M3D_V17_PERIPHERAL_MULTIREZ=1"
  echo [Multires] %%N
  "moteur3d_v17_benchmark.exe" "benchmark-civic\scene.json" "%ROOT%\multires-%%N" --quick --camera-id v4_piscine --hour 19 --raster-msaa 1
  if errorlevel 1 goto :failed
)
echo.
echo TERMINE. Comparer gpuDrawMs dans summary.json et les rendus PNG.
echo Archive des resultats en cours...
powershell -NoProfile -Command "Compress-Archive -Path '%ROOT%\*' -DestinationPath '%ROOT%.zip' -Force"
if errorlevel 1 echo ATTENTION: ZIP non cree, mais dossier de resultats disponible.
echo.
echo ENVOYER CE FICHIER POUR ANALYSE: %ROOT%.zip
pause
exit /b 0
:failed
echo ERREUR: benchmark GPU interrompu, consulter le dossier %ROOT%.
pause
exit /b 2
