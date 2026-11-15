@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ========================================================
echo CSE WARP - DX11 / VULKAN 1.3.204+ RASTER A/B (HD620)
echo Vulkan loader and GPU must each support API 1.3.204 or higher.
echo ========================================================
if not exist "v17_vulkan_ab_bench.exe" (
  echo Missing v17_vulkan_ab_bench.exe. Extract entire build artifact.
  pause
  exit /b 1
)
if not exist "vulkan\raster.vert.spv" (
  echo Missing Vulkan SPIR-V shaders in vulkan\
  pause
  exit /b 1
)
set "SCENE=benchmark-civic\scene.json"
if not exist "%SCENE%" set "SCENE=pavillon01\scene.json"
if not exist "%SCENE%" (
  echo Missing benchmark-civic or pavillon01 scene. Copy a V17 scene beside the executable.
  pause
  exit /b 1
)
echo Scene: %SCENE%
echo GPU test: DirectX11 vs native Vulkan - identical camera
echo (compile/run time is not a claim of visual fidelity or equal image quality)
"v17_vulkan_gpu_smoke.exe" "vulkan_smoke.png"
if errorlevel 1 goto fail
"v17_vulkan_ab_bench.exe" "%SCENE%" "AB_VULKAN_HD620" 20 1366 768
if errorlevel 1 goto fail
powershell -NoProfile -Command "Compress-Archive -Path 'AB_VULKAN_HD620\*','vulkan_smoke.png' -DestinationPath 'CSE_WARP_VULKAN_RASTER_RESULTS.zip' -Force"
echo.
echo DONE: CSE_WARP_VULKAN_RASTER_RESULTS.zip
pause
exit /b 0
:fail
echo Vulkan rendering failed. Please send the console output or a screenshot.
pause
exit /b 1
