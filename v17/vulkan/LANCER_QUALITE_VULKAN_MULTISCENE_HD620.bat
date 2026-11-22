@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo =================================================================
echo CSE WARP V17 VULKAN - FIDELITE DX11 MULTI-SCENES - Intel HD620
echo Les scenes sont des tests uniquement. Aucune optimisation par scene.
echo Z-prepass Vulkan permanent, sans comparaison automatique.
echo 12 images x 4 scenes, en 1366 x 768. Heatmap DX11 / Vulkan.
echo =================================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "vulkan\raster.vert.spv" goto missing
set "CSE_WARP_VK_FAST_FRAGMENT=0"
set "CSE_WARP_VK_BGRA_READBACK=0"
set "CSE_WARP_VK_DEPTH_PREPASS="
set "ERRORS="
for %%S in (pavillon01 material-lab pripyat-photo2scene-proof benchmark-civic) do (
  if exist "%%S\scene.json" (
    echo Checking scene %%S
    "v17_vulkan_ab_bench.exe" "%%S\scene.json" "QUALITE_%%S" 12 1366 768 >"quality_%%S.log" 2>&1
    if errorlevel 1 (
      echo ERROR scene %%S - see quality_%%S.log
      set "ERRORS=1"
    )
  ) else (
    echo ERROR scene missing: %%S\scene.json
    set "ERRORS=1"
  )
)
powershell -NoProfile -Command "$files=@('QUALITE_pavillon01','QUALITE_material-lab','QUALITE_pripyat-photo2scene-proof','QUALITE_benchmark-civic'); $files+=Get-ChildItem -Name 'quality_*.log'; $files=$files | Where-Object { Test-Path $_ }; if($files.Count -gt 0){Compress-Archive -Path $files -DestinationPath 'CSE_WARP_VULKAN_FIDELITE_MULTISCENE_HD620.zip' -Force}else{exit 1}"
if errorlevel 1 goto fail
echo.
echo DONE: CSE_WARP_VULKAN_FIDELITE_MULTISCENE_HD620.zip
echo This archive contains DX11/Vulkan captures, heatmaps, metrics and logs.
if defined ERRORS (
  echo WARNING: One or more scenes failed. Send ZIP to diagnose.
  pause
  exit /b 4
)
pause
exit /b 0
:missing
echo Missing executable or shaders. Extract whole CI artifact.
pause
exit /b 2
:fail
echo Error writing diagnostics archive. Keep logs.
pause
exit /b 3
