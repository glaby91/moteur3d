@echo off
setlocal
cd /d "%~dp0"
if not exist "moteur3d_v17_campaign.exe" (
  echo ERREUR: moteur3d_v17_campaign.exe absent. Extrais ce fichier dans le dossier du moteur.
  pause
  exit /b 2
)
if not exist "audit_luminosite.ps1" (
  echo ERREUR: audit_luminosite.ps1 absent.
  pause
  exit /b 2
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0audit_luminosite.ps1" -Root "%~dp0"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo Audit termine. Consulte le dossier benchmark-results.
) else (
  echo Echec de l'audit, code %RC%. Le rapport de console indique la cause.
)
pause
exit /b %RC%
