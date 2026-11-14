param(
    [Parameter(Mandatory=$false)]
    [string]$Root = $PSScriptRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
try {
    Add-Type -AssemblyName System.IO.Compression
    $rootPath = (Resolve-Path -LiteralPath $Root).ProviderPath.TrimEnd([char]'\\',[char]'/')
    $results = @(Get-ChildItem -LiteralPath $rootPath -Directory -Filter 'FINITIONS_*')
    $logs = @(Get-ChildItem -LiteralPath $rootPath -File -Filter 'finitions_*.log')
    $files = New-Object 'System.Collections.Generic.List[System.IO.FileInfo]'
    foreach ($folder in $results) {
        foreach ($file in (Get-ChildItem -LiteralPath $folder.FullName -File -Recurse)) {
            $files.Add($file)
        }
    }
    foreach ($file in $logs) { $files.Add($file) }
    if ($files.Count -eq 0) {
        throw 'Aucun resultat a compresser. Chercher les dossiers FINITIONS_* et les fichiers finitions_*.log.'
    }
    $destination = Join-Path $rootPath 'CSE_WARP_VULKAN_FINITIONS_HD620.zip'
    $temporary = "$destination.partial"
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    Write-Host ("Dossiers de test : {0} ; journaux : {1} ; fichiers : {2}" -f $results.Count,$logs.Count,$files.Count)
    Write-Host 'Compression ZIP par flux : pas de limite Compress-Archive de 2 Go par fichier.'
    $stream = [System.IO.File]::Open($temporary,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None)
    try {
        $zip = [System.IO.Compression.ZipArchive]::new(
            $stream,[System.IO.Compression.ZipArchiveMode]::Create,$true)
        try {
            $index = 0
            foreach ($file in $files) {
                $relative = $file.FullName.Substring($rootPath.Length).TrimStart([char]'\\',[char]'/').Replace('\\','/')
                $entry = $zip.CreateEntry($relative,[System.IO.Compression.CompressionLevel]::Fastest)
                $source = [System.IO.File]::OpenRead($file.FullName)
                try {
                    $target = $entry.Open()
                    try { $source.CopyTo($target) }
                    finally { $target.Dispose() }
                } finally { $source.Dispose() }
                $index++
                if (($index % 10) -eq 0 -or $index -eq $files.Count) {
                    Write-Host ("ZIP : {0}/{1} fichiers" -f $index,$files.Count)
                }
            }
        } finally { $zip.Dispose() }
    } finally { $stream.Dispose() }

    # Verify the central directory before replacing a previous successful ZIP.
    $verifyStream = [System.IO.File]::OpenRead($temporary)
    try {
        $check = [System.IO.Compression.ZipArchive]::new(
            $verifyStream,[System.IO.Compression.ZipArchiveMode]::Read,$true)
        try {
            if ($check.Entries.Count -ne $files.Count) {
                throw ("Archive incomplete : {0} fichiers au lieu de {1}" -f $check.Entries.Count,$files.Count)
            }
            $expectedBytes = [long]0
            foreach ($f in $files) { $expectedBytes += $f.Length }
            $actualBytes = [long]0
            foreach ($e in $check.Entries) { $actualBytes += $e.Length }
            if ($actualBytes -ne $expectedBytes) {
                throw ("Longueurs originales incoherentes : {0} / {1}" -f $actualBytes,$expectedBytes)
            }
        } finally { $check.Dispose() }
    } finally { $verifyStream.Dispose() }
    if (Test-Path -LiteralPath $destination) {
        Remove-Item -LiteralPath $destination -Force
    }
    Move-Item -LiteralPath $temporary -Destination $destination
    $bytes = (Get-Item -LiteralPath $destination).Length
    Write-Host ("ZIP TERMINE : {0}" -f $destination)
    Write-Host ("Taille : {0:N1} Mo ; fichiers verifies : {1}" -f ($bytes/1MB),$files.Count)
    exit 0
} catch {
    Write-Host ("ECHEC ZIP : {0}" -f $_.Exception.Message)
    Write-Host $_.ScriptStackTrace
    Write-Host 'Les resultats FINITIONS_* et les journaux ne sont pas modifies.'
    exit 1
}
