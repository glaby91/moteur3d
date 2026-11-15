@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist "v17_vulkan_khronos_bench.exe" (
 echo ERROR: benchmark executable missing. Unzip complete GitHub Actions artifact.
 pause
 exit /b 1
)
set "OUT=%CD%\KHRONOS_BLACK_SCREEN_DIAGNOSTIC"
if not exist "%OUT%" mkdir "%OUT%"
set "CSE_WARP_VK_BENCH_PRESENT_MODE=FIFO"
set "CSE_WARP_VK_BENCH_SWAP_IMAGES=3"
set "CSE_WARP_VK_MSAA=1"
set "CSE_WARP_VK_FAST_FRAGMENT=0"
echo ===============================================================
echo KHRONOS - BLACK SCREEN: 3 independent checks (V17.32 engine)
echo ===============================================================
echo.
echo STEP 1/3 - Offscreen renderer image. Reference BMP should show cube + sky.
v17_vulkan_khronos_bench.exe --width 800 --height 450 --cubes 1 --lights 1 --motion fixed --offscreen-check "%OUT%\OFFSCREEN_REFERENCE.bmp" > "%OUT%\01_OFFSCREEN.log" 2>&1
echo Offscreen exit code: %errorlevel%
echo.
echo STEP 2/3 - Native GPU swapchain, diagnostic magenta fill.
echo >>> EXPECTED WINDOW COLOR: BRIGHT MAGENTA <<<
set "CSE_WARP_VK_KHRONOS_CSV=%OUT%\02_MAGENTA_GPU.csv"
v17_vulkan_khronos_bench.exe --width 800 --height 450 --cubes 1 --lights 1 --motion fixed --seconds 5 --visual-probe 1 > "%OUT%\02_MAGENTA.log" 2>&1
echo Magenta exit code: %errorlevel%
echo.
echo STEP 3/3 - Native Vulkan GPU with actual cube + sky.
echo >>> EXPECTED: CUBE + SKY.  BLACK MEANS DISPLAY/BLIT OR IMAGE FAULT <<<
set "CSE_WARP_VK_KHRONOS_CSV=%OUT%\03_REAL_GPU.csv"
v17_vulkan_khronos_bench.exe --width 800 --height 450 --cubes 1 --lights 1 --motion fixed --seconds 5 > "%OUT%\03_REAL_GPU.log" 2>&1
echo Real GPU exit code: %errorlevel%
set "CSE_WARP_VK_KHRONOS_CSV="
set "CSE_WARP_VK_KHRONOS_VISUAL_PROBE="
powershell.exe -NoProfile -Command "Compress-Archive -LiteralPath '%OUT%' -DestinationPath '%CD%\KHRONOS_BLACK_SCREEN_DIAGNOSTIC.zip' -Force"
echo.
echo PLEASE SEND: %CD%\KHRONOS_BLACK_SCREEN_DIAGNOSTIC.zip
echo And tell us whether step 2 displayed MAGENTA and step 3 a CUBE.
pause
