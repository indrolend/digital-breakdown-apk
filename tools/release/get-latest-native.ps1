[CmdletBinding()]
param(
    [ValidateSet('Windows','Android','All')]
    [string]$Platform = 'Windows',
    [switch]$Launch,
    [switch]$Force,
    [string]$FallbackExecutable,
    [switch]$CheckOnly,
    [string]$InstalledVersion = '0.0.0',
    [string]$StatusPath
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

function Write-ProgressEvent {
    param([int]$Percent, [string]$Message)
    Write-Output ("@@DBPROGRESS|{0}|{1}" -f ([Math]::Max(0, [Math]::Min(100, $Percent))), $Message)
}

function Get-Manifest {
    $uri = "${ManifestUrl}?t=$([DateTimeOffset]::UtcNow.ToUnixTimeSeconds())"
    $manifest = Invoke-RestMethod -UseBasicParsing -Uri $uri -TimeoutSec 20
    if ([int]$manifest.schemaVersion -ne 3) { throw 'Unsupported release manifest schema.' }
    if ([string]$manifest.channel -ne 'latest-native') { throw 'Unexpected release channel.' }
    if ([string]$manifest.version -notmatch '^\d+\.\d+\.\d+$') { throw 'Release manifest has an invalid semantic version.' }
    if ([string]$manifest.commit -notmatch '^[0-9a-fA-F]{40}$') { throw 'Release manifest has an invalid commit identity.' }
    if ([string]$manifest.shortCommit -notmatch '^[0-9a-fA-F]{7}$' -or
        -not ([string]$manifest.commit).StartsWith([string]$manifest.shortCommit, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Release manifest short commit does not match its commit.'
    }
    if ($Platform -in @('Windows','All')) {
        if ([string]$manifest.windows.sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'Windows release checksum is invalid.' }
        if ([string]$manifest.windows.url -ne 'https://github.com/indrolend/digital-breakdown-apk/releases/download/latest-native/DigitalBreakdown-Windows.zip') {
            throw 'Windows release URL is not on the approved update origin.'
        }
    }
    if ($Platform -in @('Android','All')) {
        if ([string]$manifest.android.sha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'Android release checksum is invalid.' }
        if ([string]$manifest.android.url -ne 'https://github.com/indrolend/digital-breakdown-apk/releases/download/latest-native/DigitalBreakdown-Android.apk') {
            throw 'Android release URL is not on the approved update origin.'
        }
    }
    return $manifest
}

function Expand-VerifiedWindowsPackage {
    param(
        [Parameter(Mandatory)] [string]$Archive,
        [Parameter(Mandatory)] [string]$Destination
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $root = [IO.Path]::GetFullPath($Destination).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('/', [IO.Path]::DirectorySeparatorChar)
            if ([IO.Path]::IsPathRooted($name)) { throw "Package contains a rooted path: $($entry.FullName)" }
            $target = [IO.Path]::GetFullPath((Join-Path $Destination $name))
            if (-not $target.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Package contains a path outside its release directory: $($entry.FullName)"
            }
        }
    } finally {
        $zip.Dispose()
    }
    Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
}

function Assert-WindowsRuntime {
    param([Parameter(Mandatory)] [string]$Root)
    $required = @(
        'DigitalBreakdown.exe',
        'build-info.json',
        'updater\get-latest-native.ps1',
        'audio\menu_music.mp3',
        'audio\game_music.mp3',
        'audio\tv_room_pad.mp3'
    )
    foreach ($relative in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $relative) -PathType Leaf)) {
            throw "Windows package is incomplete: missing $relative"
        }
    }
    foreach ($directory in @('models','tv-gifs')) {
        $path = Join-Path $Root $directory
        if (-not (Test-Path -LiteralPath $path -PathType Container) -or
            -not (Get-ChildItem -LiteralPath $path -File -ErrorAction Stop | Select-Object -First 1)) {
            throw "Windows package is incomplete: missing $directory assets"
        }
    }
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
$currentVersion = try { [version]$InstalledVersion } catch { [version]'0.0.0' }
$releaseVersion = [version]([string]$manifest.version)
if ($CheckOnly) {
    if (-not $StatusPath) { throw 'CheckOnly requires StatusPath.' }
    $statusDirectory = Split-Path -Parent $StatusPath
    if ($statusDirectory) { New-Item -ItemType Directory -Force -Path $statusDirectory | Out-Null }
    $temporaryStatus = "$StatusPath.new"
    @(
        "available=$(if ($releaseVersion -gt $currentVersion) { 1 } else { 0 })"
        "version=$($manifest.version)"
        "commit=$($manifest.commit)"
    ) | Set-Content -LiteralPath $temporaryStatus -Encoding ASCII
    Move-Item -LiteralPath $temporaryStatus -Destination $StatusPath -Force
    Write-ProgressEvent 100 'Update check complete'
    exit 0
}
$fallbackVersion = [version]'0.0.0'
if ($FallbackExecutable -and (Test-Path -LiteralPath $FallbackExecutable -PathType Leaf)) {
    $fallbackInfoPath = Join-Path (Split-Path -Parent $FallbackExecutable) 'build-info.json'
    if (Test-Path -LiteralPath $fallbackInfoPath -PathType Leaf) {
        try {
            $fallbackInfo = Get-Content -LiteralPath $fallbackInfoPath -Raw | ConvertFrom-Json
            if ([string]$fallbackInfo.version -match '^\d+\.\d+\.\d+$') { $fallbackVersion = [version]([string]$fallbackInfo.version) }
        } catch { $fallbackVersion = [version]'0.0.0' }
    }
}
if ($Launch -and -not $Force -and $fallbackVersion -ge $releaseVersion) {
    Write-ProgressEvent 92 'Bundled release is current'
    Start-Process -FilePath $FallbackExecutable -WorkingDirectory (Split-Path -Parent $FallbackExecutable)
    Write-ProgressEvent 100 'Current release launched'
    exit 0
}
$installed = @{}

if ($Platform -in @('Windows','All')) {
    if (-not $manifest.windows.available) { throw 'Windows release is not available.' }

    $zip = Join-Path $DownloadRoot "DigitalBreakdown-Windows-$($manifest.shortCommit).zip"
    Get-VerifiedFile -Url ([string]$manifest.windows.url) -ExpectedSha256 ([string]$manifest.windows.sha256) -Destination $zip -StartPercent 20 -EndPercent 58 -Label 'Windows release' | Out-Null

    Write-ProgressEvent 65 'Preparing Windows release files'
    $target = Join-Path $ReleaseRoot "$($manifest.shortCommit)\windows"
    $exe = Join-Path $target 'DigitalBreakdown.exe'
    if ($Force -or -not (Test-Path $exe)) {
        $tempTarget = "$target.extracting"
        Remove-Item $tempTarget -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $tempTarget | Out-Null
        Expand-VerifiedWindowsPackage -Archive $zip -Destination $tempTarget

        $candidate = Get-ChildItem -Path $tempTarget -Filter 'DigitalBreakdown.exe' -Recurse | Select-Object -First 1
        if (-not $candidate) { throw 'Windows package does not contain DigitalBreakdown.exe.' }
        Assert-WindowsRuntime -Root $candidate.Directory.FullName
        $packageInfo = Get-Content -LiteralPath (Join-Path $candidate.Directory.FullName 'build-info.json') -Raw | ConvertFrom-Json
        if ([string]$packageInfo.commit -ne [string]$manifest.commit -or [string]$packageInfo.version -ne [string]$manifest.version) {
            throw 'Windows package identity does not match the verified release manifest metadata.'
        }

        Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
        Move-Item $candidate.Directory.FullName $target
        Remove-Item $tempTarget -Recurse -Force -ErrorAction SilentlyContinue
        $exe = Join-Path $target 'DigitalBreakdown.exe'
    }
    Write-ProgressEvent 82 'Windows release ready'

    $installed.windows = [pscustomobject]@{
        path = $exe
        sha256 = [string]$manifest.windows.sha256
    }

    # Refresh only from a package whose complete archive has already passed
    # SHA-256 verification and runtime validation. The original launcher checks
    # this per-user bootstrap location first on subsequent launches.
    $verifiedUpdater = Join-Path $target 'updater\get-latest-native.ps1'
    $bootstrapRoot = Join-Path $StateRoot 'updater'
    $bootstrapUpdater = Join-Path $bootstrapRoot 'get-latest-native.ps1'
    $bootstrapTemporary = "$bootstrapUpdater.new"
    New-Item -ItemType Directory -Force -Path $bootstrapRoot | Out-Null
    Copy-Item -LiteralPath $verifiedUpdater -Destination $bootstrapTemporary -Force
    Move-Item -LiteralPath $bootstrapTemporary -Destination $bootstrapUpdater -Force

    if ($Launch) {
        Write-ProgressEvent 92 'Launching latest Windows release'
        Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
    }
}

if ($Platform -in @('Android','All')) {
    if (-not $manifest.android.available) { throw 'Android release is not available.' }

    $apk = Join-Path $DownloadRoot "DigitalBreakdown-Android-$($manifest.shortCommit).apk"
    $start = if ($Platform -eq 'All') { 60 } else { 20 }
    $end = if ($Platform -eq 'All') { 88 } else { 86 }
    Get-VerifiedFile -Url ([string]$manifest.android.url) -ExpectedSha256 ([string]$manifest.android.sha256) -Destination $apk -StartPercent $start -EndPercent $end -Label 'Android release' | Out-Null

    $installed.android = [pscustomobject]@{
        path = $apk
        sha256 = [string]$manifest.android.sha256
        applicationId = [string]$manifest.android.applicationId
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
