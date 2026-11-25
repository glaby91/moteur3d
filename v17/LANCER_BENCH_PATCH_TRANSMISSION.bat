@echo off
setlocal
cd /d "%~dp0"
set "CSE_WARP_BENCHMARK_SCRIPT_COMMIT=483ecdcc2d48b138624ccc080ac86103e70ddb61"
echo CSE WARP - patch de transmission - benchmark 4 moteurs
echo Verifying local benchmark files...
if not exist "moteur3d_v17_campaign.exe" (
  echo ERROR: moteur3d_v17_campaign.exe is missing.
  pause
  exit /b 2
)
if not exist "audit_luminosite.ps1" (
  echo ERROR: audit_luminosite.ps1 is missing.
  pause
  exit /b 2
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0audit_luminosite.ps1" -Root "%CD%"
set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo Benchmark complete. Check benchmark-results for the ZIP.
) else (
  echo Benchmark failed with code %RC%.
)
pause
exit /b %RC%
