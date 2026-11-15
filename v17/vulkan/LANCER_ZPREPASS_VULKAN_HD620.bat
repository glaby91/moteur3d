@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ===============================================================
echo CSE WARP V17 - VALIDATION GPU VULKAN Z-PREPASS PERMANENT HD620
echo Le prepass est integre au backend: aucun interrupteur, aucun A/B.
echo 40 images, empreintes et comparaison DirectX11 de reference.
echo ===============================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "v17_vulkan_gpu_smoke.exe" goto missing
if not exist "vulkan\raster.frag.spv" goto missing
set "SCENE=benchmark-civic\scene.json"
if not exist "%SCENE%" set "SCENE=pavillon01\scene.json"
if not exist "%SCENE%" goto missing
echo Scene: %SCENE%
set "CSE_WARP_VK_FAST_FRAGMENT=0"
set "CSE_WARP_VK_BGRA_READBACK=0"
echo Test fonctionnel materiel avec prepass permanent...
"v17_vulkan_gpu_smoke.exe" "vulkan_zprepass_smoke.png" >"vulkan_zprepass_smoke.log" 2>&1
if errorlevel 1 goto fail
echo Mesure de 40 images sur le backend de production Vulkan...
"v17_vulkan_ab_bench.exe" "%SCENE%" "VK_ZPREPASS_DEFAULT" 40 1366 768 >"vk_zprepass_default.log" 2>&1
if errorlevel 1 goto fail
powershell -NoProfile -Command "Compress-Archive -Path 'VK_ZPREPASS_DEFAULT','vulkan_zprepass_smoke.png','vulkan_zprepass_smoke.log','vk_zprepass_default.log' -DestinationPath 'CSE_WARP_VULKAN_ZPREPASS_DEFAULT_HD620.zip' -Force"
if errorlevel 1 goto fail
echo.
echo DONE: CSE_WARP_VULKAN_ZPREPASS_DEFAULT_HD620.zip
pause
exit /b 0
:missing
echo Archive incomplete. Extraire TOUS les fichiers du build GitHub.
pause
exit /b 2
:fail
echo Echec de la validation Z-prepass: transmettre les journaux disponibles.
pause
exit /b 1
