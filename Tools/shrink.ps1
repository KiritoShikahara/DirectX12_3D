# 使い方: powershell -File Tools/shrink.ps1 <in.png> <out.png> [width]
param([string]$In, [string]$Out, [int]$Width = 640)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile((Resolve-Path $In))
$h = [int]($img.Height * $Width / $img.Width)
$bmp = New-Object System.Drawing.Bitmap($img, $Width, $h)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$img.Dispose(); $bmp.Dispose()
