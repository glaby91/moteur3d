$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$partsDirectory = Join-Path $repositoryRoot 'tools\assets\v17_603_scene_overlay'
$parts = @(Get-ChildItem -LiteralPath $partsDirectory -Filter 'part-*.bin' -File | Sort-Object Name)

if ($parts.Count -ne 43) {
    throw "Expected 43 V17.603 overlay parts, found $($parts.Count)."
}

$buildDirectory = Join-Path $repositoryRoot 'build-v17'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
$zipPath = Join-Path $buildDirectory 'v17.600-to-v17.603-scene-overlay.zip'
$stream = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::Create)

try {
    foreach ($part in $parts) {
        $bytes = [System.IO.File]::ReadAllBytes($part.FullName)
        $stream.Write($bytes, 0, $bytes.Length)
    }
}
finally {
    $stream.Dispose()
}

Write-Host "Reconstructed ZIP: $zipPath"
