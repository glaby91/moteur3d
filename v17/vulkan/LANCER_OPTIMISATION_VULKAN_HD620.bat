@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ============================================================
echo CSE WARP V17 - VULKAN FRAGMENT OPTIMIZATION A/B (INTEL HD620)
echo Original Vulkan (BASE) vs FAST with identical DX11 reference
echo Four rounds ABBA, 40 frames each, 1366x768, same scene.
echo The baseline remains the application default.
echo ============================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "vulkan\raster_fast.frag.spv" goto missing
set "SCENE=benchmark-civic\scene.json"
if not exist "%SCENE%" set "SCENE=pavillon01\scene.json"
if not exist "%SCENE%" goto missing
echo Scene: %SCENE%
set "CSE_WARP_VK_FAST_FRAGMENT=0"
echo [1/4] Vulkan baseline A
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_BASE_A" 40 1366 768 >"vk_base_a.log" 2>&1
if errorlevel 1 goto fail
set "CSE_WARP_VK_FAST_FRAGMENT=1"
echo [2/4] Vulkan optimized A
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_FAST_A" 40 1366 768 >"vk_fast_a.log" 2>&1
if errorlevel 1 goto fail
echo [3/4] Vulkan optimized B
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_FAST_B" 40 1366 768 >"vk_fast_b.log" 2>&1
if errorlevel 1 goto fail
set "CSE_WARP_VK_FAST_FRAGMENT=0"
echo [4/4] Vulkan baseline B
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_BASE_B" 40 1366 768 >"vk_base_b.log" 2>&1
if errorlevel 1 goto fail
powershell -NoProfile -Command "Compress-Archive -Path 'VK_BASE_A','VK_BASE_B','VK_FAST_A','VK_FAST_B','vk_base_a.log','vk_base_b.log','vk_fast_a.log','vk_fast_b.log' -DestinationPath 'CSE_WARP_VULKAN_OPTIMISATION_HD620.zip' -Force"
if errorlevel 1 goto fail
echo.
echo Complete: CSE_WARP_VULKAN_OPTIMISATION_HD620.zip
echo Send this ZIP for statistical / image-parity analysis.
pause
exit /b 0
:missing
echo The executable, shader, or reference scene is missing. Extract the complete CI artifact.
pause
exit /b 2
:fail
echo One test failed. Send the .log files and any results folders already generated.
pause
exit /b 1
