@echo off
setlocal
cd /d "%~dp0"
echo ===================================================
echo CSE WARP Vulkan HD620 - comparaison A/B a 1280x720
echo Profil A: synchronisation 1 frame, PBR rebuild global,
echo            shaders de reference
echo Profil B: 2 frames en vol, cache de geometrie/PBR,
echo            shaders optimises (rendu PBR conserve)
echo ===================================================
echo.
echo ETAPE A: reference (12 scenarios de 8 s)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RUN_KHRONOS_HD620.ps1" -Profile baseline -Seconds 8 -Repetitions 1 -WarmupSeconds 2 -Width 1280 -Height 720
if errorlevel 1 (
 echo La campagne A a signale une erreur. Verifier les logs.
 pause
 exit /b 1
)
echo.
echo ETAPE B: version optimisee (12 scenarios de 8 s)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RUN_KHRONOS_HD620.ps1" -Profile optimized -Seconds 8 -Repetitions 1 -WarmupSeconds 2 -Width 1280 -Height 720
if errorlevel 1 (
 echo La campagne B a signale une erreur. Verifier les logs.
 pause
 exit /b 1
)
echo.
echo Termine. Envoyer les DEUX fichiers .zip KHRONOS *_baseline_* et *_optimized_*
pause
