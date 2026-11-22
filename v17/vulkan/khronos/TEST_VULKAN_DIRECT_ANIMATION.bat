@echo off
setlocal
cd /d "%~dp0"
if not exist "%~dp0v17_vulkan_khronos_bench.exe" (
  echo Missing v17_vulkan_khronos_bench.exe next to this launcher.
  pause
  exit /b 2
)
set "CSE_WARP_VK_KHRONOS_CSV=%~dp0VULKAN_DIRECT_ANIMATION.csv"
echo Opening the standalone Vulkan Win32 window.
echo One spinning cube, direct swapchain presentation, CSV capture enabled.
echo Press Escape in the render window to close it.
"%~dp0v17_vulkan_khronos_bench.exe" --cubes 1 --lights 1 --motion spin --continuous --width 916 --height 628
set "RESULT=%ERRORLEVEL%"
echo.
echo Vulkan test exit code: %RESULT%
echo Frame measurements: "%CSE_WARP_VK_KHRONOS_CSV%"
pause
exit /b %RESULT%
