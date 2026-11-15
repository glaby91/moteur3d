@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
set "BENCH_EXE=v17_vulkan_khronos_console.exe"
if not exist "%BENCH_EXE%" set "BENCH_EXE=v17_vulkan_khronos_bench.exe"
if not exist "%BENCH_EXE%" (
 echo ERROR: executable missing. Extract the complete GitHub Actions artifact.
 pause
 exit /b 1
)
echo Diagnostic executable: %BENCH_EXE%
set "OUT=%CD%\KHRONOS_BLACK_SCREEN_DIAGNOSTIC"
if not exist "%OUT%" mkdir "%OUT%"
set "CSE_WARP_VK_BENCH_PRESENT_MODE=FIFO"
set "CSE_WARP_VK_BENCH_SWAP_IMAGES=3"
set "CSE_WARP_VK_MSAA=1"
set "CSE_WARP_VK_FAST_FRAGMENT=0"
set "CSE_WARP_VK_FRAMES_IN_FLIGHT=1"
set "CSE_WARP_VK_DIAG_TEARDOWN=1"
set "CSE_WARP_VK_KHRONOS_CSV="
echo ===============================================================
echo KHRONOS - BLACK SCREEN: 3 independent checks (V17.32 engine)
echo ===============================================================
echo.
echo STEP 1/3 - Offscreen renderer image and explicit GPU cleanup.
"%BENCH_EXE%" --width 800 --height 450 --cubes 1 --lights 1 --motion fixed --offscreen-check "%OUT%\OFFSCREEN_REFERENCE.bmp" 1> "%OUT%\01_OFFSCREEN.log" 2>&1
set "RC1=%ERRORLEVEL%"
echo Offscreen exit code: %RC1%
echo.
echo STEP 2/3 - Native GPU swapchain, magenta fill.
rem Validate the optimized two-frames-in-flight presentation path.
set "CSE_WARP_VK_FRAMES_IN_FLIGHT=2"
echo EXPECTED WINDOW COLOR: BRIGHT MAGENTA
set "CSE_WARP_VK_KHRONOS_CSV=%OUT%\02_MAGENTA_GPU.csv"
"%BENCH_EXE%" --width 800 --height 450 --cubes 1 --lights 1 --motion fixed --seconds 5 --visual-probe 1 1> "%OUT%\02_MAGENTA.log" 2>&1
set "RC2=%ERRORLEVEL%"
echo Magenta exit code: %RC2%
echo.
echo STEP 3/3 - Native Vulkan GPU with actual cube and sky.
echo EXPECTED: CUBE AND SKY. Black image indicates presentation issue.
set "CSE_WARP_VK_KHRONOS_CSV=%OUT%\03_REAL_GPU.csv"
"%BENCH_EXE%" --width 800 --height 450 --cubes 1 --lights 1 --motion fixed --seconds 5 1> "%OUT%\03_REAL_GPU.log" 2>&1
set "RC3=%ERRORLEVEL%"
echo Real GPU exit code: %RC3%
set "CSE_WARP_VK_KHRONOS_CSV="
set "CSE_WARP_VK_KHRONOS_VISUAL_PROBE="
echo.
echo Package diagnostics into ZIP.
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -LiteralPath '%OUT%' -DestinationPath '%CD%\KHRONOS_BLACK_SCREEN_DIAGNOSTIC.zip' -Force"
set "ZIP_RC=%ERRORLEVEL%"
if not "%RC1%"=="0" echo WARNING - Offscreen test failed. Inspect 01_OFFSCREEN.log
if not "%RC2%"=="0" echo WARNING - Magenta test failed. Inspect 02_MAGENTA.log
if not "%RC3%"=="0" echo WARNING - Real GPU test failed. Inspect 03_REAL_GPU.log
if not "%ZIP_RC%"=="0" echo WARNING - ZIP creation failed. Original logs remain in %OUT%
echo.
echo Diagnostic finished. Please send KHRONOS_BLACK_SCREEN_DIAGNOSTIC.zip
echo Offscreen=%RC1% Magenta=%RC2% RealGPU=%RC3%
echo.
pause
exit /b 0
