#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$PackageRoot = (Get-Location).Path,
    [string]$ScenePath = 'benchmark-civic\scene.json',
    [ValidateRange(1,30)][int]$Rounds = 4,
    [ValidateRange(2,240)][int]$Frames = 12,
    [ValidateRange(0,20)][int]$Warmup = 2,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public sealed class LumaParityRegion {
    public string Name;
    public double MeanLinear, MedianLinear, P95Linear, NearBlackFraction, NearWhiteFraction;
}
public static class LumaParityImage {
    private sealed class Accumulator {
        public readonly long[] Histogram = new long[4096];
        public long Count, Black, White;
        public double Sum;
        public void Add(double value) {
            int bin = Math.Min(4095, (int)(value * 4096.0));
            Histogram[bin]++; Count++; Sum += value;
            if (value < 0.01) Black++;
            if (value > 0.99) White++;
        }
        public double Quantile(double q) {
            long target = (long)Math.Floor((Count - 1) * q), seen = 0;
            for (int i = 0; i < Histogram.Length; i++) {
                seen += Histogram[i];
                if (seen > target) return (i + 0.5) / Histogram.Length;
            }
            return 0.0;
        }
    }
    private static double Linear(byte value) {
        double c = value / 255.0;
        return c <= 0.04045 ? c / 12.92 : Math.Pow((c + 0.055) / 1.055, 2.4);
    }
    public static LumaParityRegion[] Analyze(string path) {
        using (Bitmap source = (Bitmap)Bitmap.FromFile(path))
        using (Bitmap bitmap = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb)) {
            int width = bitmap.Width, height = bitmap.Height;
            using (Graphics graphics = Graphics.FromImage(bitmap)) graphics.DrawImage(source, 0, 0, width, height);
            Accumulator[] accumulators = new Accumulator[10];
            for (int i = 0; i < accumulators.Length; i++) accumulators[i] = new Accumulator();
            Rectangle bounds = new Rectangle(0, 0, width, height);
            BitmapData data = bitmap.LockBits(bounds, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try {
                byte[] row = new byte[Math.Abs(data.Stride)];
                for (int y = 0; y < height; y++) {
                    Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), row, 0, row.Length);
                    int cellY = Math.Min(2, y * 3 / height);
                    for (int x = 0; x < width; x++) {
                        int at = x * 4;
                        double r = Linear(row[at + 2]), g = Linear(row[at + 1]), b = Linear(row[at]);
                        double luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
                        accumulators[0].Add(luma);
                        int cellX = Math.Min(2, x * 3 / width);
                        accumulators[1 + cellY * 3 + cellX].Add(luma);
                    }
                }
            } finally { bitmap.UnlockBits(data); }
            string[] names = { "full", "r1c1", "r1c2", "r1c3", "r2c1", "r2c2", "r2c3", "r3c1", "r3c2", "r3c3" };
            LumaParityRegion[] result = new LumaParityRegion[10];
            for (int i = 0; i < result.Length; i++) {
                Accumulator a = accumulators[i];
                result[i] = new LumaParityRegion {
                    Name = names[i], MeanLinear = a.Sum / a.Count,
                    MedianLinear = a.Quantile(0.5), P95Linear = a.Quantile(0.95),
                    NearBlackFraction = a.Black / (double)a.Count,
                    NearWhiteFraction = a.White / (double)a.Count
                };
            }
            return result;
        }
    }
}
'@

function Add-ImageRows {
    param([System.Collections.ArrayList]$Rows, $Case, [string]$Backend, [string]$ImagePath,
          [double]$Tolerance, [double]$ReferenceMean)
    $width = [int]$Case.width; $height = [int]$Case.height
    $regions = [LumaParityImage]::Analyze($ImagePath)
    foreach ($s in $regions) {
        $ratio = if ($ReferenceMean -gt 0) { $s.MeanLinear / $ReferenceMean } else { 1.0 }
        $status = if ($Backend -eq 'D3D11 Raster' -or [Math]::Abs($ratio-1.0) -le $Tolerance) { 'OK' } else { 'REVIEW' }
        [void]$Rows.Add([pscustomobject]@{
            Scene=$Case.scene; Camera=$Case.camera; Width=$width; Height=$height
            Backend=$Backend; Region=$s.Name; MeanLinear=$s.MeanLinear; MedianLinear=$s.MedianLinear
            P95Linear=$s.P95Linear; RelativeToD3D11=$ratio; Tolerance=$Tolerance; Status=$status
            NearBlackFraction=$s.NearBlackFraction; NearWhiteFraction=$s.NearWhiteFraction
            Image=$ImagePath
        })
    }
}

if ($SelfTest) {
    $fixture = Join-Path ([IO.Path]::GetTempPath()) ("luma-parity-selftest-" + [Guid]::NewGuid().ToString('N') + '.png')
    $bitmap = [System.Drawing.Bitmap]::new(6, 6, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt 6; $y++) { for ($x = 0; $x -lt 6; $x++) { $bitmap.SetPixel($x, $y, [Drawing.Color]::White) } }
        $bitmap.Save($fixture, [System.Drawing.Imaging.ImageFormat]::Png)
        $regions = [LumaParityImage]::Analyze($fixture)
        if ($regions.Length -ne 10 -or [Math]::Abs($regions[0].MeanLinear - 1.0) -gt 0.001 -or
            [Math]::Abs($regions[0].NearWhiteFraction - 1.0) -gt 0.001) {
            throw 'Analyseur de luminance : résultat inattendu sur une image blanche 6×6.'
        }
        Write-Host 'Self-test luminance OK : décodage PNG, mesure linéaire et 9 régions.'
    } finally {
        $bitmap.Dispose()
        Remove-Item -LiteralPath $fixture -Force -ErrorAction SilentlyContinue
    }
    return
}

$root = (Resolve-Path $PackageRoot).Path
$exe = Join-Path $root 'moteur3d_v17_campaign.exe'
$scene = if ([IO.Path]::IsPathRooted($ScenePath)) { $ScenePath } else { Join-Path $root $ScenePath }
if (!(Test-Path -LiteralPath $exe)) { throw "Exécutable absent : $exe" }
if (!(Test-Path -LiteralPath $scene)) { throw "Scène absente : $scene" }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$outRoot = Join-Path $root (Join-Path 'benchmarks-luminosite' $stamp)
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null
$campaignRoot = Join-Path $outRoot 'campaign'
Write-Host "Campagne V17 multi-backend : $scene"
Write-Host "Configuration : $Rounds séries × $Frames images, échauffement $Warmup."
& $exe $scene $campaignRoot '--full-benchmark' '--rounds' "$Rounds" '--frames' "$Frames" '--warmup' "$Warmup"
if ($LASTEXITCODE -ne 0) { throw "La campagne moteur a échoué (code $LASTEXITCODE). Voir $campaignRoot" }

$summaryFile = Get-ChildItem -LiteralPath $campaignRoot -Filter 'summary.json' -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (!$summaryFile) { throw "summary.json introuvable sous $campaignRoot" }
$summary = Get-Content -LiteralPath $summaryFile.FullName -Raw | ConvertFrom-Json
$cases = @($summary.cases | Where-Object { $_.status -eq 'ok' -and $_.image })
if (!$cases.Count) { throw 'Aucune capture backend exploitable dans summary.json.' }
$groups = $cases | Group-Object { "$($_.scene)|$($_.camera)|$($_.width)x$($_.height)" }
$rows = New-Object System.Collections.ArrayList
$backendSpecs = @(
    @{Name='D3D11 Raster'; Match='Raster GPU qualité constante'; Tolerance=0.0; Reference=$true},
    @{Name='Vulkan Raster'; Match='Vulkan Raster preview \+ readback'; Tolerance=0.05; Reference=$false},
    @{Name='LumaTrace BVH4'; Match='LumaTrace BVH4 GPU lighting \+ Raster preview/readback'; Tolerance=0.10; Reference=$false}
)
foreach ($group in $groups) {
    $members = @($group.Group)
    $reference = $members | Where-Object { $_.variant -match $backendSpecs[0].Match -and [int]$_.readbackDepth -eq 1 } | Select-Object -First 1
    if (!$reference -or !$reference.image) { continue }
    $referencePath = Join-Path $summaryFile.DirectoryName $reference.image
    if (!(Test-Path -LiteralPath $referencePath)) { throw "Capture D3D11 manquante : $referencePath" }
    $referenceAnalysis = [LumaParityImage]::Analyze($referencePath)
    $refStats = $referenceAnalysis[0]
    Add-ImageRows $rows $reference 'D3D11 Raster' $referencePath 0.0 $refStats.MeanLinear
    foreach ($spec in $backendSpecs | Select-Object -Skip 1) {
        $candidate = $members | Where-Object { $_.variant -match $spec.Match } | Select-Object -First 1
        if (!$candidate -or !$candidate.image) { continue }
        $candidatePath = Join-Path $summaryFile.DirectoryName $candidate.image
        if (!(Test-Path -LiteralPath $candidatePath)) { throw "Capture $($spec.Name) manquante : $candidatePath" }
        Add-ImageRows $rows $candidate $spec.Name $candidatePath $spec.Tolerance $refStats.MeanLinear
    }
}
if (!$rows.Count) { throw 'Aucun groupe ne contient une capture D3D11 de référence à profondeur 1.' }
$rows | Export-Csv -LiteralPath (Join-Path $outRoot 'luminance.csv') -NoTypeInformation -Delimiter ';' -Encoding UTF8
$report = [pscustomobject]@{
    SchemaVersion=1; CampaignSummary=$summaryFile.FullName; Manifest='manifest.json'
    Reference='D3D11 Raster (readback depth 1)'; Luminance='linear Rec.709 from sRGB PNG'
    ReviewBands=[pscustomobject]@{Vulkan=0.05; LumaTrace=0.10}
    Rows=@($rows.ToArray()); Notice='REVIEW signale un écart à examiner; aucune correction automatique.'
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outRoot 'luminance.json') -Encoding UTF8
Write-Host "Rapports créés : $outRoot"
Write-Host "REVIEW = moyenne régionale hors bande initiale; comparer d'abord la région full et inspecter le PNG correspondant."
