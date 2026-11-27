@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist "moteur3d_v17_gpu_trajectory.exe" goto missing
if not exist "route30.ps1" goto missing
set "DX11_AB_BAT=%~f0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $lines=Get-Content -LiteralPath $env:DX11_AB_BAT; $i=[Array]::IndexOf($lines,':POWERSHELL'); if($i -lt 0){throw 'Script missing'}; & ([scriptblock]::Create(($lines[($i+1)..($lines.Length-1)] -join [Environment]::NewLine)))"
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo ECHEC : code %RC%. Conserver les resultats partiels.
pause
exit /b %RC%
:missing
echo Placer ce BAT a cote de moteur3d_v17.exe dans le package V17.025 complet.
pause
exit /b 2
:POWERSHELL
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = (Get-Location).Path
. (Join-Path $root 'route30.ps1') -Root $root
$stamp = Get-Date -Format yyyyMMdd-HHmmssfff
$out = Join-Path $root "benchmark-results\DX11-R001-AB-$stamp"
$stage = Join-Path ([IO.Path]::GetTempPath()) ('warp-dx11-ab-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $out,$stage -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
try {
    $scene = Get-RouteScene (Join-Path $root 'benchmark-tools\archives') $stage
    $meta = Get-Content -LiteralPath $scene.Manifest -Raw | ConvertFrom-Json
    if ($meta.revision -ne 'V17.631') { throw "Scene attendue V17.631, recue $($meta.revision). Comparaison annulee." }
    Copy-Item -LiteralPath $scene.Manifest -Destination $out
    if (Test-Path 'BUILD_INFO.json') { Copy-Item 'BUILD_INFO.json' $out }
    $plugins = @(@($root,(Join-Path $root 'plugins')) | Where-Object { Test-Path (Join-Path $_ 'imageformats\qjpeg.dll') })
    if (!$plugins.Count) { throw 'Plugin JPEG Qt manquant.' }
    $env:QT_PLUGIN_PATH = $plugins -join ';'
    $env:PATH = "$root;$env:PATH"
    $env:M3D_V17_STATIC_GEOMETRY_PREP = '1'
    $env:M3D_V17_PERIPHERAL_MULTIREZ = '0'
    $env:M3D_V17_ANIMATE_CLOUDS = '0'
    $env:M3D_V17_DISABLE_FRUSTUM_CULLING = '0'
    $env:M3D_V17_DISABLE_GPU_INSTANCING = '0'
    $env:M3D_V17_DISABLE_BOX_INSTANCING = '0'
    $env:M3D_V17_DISABLE_ELLIPSE_INSTANCING = '0'
    $env:M3D_V17_DISABLE_FOLIAGE_INSTANCING = '0'
    $env:M3D_V17_DISABLE_LOD = '0'
    $env:M3D_V17_DISABLE_SHADER_OPT = '0'
    $env:M3D_V17_DISABLE_LIGHT_AABB_CULL = '0'
    $exe = Join-Path $root 'moteur3d_v17_gpu_trajectory.exe'
    $rows = @()
    $order = @(0,1,1,0)
    for ($i=0; $i -lt $order.Count; $i++) {
        $mode = $order[$i]
        $env:M3D_V17_DX11_R001 = [string]$mode
        $label = if ($mode -eq 0) {'OFF'} else {'ON'}
        $run = Join-Path $out ('{0:D2}-R001-{1}' -f ($i+1),$label)
        New-Item -ItemType Directory -Path $run | Out-Null
        Write-Host "=== Passe $($i+1)/4 : DX11 R001 $label ==="
        & $exe $scene.Path $run '--z-prepass=on' '--light-aabb=on'
        if ($LASTEXITCODE -ne 0) { throw "Benchmark echoue : passe $($i+1), code $LASTEXITCODE" }
        $file = Get-ChildItem -LiteralPath $run -Recurse -Filter summary.json | Select-Object -First 1
        if (!$file) { throw 'summary.json manquant.' }
        $s = (Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json).summary
        $rows += [pscustomobject]@{pass=$i+1;mode=$label;fps=$s.fpsDuringCalculation;mean_ms=$s.meanWallMs;p95_ms=$s.p95WallMs;gpu_draw_ms=$s.gpuDrawMeanMs;scene=$meta.revision;width=$s.width;height=$s.height}
        $rows | Export-Csv -LiteralPath (Join-Path $out 'COMPARAISON.csv') -NoTypeInformation -Encoding UTF8
    }
    $off = ($rows | Where-Object mode -eq 'OFF' | Measure-Object mean_ms -Average).Average
    $on = ($rows | Where-Object mode -eq 'ON' | Measure-Object mean_ms -Average).Average
    if ($off -le 0 -or $on -le 0) { throw 'Temps moyens invalides.' }
    $gain = 100*($off/$on-1)
    $report = [pscustomobject]@{fps_off=1000/$off;fps_on=1000/$on;gain_fps_pct=$gain;mean_ms_off=$off;mean_ms_on=$on;scene=$meta.revision;order='OFF ON ON OFF';note='DX11 hardware preview with readback; 90 frames per pass; not native DXGI Present.'}
    $report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'BILAN.json') -Encoding UTF8
    $report | Format-List | Out-Host
    $zip = "$out.zip"
    [IO.Compression.ZipFile]::CreateFromDirectory($out,$zip)
    Write-Host "SUCCESS : envoyer $zip"
} finally {
    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
}
