#requires -version 5.1
param(
    [string]$Root,
    [ValidateSet('on','off')][string]$GeometryPrep = 'on',
    [switch]$AdaptiveAB,
    [switch]$AdaptiveTour
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Test-RouteArchive($Path, $Sha256, $Size) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    if ((Get-Item -LiteralPath $Path).Length -ne $Size) { return $false }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $Sha256
}

function Get-RouteArchive($Cache, $Name, $Url, $Sha256, $Size) {
    if ($Sha256 -notmatch '^[a-f0-9]{64}$' -or $Size -le 0) {
        throw "Metadonnees de telechargement invalides pour $Name."
    }
    [IO.Directory]::CreateDirectory($Cache) | Out-Null
    $path = Join-Path $Cache "$Name-$Sha256.zip"
    if (Test-RouteArchive $path $Sha256 $Size) {
        Write-Host "Cache verifie : $Name"
        return $path
    }
    Write-Host "Telechargement : $Name ($Size octets)"
    $partial = Join-Path $Cache ([guid]::NewGuid().ToString() + '.part')
    try {
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $partial
        if (!(Test-RouteArchive $partial $Sha256 $Size)) {
            throw "Archive $Name incomplete ou SHA-256 incorrect. Relancer le BAT."
        }
        try {
            Move-Item -LiteralPath $partial -Destination $path -Force
        } catch {
            # Another benchmark can have published the same verified archive first.
            if (!(Test-RouteArchive $path $Sha256 $Size)) { throw }
        }
        return $path
    } finally {
        if ([IO.File]::Exists($partial)) { [IO.File]::Delete($partial) }
    }
}

function Get-RouteScene($Cache, $Stage) {
    Write-Host 'Recherche de la derniere scene publiee sur glaby91/moteur3d-scene...'
    $headers = @{ Accept = 'application/vnd.github+json'; 'User-Agent' = 'CSE-WARP-Parcours30' }
    $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/glaby91/moteur3d-scene/releases/latest' -Headers $headers
    $archive = $release.assets | Where-Object name -eq 'V17-scene.zip' | Select-Object -First 1
    $version = $release.assets | Where-Object name -eq 'SCENE_VERSION.json' | Select-Object -First 1
    if (!$archive -or !$version) { throw "Release de scene incomplete : $($release.tag_name)" }
    $archiveHash = ([string]$archive.digest) -replace '^sha256:', ''
    $zip = Get-RouteArchive $Cache 'V17-scene' $archive.browser_download_url $archiveHash $archive.size

    $directory = Join-Path $Stage 'scene'
    Write-Host "Extraction de la scene officielle : $($release.tag_name)"
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $directory)
    $versionPath = Join-Path $directory 'SCENE_VERSION.json'
    $versionHash = ([string]$version.digest) -replace '^sha256:', ''
    if (!(Test-RouteArchive $versionPath $versionHash $version.size)) {
        throw "Le manifeste dans le ZIP differe de celui de la release $($release.tag_name)."
    }
    $manifest = Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.scene -ne 'benchmark-civic/scene.json' -or !$manifest.revision -or
        $manifest.repository -ne 'glaby91/moteur3d-scene' -or
        $release.tag_name -notlike "scene-$($manifest.revision)-*") {
        throw "Le manifeste ne correspond pas a la scene annoncee : $($release.tag_name)"
    }
    $scenePath = Join-Path $directory $manifest.scene
    $scene = Get-Content -LiteralPath $scenePath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($scene.revision -ne $manifest.revision) { throw "Revision scene.json incoherente : $($scene.revision)" }
    $fresco = Join-Path (Split-Path -LiteralPath $scenePath) 'textures\energetik_fresco_mosaic.jpg'
    if (!(Test-Path -LiteralPath $fresco -PathType Leaf) -or (Get-Item -LiteralPath $fresco).Length -eq 0) {
        throw "Texture de fresque absente de la scene publiee : $fresco"
    }
    Write-Host "Scene prete : $($manifest.revision)"
    return [pscustomobject]@{ Path = $scenePath; Manifest = $versionPath }
}

# Local hardware only. No host name, account, serial number or network identifier.
# Collect before launching the renderer so metadata survives a shutdown hang.
function Get-RouteSystemInfo([string]$PackageRoot, [string]$Exe, [string]$PrepMode) {
    $warnings = New-Object 'System.Collections.Generic.List[string]'
    $cpu = @()
    $gpu = @()
    $memory = @()
    $system = $null
    $os = $null
    $build = $null
    $exeHash = $null
    $exeVersion = $null
    try {
        $cpu = @(Get-CimInstance -ClassName Win32_Processor -OperationTimeoutSec 10 -ErrorAction Stop |
            Select-Object Name, Manufacturer, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed)
    } catch { $warnings.Add('CPU inventory unavailable') }
    try {
        $gpu = @(Get-CimInstance -ClassName Win32_VideoController -OperationTimeoutSec 10 -ErrorAction Stop |
            Select-Object Name, DriverVersion, DriverDate, VideoProcessor, CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate, Status)
    } catch { $warnings.Add('GPU inventory unavailable') }
    try {
        $memory = @(Get-CimInstance -ClassName Win32_PhysicalMemory -OperationTimeoutSec 10 -ErrorAction Stop |
            Select-Object Capacity, Speed, ConfiguredClockSpeed, SMBIOSMemoryType)
    } catch { $warnings.Add('Memory module inventory unavailable') }
    try {
        $system = Get-CimInstance -ClassName Win32_ComputerSystem -OperationTimeoutSec 10 -ErrorAction Stop |
            Select-Object Manufacturer, Model, TotalPhysicalMemory, SystemType
    } catch { $warnings.Add('System inventory unavailable') }
    try {
        $os = Get-CimInstance -ClassName Win32_OperatingSystem -OperationTimeoutSec 10 -ErrorAction Stop |
            Select-Object Caption, Version, BuildNumber, OSArchitecture
    } catch { $warnings.Add('Windows inventory unavailable') }
    try {
        $build = Get-Content -LiteralPath (Join-Path $PackageRoot 'BUILD_INFO.json') -Raw -Encoding UTF8 |
            ConvertFrom-Json -ErrorAction Stop
    } catch { $warnings.Add('Package BUILD_INFO.json unavailable or invalid') }
    try {
        $exeHash = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
        $exeVersion = (Get-Item -LiteralPath $Exe -ErrorAction Stop).VersionInfo.FileVersion
    } catch { $warnings.Add('Executable fingerprint unavailable') }
    return [ordered]@{
        schemaVersion = 1
        capturedUtc = [DateTime]::UtcNow.ToString('o')
        computer = $system
        processors = $cpu
        videoControllers = $gpu
        physicalMemory = $memory
        windows = $os
        package = $build
        benchmarkExecutable = [ordered]@{ fileName = 'route30.exe'; sha256 = $exeHash; fileVersion = $exeVersion }
        benchmark = [ordered]@{ width = 1280; height = 720; geometryPreparation = $PrepMode; vsyncRequested = 'off'; indirectLightingRequested = $true }
        warnings = @($warnings.ToArray())
    }
}

function Write-RouteSystemInfo([string]$PackageRoot, [string]$Exe, [string]$PrepMode, [string]$Path) {
    $info = Get-RouteSystemInfo $PackageRoot $Exe $PrepMode
    $info | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Invoke-Route30([string]$PackageRoot, [ValidateSet('on','off')][string]$PrepMode = 'on', [switch]$AdaptiveAB, [switch]$AdaptiveTour) {
    $PrepMode = $PrepMode.ToLowerInvariant()
    $PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).ProviderPath
    # Persist machine metadata at launch, even if scene download or GPU startup fails.
    $out = Join-Path $PackageRoot ('benchmark-results\parcours30-' + (Get-Date -Format yyyyMMdd-HHmmssfff))
    [IO.Directory]::CreateDirectory($out) | Out-Null
    $exe = Join-Path $PackageRoot 'route30.exe'
    $systemInfoPath = Join-Path $out 'SYSTEM_INFO.json'
    Write-Host "Dossier de resultats : $out"
    Write-Host 'Collecte de la configuration materielle...'
    Write-RouteSystemInfo $PackageRoot $exe $PrepMode $systemInfoPath
    Write-Host "Configuration materielle : $systemInfoPath"
    $cache = Join-Path $PackageRoot 'benchmark-tools\archives'
    $stage = Join-Path ([IO.Path]::GetTempPath()) ('warp-parcours-' + [guid]::NewGuid())
    [IO.Directory]::CreateDirectory($stage) | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    try {
        $scene = Get-RouteScene $cache $stage
        $pluginRoots = @(@($PackageRoot, (Join-Path $PackageRoot 'plugins')) | Where-Object {
            Test-Path -LiteralPath (Join-Path $_ 'imageformats\qjpeg.dll') -PathType Leaf
        })
        if (!$pluginRoots.Count) {
            throw "Decodeur JPEG Qt introuvable : placer le BAT dans le package V17 complet (imageformats\qjpeg.dll ou plugins\imageformats\qjpeg.dll)."
        }
        $env:QT_PLUGIN_PATH = (@($pluginRoots) + @($env:QT_PLUGIN_PATH) | Where-Object { $_ }) -join ';'
        $env:PATH = "$PackageRoot;$env:PATH"

        if (Test-Path -LiteralPath $exe -PathType Leaf) {
            Write-Host "Utilisation du moteur de parcours inclus dans le package V17."
        } else {
            throw 'route30.exe manquant : utiliser le package V17 complet (ancienne version de secours incompatible avec VSync OFF).'
        }
        $versionPath = Join-Path $out 'SCENE_VERSION.json'
        Copy-Item -LiteralPath $scene.Manifest -Destination $versionPath
        $info = Get-Content -LiteralPath $systemInfoPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $actualScene = Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $info | Add-Member -NotePropertyName testedScene -NotePropertyValue $actualScene -Force
        $mode = if ($AdaptiveTour) { 'adaptive-tour-all-cameras' } elseif ($AdaptiveAB) { 'adaptive-ab-pool' } else { 'route30' }
        $info.benchmark | Add-Member -NotePropertyName campaign -NotePropertyValue $mode -Force
        $info | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $systemInfoPath -Encoding UTF8
        $csv = Join-Path $out 'resultats.csv'
        Write-Host "Parcours GPU direct : 1280 x 720, VSync desactivee, rebonds indirects actives, 30 s par backend. Preparation des triangles : $PrepMode."
        if ($AdaptiveTour) { Write-Host 'Circuit integral : toutes les cameras, quatre horaires fixes et transition jour/nuit.'; & $exe $scene.Path $csv --geometry-prep $PrepMode --adaptive-tour }
        elseif ($AdaptiveAB) { & $exe $scene.Path $csv --geometry-prep $PrepMode --adaptive-ab }
        else { & $exe $scene.Path $csv --geometry-prep $PrepMode }
        $engineExit = $LASTEXITCODE
        if (!(Test-Path -LiteralPath $csv -PathType Leaf)) {
            throw "Aucun CSV produit (code moteur $engineExit). Dossier : $out"
        }
        Get-Content -LiteralPath $csv
        Compress-Archive -LiteralPath @($csv, $versionPath, $systemInfoPath) -DestinationPath "$out.zip"
        if ($engineExit -ne 0) { throw "Campagne incomplete (code moteur $engineExit). Resultats : $out.zip" }
        Write-Host "Resultats : $out.zip"
    } finally {
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
}

if ($MyInvocation.InvocationName -ne '.') { Invoke-Route30 -PackageRoot $Root -PrepMode $GeometryPrep -AdaptiveAB:$AdaptiveAB -AdaptiveTour:$AdaptiveTour }
