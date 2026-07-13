[CmdletBinding()]
param(
    [ValidateSet('Windows','Android','All')]
    [string]$Platform = 'Windows',
    [switch]$Launch,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$ManifestUrl = 'https://github.com/indrolend/digital-breakdown-apk/releases/download/latest-native/build-manifest.json'
$StateRoot = Join-Path $env:LOCALAPPDATA 'DigitalBreakdown'
$DownloadRoot = Join-Path $StateRoot 'downloads'
$ReleaseRoot = Join-Path $StateRoot 'releases'
$StatePath = Join-Path $StateRoot 'release-state.json'

New-Item -ItemType Directory -Force -Path $StateRoot, $DownloadRoot, $ReleaseRoot | Out-Null

function Get-Manifest {
    $uri = "$ManifestUrl?t=$([DateTimeOffset]::UtcNow.ToUnixTimeSeconds())"
    $manifest = Invoke-RestMethod -UseBasicParsing -Uri $uri -TimeoutSec 20
    if (-not $manifest.commit -or -not $manifest.shortCommit) {
        throw 'Release manifest is missing commit identity.'
    }
    return $manifest
}

function Get-VerifiedFile {
    param(
        [Parameter(Mandatory)] [string]$Url,
        [Parameter(Mandatory)] [string]$ExpectedSha256,
        [Parameter(Mandatory)] [string]$Destination
    )

    $expected = $ExpectedSha256.ToLowerInvariant()
    if ((Test-Path $Destination) -and -not $Force) {
        $existing = (Get-FileHash $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existing -eq $expected) { return $Destination }
    }

    $temp = "$Destination.download"
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $temp -TimeoutSec 180

    $actual = (Get-FileHash $temp -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
        Remove-Item $temp -Force -ErrorAction SilentlyContinue
        throw "Checksum mismatch for $([IO.Path]::GetFileName($Destination))."
    }

    Move-Item $temp $Destination -Force
    return $Destination
}

function Save-State {
    param([object]$Manifest, [hashtable]$Installed)

    [pscustomobject]@{
        schemaVersion = 1
        commit = [string]$Manifest.commit
        shortCommit = [string]$Manifest.shortCommit
        channel = [string]$Manifest.channel
        installed = $Installed
        updatedAt = (Get-Date).ToString('o')
    } | ConvertTo-Json -Depth 8 | Set-Content -Path $StatePath -Encoding UTF8
}

$manifest = Get-Manifest
$installed = @{}

if ($Platform -in @('Windows','All')) {
    if (-not $manifest.windows.available) { throw 'Windows release is not available.' }

    $zip = Join-Path $DownloadRoot "DigitalBreakdown-Windows-$($manifest.shortCommit).zip"
    Get-VerifiedFile -Url ([string]$manifest.windows.url) -ExpectedSha256 ([string]$manifest.windows.sha256) -Destination $zip | Out-Null

    $target = Join-Path $ReleaseRoot "$($manifest.shortCommit)\windows"
    $exe = Join-Path $target 'DigitalBreakdown.exe'
    if ($Force -or -not (Test-Path $exe)) {
        $tempTarget = "$target.extracting"
        Remove-Item $tempTarget -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $tempTarget | Out-Null
        Expand-Archive -Path $zip -DestinationPath $tempTarget -Force

        $candidate = Get-ChildItem -Path $tempTarget -Filter 'DigitalBreakdown.exe' -Recurse | Select-Object -First 1
        if (-not $candidate) { throw 'Windows package does not contain DigitalBreakdown.exe.' }

        Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
        Move-Item $candidate.Directory.FullName $target
        Remove-Item $tempTarget -Recurse -Force -ErrorAction SilentlyContinue
        $exe = Join-Path $target 'DigitalBreakdown.exe'
    }

    $installed.windows = [pscustomobject]@{
        path = $exe
        sha256 = [string]$manifest.windows.sha256
    }

    if ($Launch) {
        Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
    }
}

if ($Platform -in @('Android','All')) {
    if (-not $manifest.android.available) { throw 'Android release is not available.' }

    $apk = Join-Path $DownloadRoot "DigitalBreakdown-Android-$($manifest.shortCommit).apk"
    Get-VerifiedFile -Url ([string]$manifest.android.url) -ExpectedSha256 ([string]$manifest.android.sha256) -Destination $apk | Out-Null

    $installed.android = [pscustomobject]@{
        path = $apk
        sha256 = [string]$manifest.android.sha256
        applicationId = [string]$manifest.android.applicationId
    }
}

Save-State -Manifest $manifest -Installed $installed

[pscustomobject]@{
    commit = [string]$manifest.commit
    shortCommit = [string]$manifest.shortCommit
    platform = $Platform
    launched = [bool]$Launch
    statePath = $StatePath
    installed = $installed
} | ConvertTo-Json -Depth 8
