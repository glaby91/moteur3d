@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ===================================================================
echo CSE WARP - VULKAN vs DIRECTX11 / COLOR MANAGEMENT FIDELITY A/B
echo Legacy UNORM vs linear shader to sRGB render target
echo Same scene, camera, resolution, Z-prepass, textures, geometry.
echo sRGB is the default. Test only: compare with legacy UNORM.
echo ===================================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "vulkan\raster.frag.spv" goto missing
set "CSE_WARP_VK_FAST_FRAGMENT=0"
set "CSE_WARP_VK_BGRA_READBACK=0"
set "ERRORS="
for %%S in (pavillon01 material-lab pripyat-photo2scene-proof benchmark-civic) do (
 if exist "%%S\scene.json" (
  echo [%%S] Legacy RGBA UNORM baseline
  set "CSE_WARP_VK_SRGB_COLOR_TEST=0"
  call :legacy "%%S"
  echo [%%S] Hardware SRGB attachment, linear shader
  set "CSE_WARP_VK_SRGB_COLOR_TEST=1"
  call :srgb "%%S"
 ) else (
  echo ERROR Missing %%S\scene.json
  set "ERRORS=1"
 )
)
powershell -NoProfile -Command "$dirs=Get-ChildItem -Directory -Filter 'COLOR_*' | Select-Object -ExpandProperty FullName; $logs=Get-ChildItem -File -Filter 'color_*.log' | Select-Object -ExpandProperty FullName; $f=@($dirs)+@($logs); if($f.Count -eq 0){exit 1}; Compress-Archive -Path $f -DestinationPath 'CSE_WARP_VULKAN_SRGB_FIDELITE_HD620.zip' -Force"
if errorlevel 1 goto fail
echo.
echo DONE: CSE_WARP_VULKAN_SRGB_FIDELITE_HD620.zip
echo Send ZIP: compare MAE and heatmaps; default remains sRGB.
if defined ERRORS (
 echo WARNING: Some tests failed. Check archived logs.
 pause
 exit /b 4
)
pause
exit /b 0
:legacy
"v17_vulkan_ab_bench.exe" "%~1\scene.json" "COLOR_LEGACY_%~1" 12 1366 768 >"color_legacy_%~1.log" 2>&1
if errorlevel 1 set "ERRORS=1"
exit /b 0
:srgb
"v17_vulkan_ab_bench.exe" "%~1\scene.json" "COLOR_SRGB_%~1" 12 1366 768 >"color_srgb_%~1.log" 2>&1
if errorlevel 1 set "ERRORS=1"
exit /b 0
:missing
echo Missing executable or shaders. Extract entire artifact ZIP.
pause
exit /b 2
:fail
echo Error making ZIP. Logs are available in this folder.
pause
exit /b 3
