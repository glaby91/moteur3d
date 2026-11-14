@echo off
setlocal
cd /d "%~dp0"
echo =========================================
echo CSE WARP - Vulkan vs DirectX11 - HD620
 echo Benchmark COMPUTE uniquement - PAS Raster V17
 echo =========================================
if not exist "cse_warp_vk_dx11_bench.exe" (echo Executable absent du dossier.&pause&exit /b 1)
cse_warp_vk_dx11_bench.exe --probe > vulkan_dx11_probe.txt 2>&1
 type vulkan_dx11_probe.txt
cse_warp_vk_dx11_bench.exe --api both --width 1366 --height 768 --loops 32 --warmup 6 --repeat 30 --output bench_hd620
if errorlevel 1 (
 echo ECHEC - consulter les messages ci-dessus et vulkan_dx11_probe.txt.
 pause
 exit /b 1
)
echo Resultats: bench_hd620.csv et bench_hd620.json
pause
