param(
    [string]$Root = $PSScriptRoot,
    [string]$Branch = "benchmark/luminosity-audit-v1"
)
$ErrorActionPreference = "Stop"
$Root = [IO.Path]::GetFullPath($Root)
$exe = Join-Path $Root "moteur3d_v17_campaign.exe"
$scene = Join-Path $Root "benchmark-civic\scene.json"
$sceneRoot = Join-Path $Root "benchmark-civic"
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
Add-Type -AssemblyName System.Drawing
$lumaAuditSource = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

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
"@
Add-Type -TypeDefinition $lumaAuditSource -ReferencedAssemblies @([System.Drawing.Bitmap].Assembly.Location)
$names = @("CPU","DX11","Vulkan","LumaTrace")
$data = @()
foreach ($name in $names) {
    $data += [LumaAudit]::Read($name,(Join-Path $images $required[$name]))
}
$measure = [LumaAudit]::ComputeJson($data) | ConvertFrom-Json
$engine = Get-Content -LiteralPath (Join-Path $campaign.FullName "summary.json") -Raw | ConvertFrom-Json
$report = [ordered]@{
    audit = "CSE WARP V17 lighting parity"
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
}
$reportPath = Join-Path $campaign.FullName "luminosity_audit.json"
$csvPath = Join-Path $campaign.FullName "luminosity_audit.csv"
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding UTF8
$measure.backends | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
Write-Host ""
Write-Host "Mesures luminance (masque opaque commun):"
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
