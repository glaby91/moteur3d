# CSE WARP V17.437 default reproducible performance campaign.
# Real DirectX11 hardware; three identical 90-frame trajectories, 1366x768.
# Never silently enable WARP software or change PBR/MSAA settings.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$exe = Join-Path $root 'moteur3d_v17_gpu_trajectory.exe'
$scene = Join-Path $root 'benchmark-civic/scene-v7-corrections.json'
if(!(Test-Path $exe)){throw "Benchmark engine not found: $exe"}
if(!(Test-Path $scene)){throw "Architectural scene not found: $scene"}
$env:M3D_V16_FORCE_WARP = ''
$folder = Join-Path $root ("RESULTATS-V17.437-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$profiles = @(
    @{name='default_z_on_aabb_off'; z='on'; aabb='off'},
    @{name='reference_z_off_aabb_off'; z='off'; aabb='off'},
    @{name='candidate_z_on_aabb_on'; z='on'; aabb='on'}
)
foreach($profile in $profiles){
    $dest=Join-Path $folder $profile.name
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    Write-Host ("Running " + $profile.name + " on the GPU. 90 frames each; PBR 2048; MSAA 2x.")
    & $exe $scene $dest ("--z-prepass="+$profile.z) ("--light-aabb="+$profile.aabb)
    if($LASTEXITCODE -ne 0){throw "Benchmark failed on $($profile.name): exit $LASTEXITCODE"}
}
$rows = @()
foreach($profile in $profiles){
    $dir = Join-Path $folder $profile.name
    $summaries=@(Get-ChildItem $dir -Filter summary.json -Recurse)
    if($summaries.Count -ne 1){throw "Exactly one summary expected for $($profile.name)"}
    $s=(Get-Content $summaries[0].FullName -Raw | ConvertFrom-Json).summary
    if([bool]$s.zPrepassEnabled -ne ($profile.z -eq 'on') -or
       [bool]$s.lightAabbCullEnabled -ne ($profile.aabb -eq 'on')){
        throw "Unexpected effective experiment switches in $($profile.name)"
    }
    $rows += [pscustomobject]@{
        Profile=$profile.name
        ZPrepass=$profile.z
        LightAABB=$profile.aabb
        Frames=$s.sampleFrames
        MedianRenderMs=$s.medianWallMs
        P95RenderMs=$s.p95WallMs
        P99RenderMs=$s.p99WallMs
        MeanRenderMs=$s.meanWallMs
        FPSDuringCalculation=$s.fpsDuringCalculation
        GPUDrawMeanMs=$s.gpuDrawMeanMs
    }
}
$rows | Export-Csv (Join-Path $folder 'COMPARAISON-V17.437.csv') -NoTypeInformation -Delimiter ';' -Encoding UTF8
$rows | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $folder 'COMPARAISON-V17.437.json') -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host "CSV and JSON saved to $folder"
Write-Host "These are GPU calculation times, NOT actual Qt image presentation times."
Write-Host "For screen stutter: explore the scene then Outils -> Exporter benchmark fluidite."
