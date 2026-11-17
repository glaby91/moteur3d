@echo off
setlocal
cd /d "%~dp0"
if not exist "benchmark-results" mkdir "benchmark-results"
echo CSE WARP - benchmark DirectX 11, camera fixe
echo Scene : Energetik V17.500 ; camera : v8_glazing_raccords
echo Resolution : 1366x768 ; MSAA : 2x ; PBR : 2048 ; 2 warmups + 90 frames
echo The result folder includes summary.json, system_info.txt, frames.csv and 3 images.
echo Close other GPU-heavy apps before starting, then wait until SUCCESS.
echo.
"moteur3d_v17_gpu_trajectory.exe" "benchmark-civic\scene.json" "benchmark-results" --fixed-camera
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" echo Success. Zip the newest BENCH-GPU-HD620 folder in benchmark-results and send it back.
if not "%RC%"=="0" echo Benchmark failed with code %RC%. Check the message above.
pause
exit /b %RC%
