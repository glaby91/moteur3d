#Requires -Version 5.1
<#
  CSE WARP V17.443 - MSAA GPU raster/resolve/staging readback, all scene cameras.
  Leave the production GUI closed; these commands run an isolated benchmark
  binary and do not touch the interactive executable or scene JSON.
#>
param(
  [Parameter(Mandatory=$true)][string]$RuntimeRoot,
  [string]$OutputRoot="",
  [ValidateRange(1,6)][int]$Passes=3,
  [ValidateRange(6,120)][int]$Frames=24,
  [switch]$SoftwareWarp
)
$ErrorActionPreference="Stop"
$root=(Resolve-Path $RuntimeRoot).Path
if(-not $OutputRoot){$OutputRoot=Join-Path $root "benchmarks-msaa-multicam"}
New-Item -ItemType Directory -Path $OutputRoot -Force|Out-Null
$out=(Resolve-Path $OutputRoot).Path
$exe=Join-Path $root "outils\benchmark\moteur3d_v17_benchmark.exe"
$scene=Join-Path $root "benchmark-civic\scene.json"
if(!(Test-Path $exe) -or !(Test-Path $scene)){throw "Diagnostic binary or scene.json missing in $root"}
$v=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
if($v.FileMajorPart -ne 17 -or $v.FileMinorPart -ne 443){throw "Only CSE WARP V17.443 is allowed"}
if(Get-Process -Name "moteur3d_v17" -ErrorAction SilentlyContinue){
  throw "Close the GUI and stop Exploration before starting the diagnostic."
}
$env:PATH=$root+";"+$env:PATH
$env:QT_PLUGIN_PATH=$root
$env:QT_QPA_PLATFORM="offscreen"
if($SoftwareWarp){$env:M3D_V16_FORCE_WARP="1"}else{Remove-Item Env:M3D_V16_FORCE_WARP -ErrorAction SilentlyContinue}
$variants=@(
  @{id="MSAA1_DEPTH1";msaa=1;depth=1;res="1366x768"},
  @{id="MSAA2_DEPTH1";msaa=2;depth=1;res="1366x768"},
  @{id="MSAA4_DEPTH1";msaa=4;depth=1;res="1366x768"},
  @{id="MSAA8_DEPTH1";msaa=8;depth=1;res="1366x768"},
  @{id="MSAA2_DEPTH3";msaa=2;depth=3;res="1366x768"},
  @{id="MSAA4_DEPTH3";msaa=4;depth=3;res="1366x768"},
  @{id="MSAA2_960";msaa=2;depth=1;res="960x540"},
  @{id="MSAA2_FHD";msaa=2;depth=1;res="1920x1080"}
)
$hardware=[ordered]@{
 timestampUtc=(Get-Date).ToUniversalTime().ToString("o")
 computer=$env:COMPUTERNAME
 cpus=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
 installedDisplayAdapters=@(Get-CimInstance Win32_VideoController|Select-Object Name,DriverVersion)
 note="Installed adapters are not a verified rendering-device identity"
 forcedSoftwareWarp=[bool]$SoftwareWarp
 scene=$scene
 benchmarkExecutable=$exe
 version=$v.FileVersion
 passes=$Passes
 frames=$Frames
}
$hardware|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out "hardware.json") -Encoding UTF8
$rows=@()
for($pass=1;$pass -le $Passes;$pass++){
  $ordered=@($variants)
  if($pass%2 -eq 0){[array]::Reverse($ordered)}
  foreach($case in $ordered){
    $id=$case.id
    $folder=Join-Path $out ("P{0:D2}_{1}" -f $pass,$id)
    New-Item -ItemType Directory -Path $folder -Force|Out-Null
    $args=@($scene,$folder,"--quick","--all-cameras",
      "--resolution",$case.res,"--frames","$Frames",
      "--hour","12","--readback-depth","$($case.depth)",
      "--raster-msaa","$($case.msaa)","--raster-anisotropy","8",
      "--raster-lod-percent","100","--raster-culling","1",
      "--raster-lod-enabled","1")
    Write-Host "Running $id ($pass/$Passes) on ALL stored scene cameras"
    $log=& $exe @args 2>&1
    $exit=$LASTEXITCODE
    $log|Out-File (Join-Path $folder "console.txt") -Encoding UTF8
    $summaries=@(Get-ChildItem $folder -Recurse -Filter summary.json -File)
    if($exit -ne 0 -or $summaries.Count -ne 1){
      throw "Benchmark $id pass $pass failed exit=$exit summaries=$($summaries.Count); see $folder/console.txt"
    }
    $report=Get-Content $summaries[0].FullName -Raw|ConvertFrom-Json
    $cases=@($report.cases)
    if($cases.Count -lt 2){throw "Expected two or more real scene camera views. Got $($cases.Count) in $id"}
    foreach($result in $cases){
      if($result.status -ne "ok" -or -not $result.gpuTimingValid -or -not $result.gpuTransferSplitValid){
        throw "GPU timestamp split unavailable or rendering failed: $id / camera=$($result.camera)"
      }
      if([int]$result.readbackDepth -ne $case.depth){throw "Wrong readback depth: $id"}
      if([int]$result.frames -ne $Frames){throw "Wrong frame count: $id"}
    }
    $rows+= [PSCustomObject]([ordered]@{
      id=$id
      pass=$pass
      msaa=$case.msaa
      readbackDepth=$case.depth
      resolution=$case.res
      requestedFrames=$Frames
      summary=$summaries[0].FullName
      cameras=@($cases|ForEach-Object{$_.camera})
    })
    $rows|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out "manifest.json") -Encoding UTF8
  }
}
Write-Host "Done. Run python tools/analyze_msaa_multicam_v443.py `"$out`""
