param([string]$Root)
$ErrorActionPreference='Stop'
$scene=Join-Path $Root 'benchmark-civic\scene.json'
if(!(Test-Path -LiteralPath $scene)){throw "Scene absente: $scene"}
$release='f33ed2bf03ef7a2e068aeb93f0c4b91279553e95'
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
$pluginRoots=@($Root,(Join-Path $Root 'plugins')) | Where-Object {
 Test-Path -LiteralPath (Join-Path $_ 'imageformats\qjpeg.dll') -PathType Leaf
}
if(!$pluginRoots){throw "Decodeur JPEG Qt absent du package: imageformats\qjpeg.dll introuvable sous $Root ou plugins."}
$env:QT_PLUGIN_PATH=(@($pluginRoots)+@($env:QT_PLUGIN_PATH) -ne '') -join ';'
$fresco=Join-Path $Root 'benchmark-civic\textures\energetik_fresco_mosaic.jpg'
if(!(Test-Path -LiteralPath $fresco -PathType Leaf)){throw "Texture de fresque absente: $fresco"}
$env:PATH="$Root;$env:PATH"
$out=Join-Path $Root ('benchmark-results\parcours30-'+(Get-Date -Format yyyyMMdd-HHmmssfff))
New-Item -Type Directory -Force $out | Out-Null
& $exe $scene "$out\resultats.csv"
if($LASTEXITCODE){throw "Echec moteur $LASTEXITCODE - sorties partielles: $out"}
Get-Content "$out\resultats.csv"
Compress-Archive "$out\resultats.csv" "$out.zip"
Write-Host "Resultats: $out.zip"
