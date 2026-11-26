@echo off
setlocal
cd /d "%~dp0"
if exist "route30.ps1" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\route30.ps1" -Root "."
) else (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $p=Join-Path $env:TEMP ('warp-route-'+[guid]::NewGuid()+'.ps1'); try { Invoke-WebRequest -UseBasicParsing 'https://raw.githubusercontent.com/glaby91/moteur3d/master/v17/tools/route30.ps1' -OutFile $p; & $p -Root (Get-Location).Path } finally { Remove-Item $p -ErrorAction SilentlyContinue }"
)
set "RC=%ERRORLEVEL%"
pause
exit /b %RC%
