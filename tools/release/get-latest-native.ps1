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

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectIdentityPath = Join-Path $RepoRoot 'distribution\project.json'
$ProjectIdentity = Get-Content -Raw -Path $ProjectIdentityPath | ConvertFrom-Json
if (-not $ProjectIdentity.manifest) { throw "Project identity is missing its canonical manifest: $ProjectIdentityPath" }
$ManifestUrl = [string]$ProjectIdentity.manifest
$StateRoot = Join-Path $env:LOCALAPPDATA 'DigitalBreakdown'
$DownloadRoot = Join-Path $StateRoot 'downloads'
$ReleaseRoot = Join-Path $StateRoot 'releases'
$StatePath = Join-Path $StateRoot 'release-state.json'

New-Item -ItemType Directory -Force -Path $StateRoot, $DownloadRoot, $ReleaseRoot | Out-Null

function Write-ProgressEvent {
    param([int]$Percent, [string]$Message)
    Write-Output ("@@DBPROGRESS|{0}|{1}" -f ([Math]::Max(0, [Math]::Min(100, $Percent))), $Message)
}

function Get-Manifest {
    $uri = "${ManifestUrl}?t=$([DateTimeOffset]::UtcNow.ToUnixTimeSeconds())"
    $manifest = Invoke-RestMethod -UseBasicParsing -Uri $uri -TimeoutSec 20
    if (-not $manifest.commit -or -not $manifest.shortCommit) { throw 'Release manifest is missing commit identity.' }
    if ($manifest.schemaVersion -and [int]$manifest.schemaVersion -gt 3) { throw "Unsupported release manifest schema $($manifest.schemaVersion)." }
    return $manifest
}

function Get-Artifact {
    param(
        [Parameter(Mandatory)] [object]$Manifest,
        [Parameter(Mandatory)] [string]$Platform
    )

    if ($Manifest.artifacts) {
        $match = @($Manifest.artifacts | Where-Object { $_.platform -eq $Platform } | Select-Object -First 1)
        if ($match.Count -gt 0) { return $match[0] }
    }

    if ($Platform -eq 'windows' -and $Manifest.windows -and $Manifest.windows.available) {
        return [pscustomobject]@{
            platform = 'windows'
            filename = 'DigitalBreakdown-Windows.zip'
            url = [string]$Manifest.windows.url
            sha256 = [string]$Manifest.windows.sha256
            package = 'zip'
            architecture = [string]$Manifest.windows.architecture
        }
    }
    if ($Platform -eq 'android' -and $Manifest.android -and $Manifest.android.available) {
        return [pscustomobject]@{
            platform = 'android'
            filename = 'DigitalBreakdown-Android.apk'
            url = [string]$Manifest.android.url
            sha256 = [string]$Manifest.android.sha256
            package = 'apk'
            architecture = [string]$Manifest.android.architecture
            applicationId = [string]$Manifest.android.applicationId
        }
    }
    return $null
}

function Get-VerifiedFile {
    param(
        [Parameter(Mandatory)] [string]$Url,
        [Parameter(Mandatory)] [string]$ExpectedSha256,
        [Parameter(Mandatory)] [string]$Destination,
        [Parameter(Mandatory)] [int]$StartPercent,
        [Parameter(Mandatory)] [int]$EndPercent,
        [Parameter(Mandatory)] [string]$Label
    )

    $expected = $ExpectedSha256.ToLowerInvariant()
    if ((Test-Path $Destination) -and -not $Force) {
        Write-ProgressEvent $StartPercent "Verifying cached $Label"
        $existing = (Get-FileHash $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existing -eq $expected) {
            Write-ProgressEvent $EndPercent "$Label already downloaded and verified"
            return $Destination
        }
    }

    Write-ProgressEvent $StartPercent "Downloading $Label"
    $temp = "$Destination.download"
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $temp -TimeoutSec 180

    Write-ProgressEvent ([Math]::Max($StartPercent, $EndPercent - 5)) "Verifying $Label checksum"
    $actual = (Get-FileHash $temp -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
        Remove-Item $temp -Force -ErrorAction SilentlyContinue
        throw "Checksum mismatch for $([IO.Path]::GetFileName($Destination))."
    }

    Move-Item $temp $Destination -Force
    Write-ProgressEvent $EndPercent "$Label downloaded and verified"
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

Write-ProgressEvent 4 'Checking latest native release'
$manifest = Get-Manifest
Write-ProgressEvent 14 "Release $($manifest.shortCommit) found"
$installed = @{}

if ($Platform -in @('Windows','All')) {
    $windowsArtifact = Get-Artifact -Manifest $manifest -Platform 'windows'
    if (-not $windowsArtifact) { throw 'Windows release is not available.' }

    $zip = Join-Path $DownloadRoot "DigitalBreakdown-Windows-$($manifest.shortCommit).zip"
    Get-VerifiedFile -Url ([string]$windowsArtifact.url) -ExpectedSha256 ([string]$windowsArtifact.sha256) -Destination $zip -StartPercent 20 -EndPercent 58 -Label 'Windows release' | Out-Null

    Write-ProgressEvent 65 'Preparing Windows release files'
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
    Write-ProgressEvent 82 'Windows release ready'

    $installed.windows = [pscustomobject]@{
        path = $exe
        sha256 = [string]$windowsArtifact.sha256
        architecture = [string]$windowsArtifact.architecture
    }

    if ($Launch) {
        Write-ProgressEvent 92 'Launching latest Windows release'
        Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
    }
}

if ($Platform -in @('Android','All')) {
    $androidArtifact = Get-Artifact -Manifest $manifest -Platform 'android'
    if (-not $androidArtifact) { throw 'Android release is not available.' }

    $apk = Join-Path $DownloadRoot "DigitalBreakdown-Android-$($manifest.shortCommit).apk"
    $start = if ($Platform -eq 'All') { 60 } else { 20 }
    $end = if ($Platform -eq 'All') { 88 } else { 86 }
    Get-VerifiedFile -Url ([string]$androidArtifact.url) -ExpectedSha256 ([string]$androidArtifact.sha256) -Destination $apk -StartPercent $start -EndPercent $end -Label 'Android release' | Out-Null

    $installed.android = [pscustomobject]@{
        path = $apk
        sha256 = [string]$androidArtifact.sha256
        applicationId = if ($androidArtifact.applicationId) { [string]$androidArtifact.applicationId } else { [string]$manifest.android.applicationId }
        architecture = [string]$androidArtifact.architecture
    }
}

Write-ProgressEvent 96 'Saving release state'
Save-State -Manifest $manifest -Installed $installed
Write-ProgressEvent 100 $(if ($Launch) { 'Latest release launched' } else { 'Release download complete' })

[pscustomobject]@{
    commit = [string]$manifest.commit
    shortCommit = [string]$manifest.shortCommit
    platform = $Platform
    launched = [bool]$Launch
    statePath = $StatePath
    installed = $installed
} | ConvertTo-Json -Depth 8
