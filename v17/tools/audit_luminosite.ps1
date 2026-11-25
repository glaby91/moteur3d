param(
    [string]$Root = $PSScriptRoot,
    [string]$Branch = "benchmark/luminosity-audit-v1",
    [switch]$SelfTest
)
$ErrorActionPreference = "Stop"
$Root = [IO.Path]::GetFullPath($Root)
$exe = Join-Path $Root "moteur3d_v17_campaign.exe"
$scene = Join-Path $Root "benchmark-civic\scene.json"
$sceneRoot = Join-Path $Root "benchmark-civic"

Add-Type -AssemblyName System.Drawing
$lumaAuditSource = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

public sealed class LumaImage {
    public string Name;
    public int Width, Height;
    public byte[] BGRA;
}
public static class LumaAudit {
    static double Lin(double x) {
        return x <= 0.04045 ? x/12.92 : Math.Pow((x+0.055)/1.055,2.4);
    }
    static double Y(byte[] p, int k, bool linear) {
        double b=p[k]/255.0, g=p[k+1]/255.0, r=p[k+2]/255.0;
        if(linear){r=Lin(r);g=Lin(g);b=Lin(b);}
        return .2126*r+.7152*g+.0722*b;
    }
    public static LumaImage Read(string name,string path) {
        using(Bitmap bmp=new Bitmap(path)) {
            var rect=new Rectangle(0,0,bmp.Width,bmp.Height);
            var data=bmp.LockBits(rect,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
            try {
                var result=new LumaImage{Name=name,Width=bmp.Width,Height=bmp.Height,
                    BGRA=new byte[bmp.Width*bmp.Height*4]};
                int rowBytes=bmp.Width*4;
                for(int y=0;y<bmp.Height;y++)
                    Marshal.Copy(IntPtr.Add(data.Scan0,y*data.Stride),result.BGRA,y*rowBytes,rowBytes);
                return result;
            } finally { bmp.UnlockBits(data); }
        }
    }
    static string N(double x){return x.ToString("0.########",CultureInfo.InvariantCulture);}
    static void Add(StringBuilder b,string key,double val){b.Append('"').Append(key).Append("\":").Append(N(val));}
    public static string ComputeJson(LumaImage[] imgs) {
        if(imgs==null||imgs.Length<2) throw new ArgumentException("At least two images are required.");
        int w=imgs[0].Width,h=imgs[0].Height,n=w*h;
        foreach(var im in imgs)
            if(im.Width!=w||im.Height!=h||im.BGRA.Length!=n*4)
                throw new ArgumentException("All PNG captures must have the same dimensions.");
        long commonOpaque=0,commonVisible=0;
        double[] commonDisplay=new double[imgs.Length],commonLinear=new double[imgs.Length];
        long[] ownOpaque=new long[imgs.Length],alpha0=new long[imgs.Length],alphaPartial=new long[imgs.Length],clipped=new long[imgs.Length];
        double[] ownDisplay=new double[imgs.Length],ownLinear=new double[imgs.Length];
        for(int px=0;px<n;px++){
            bool allOpaque=true,allVisible=true;
            for(int j=0;j<imgs.Length;j++){
                int k=px*4;byte a=imgs[j].BGRA[k+3];
                if(a!=255)allOpaque=false;if(a==0)allVisible=false;
                if(a==0)alpha0[j]++;else if(a<255)alphaPartial[j]++;
                if(a==255 && Math.Max(imgs[j].BGRA[k],Math.Max(imgs[j].BGRA[k+1],imgs[j].BGRA[k+2]))>=254)clipped[j]++;
                if(a==255){ownOpaque[j]++;ownDisplay[j]+=Y(imgs[j].BGRA,k,false);ownLinear[j]+=Y(imgs[j].BGRA,k,true);}
            }
            if(allVisible)commonVisible++;
            if(allOpaque){
                commonOpaque++;
                for(int j=0;j<imgs.Length;j++){
                    int k=px*4;commonDisplay[j]+=Y(imgs[j].BGRA,k,false);commonLinear[j]+=Y(imgs[j].BGRA,k,true);
                }
            }
        }
        var b=new StringBuilder();
        b.Append("{\"width\":").Append(w).Append(",\"height\":").Append(h)
         .Append(",\"backendCount\":").Append(imgs.Length)
         .Append(",\"commonVisiblePixels\":").Append(commonVisible)
         .Append(",\"commonOpaquePixels\":").Append(commonOpaque)
         .Append(",\"mask\":\"For backend comparison, include only pixels with alpha=255 in every capture; report transparency separately.\"")
         .Append(",\"backends\":[");
        for(int j=0;j<imgs.Length;j++){
            if(j>0)b.Append(',');
            double total=n;
            b.Append("{\"backend\":\"").Append(imgs[j].Name).Append("\",\"alphaZeroPct\":").Append(N(100.0*alpha0[j]/total))
             .Append(",\"alphaPartialPct\":").Append(N(100.0*alphaPartial[j]/total))
             .Append(",\"ownOpaquePct\":").Append(N(100.0*ownOpaque[j]/total))
             .Append(",\"ownOpaqueMeanDisplayY_0_255\":").Append(N(ownOpaque[j]>0?255.0*ownDisplay[j]/ownOpaque[j]:0))
             .Append(",\"ownOpaqueMeanLinearY_0_1\":").Append(N(ownOpaque[j]>0?ownLinear[j]/ownOpaque[j]:0))
             .Append(",\"commonOpaqueMeanDisplayY_0_255\":").Append(N(commonOpaque>0?255.0*commonDisplay[j]/commonOpaque:0))
             .Append(",\"commonOpaqueMeanLinearY_0_1\":").Append(N(commonOpaque>0?commonLinear[j]/commonOpaque:0))
             .Append(",\"clippedChannelPixelsPct\":").Append(N(ownOpaque[j]>0?100.0*clipped[j]/ownOpaque[j]:0)).Append('}');
        }
        b.Append("]}");return b.ToString();
    }
}

public static class LumaRegionAudit {
    static double Lin(double x) { return x<=0.04045?x/12.92:Math.Pow((x+0.055)/1.055,2.4); }
    static double Y(byte[] p,int k,bool linear) {
        double b=p[k]/255.0,g=p[k+1]/255.0,r=p[k+2]/255.0;
        if(linear){r=Lin(r);g=Lin(g);b=Lin(b);}
        return .2126*r+.7152*g+.0722*b;
    }
    static string N(double x){return x.ToString("0.########",CultureInfo.InvariantCulture);}
    public static string ComputeJson(LumaImage[] imgs,string reference) {
        if(imgs==null||imgs.Length<2) throw new ArgumentException("At least two captures are required.");
        int w=imgs[0].Width,h=imgs[0].Height;
        foreach(var im in imgs) if(im.Width!=w||im.Height!=h||im.BGRA.Length!=w*h*4)
            throw new ArgumentException("All captures must have identical dimensions.");
        int refIndex=-1;
        for(int j=0;j<imgs.Length;j++) if(String.Equals(imgs[j].Name,reference,StringComparison.OrdinalIgnoreCase)) refIndex=j;
        if(refIndex<0) throw new ArgumentException("Reference backend not found: "+reference);
        var regions=new List<Tuple<string,int,int,int,int>>();
        regions.Add(Tuple.Create("full",0,0,w,h));
        for(int row=0;row<3;row++) for(int col=0;col<3;col++)
            regions.Add(Tuple.Create("grid_r"+(row+1)+"_c"+(col+1),(col*w)/3,(row*h)/3,((col+1)*w)/3,(row+1)*h/3));
        var b=new StringBuilder("{\"referenceBackend\":\"").Append(reference).Append("\",\"grid\":\"3x3 normalized image cells; cell deltas compare against the same cell\",\"regions\":[");
        for(int ri=0;ri<regions.Count;ri++){
            if(ri>0)b.Append(',');
            var r=regions[ri];int x0=r.Item2,y0=r.Item3,x1=r.Item4,y1=r.Item5;
            long common=0,area=(long)(x1-x0)*(y1-y0);
            double[] sumD=new double[imgs.Length],sumL=new double[imgs.Length];
            var values=new List<double>[imgs.Length];for(int j=0;j<imgs.Length;j++)values[j]=new List<double>();
            for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
                int px=y*w+x,k=px*4;bool opaque=true;
                for(int j=0;j<imgs.Length;j++)if(imgs[j].BGRA[k+3]!=255){opaque=false;break;}
                if(!opaque)continue;
                common++;
                for(int j=0;j<imgs.Length;j++){double d=Y(imgs[j].BGRA,k,false),l=Y(imgs[j].BGRA,k,true);sumD[j]+=d;sumL[j]+=l;values[j].Add(l);}
            }
            b.Append("{\"name\":\"").Append(r.Item1).Append("\",\"x\":").Append(x0).Append(",\"y\":").Append(y0)
             .Append(",\"width\":").Append(x1-x0).Append(",\"height\":").Append(y1-y0).Append(",\"commonOpaquePixels\":").Append(common)
             .Append(",\"commonOpaqueCoveragePct\":").Append(N(area>0?100.0*common/area:0)).Append(",\"backends\":[");
            double refD=common>0?sumD[refIndex]/common:0,refL=common>0?sumL[refIndex]/common:0;
            for(int j=0;j<imgs.Length;j++){
                if(j>0)b.Append(',');
                values[j].Sort();int count=values[j].Count;
                double meanD=common>0?sumD[j]/common:0,meanL=common>0?sumL[j]/common:0;
                double p50=count>0?values[j][(int)Math.Floor((count-1)*0.50)]:0;
                double p95=count>0?values[j][(int)Math.Floor((count-1)*0.95)]:0;
                b.Append("{\"backend\":\"").Append(imgs[j].Name).Append("\",\"meanDisplayY_0_255\":").Append(N(255.0*meanD))
                 .Append(",\"meanLinearY_0_1\":").Append(N(meanL)).Append(",\"p50LinearY_0_1\":").Append(N(p50))
                 .Append(",\"p95LinearY_0_1\":").Append(N(p95)).Append(",\"deltaDisplayPctVsReference\":");
                if(refD>1e-12)b.Append(N(100.0*(meanD/refD-1.0)));else b.Append("null");
                b.Append(",\"deltaLinearPctVsReference\":");
                if(refL>1e-12)b.Append(N(100.0*(meanL/refL-1.0)));else b.Append("null");
                b.Append('}');
            }
            b.Append("]}");
        }
        b.Append("]}");return b.ToString();
    }
    public static string SelfTest() {
        var a=new LumaImage{Name="DX11",Width=6,Height=6,BGRA=new byte[6*6*4]};
        var half=new LumaImage{Name="Half",Width=6,Height=6,BGRA=new byte[6*6*4]};
        for(int i=0;i<36;i++){int k=i*4;a.BGRA[k]=a.BGRA[k+1]=a.BGRA[k+2]=100;a.BGRA[k+3]=255;half.BGRA[k]=half.BGRA[k+1]=half.BGRA[k+2]=50;half.BGRA[k+3]=255;}
        string same=ComputeJson(new LumaImage[]{a,a},"DX11");
        string changed=ComputeJson(new LumaImage[]{a,half},"DX11");
        if(same.IndexOf("\"deltaDisplayPctVsReference\":0",StringComparison.Ordinal)<0)
            throw new InvalidOperationException("Self-test failed: identical images must report 0% delta.");
        if(changed.IndexOf("\"deltaDisplayPctVsReference\":-50",StringComparison.Ordinal)<0)
            throw new InvalidOperationException("Self-test failed: half-brightness image must report -50% display-Y delta.");
        return "OK: identical captures = 0%; half-level grayscale = -50%; ROI reference uses same region.";
    }
}
"@

function New-RoughPoolControl([string]$SourceRoot, [string]$DestinationRoot) {
    $materialRoot = Join-Path $SourceRoot "materials"
    if (!(Test-Path -LiteralPath $materialRoot -PathType Container)) {
        throw "Dossier materials absent dans la scène : $materialRoot"
    }
    $poolMaterialMatches = @()
    foreach ($file in Get-ChildItem -LiteralPath $materialRoot -Filter "*.json" -File -Recurse) {
        try {
            $doc = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
            if ($doc.materials) {
                for ($i = 0; $i -lt $doc.materials.Count; $i++) {
                    if ([string]::Equals([string]$doc.materials[$i].id,"cse_pool_water",[StringComparison]::OrdinalIgnoreCase)) {
                        $poolMaterialMatches += [pscustomobject]@{ File = $file; Document = $doc; Index = $i }
                    }
                }
            }
        } catch {
            throw "Impossible de lire le fichier matériaux $($file.FullName) : $($_.Exception.Message)"
        }
    }
    if ($poolMaterialMatches.Count -ne 1) {
        $found = if ($poolMaterialMatches.Count) { ($poolMaterialMatches | ForEach-Object { $_.File.FullName }) -join ", " } else { "aucun" }
        throw "Le matériau exact cse_pool_water doit être unique (trouvé : $($poolMaterialMatches.Count), $found). Variante annulée sans lancer de rendu."
    }
    $target = $poolMaterialMatches[0]
    $material = $target.Document.materials[$target.Index]
    if ($null -eq $material.roughness) { throw "Le matériau cse_pool_water n'a pas de propriété roughness." }
    $originalRoughness = [double]$material.roughness
    if ($originalRoughness -lt 0 -or $originalRoughness -gt 1) { throw "Roughness hors plage [0,1] : $originalRoughness" }

    if (Test-Path -LiteralPath $DestinationRoot) { Remove-Item -LiteralPath $DestinationRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null
    Copy-Item -Path (Join-Path $SourceRoot "*") -Destination $DestinationRoot -Recurse -Force
    $sourcePrefix = $SourceRoot.TrimEnd([char[]]@([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)) + [IO.Path]::DirectorySeparatorChar
    $relative = $target.File.FullName.Substring($sourcePrefix.Length)
    $destinationFile = Join-Path $DestinationRoot $relative
    $material.roughness = 0.95
    $json = $target.Document | ConvertTo-Json -Depth 100
    [IO.File]::WriteAllText($destinationFile,$json,[Text.UTF8Encoding]::new($false))
    $check = Get-Content -LiteralPath $destinationFile -Raw | ConvertFrom-Json
    $checkMatches = @($check.materials | Where-Object { [string]::Equals([string]$_.id,"cse_pool_water",[StringComparison]::OrdinalIgnoreCase) })
    if ($checkMatches.Count -ne 1 -or [math]::Abs([double]$checkMatches[0].roughness - 0.95) -gt 0.000001) {
        throw "La validation du matériau témoin cse_pool_water a échoué."
    }
    return [pscustomobject]@{
        materialId = "cse_pool_water"
        materialFile = $relative.Replace("\","/")
        baselineRoughness = $originalRoughness
        controlRoughness = 0.95
        otherMaterialPropertiesPreserved = $true
        sceneCopyValidated = $true
    }
}

Add-Type -TypeDefinition $lumaAuditSource -ReferencedAssemblies @([System.Drawing.Bitmap].Assembly.Location)
$roiSelfTest = [LumaRegionAudit]::SelfTest()
Write-Host "Validation des métriques: $roiSelfTest"
if ($SelfTest) { exit 0 }

# Run the engine against the newest scene published by the companion repository.
$release = Invoke-RestMethod -Uri "https://api.github.com/repos/glaby91/moteur3d-scene/releases/latest" -Headers @{ "Accept" = "application/vnd.github+json"; "User-Agent" = "CSE-WARP-LuminosityBenchmark" }
$sceneAsset = $release.assets | Where-Object { $_.name -eq "V17-scene.zip" } | Select-Object -First 1
if (!$sceneAsset) { throw "Latest scene release $($release.tag_name) has no V17-scene.zip asset." }
$stageRoot = Join-Path $env:TEMP ("cse-warp-scene-" + [guid]::NewGuid().ToString("N"))
$archivePath = Join-Path $stageRoot "V17-scene.zip"
$extractRoot = Join-Path $stageRoot "extract"
New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
try {
    Invoke-WebRequest -Uri $sceneAsset.browser_download_url -OutFile $archivePath
    Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot -Force
    $releaseScene = Get-ChildItem -LiteralPath $extractRoot -Directory -Recurse |
        Where-Object { $_.Name -eq "benchmark-civic" -and (Test-Path (Join-Path $_.FullName "scene.json")) } |
        Select-Object -First 1
    if (!$releaseScene) {
        $releaseScene = Get-Item -LiteralPath $extractRoot
        if (!(Test-Path (Join-Path $releaseScene.FullName "scene.json"))) {
            throw "Scene release archive $($release.tag_name) does not contain benchmark-civic/scene.json."
        }
    }
    $script:sceneVersion = $release.tag_name
    $versionFile = Get-ChildItem -LiteralPath $extractRoot -Filter "SCENE_VERSION.json" -File -Recurse | Select-Object -First 1
    if ($versionFile) {
        try {
            $versionData = Get-Content -LiteralPath $versionFile.FullName -Raw | ConvertFrom-Json
            if ($versionData.revision) { $script:sceneVersion = $versionData.revision }
        } catch { Write-Warning "SCENE_VERSION.json unreadable; using release tag $($release.tag_name)." }
    }
    $stagedScene = Join-Path $stageRoot "benchmark-civic"
    New-Item -ItemType Directory -Force -Path $stagedScene | Out-Null
    Copy-Item -Path (Join-Path $releaseScene.FullName "*") -Destination $stagedScene -Recurse -Force
    if (!(Test-Path -LiteralPath (Join-Path $stagedScene "scene.json") -PathType Leaf)) {
        throw "Latest scene archive did not stage scene.json at the expected location."
    }
    $destinationScene = Join-Path $Root "benchmark-civic"
    if (Test-Path -LiteralPath $destinationScene) { Remove-Item -LiteralPath $destinationScene -Recurse -Force }
    Move-Item -LiteralPath $stagedScene -Destination $destinationScene
    $scene = Join-Path $destinationScene "scene.json"
    Write-Host "Scène la plus récente chargée: $script:sceneVersion ($($release.tag_name))"
} finally {
    if (Test-Path -LiteralPath $stageRoot) { Remove-Item -LiteralPath $stageRoot -Recurse -Force }
}

$resultsRoot = Join-Path $Root "benchmark-results"
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Exécutable absent : $exe" }
if (!(Test-Path -LiteralPath $scene -PathType Leaf)) { throw "Scène complète absente : $scene" }
New-Item -ItemType Directory -Force -Path $resultsRoot | Out-Null
$runId = Get-Date -Format "yyyyMMdd_HHmmss"
$outBase = Join-Path $resultsRoot "run-$runId"
New-Item -ItemType Directory -Force -Path $outBase | Out-Null
Write-Host "CSE WARP - audit luminosité 4 moteurs"
Write-Host "Scène: $scene"
Write-Host "Caméra: v7_pool_fresco (repli automatique v4_piscine / première caméra)"
Write-Host "Résolution: 1000 x 680 ; un échauffement + deux images mesurées"
Write-Host "Backends: DX11, LumaTrace, Vulkan, CPU direct"
& $exe $scene $outBase --lighting-audit --raster-msaa 1 --raster-anisotropy 8 --raster-lod-percent 100 --raster-culling 1 --raster-lod-enabled 0
if ($LASTEXITCODE -ne 0) { throw "Le rendu moteur a échoué (code $LASTEXITCODE)." }
$campaign = Get-ChildItem -LiteralPath $outBase -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$campaign) { throw "Le moteur n'a pas créé de dossier de résultats." }
$images = Join-Path $campaign.FullName "images"
$required = [ordered]@{
    CPU = "full_scene0_cpu_direct_1000x680.png"
    DX11 = "scene0_cam0_1000x680_depth1.png"
    Vulkan = "full_scene0_cam0_1000x680_vulkan.png"
    LumaTrace = "full_scene0_cam0_1000x680_lumatrace.png"
}
foreach ($name in $required.Keys) {
    $path = Join-Path $images $required[$name]
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Image $name absente : $path" }
}
$names = @("CPU","DX11","Vulkan","LumaTrace")
$data = @()
foreach ($name in $names) {
    $data += [LumaAudit]::Read($name,(Join-Path $images $required[$name]))
}
$measure = [LumaAudit]::ComputeJson($data) | ConvertFrom-Json
$regionMeasure = [LumaRegionAudit]::ComputeJson($data,"DX11") | ConvertFrom-Json
$engine = Get-Content -LiteralPath (Join-Path $campaign.FullName "summary.json") -Raw | ConvertFrom-Json
$report = [ordered]@{
    audit = "CSE WARP V17 lighting parity"
    benchmarkScriptCommit = $env:CSE_WARP_BENCHMARK_SCRIPT_COMMIT
    createdLocal = (Get-Date).ToString("o")
    machine = [ordered]@{
        computer = $env:COMPUTERNAME
        os = (Get-CimInstance Win32_OperatingSystem).Caption
        cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name)
        gpu = @((Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name }))
    }
    capture = [ordered]@{
        scene = "benchmark-civic/scene.json"; sceneRelease = $script:sceneVersion
        camera = $engine.cases[0].camera
        resolution = "1000x680"
        rasterMsaa = 1
        framesPerBackend = 2
        alphaPolicy = "common fully opaque mask for cross-backend luminance; alpha coverage reported per backend"
    }
    engineSummary = $engine
    luminance = $measure
    roiReference = "DX11; each ROI is compared against the matching DX11 ROI"
    regionalLuminance = $regionMeasure
    metricSelfTest = $roiSelfTest
}
# Controlled material variant: duplicate the same scene and raise only pool-water roughness.
$controlSceneRoot = Join-Path $Root "benchmark-civic-control-rough-water"
$materialControl = New-RoughPoolControl $sceneRoot $controlSceneRoot
$controlOutBase = Join-Path $campaign.FullName "control-rough-water"
New-Item -ItemType Directory -Force -Path $controlOutBase | Out-Null
Write-Host ""
Write-Host "Contrôle matériau: cse_pool_water roughness $($materialControl.baselineRoughness) -> $($materialControl.controlRoughness)"
& $exe (Join-Path $controlSceneRoot "scene.json") $controlOutBase --lighting-audit --raster-msaa 1 --raster-anisotropy 8 --raster-lod-percent 100 --raster-culling 1 --raster-lod-enabled 0
if ($LASTEXITCODE -ne 0) { throw "Le rendu témoin matériau a échoué (code $LASTEXITCODE)." }
$controlCampaign = Get-ChildItem -LiteralPath $controlOutBase -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$controlCampaign) { throw "Le moteur n'a pas créé de dossier de résultats pour la variante témoin." }
$controlImages = Join-Path $controlCampaign.FullName "images"
foreach ($name in $required.Keys) {
    $pathCheck = Join-Path $controlImages $required[$name]
    if (!(Test-Path -LiteralPath $pathCheck -PathType Leaf)) { throw "Image témoin $name absente : $pathCheck" }
}
$controlData = @()
foreach ($name in $names) {
    $controlData += [LumaAudit]::Read($name,(Join-Path $controlImages $required[$name]))
}
$controlMeasure = [LumaAudit]::ComputeJson($controlData) | ConvertFrom-Json
$controlRegionMeasure = [LumaRegionAudit]::ComputeJson($controlData,"DX11") | ConvertFrom-Json
$controlEngine = Get-Content -LiteralPath (Join-Path $controlCampaign.FullName "summary.json") -Raw | ConvertFrom-Json
if ([string]$controlEngine.cases[0].camera -ne [string]$engine.cases[0].camera) {
    throw "La caméra du rendu témoin diffère de la référence. Comparaison annulée."
}
$controlDeltaRows = foreach ($baseRegion in $regionMeasure.regions) {
    $testRegion = $controlRegionMeasure.regions | Where-Object name -eq $baseRegion.name | Select-Object -First 1
    foreach ($baseBackend in $baseRegion.backends) {
        $testBackend = $testRegion.backends | Where-Object backend -eq $baseBackend.backend | Select-Object -First 1
        if (!$testBackend) { continue }
        $displayDelta = [double]$testBackend.meanDisplayY_0_255 - [double]$baseBackend.meanDisplayY_0_255
        $linearDelta = [double]$testBackend.meanLinearY_0_1 - [double]$baseBackend.meanLinearY_0_1
        [pscustomobject]@{
            region = $baseRegion.name
            backend = $baseBackend.backend
            baselineMeanDisplayY_0_255 = $baseBackend.meanDisplayY_0_255
            roughWaterMeanDisplayY_0_255 = $testBackend.meanDisplayY_0_255
            deltaDisplayY = $displayDelta
            deltaDisplayPct = if ([double]$baseBackend.meanDisplayY_0_255 -gt 0) { 100.0 * $displayDelta / [double]$baseBackend.meanDisplayY_0_255 } else { $null }
            baselineMeanLinearY_0_1 = $baseBackend.meanLinearY_0_1
            roughWaterMeanLinearY_0_1 = $testBackend.meanLinearY_0_1
            deltaLinearY = $linearDelta
            deltaLinearPct = if ([double]$baseBackend.meanLinearY_0_1 -gt 0) { 100.0 * $linearDelta / [double]$baseBackend.meanLinearY_0_1 } else { $null }
            commonOpaqueCoverageBaselinePct = $baseRegion.commonOpaqueCoveragePct
            commonOpaqueCoverageControlPct = $testRegion.commonOpaqueCoveragePct
        }
    }
}
$controlReport = [ordered]@{
    variant = "same scene/camera/lights; only cse_pool_water roughness changed"
    material = $materialControl
    camera = $controlEngine.cases[0].camera
    engineSummary = $controlEngine
    luminance = $controlMeasure
    roiReference = "DX11; each ROI is compared against the matching DX11 ROI"
    regionalLuminance = $controlRegionMeasure
    baselineToControlDelta = @($controlDeltaRows)
}
$report["materialControl"] = $controlReport

$reportPath = Join-Path $campaign.FullName "luminosity_audit.json"
$csvPath = Join-Path $campaign.FullName "luminosity_audit.csv"
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding UTF8
$measure.backends | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$roiRows = foreach ($region in $regionMeasure.regions) {
    foreach ($backend in $region.backends) {
        [pscustomobject]@{
            region = $region.name; x = $region.x; y = $region.y; width = $region.width; height = $region.height
            commonOpaquePixels = $region.commonOpaquePixels; commonOpaqueCoveragePct = $region.commonOpaqueCoveragePct
            backend = $backend.backend; meanDisplayY_0_255 = $backend.meanDisplayY_0_255
            meanLinearY_0_1 = $backend.meanLinearY_0_1; p50LinearY_0_1 = $backend.p50LinearY_0_1
            p95LinearY_0_1 = $backend.p95LinearY_0_1
            deltaDisplayPctVsDX11SameRegion = $backend.deltaDisplayPctVsReference
            deltaLinearPctVsDX11SameRegion = $backend.deltaLinearPctVsReference
        }
    }
}
$roiCsvPath = Join-Path $campaign.FullName "luminosity_regions.csv"
$roiRows | Export-Csv -LiteralPath $roiCsvPath -NoTypeInformation -Encoding UTF8
$controlReportPath = Join-Path $controlCampaign.FullName "material_control_audit.json"
$controlReport | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $controlReportPath -Encoding UTF8
$controlMeasure.backends | Export-Csv -LiteralPath (Join-Path $controlCampaign.FullName "luminosity_audit.csv") -NoTypeInformation -Encoding UTF8
$controlRoiRows = foreach ($region in $controlRegionMeasure.regions) {
    foreach ($backend in $region.backends) {
        [pscustomobject]@{
            region = $region.name; x = $region.x; y = $region.y; width = $region.width; height = $region.height
            commonOpaquePixels = $region.commonOpaquePixels; commonOpaqueCoveragePct = $region.commonOpaqueCoveragePct
            backend = $backend.backend; meanDisplayY_0_255 = $backend.meanDisplayY_0_255
            meanLinearY_0_1 = $backend.meanLinearY_0_1; p50LinearY_0_1 = $backend.p50LinearY_0_1
            p95LinearY_0_1 = $backend.p95LinearY_0_1
            deltaDisplayPctVsDX11SameRegion = $backend.deltaDisplayPctVsReference
            deltaLinearPctVsDX11SameRegion = $backend.deltaLinearPctVsReference
        }
    }
}
$controlRoiRows | Export-Csv -LiteralPath (Join-Path $controlCampaign.FullName "luminosity_regions.csv") -NoTypeInformation -Encoding UTF8
$controlDeltaRows | Export-Csv -LiteralPath (Join-Path $campaign.FullName "material_control_comparison.csv") -NoTypeInformation -Encoding UTF8
Write-Host ""
Write-Host "Mesures luminance (masque opaque commun, scène originale):"
$measure.backends | Format-Table backend,ownOpaquePct,commonOpaqueMeanDisplayY_0_255,commonOpaqueMeanLinearY_0_1,alphaZeroPct -AutoSize

$zip = Join-Path $resultsRoot "CSE-WARP-luminosity-$runId.zip"
Compress-Archive -Path (Join-Path $campaign.FullName "*") -DestinationPath $zip -Force
Write-Host "Archive locale: $zip"

$gh = Get-Command gh -ErrorAction SilentlyContinue
$git = Get-Command git -ErrorAction SilentlyContinue
if (!$gh -or !$git) {
    Write-Warning "GitHub CLI ou Git absent. L'archive est prête, mais l'envoi automatique est indisponible."
    Write-Host "Installe GitHub CLI, connecte-toi avec 'gh auth login', puis relance le .bat."
    exit 0
}
& $gh.Source auth status --hostname github.com *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Warning "GitHub CLI n'est pas connecté. L'archive est prête mais non envoyée."
    Write-Host "Connecte-toi une fois avec 'gh auth login -h github.com', puis relance le .bat."
    exit 0
}
& $gh.Source auth setup-git --hostname github.com
if ($LASTEXITCODE -ne 0) { throw "Impossible de configurer Git avec la session GitHub CLI." }
$tempRepo = Join-Path $env:TEMP "cse-warp-luminosity-$runId"
$remotePath = "benchmarks/luminosity-audit/runs/$runId"
& $git.Source clone --depth 1 --filter=blob:none --sparse --branch $Branch "https://github.com/glaby91/moteur3d.git" $tempRepo
if ($LASTEXITCODE -ne 0) { throw "Clone de la branche $Branch impossible. L'archive locale reste disponible." }
& $git.Source -C $tempRepo sparse-checkout set "benchmarks/luminosity-audit"
if ($LASTEXITCODE -ne 0) { throw "Initialisation sparse-checkout impossible." }
$destination = Join-Path $tempRepo ($remotePath.Replace("/",[IO.Path]::DirectorySeparatorChar))
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -Path (Join-Path $campaign.FullName "*") -Destination $destination -Recurse -Force
& $git.Source -C $tempRepo config user.name "CSE WARP Benchmark"
& $git.Source -C $tempRepo config user.email "cse-warp-benchmark@users.noreply.github.com"
& $git.Source -C $tempRepo add $remotePath
& $git.Source -C $tempRepo commit -m "Add lighting audit results $runId"
if ($LASTEXITCODE -ne 0) { throw "Création du commit de résultats impossible." }
& $git.Source -C $tempRepo push origin $Branch
if ($LASTEXITCODE -ne 0) { throw "Push GitHub impossible. L'archive locale reste dans $zip." }
Write-Host ""
Write-Host "Images et mesures envoyées dans $Branch/$remotePath"
