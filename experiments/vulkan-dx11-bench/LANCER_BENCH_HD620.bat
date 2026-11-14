@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ===============================================================
echo CSE WARP - CAMPAGNE TURBO VULKAN vs DIRECTX11 - Intel HD620
echo COMPUTE UNIQUEMENT (pas le Raster V17)
echo ===============================================================
if not exist "cse_warp_vk_dx11_bench.exe" (echo ERREUR : executable absent.&pause&exit /b 1)
set "EXE=cse_warp_vk_dx11_bench.exe"
set "OPT=--width 1366 --height 768 --loops 32 --warmup 6 --repeat 30"
"%EXE%" --probe > vulkan_dx11_probe.txt 2>&1
if errorlevel 1 (echo ERREUR diagnostic Vulkan.&type vulkan_dx11_probe.txt&pause&exit /b 1)
type vulkan_dx11_probe.txt
echo.
echo === A : Reference originale DX11 et Vulkan ===
call :run baseline --api both --vk-mode legacy
echo === B : HOST_CACHED + persistent map, memes sorties ===
call :run cached --api both --vk-mode cached
echo === C : Buffer directement accessible CPU, sans staging ===
call :run direct --api both --vk-mode direct
echo === D : Mem cache avec commande re-enregistree ===
call :run cached_record --api vulkan --vk-mode cached --vk-record
echo === E : HOST_CACHED et 2 travaux en vol ===
call :run cached_ring2 --api vulkan --vk-mode cached --vk-inflight 2
echo === F : HOST_CACHED et 3 travaux en vol ===
call :run cached_ring3 --api vulkan --vk-mode cached --vk-inflight 3
echo === G : Acces direct et 3 travaux en vol ===
call :run direct_ring3 --api vulkan --vk-mode direct --vk-inflight 3
echo === H : GPU compute sans lecture de 4 Mio par dispatch ===
call :run compute_only --api vulkan --vk-mode compute
echo === I : GPU compute sans lecture, 3 travaux en vol ===
call :run compute_ring3 --api vulkan --vk-mode compute --vk-inflight 3
echo.
echo === ZIP des resultats ===
powershell -NoProfile -Command "Compress-Archive -Path 'bench_hd620_*.json','bench_hd620_*.csv','vulkan_dx11_probe.txt' -DestinationPath 'CSE_WARP_HD620_TURBO_RESULTS.zip' -Force" 
if errorlevel 1 (
 echo Attention : echec ZIP, les JSON CSV sont toujours disponibles.
) else (
 echo Fichier a transmettre : CSE_WARP_HD620_TURBO_RESULTS.zip
)
echo Resultats conserves dans le repertoire courant.
pause
exit /b 0

:run
set "NAME=%~1"
shift
"%EXE%" %OPT% %* --output "bench_hd620_%NAME%"
if errorlevel 1 (
 echo ECHEC VARIANTE %NAME% - consulter le message ci-dessus.
 pause
 exit /b 1
)
exit /b 0
