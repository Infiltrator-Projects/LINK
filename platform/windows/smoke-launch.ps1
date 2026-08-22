# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
    Launch-smoke-test a LINK-family Windows Discover executable.
.DESCRIPTION
    A successful compile is not sufficient evidence that a distributable GUI
    can start on a clean Windows runner. This script starts the executable,
    waits for a top-level window, validates the product title and then closes
    the process. Any immediate loader failure, missing runtime dependency,
    startup crash or absent main window fails CI before a release can publish.
#>
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$ExpectedTitle
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Discover executable does not exist: $Executable"
}

$process = Start-Process -FilePath $Executable -PassThru
try {
    $deadline = (Get-Date).AddSeconds(12)
    do {
        Start-Sleep -Milliseconds 150
        $process.Refresh()
        if ($process.HasExited) {
            throw "Discover process exited during startup with code $($process.ExitCode)."
        }
    } while ($process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)

    if ($process.MainWindowHandle -eq 0) {
        throw 'Discover process stayed alive but never created a main window.'
    }

    $process.Refresh()
    if ($process.MainWindowTitle -notlike "*$ExpectedTitle*") {
        throw "Unexpected main-window title: '$($process.MainWindowTitle)'"
    }

    if (-not $process.CloseMainWindow()) {
        throw 'Discover main window could not be closed cleanly.'
    }
    if (-not $process.WaitForExit(5000)) {
        throw 'Discover process did not exit after its main window closed.'
    }
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    $process.Dispose()
}
