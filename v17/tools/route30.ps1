#requires -version 5.1
param(
    [string]$Root,
    [ValidateSet('on','off')][string]$GeometryPrep = 'on'
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

function Invoke-Route30([string]$PackageRoot, [ValidateSet('on','off')][string]$PrepMode = 'on') {
    $PrepMode = $PrepMode.ToLowerInvariant()
    $PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).ProviderPath
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

        $exe = Join-Path $PackageRoot 'route30.exe'
        if (Test-Path -LiteralPath $exe -PathType Leaf) {
            Write-Host "Utilisation du moteur de parcours inclus dans le package V17."
        } else {
            $helperUrl = 'https://github.com/glaby91/moteur3d/releases/download/route30-ee05a7f0d68a5c5c31a5d42fc7e3ddf29232c700/route30.zip'
            $helperHash = '20e745099002a40091a4a2624f5757b1a8d63e7894f7288a4ab41ae392887739'
            $toolZip = Get-RouteArchive $cache 'route30' $helperUrl $helperHash 423462
            $bin = Join-Path $stage 'bin'
            [IO.Compression.ZipFile]::ExtractToDirectory($toolZip, $bin)
            $exe = Join-Path $bin 'route30.exe'
        }
        $out = Join-Path $PackageRoot ('benchmark-results\parcours30-' + (Get-Date -Format yyyyMMdd-HHmmssfff))
        [IO.Directory]::CreateDirectory($out) | Out-Null
        $versionPath = Join-Path $out 'SCENE_VERSION.json'
        Copy-Item -LiteralPath $scene.Manifest -Destination $versionPath
        $csv = Join-Path $out 'resultats.csv'
        Write-Host "Parcours GPU direct : 1280 x 720, VSync activee, 30 s par backend. Preparation des triangles : $PrepMode."
        $log = Join-Path $out 'diagnostic.log'
        $stdout = Join-Path $out 'stdout.log'
        $stderr = Join-Path $out 'stderr.log'
        $start = Get-Date
        $process = Start-Process -FilePath $exe -ArgumentList @(('"{0}"' -f $scene.Path), ('"{0}"' -f $csv), '--geometry-prep', $PrepMode) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -Wait
        $engineExit = $process.ExitCode
        "ExitCode=$engineExit; start=$start; end=$(Get-Date); prep=$PrepMode" | Set-Content -LiteralPath $log
        Get-Content -LiteralPath $stdout,$stderr | Add-Content -LiteralPath $log
        Get-Content -LiteralPath $log
        Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,AdapterRAM | ConvertTo-Json | Set-Content (Join-Path $out 'gpu.json')
        try {
            Get-WinEvent -FilterHashtable @{LogName='System';StartTime=$start.AddSeconds(-10)} -ErrorAction Stop |
                Where-Object { $_.ProviderName -match 'Display|DxgKrnl|nvlddmkm|igfx|amdkmdag|WHEA' } |
                Select-Object TimeCreated,Id,ProviderName,Message | ConvertTo-Json -Depth 3 |
                Set-Content (Join-Path $out 'gpu-events.json')
        } catch { "No matching events or access unavailable: $_" | Set-Content (Join-Path $out 'gpu-events.json') }
        if (!(Test-Path -LiteralPath $csv -PathType Leaf)) {
            throw "Aucun CSV produit (code moteur $engineExit). Dossier : $out"
        }
        Get-Content -LiteralPath $csv
        Compress-Archive -Path (Join-Path $out "*") -DestinationPath "$out.zip"
        if ($engineExit -ne 0) { throw "Campagne incomplete (code moteur $engineExit). Resultats : $out.zip" }
        Write-Host "Resultats : $out.zip"
    } finally {
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
}

if ($MyInvocation.InvocationName -ne '.') { Invoke-Route30 -PackageRoot $Root -PrepMode $GeometryPrep }
