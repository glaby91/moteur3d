#Requires -Version 5.1
<#
.SYNOPSIS
  V17.443 diagnostic benchmark of CSE WARP raster bottlenecks.
.DESCRIPTION
  Runs ONE variable at a time (pixel shader / MSAA / anisotropy / culling / LOD /
  resolution), repeating the same camera with matched scene and lighting.
  Never changes the installed engine or scene. Not an architectural test.
  Requires the DIAGNOSTIC executable built from bench/v17-443-diagnostic-goulots-261113
  because it adds --frames and --resolution. The ordinary V17.443 release
  cannot be used with these flags.
.EXAMPLE
  pwsh -File tools/bench_v443_diagnostic.ps1 -RuntimeRoot "C:\CSE-WARP-DIAG" -Passes 3 -Frames 24
#>
param(
  [Parameter(Mandatory=$true)][string]$RuntimeRoot,
  [string]$OutputRoot="",
  [ValidateRange(1,10)][int]$Passes=3,
  [ValidateRange(6,240)][int]$Frames=24,
  [string]$CameraId="v4_face",
  [switch]$SoftwareWarp
)
$ErrorActionPreference="Stop"
$root=(Resolve-Path $RuntimeRoot).Path
if(-not $OutputRoot){$OutputRoot=Join-Path $root "benchmarks-diagnostic"}
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$out=(Resolve-Path $OutputRoot).Path
$exe=Join-Path $root "outils\benchmark\moteur3d_v17_benchmark.exe"
$scene=Join-Path $root "benchmark-civic\scene.json"
if(-not (Test-Path $exe)){throw "Executable absent: $exe"}
if(-not (Test-Path $scene)){throw "Scene absente: $scene"}
$v=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
if($v.FileMajorPart -ne 17 -or $v.FileMinorPart -ne 443){
  throw "Le benchmark doit provenir de la branche DIAGNOSTIC V17.443. Version : $($v.FileVersion)"
}
if(Get-Process -Name "moteur3d_v17" -ErrorAction SilentlyContinue){
  throw "Fermer CSE WARP Exploration et la fenetre principale AVANT de mesurer; ne pas melanger les deux processus."
}
$env:PATH=$root+";"+$env:PATH
$env:QT_PLUGIN_PATH=$root
$env:QT_QPA_PLATFORM="offscreen"
if($SoftwareWarp){$env:M3D_V16_FORCE_WARP="1"}
else{Remove-Item Env:M3D_V16_FORCE_WARP -ErrorAction SilentlyContinue}
$tests=@(
  @{name="REF";group="reference";res="1366x768";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="MSAA_1";group="msaa";res="1366x768";msaa=1;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="MSAA_4";group="msaa";res="1366x768";msaa=4;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="MSAA_8";group="msaa";res="1366x768";msaa=8;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="ANISO_1";group="anisotropy";res="1366x768";msaa=2;aniso=1;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="ANISO_4";group="anisotropy";res="1366x768";msaa=2;aniso=4;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="ANISO_16";group="anisotropy";res="1366x768";msaa=2;aniso=16;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="PIXEL_SHADER_LEGACY";group="shader";res="1366x768";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=1;light=0},
  @{name="LIGHT_AABB_CULL";group="shader";res="1366x768";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=1},
  @{name="SHADOW_SAMPLING_OFF";group="shadows";res="1366x768";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0;shadowOff=1},
  @{name="PBR_DETAIL_OFF";group="materials";res="1366x768";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0;detailOff=1},
  @{name="FRUSTUM_OFF";group="geometry";res="1366x768";msaa=2;aniso=8;cull=0;lod=100;lodOn=1;legacy=0;light=0},
  @{name="LOD_OFF";group="geometry";res="1366x768";msaa=2;aniso=8;cull=1;lod=100;lodOn=0;legacy=0;light=0},
  @{name="LOD_50";group="geometry";res="1366x768";msaa=2;aniso=8;cull=1;lod=50;lodOn=1;legacy=0;light=0},
  @{name="LOD_150";group="geometry";res="1366x768";msaa=2;aniso=8;cull=1;lod=150;lodOn=1;legacy=0;light=0},
  @{name="PIXELS_960";group="resolution";res="960x540";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0},
  @{name="PIXELS_1920";group="resolution";res="1920x1080";msaa=2;aniso=8;cull=1;lod=100;lodOn=1;legacy=0;light=0}
)
$hardware=[ordered]@{
  dateUtc=(Get-Date).ToUniversalTime().ToString("o")
  machine=$env:COMPUTERNAME
  cpus=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
  displayControllers=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
  executable=$exe
  executableVersion=$v.FileVersion
  softwareWarp=[bool]$SoftwareWarp
  camera=$CameraId
  frames=$Frames
  passes=$Passes
  scene=$scene
  note="List of installed video devices does NOT prove actual D3D adapter; WARP is explicitly forced only when -SoftwareWarp is specified."
}
$hardware|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out "hardware.json") -Encoding UTF8
$manifest=@()
for($pass=1;$pass -le $Passes;$pass++){
  # Reverse alternating passes to reduce fixed-order frequency/thermal drift.
  $ordered=@($tests)
  if($pass % 2 -eq 0){[array]::Reverse($ordered)}
  foreach($case in $ordered){
    $folder=Join-Path $out ("P{0:D2}_{1}" -f $pass,$case.name)
    New-Item -ItemType Directory -Path $folder -Force | Out-Null
    if($case.legacy -eq 1){$env:M3D_V17_DISABLE_SHADER_OPT="1"}
    else{Remove-Item Env:M3D_V17_DISABLE_SHADER_OPT -ErrorAction SilentlyContinue}
    if($case.light -eq 1){$env:M3D_V17_EXPERIMENT_LIGHT_AABB_CULL="1"}
    else{Remove-Item Env:M3D_V17_EXPERIMENT_LIGHT_AABB_CULL -ErrorAction SilentlyContinue}
    if($case.shadowOff -eq 1){$env:M3D_V17_DIAG_NO_SHADOW_SAMPLING="1"}
    else{Remove-Item Env:M3D_V17_DIAG_NO_SHADOW_SAMPLING -ErrorAction SilentlyContinue}
    if($case.detailOff -eq 1){$env:M3D_V17_DIAG_NO_MATERIAL_DETAIL="1"}
    else{Remove-Item Env:M3D_V17_DIAG_NO_MATERIAL_DETAIL -ErrorAction SilentlyContinue}
    $args=@($scene,$folder,"--quick","--camera-id",$CameraId,
      "--resolution",$case.res,"--frames","$Frames","--readback-depth","1",
      "--hour","12",
      "--raster-msaa","$($case.msaa)",
      "--raster-anisotropy","$($case.aniso)",
      "--raster-culling","$($case.cull)",
      "--raster-lod-percent","$($case.lod)",
      "--raster-lod-enabled","$($case.lodOn)")
    Write-Host "BENCH $pass/$Passes $($case.name) $($case.res)"
    $clock=[Diagnostics.Stopwatch]::StartNew()
    $output=& $exe @args 2>&1
    $exit=$LASTEXITCODE
    $clock.Stop()
    $output|Out-File (Join-Path $folder "console.txt") -Encoding UTF8
    $summaries=@(Get-ChildItem $folder -Recurse -Filter "summary.json" -File)
    $row=[ordered]@{
      id=$case.name;group=$case.group;pass=$pass;resolution=$case.res
      msaa=$case.msaa;anisotropy=$case.aniso;culling=$case.cull
      lodPercent=$case.lod;lodEnabled=$case.lodOn
      shaderLegacy=$case.legacy;lightAabbCull=$case.light
      noShadowSampling=if($case.shadowOff){1}else{0}
      noMaterialDetail=if($case.detailOff){1}else{0}
      exitCode=$exit;elapsedWallSeconds=[math]::Round($clock.Elapsed.TotalSeconds,3)
      summary=if($summaries.Count -eq 1){$summaries[0].FullName}else{""}
    }
    $manifest += [PSCustomObject]$row
    $manifest|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out "manifest.json") -Encoding UTF8
    if($exit -ne 0 -or $summaries.Count -ne 1){
      throw "Failure on $($case.name) repetition $pass : exit=$exit, summaries=$($summaries.Count). See $folder/console.txt"
    }
  }
}
Remove-Item Env:M3D_V17_DISABLE_SHADER_OPT -ErrorAction SilentlyContinue
Remove-Item Env:M3D_V17_EXPERIMENT_LIGHT_AABB_CULL -ErrorAction SilentlyContinue
Remove-Item Env:M3D_V17_DIAG_NO_SHADOW_SAMPLING -ErrorAction SilentlyContinue
Remove-Item Env:M3D_V17_DIAG_NO_MATERIAL_DETAIL -ErrorAction SilentlyContinue
Write-Host "Benchmark complete: $out"
Write-Host "Run: python tools/analyze_diagnostic_v443.py `"$out`""
