@echo off
cd /d "%~dp0"
setlocal
set "M3D_V16_FORCE_WARP="
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%i"
set "OUT=%~dp0escargot-scale-%STAMP%"
if "%~1"=="" (
 v17_escargot_scale.exe "%OUT%" 1280 720 12 4096
) else (
 v17_escargot_scale.exe "%OUT%" 1280 720 12 4096 "%~1"
)
set "RC=%ERRORLEVEL%"
powershell -NoProfile -Command "if(Test-Path -LiteralPath $env:OUT){Compress-Archive -Path (Join-Path $env:OUT '*') -DestinationPath ($env:OUT+'.zip') -Force}"
echo Exit code %RC%
pause
exit /b %RC%
