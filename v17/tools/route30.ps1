param([string]$Root)
$ErrorActionPreference='Stop'
$scene=Join-Path $Root 'benchmark-civic\scene.json'
if(!(Test-Path -LiteralPath $scene)){throw "Scene absente: $scene"}
$release='8fbc70d9ff008304c6b83121bd290b2cdaec46c3'
$cache=Join-Path $Root "benchmark-tools\route30-$release"
$exe=Join-Path $cache 'route30.exe'
if(!(Test-Path $exe)){
 $stage=Join-Path $env:TEMP ([guid]::NewGuid().ToString())
 New-Item -Type Directory $stage | Out-Null
 try {
  $url="https://github.com/glaby91/moteur3d/releases/download/route30-$release/route30.zip"
  Invoke-WebRequest -UseBasicParsing $url -OutFile "$stage\tool.zip"
  Expand-Archive "$stage\tool.zip" "$stage\bin"
  New-Item -Type Directory -Force (Split-Path $cache) | Out-Null
  Move-Item "$stage\bin" $cache
 } finally {Remove-Item $stage -Recurse -Force}
}
$env:PATH="$Root;$env:PATH"
$out=Join-Path $Root ('benchmark-results\parcours30-'+(Get-Date -Format yyyyMMdd-HHmmssfff))
New-Item -Type Directory -Force $out | Out-Null
& $exe $scene "$out\resultats.csv"
if($LASTEXITCODE){throw "Echec moteur $LASTEXITCODE - sorties partielles: $out"}
Get-Content "$out\resultats.csv"
Compress-Archive "$out\resultats.csv" "$out.zip"
Write-Host "Resultats: $out.zip"
