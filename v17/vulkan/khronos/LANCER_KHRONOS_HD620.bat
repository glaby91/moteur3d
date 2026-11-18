@echo off
setlocal
cd /d "%~dp0"
echo CSE WARP Vulkan V17.32 - Benchmark Khronos 12 scenarios, 3 passes
echo IMPORTANT: keep laptop on mains power, do not resize window during tests.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RUN_KHRONOS_HD620.ps1" -Seconds 20 -Repetitions 3 -WarmupSeconds 4
if errorlevel 1 echo ERROR - refer to generated logs.
echo Send KHRONOS_V17_32_HD620_*.zip to ChatGPT.
pause
