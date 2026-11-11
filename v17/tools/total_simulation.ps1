# CSE WARP V17: exhaustive functional WARP simulation in one existing Windows build.
# All 6 scenes, 2 cameras, 2 resolutions, 3 buffered modes, repeated frames,
# CPU scaling, Compute, geometry/PBR/BVH/shadows/memory and identical PNGs.
param(
    [string]$BinaryDirectory = "build-v17/Release",
    [string]$OutputDirectory = "build-v17/total-simulation"
)
$ErrorActionPreference = "Stop"
$env:M3D_V16_FORCE_WARP = "1"
$exe = Join-Path $BinaryDirectory "moteur3d_v17_campaign.exe"
$scene = Join-Path $BinaryDirectory "benchmark-civic/scene-extreme.json"
if (!(Test-Path $exe) -or !(Test-Path $scene)) {
    throw "Simulation prerequisites missing: campaign EXE or extreme scene"
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
Write-Host "=== FULL CSE WARP V17 FUNCTIONAL SIMULATION: WARP SOFTWARE RENDERER ==="
Write-Host "6 scenes / 2 cameras / 2 frame sizes / 3 readback modes / 2 rounds / CPU and Compute"
& $exe $scene $OutputDirectory --simulation --all-scenes --include-cpu --raster-msaa 2 --raster-anisotropy 8 --raster-lod-percent 100 --raster-culling 1 --raster-lod-enabled 1
if ($LASTEXITCODE -ne 0) {
    throw "Full simulation failed: exit=$LASTEXITCODE. Inspect partial JSON in $OutputDirectory"
}
$summary = Get-ChildItem $OutputDirectory -Recurse -Filter summary.json | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$summary) { throw "Simulation summary.json missing" }
$s = Get-Content $summary.FullName -Raw | ConvertFrom-Json
$root = $summary.Directory.FullName
$cases = @($s.cases)
$side = @($s.subsystemTests)
$expected = 72
if (!$s.complete -or !$s.system.simulation -or !$s.system.allScenes -or $s.system.sceneCount -ne 6 -or $cases.Count -ne $expected) {
    throw "Incomplete scene matrix: scenes=$($s.system.sceneCount), Raster=$($cases.Count)/$expected"
}
$errors = @($cases | Where-Object {
    $_.status -ne "ok" -or !$_.pixelIdentical -or $_.roundsCompleted -ne 2 -or
    $_.frames -ne 4 -or $_.p95Ms -le 0 -or $_.streamFps -le 0 -or
    $_.gpuDrawCalls -le 0 -or $_.width -notin @(256,480) -or
    $_.height -notin @(144,270) -or $_.readbackDepth -notin @(1,2,3)
})
if ($errors.Count) {
    $errors | Format-Table scene,camera,width,height,readbackDepth,status,error | Out-String | Write-Host
    throw "$($errors.Count) invalid Raster results"
}
$groups = @($cases | Group-Object -Property {
    "$($_.scene)|$($_.camera)|$($_.width)|$($_.height)"
})
if ($groups.Count -ne 24) { throw "Expected 24 scene/camera/size groups, got $($groups.Count)" }
$pngCount=0
foreach ($group in $groups) {
    $modes=@($group.Group | Sort-Object -Property readbackDepth)
    if ($modes.Count -ne 3 -or $modes[0].readbackDepth -ne 1 -or
        $modes[1].readbackDepth -ne 2 -or $modes[2].readbackDepth -ne 3) {
        throw "1/2/3 buffer coverage incomplete: $($group.Name)"
    }
    $hashes=@()
    foreach ($mode in $modes) {
        $image=Join-Path $root $mode.image
        if (!(Test-Path $image) -or (Get-Item $image).Length -lt 500) {
            throw "Missing or empty rendered image: $image"
        }
        $hashes+=(Get-FileHash $image -Algorithm SHA256).Hash
        $pngCount++
    }
    if (@($hashes | Select-Object -Unique).Count -ne 1) {
        throw "Different PNG pixels with 1/2/3 buffers: $($group.Name)"
    }
}
$parse=@($side | Where-Object {$_.variant -eq "scene_parse" -and $_.status -eq "ok"})
$pbr=@($side | Where-Object {$_.variant -eq "pbr_mip_generation" -and $_.elapsedMs -ge 0})
$geom=@($side | Where-Object {$_.variant -eq "geometry_instancing" -and $_.elapsedMs -ge 0})
$bvh=@($side | Where-Object {$_.variant -eq "bvh_build" -and $_.elapsedMs -ge 0})
$shadows=@($side | Where-Object {$_.variant -eq "shadowmap_cpu_build" -and $_.status -eq "ok"})
$memory=@($side | Where-Object {$_.variant -eq "memory_after_scene_build" -and $_.workingMiB -gt 0})
$cpu=@($side | Where-Object {$_.variant -eq "cpu_direct_scaling" -and $_.runs -eq 2})
$compute=@($side | Where-Object {$_.variant -eq "d3d11_compute_96x54" -and $_.status -eq "ok"})
foreach ($test in @(
    @{name="parse";count=$parse.Count;minimum=6},
    @{name="PBR";count=$pbr.Count;minimum=6},
    @{name="geometry";count=$geom.Count;minimum=6},
    @{name="BVH";count=$bvh.Count;minimum=6},
    @{name="shadows";count=$shadows.Count;minimum=6},
    @{name="memory";count=$memory.Count;minimum=6},
    @{name="CPU";count=$cpu.Count;minimum=1},
    @{name="Compute";count=$compute.Count;minimum=1}
)) {
    if ($test.count -lt $test.minimum) {
        throw "Sub-test $($test.name) has only $($test.count)/$($test.minimum) results"
    }
}
$frameCsv = Join-Path $root "campaign_frames.csv"
if (!(Test-Path $frameCsv)) { throw "Frame-level CSV is missing" }
$frames = @(Import-Csv $frameCsv)
if ($frames.Count -ne 288) { throw "Expected 288 measured frames, got $($frames.Count)" }
$maxMemory = ($memory | Measure-Object -Property workingMiB -Maximum).Maximum
if ($maxMemory -ge 8192) { throw "Working set exceeded 8 GiB simulation budget" }
$verdict = [ordered]@{
    engine = "CSE WARP V17"
    version = $s.system.version
    result = "PASS"
    gpu = "Microsoft WARP (software, not Intel HD 620)"
    scenes = 6
    rasterCases = $cases.Count
    cameraResolutionGroups = $groups.Count
    framesMeasured = $frames.Count
    imagesVerified = $pngCount
    cpuScalingConfigurations = $cpu.Count
    computeTestsPassed = $compute.Count
    maximumSceneWorkingMiB = [math]::Round($maxMemory,2)
    graphicsSettings = $s.system.quality
    note = "Functional matrix only. No claim of native GPU FPS or final 2048px image quality."
}
$verdictPath = Join-Path $root "total_simulation_verdict.json"
$verdict | ConvertTo-Json -Depth 8 | Set-Content $verdictPath -Encoding UTF8
Write-Host "=== TOTAL SIMULATION PASS ==="
Write-Host "Scenes: 6; Raster: $($cases.Count); frames: $($frames.Count); PNGs: $pngCount; CPU: $($cpu.Count); Compute: $($compute.Count)"
Write-Host "Highest scene working set (MiB): $maxMemory"
Write-Host "Verdict: $verdictPath"
