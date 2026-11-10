# Generate a standalone multi-resolution Windows ICO for CSE WARP V17.
# No external packages: Windows GDI+ (System.Drawing) and PowerShell only.
param([Parameter(Mandatory = $true)][string] $OutputPath)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function RoundedRectangle([int]$x,[int]$y,[int]$w,[int]$h,[int]$radius) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $d = 2*$radius
    $path.AddArc($x,$y,$d,$d,180,90)
    $path.AddArc($x+$w-$d,$y,$d,$d,270,90)
    $path.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90)
    $path.AddArc($x,$y+$h-$d,$d,$d,90,90)
    $path.CloseFigure()
    return $path
}
$bmp=[System.Drawing.Bitmap]::new(256,256,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$g.Clear([System.Drawing.Color]::FromArgb(255,12,18,29))

$outer=RoundedRectangle 8 8 240 240 38
$baseBrush=[System.Drawing.Drawing2D.LinearGradientBrush]::new(
    [System.Drawing.Rectangle]::new(12,12,232,232),
    [System.Drawing.Color]::FromArgb(255,35,44,56),
    [System.Drawing.Color]::FromArgb(255,10,17,30), 90.0)
$g.FillPath($baseBrush,$outer)
$outerPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,235,242,249),8)
$g.DrawPath($outerPen,$outer)
$inner=RoundedRectangle 31 32 194 194 28
$innerPen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,22,198,241),7)
$g.DrawPath($innerPen,$inner)

# A subtle cyan-blue globe element inspired by the CSE WARP logo.
$globeBrush=[System.Drawing.Drawing2D.LinearGradientBrush]::new(
    [System.Drawing.Rectangle]::new(31,178,92,47),
    [System.Drawing.Color]::FromArgb(255,11,136,201),
    [System.Drawing.Color]::FromArgb(255,2,28,68), 70.0)
$g.FillEllipse($globeBrush,26,188,108,80)
$globePen=[System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,35,221,255),3)
$g.DrawArc($globePen,28,188,108,80,184,140)

$white=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
$cyan=[System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,17,218,248))
$style=[System.Drawing.FontStyle]::Bold
$unit=[System.Drawing.GraphicsUnit]::Pixel
$fCse=[System.Drawing.Font]::new('Arial',57,$style,$unit)
$fWarp=[System.Drawing.Font]::new('Arial',50,$style,$unit)
$fV17=[System.Drawing.Font]::new('Arial',69,$style,$unit)
$g.DrawString('CSE',$fCse,$white,[float]53,[float]40)
$g.DrawString('WARP',$fWarp,$white,[float]37,[float]95)
$g.DrawString('V17',$fV17,$cyan,[float]69,[float]151)

$bytes=New-Object 'System.Collections.Generic.List[byte[]]'
foreach($size in @(16,24,32,48,64,128,256)){
    $scaled=[System.Drawing.Bitmap]::new($size,$size,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $sg=[System.Drawing.Graphics]::FromImage($scaled)
    $sg.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $sg.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $sg.DrawImage($bmp,[System.Drawing.Rectangle]::new(0,0,$size,$size))
    $sg.Dispose()
    $ms=[System.IO.MemoryStream]::new()
    $scaled.Save($ms,[System.Drawing.Imaging.ImageFormat]::Png)
    $bytes.Add($ms.ToArray())
    $ms.Dispose();$scaled.Dispose()
}
$full=[System.IO.Path]::GetFullPath($OutputPath)
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($full)) | Out-Null
$stream=[System.IO.File]::Create($full)
$writer=[System.IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0)     # ICO reserved
$writer.Write([UInt16]1)     # ICO type
$writer.Write([UInt16]$bytes.Count)
$offset=[uint32](6+16*$bytes.Count)
$sizes=@(16,24,32,48,64,128,256)
for($i=0;$i -lt $bytes.Count;$i++){
    $size=$sizes[$i]
    $writer.Write([byte]$(if($size -eq 256){0}else{$size}))
    $writer.Write([byte]$(if($size -eq 256){0}else{$size}))
    $writer.Write([byte]0)   # palette colours
    $writer.Write([byte]0)   # reserved
    $writer.Write([UInt16]1) # planes
    $writer.Write([UInt16]32)# bpp
    $writer.Write([UInt32]$bytes[$i].Length)
    $writer.Write([UInt32]$offset)
    $offset += [uint32]$bytes[$i].Length
}
foreach($entry in $bytes){$writer.Write([byte[]]$entry)}
$writer.Flush();$writer.Dispose();$stream.Dispose()
$fCse.Dispose();$fWarp.Dispose();$fV17.Dispose()
$white.Dispose();$cyan.Dispose();$globeBrush.Dispose();$globePen.Dispose()
$outerPen.Dispose();$innerPen.Dispose();$baseBrush.Dispose()
$outer.Dispose();$inner.Dispose();$g.Dispose();$bmp.Dispose()
Write-Host "Generated CSE WARP V17 multi-resolution ICO: $full"
