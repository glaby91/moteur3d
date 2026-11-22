@echo off
cd /d "%~dp0"
set "M3D_V17_VK_PROFILE_MODE=1"
set "CSE_WARP_VK_KHRONOS_VISUAL_PROBE=1"
start "" /wait "moteur3d_v17.exe" "benchmark-civic\scene.json"
