@echo off
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"
echo ====================================================================
echo CSE WARP V17 - Finition Vulkan DX11 / Eau GI MSAA
echo 4 scenes x MSAA 1,2,4 - meme qualite demandee aux deux moteurs.
echo Z-prepass et sRGB Vulkan fixes. Sans calibration automatique.
echo ====================================================================
if not exist "v17_vulkan_ab_bench.exe" goto missing
if not exist "vulkan\sky.frag.spv" goto missing
set "CSE_WARP_VK_FAST_FRAGMENT=0"
set "CSE_WARP_VK_BGRA_READBACK=0"
set "CSE_WARP_VK_SRGB_COLOR_TEST=1"
set "ERRORS="
for %%Q in (1 2 4) do (
 set "CSE_WARP_VK_MSAA=%%Q"
 for %%S in (pavillon01 material-lab pripyat-photo2scene-proof benchmark-civic) do (
  if exist "%%S\scene.json" (
   echo Running %%S at requested MSAA %%Q
   "v17_vulkan_ab_bench.exe" "%%S\scene.json" "FINITIONS_%%S_MSAA%%Q" 12 1366 768 >"finitions_%%S_MSAA%%Q.log" 2>&1
   if errorlevel 1 (
    echo FAIL %%S %%Q - see log
    set "ERRORS=1"
   )
  ) else (
   echo MISSING SCENE %%S
   set "ERRORS=1"
  )
 )
)
rem Independent rescue tool: ZIP can be recreated without rerunning benchmarks.
call "%~dp0CREER_ZIP_FINITIONS_HD620.bat"
if errorlevel 1 goto fail
echo.
echo DONE: CSE_WARP_VULKAN_FINITIONS_HD620.zip
if defined ERRORS (
 echo Some tests failed. Send this ZIP with all collected results.
 pause
 exit /b 4
)
pause
exit /b 0
:missing
echo Archive incomplete. Extract all files from the GitHub artifact.
pause
exit /b 2
:fail
echo Cannot package diagnostic ZIP. Keep the logs.
pause
exit /b 3
