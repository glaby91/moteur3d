@echo off
cd /d "%~dp0"
setlocal
set "SCENE=%~1"
if not defined SCENE set "SCENE=%~dp0benchmark-civic\scene.json"
if not exist "%SCENE%" (
 echo Drag benchmark-civic\scene.json onto this BAT, or pass it as first argument.
 pause
 exit /b 2
)
set "M3D_V16_FORCE_WARP="
set "W=1280"
set "H=720"
set "REPS=5"
set "RAYS=4096"
if not "%~2"=="" set "W=%~2"
if not "%~3"=="" set "H=%~3"
if not "%~4"=="" set "REPS=%~4"
if not "%~5"=="" set "RAYS=%~5"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%i"
v17_escargot_ab.exe "%SCENE%" "%~dp0escargot-ab-%STAMP%" %W% %H% %REPS% %RAYS%
set "RC=%ERRORLEVEL%"
echo Exit code %RC%
pause
exit /b %RC%
