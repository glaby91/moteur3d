@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
if not exist "moteur3d_v17.exe" (
 echo ERROR: V17.493 complete application not found in this directory.
 pause
 exit /b 1
)
if not exist "benchmark-civic\scene.json" (
 echo ERROR: approved V17.493 186-material scene missing.
 pause
 exit /b 1
)
set "CSE_WARP_VK_FRAMES_IN_FLIGHT=2"
set "CSE_WARP_VK_OPT_INCREMENTAL=1"
set "CSE_WARP_VK_FAST_FRAGMENT=1"
set "CSE_WARP_VK_KHRONOS_CSV=%CD%\V493_VULKAN_FRAMES.csv"
set "CSE_WARP_VK_SHADOW_PROFILE=%CD%\V493_VULKAN_SHADOW_BUILD.log"
del /q "%CSE_WARP_VK_KHRONOS_CSV%" 2>nul
del /q "%CSE_WARP_VK_SHADOW_PROFILE%" 2>nul
echo ==============================================================
echo CSE WARP V17.493 COMPLETE - Vulkan HD620 full-scene profiler
echo ==============================================================
echo In the application select Vulkan Raster with direct GPU display.
echo Check the 24h day-night simulation and start exploration.
echo Stay in the scene at least 25 seconds, then exit the application.
echo The script will create an archive with actual frame and shadow data.
echo.
start "" /wait "%CD%\moteur3d_v17.exe" "benchmark-civic\scene.json"
set "EXIT_CODE=%ERRORLEVEL%"
echo Application exit code: %EXIT_CODE%
if not exist "%CSE_WARP_VK_KHRONOS_CSV%" echo WARNING: No Vulkan frames CSV. Check native Vulkan display selection.
if not exist "%CSE_WARP_VK_SHADOW_PROFILE%" echo WARNING: No shadow rebuild detected; this is normal if all shadows reused.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$files = @('V493_VULKAN_FRAMES.csv','V493_VULKAN_SHADOW_BUILD.log','VULKAN_INTEGRATION_COMMIT.txt') | Where-Object { Test-Path $_ }; if($files.Count){ Compress-Archive -Path $files -DestinationPath 'V493_VULKAN_PROFILE.zip' -Force }"
echo.
echo PROFILE: V493_VULKAN_PROFILE.zip (if generated)
echo Send the archive to diagnose periodic stalls. Original files remain.
pause
exit /b 0
