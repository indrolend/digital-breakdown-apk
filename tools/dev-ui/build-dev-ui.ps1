[CmdletBinding()]
param(
    [switch]$Launch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Source = Join-Path $PSScriptRoot 'DigitalBreakdownDev.cs'
$Output = Join-Path $RepoRoot 'DigitalBreakdownDev.exe'

function Resolve-CSharpCompiler {
    $command = Get-Command csc.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe",
        "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe"
    )

    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if (-not (Test-Path $Source)) {
    throw "Missing source file: $Source"
}

$Compiler = Resolve-CSharpCompiler
if (-not $Compiler) {
    throw 'C# compiler not found. Install the .NET Framework developer tools or Visual Studio Build Tools.'
}

Write-Host 'Building DigitalBreakdownDev.exe...' -ForegroundColor Cyan
& $Compiler `
    /nologo `
    /target:winexe `
    /optimize+ `
    /platform:anycpu `
    /out:$Output `
    /reference:System.dll `
    /reference:System.Core.dll `
    /reference:System.Drawing.dll `
    /reference:System.Windows.Forms.dll `
    $Source

if ($LASTEXITCODE -ne 0 -or -not (Test-Path $Output)) {
    throw 'DigitalBreakdownDev.exe compilation failed.'
}

Write-Host "SUCCESS  $Output" -ForegroundColor Green

if ($Launch) {
    Start-Process -FilePath $Output -WorkingDirectory $RepoRoot
}
