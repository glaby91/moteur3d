@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\route30.ps1" -Root "." -AdaptiveAB
set "RC=%ERRORLEVEL%"
pause
exit /b %RC%
