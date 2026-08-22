# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
    Wrap a PNG image in a Windows ICO container without altering the pixels.
.DESCRIPTION
    Windows Vista and later accept PNG-compressed image entries inside ICO
    resources.  LINK uses this to make the product's canonical iPhone app icon
    the Windows executable icon as well, avoiding a second hand-maintained
    artwork file that can drift from the product identity.
#>
param(
    [Parameter(Mandatory = $true)][string]$InputPng,
    [Parameter(Mandatory = $true)][string]$OutputIco
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$png = [System.IO.File]::ReadAllBytes($InputPng)
if ($png.Length -lt 24) {
    throw "PNG is too small to contain a valid IHDR chunk: $InputPng"
}

$signature = [byte[]](0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A)
for ($i = 0; $i -lt $signature.Length; $i++) {
    if ($png[$i] -ne $signature[$i]) {
        throw "Input is not a PNG file: $InputPng"
    }
}

function Read-BigEndianUInt32([byte[]]$Bytes, [int]$Offset) {
    return ([uint32]$Bytes[$Offset] -shl 24) -bor
           ([uint32]$Bytes[$Offset + 1] -shl 16) -bor
           ([uint32]$Bytes[$Offset + 2] -shl 8) -bor
           [uint32]$Bytes[$Offset + 3]
}

$width = Read-BigEndianUInt32 $png 16
$height = Read-BigEndianUInt32 $png 20
if ($width -eq 0 -or $height -eq 0 -or $width -gt 256 -or $height -gt 256) {
    throw "Windows ICO entry must be between 1 and 256 pixels: ${width}x${height}"
}

$directory = Split-Path -Parent $OutputIco
if ($directory) {
    [System.IO.Directory]::CreateDirectory($directory) | Out-Null
}

$stream = [System.IO.File]::Create($OutputIco)
$writer = New-Object System.IO.BinaryWriter($stream)
try {
    # ICONDIR header: reserved, type=icon, image count=1.
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]1)

    # ICONDIRENTRY. A zero dimension byte represents 256 pixels by spec.
    $writer.Write([byte]($(if ($width -eq 256) { 0 } else { $width })))
    $writer.Write([byte]($(if ($height -eq 256) { 0 } else { $height })))
    $writer.Write([byte]0)       # palette size: not applicable to PNG
    $writer.Write([byte]0)       # reserved
    $writer.Write([uint16]1)     # colour planes
    $writer.Write([uint16]32)    # nominal bit depth
    $writer.Write([uint32]$png.Length)
    $writer.Write([uint32]22)    # 6-byte header + 16-byte directory entry

    # PNG-compressed icon payload. No resampling or re-encoding occurs.
    $writer.Write($png)
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}
