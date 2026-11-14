@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ================================================================
echo CSE WARP Vulkan - OPAQUE Z-PREPASS A/B - ALL SCENES
echo Vulkan original vs generic Z-prepass ABBA, same quality.
echo Per-frame checksums must be identical before any activation.
echo ================================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "vulkan\raster_fast.frag.spv" goto missing
set "SCENE=benchmark-civic\scene.json"
if not exist "%SCENE%" set "SCENE=pavillon01\scene.json"
if not exist "%SCENE%" goto missing
set "CSE_WARP_VK_FAST_FRAGMENT=1"
set "CSE_WARP_VK_BGRA_READBACK=0"
set "CSE_WARP_VK_DEPTH_PREPASS=0"
echo [1/4] Without prepass A
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_Z_OFF_A" 40 1366 768 >"vk_z_off_a.log" 2>&1
if errorlevel 1 goto fail
set "CSE_WARP_VK_DEPTH_PREPASS=1"
echo [2/4] With prepass A
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_Z_ON_A" 40 1366 768 >"vk_z_on_a.log" 2>&1
if errorlevel 1 goto fail
echo [3/4] With prepass B
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_Z_ON_B" 40 1366 768 >"vk_z_on_b.log" 2>&1
if errorlevel 1 goto fail
set "CSE_WARP_VK_DEPTH_PREPASS=0"
echo [4/4] Without prepass B
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_Z_OFF_B" 40 1366 768 >"vk_z_off_b.log" 2>&1
if errorlevel 1 goto fail
powershell -NoProfile -Command "Compress-Archive -Path 'VK_Z_OFF_A','VK_Z_OFF_B','VK_Z_ON_A','VK_Z_ON_B','vk_z_off_a.log','vk_z_off_b.log','vk_z_on_a.log','vk_z_on_b.log' -DestinationPath 'CSE_WARP_VULKAN_ZPREPASS_HD620.zip' -Force"
if errorlevel 1 goto fail
echo.
echo DONE: CSE_WARP_VULKAN_ZPREPASS_HD620.zip
pause
exit /b 0
:missing
echo Missing benchmark, scene or shaders. Extract the complete artifact.
pause
exit /b 2
:fail
echo Z-prepass benchmark failed. Keep and send logs.
pause
exit /b 1
