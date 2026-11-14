@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BENCH_FLUIDITE_PAR_DEFAUT_V437.ps1"
if errorlevel 1 echo ERREUR : voir details ci-dessus.
pause
