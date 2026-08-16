param(
    [Parameter(Mandatory = $true)]
    [string]$Exe,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$exePath = (Resolve-Path -LiteralPath $Exe).Path
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$sourceRoot = Join-Path $outputRoot 'source-captures'
$screenshotRoot = Join-Path $outputRoot 'steam-screenshots'
$lifecycleRoot = Join-Path $sourceRoot 'soul-lifecycle'
$vignetteRoot = Join-Path $sourceRoot 'combat-vignette'
New-Item -ItemType Directory -Force -Path $sourceRoot, $screenshotRoot, $lifecycleRoot, $vignetteRoot | Out-Null

$ffmpeg = Get-Command ffmpeg.exe -ErrorAction Stop
$width = 1920
$height = 1080
$common = @('--capture-width', $width, '--capture-height', $height)

function Invoke-Capture {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [switch]$StoreScreenshot
    )

    $ppm = Join-Path $sourceRoot "$Name.ppm"
    $png = Join-Path $sourceRoot "$Name.png"
    $captureArguments = @($Arguments[0], $ppm)
    if ($Arguments.Count -gt 1) { $captureArguments += $Arguments[1..($Arguments.Count - 1)] }
    & $exePath @captureArguments @common
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $ppm -PathType Leaf)) {
        throw "Capture failed: $Name"
    }
    & $ffmpeg.Source -loglevel error -y -i $ppm $png
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $png -PathType Leaf)) {
        throw "PNG conversion failed: $Name"
    }
    Remove-Item -LiteralPath $ppm
    if ($StoreScreenshot) {
        Copy-Item -LiteralPath $png -Destination (Join-Path $screenshotRoot "$Name.png")
    }
}

Invoke-Capture '01-title' @('--capture-menu-frame', '--menu-page', 'main') -StoreScreenshot
Invoke-Capture '02-human' @('--capture-human-frame', '--capture-hide-hud') -StoreScreenshot
Invoke-Capture '03-soul' @('--capture-soul-frame', '--capture-hide-hud') -StoreScreenshot
Invoke-Capture '04-crowd' @('--capture-mosh-frame', '--capture-hide-hud') -StoreScreenshot
Invoke-Capture '05-phone' @('--capture-phone-frame', '--capture-hide-hud')
Invoke-Capture '06-pause' @('--capture-menu-frame', '--menu-page', 'pause')

& $exePath --capture-soul-lifecycle $lifecycleRoot --capture-hide-hud @common
if ($LASTEXITCODE -ne 0) { throw 'Soul lifecycle capture failed.' }
Get-ChildItem -LiteralPath $lifecycleRoot -Filter '*.ppm' | ForEach-Object {
    $png = [System.IO.Path]::ChangeExtension($_.FullName, '.png')
    & $ffmpeg.Source -loglevel error -y -i $_.FullName $png
    if ($LASTEXITCODE -ne 0) { throw "PNG conversion failed: $($_.Name)" }
    Remove-Item -LiteralPath $_.FullName
}

& $exePath --capture-cpu-demo $vignetteRoot @common
if ($LASTEXITCODE -ne 0) { throw 'Combat vignette capture failed.' }
Get-ChildItem -LiteralPath $vignetteRoot -Filter '*.ppm' | ForEach-Object {
    $frameNumber = [int]([System.IO.Path]::GetFileNameWithoutExtension($_.Name).Substring(6))
    if (($frameNumber % 10) -eq 0) {
        $png = [System.IO.Path]::ChangeExtension($_.FullName, '.png')
        & $ffmpeg.Source -loglevel error -y -i $_.FullName $png
        if ($LASTEXITCODE -ne 0) { throw "PNG conversion failed: $($_.Name)" }
        $storeName = switch ($frameNumber) {
            30 { '05-approach.png' }
            60 { '06-impact.png' }
            90 { '07-ingestion.png' }
            default { $null }
        }
        if ($storeName) {
            Copy-Item -LiteralPath $png -Destination (Join-Path $screenshotRoot $storeName)
        }
    }
    Remove-Item -LiteralPath $_.FullName
}

$identity = & $exePath --build-identity-json | ConvertFrom-Json
@{
    title = 'Data'
    sourceCommit = $identity.commit
    captureWidth = $width
    captureHeight = $height
    renderer = 'production desktop renderer'
    generatedArtwork = $false
    capturedAt = (Get-Date).ToUniversalTime().ToString('o')
    screenshots = @(Get-ChildItem -LiteralPath $screenshotRoot -Filter '*.png' | Select-Object -ExpandProperty Name)
    lifecycleManifest = 'source-captures/soul-lifecycle/manifest.csv'
    vignetteStoryboard = 'source-captures/combat-vignette'
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'capture-manifest.json') -Encoding UTF8

Write-Host "STOREFRONT_CAPTURE_OK output=$outputRoot commit=$($identity.commit)"
