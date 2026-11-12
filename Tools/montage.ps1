# 複数のスクリーンショットを1枚に並べる(確認用)
# 使い方: powershell -ExecutionPolicy Bypass -File Tools/montage.ps1 <out.png> <tileWidth> <columns> <in1.png> [in2.png ...]
param([string]$Out, [int]$TileWidth, [int]$Columns, [Parameter(ValueFromRemainingArguments = $true)][string[]]$Inputs)
Add-Type -AssemblyName System.Drawing
$images = @()
foreach ($p in $Inputs) {
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $p))
    $images += [System.Drawing.Image]::FromStream((New-Object System.IO.MemoryStream(, $bytes)))
}
$tileH = [int]($images[0].Height * $TileWidth / $images[0].Width)
$rows = [math]::Ceiling($images.Count / $Columns)
$bmp = New-Object System.Drawing.Bitmap(($TileWidth * $Columns), ($tileH * $rows))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
for ($i = 0; $i -lt $images.Count; $i++) {
    $x = ($i % $Columns) * $TileWidth
    $y = [math]::Floor($i / $Columns) * $tileH
    $g.DrawImage($images[$i], $x, $y, $TileWidth, $tileH)
    $g.DrawString("$($i + 1)", (New-Object System.Drawing.Font("Arial", 14)), [System.Drawing.Brushes]::Yellow, $x + 4, $y + 4)
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
foreach ($im in $images) { $im.Dispose() }
