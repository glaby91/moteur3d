@echo off
setlocal
cd /d "%~dp0"
set "CSE_WARP_VK_PROFILE_MODE=1"
set "CSE_WARP_VK_FAST_FRAGMENT=0"
set "CSE_WARP_VK_KHRONOS_VISUAL_PROBE=2"
set "CSE_WARP_VK_SCENE_DIAGNOSTIC=1"
echo V17.509 Vulkan source-image probe (reference fragment shader)
echo A bright green viewport means the scene framebuffer-to-swapchain blit works.
del "%TEMP%\CSE_WARP_VK_SCENE_DIAGNOSTIC.txt" 2>nul
echo Scene draw/camera data: %TEMP%\CSE_WARP_VK_SCENE_DIAGNOSTIC.txt
echo This probe replaces scene pixels with green after scene drawing.
start "" "%~dp0moteur3d_v17.exe"
endlocal
