@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo =============================================================
echo CSE WARP V17 - VULKAN NATIVE BGRA8 READBACK A/B - Intel HD620
echo Same optimized fragment shader in all rounds.
echo RGBA legacy vs BGRA native: ABBA, 40 frames per round.
echo =============================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "vulkan\raster_fast.frag.spv" goto missing
if not exist "v17_vulkan_readback_smoke.exe" goto missing
"v17_vulkan_readback_smoke.exe"
if errorlevel 1 goto fail
set "SCENE=benchmark-civic\scene.json"
if not exist "%SCENE%" set "SCENE=pavillon01\scene.json"
if not exist "%SCENE%" goto missing
set "CSE_WARP_VK_FAST_FRAGMENT=1"
set "CSE_WARP_VK_BGRA_READBACK=0"
echo [1/4] RGBA legacy A
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_PIPE_RGBA_A" 40 1366 768 >"vk_pipe_rgba_a.log" 2>&1
if errorlevel 1 goto fail
set "CSE_WARP_VK_BGRA_READBACK=1"
echo [2/4] BGRA native A
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_PIPE_BGRA_A" 40 1366 768 >"vk_pipe_bgra_a.log" 2>&1
if errorlevel 1 goto fail
echo [3/4] BGRA native B
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_PIPE_BGRA_B" 40 1366 768 >"vk_pipe_bgra_b.log" 2>&1
if errorlevel 1 goto fail
set "CSE_WARP_VK_BGRA_READBACK=0"
echo [4/4] RGBA legacy B
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_PIPE_RGBA_B" 40 1366 768 >"vk_pipe_rgba_b.log" 2>&1
if errorlevel 1 goto fail
powershell -NoProfile -Command "Compress-Archive -Path 'VK_PIPE_RGBA_A','VK_PIPE_RGBA_B','VK_PIPE_BGRA_A','VK_PIPE_BGRA_B','vk_pipe_rgba_a.log','vk_pipe_rgba_b.log','vk_pipe_bgra_a.log','vk_pipe_bgra_b.log' -DestinationPath 'CSE_WARP_VULKAN_PIPELINE_HD620.zip' -Force"
if errorlevel 1 goto fail
echo.
echo DONE: CSE_WARP_VULKAN_PIPELINE_HD620.zip
pause
exit /b 0
:missing
echo Missing files. Extract the entire CI artifact ZIP.
pause
exit /b 2
:fail
echo Pipeline A/B failed. Send .log files and completed result folders.
pause
exit /b 1
