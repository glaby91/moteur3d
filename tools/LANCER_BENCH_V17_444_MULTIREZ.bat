@echo off
setlocal
cd /d "%~dp0"
if not exist moteur3d_v17_benchmark.exe (
 echo ERREUR : moteur3d_v17_benchmark.exe introuvable dans ce dossier.
 pause
 exit /b 1
)
if not exist benchmark-civic\scene.json (
 echo ERREUR : scene benchmark-civic\scene.json introuvable.
 pause
 exit /b 1
)
set M3D_V16_FORCE_WARP=0
set M3D_V17_PERIPHERAL_MULTIREZ=0
echo [1/2] Reference - GPU D3D11 - WARP logiciel desactive
moteur3d_v17_benchmark.exe benchmark-civic\scene.json resultats-multires\reference --smoke --raster-msaa 1
if errorlevel 1 goto :err
set M3D_V17_PERIPHERAL_MULTIREZ=1
echo [2/2] Rendu multires experimental - meme scene, meme camera
moteur3d_v17_benchmark.exe benchmark-civic\scene.json resultats-multires\multires --smoke --raster-msaa 1
if errorlevel 1 goto :err
echo.
echo RESULTATS: resultats-multires\reference et resultats-multires\multires
echo Comparer gpuDrawMs, meanMs, rendu PNG. Refaire plusieurs cycles sur HD620.
pause
exit /b 0
:err
echo ECHEC du benchmark. Pas de validation de gain.
pause
exit /b 2
