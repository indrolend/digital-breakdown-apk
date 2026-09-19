param(
    [Parameter(Mandatory = $true)]
    [string]$FramesDirectory,

    [string]$OutputPath,

    [ValidateSet(15, 20, 30, 60)]
    [int]$Framerate = 30
)

$ErrorActionPreference = 'Stop'

$resolvedFrames = (Resolve-Path -LiteralPath $FramesDirectory).Path
$firstFrame = Join-Path $resolvedFrames 'frame-000000.ppm'
if (-not (Test-Path -LiteralPath $firstFrame)) {
    throw "No agent recording begins at $firstFrame"
}

$ffmpeg = Get-Command ffmpeg -ErrorAction Stop
if (-not $OutputPath) {
    $OutputPath = Join-Path (Split-Path -Parent $resolvedFrames) ((Split-Path -Leaf $resolvedFrames) + '.mp4')
}
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $resolvedOutput
if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory | Out-Null
}

& $ffmpeg.Source `
    -hide_banner -loglevel error -y `
    -framerate $Framerate `
    -i (Join-Path $resolvedFrames 'frame-%06d.ppm') `
    -c:v libx264 -preset medium -crf 18 `
    -pix_fmt yuv420p -movflags +faststart `
    $resolvedOutput

if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $resolvedOutput)) {
    throw 'Agent playtest video encoding failed.'
}

$video = Get-Item -LiteralPath $resolvedOutput
Write-Host "AGENT_VIDEO_READY path=$($video.FullName) bytes=$($video.Length) fps=$Framerate" -ForegroundColor Green
