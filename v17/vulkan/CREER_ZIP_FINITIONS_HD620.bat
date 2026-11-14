@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
echo [CSE WARP] Compression des resultats deja generes, sans nouveau benchmark.
if not exist "%~dp0CREER_ZIP_FINITIONS_HD620.ps1" (
 echo ERREUR : CREER_ZIP_FINITIONS_HD620.ps1 est absent.
 exit /b 2
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0CREER_ZIP_FINITIONS_HD620.ps1" -Root "%~dp0" > "%~dp0ZIP_FINITIONS_HD620.log" 2>&1
set "ZIP_EXIT=%ERRORLEVEL%"
type "%~dp0ZIP_FINITIONS_HD620.log"
if not "%ZIP_EXIT%"=="0" (
 echo.
 echo ZIP non cree. Diagnostic: ZIP_FINITIONS_HD620.log
 echo Les FINITIONS_* et finitions_*.log sont conserves.
 exit /b %ZIP_EXIT%
)
echo [CSE WARP] ZIP disponible : CSE_WARP_VULKAN_FINITIONS_HD620.zip
exit /b 0
