# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
    Build a multi-resolution Windows ICO from the product's canonical PNG.
.DESCRIPTION
    LINK uses the canonical product PNG as the sole artwork source for Windows
    Discover.  Windows Explorer and the Win32 shell are much more reliable when
    an executable carries the normal set of icon sizes rather than a single
    oversized PNG-compressed ICO entry, so this script derives standard icon
    sizes at build time without introducing another hand-maintained asset.
#>
param(
    [Parameter(Mandatory = $true)][string]$InputPng,
    [Parameter(Mandatory = $true)][string]$OutputIco
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $InputPng)) {
    throw "Input PNG does not exist: $InputPng"
}

$directory = Split-Path -Parent $OutputIco
if ($directory) {
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
}

$source = [System.Drawing.Image]::FromFile($InputPng)
$payloads = New-Object System.Collections.Generic.List[object]
$sizes = @(16, 24, 32, 48, 64, 128, 256)

try {
    if ($source.Width -le 0 -or $source.Height -le 0) {
        throw "Canonical icon has invalid dimensions: $($source.Width)x$($source.Height)"
    }

    foreach ($size in $sizes) {
        $bitmap = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $bitmap.SetResolution(96, 96)
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($source, 0, 0, $size, $size)
            }
            finally {
                $graphics.Dispose()
            }

            $memory = New-Object System.IO.MemoryStream
            try {
                $bitmap.Save($memory, [System.Drawing.Imaging.ImageFormat]::Png)
                $payloads.Add([PSCustomObject]@{
                    Size = $size
                    Bytes = $memory.ToArray()
                })
            }
            finally {
                $memory.Dispose()
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }
}
finally {
    $source.Dispose()
}

$stream = [System.IO.File]::Create($OutputIco)
$writer = New-Object System.IO.BinaryWriter($stream)
try {
    # ICONDIR: reserved, type=icon, image count.
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$payloads.Count)

    $offset = 6 + (16 * $payloads.Count)
    foreach ($entry in $payloads) {
        $dimension = if ($entry.Size -eq 256) { 0 } else { $entry.Size }
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([byte]0)       # palette size
        $writer.Write([byte]0)       # reserved
        $writer.Write([uint16]1)     # colour planes
        $writer.Write([uint16]32)    # bit depth
        $writer.Write([uint32]$entry.Bytes.Length)
        $writer.Write([uint32]$offset)
        $offset += $entry.Bytes.Length
    }

    foreach ($entry in $payloads) {
        $writer.Write([byte[]]$entry.Bytes)
    }
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}

# Defensive verification of the resulting ICO directory.
$verify = [System.IO.File]::ReadAllBytes($OutputIco)
if ($verify.Length -lt 6 -or $verify[2] -ne 1 -or $verify[4] -ne $payloads.Count) {
    throw "Generated ICO failed structural verification: $OutputIco"
}
