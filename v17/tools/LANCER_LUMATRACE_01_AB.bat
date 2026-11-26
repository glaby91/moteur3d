@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\route30.ps1" -Root "." -AdaptiveTour
set "RC=%ERRORLEVEL%"
pause
exit /b %RC%
