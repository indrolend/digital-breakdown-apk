$ErrorActionPreference = "Stop"

$BaseCommit = "7222534d22bb6d33677e714a60678886e49fa4ae"
$TargetBranch = "restore/blob-tracking-visual"
$BootstrapBranch = "automation/blob-tracking-bootstrap"

function Invoke-Git {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Args)
    & git @Args
    if ($LASTEXITCODE -ne 0) { throw "git $($Args -join ' ') failed with exit code $LASTEXITCODE" }
}

function Replace-Once {
    param([string]$Path, [string]$Needle, [string]$Replacement)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    $text = [System.IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
    $first = $text.IndexOf($Needle, [System.StringComparison]::Ordinal)
    if ($first -lt 0) { throw "Expected source anchor not found in $Path" }
    $second = $text.IndexOf($Needle, $first + $Needle.Length, [System.StringComparison]::Ordinal)
    if ($second -ge 0) { throw "Source anchor was not unique in $Path" }
    $updated = $text.Substring(0, $first) + $Replacement + $text.Substring($first + $Needle.Length)
    [System.IO.File]::WriteAllText($Path, $updated, $utf8NoBom)
}

$Repo = (& git rev-parse --show-toplevel).Trim()
if ($LASTEXITCODE -ne 0 -or -not $Repo) { throw "Run this from the digital-breakdown-apk repository." }
Set-Location $Repo

Invoke-Git fetch origin --prune

& git ls-remote --exit-code --heads origin $TargetBranch *> $null
if ($LASTEXITCODE -eq 0) { throw "origin/$TargetBranch already exists; refusing to overwrite it." }

$baseDest = Join-Path $HOME "Projects\digital-breakdown-blob-tracking"
$Dest = $baseDest
$ordinal = 2
while (Test-Path $Dest) {
    $Dest = "$baseDest-$ordinal"
    $ordinal++
}

Invoke-Git worktree add --detach $Dest $BaseCommit
Set-Location $Dest
Invoke-Git switch -c $TargetBranch

$gameHpp = Join-Path $Dest "native\game\Game.hpp"
$gameCpp = Join-Path $Dest "native\game\Game.cpp"

$hppNeedle = @'
    float hitDirectionLocal = 0.0f;
    float vacuumPullAmount = 0.0f;
'@
$hppReplacement = @'
    float hitDirectionLocal = 0.0f;
    // Visual-only projectile impact state, ported from the localized-shot-wave browser build.
    // It drives a dent and travelling ripple through the existing soul lattice without
    // changing damage, collision, capture, networking authority, or economy rules.
    float latticeHitPulse = 0.0f;
    float latticeHitVelocity = 0.0f;
    Vec3 latticeHitDirection{0.0f,0.18f,-1.0f};
    Vec3 latticeImpactLocal{};
    float vacuumPullAmount = 0.0f;
'@
Replace-Once $gameHpp $hppNeedle $hppReplacement

$resetNeedle = @'
void Game::resetSoulLattice(TargetState& target) {
    target.latticeVisualPull=0;target.latticeVisualPullVelocity=0;target.tetherVisible=false;target.tetherWidth=0;
'@
$resetReplacement = @'
void Game::resetSoulLattice(TargetState& target) {
    target.latticeVisualPull=0;target.latticeVisualPullVelocity=0;target.tetherVisible=false;target.tetherWidth=0;
    target.latticeHitPulse=0;target.latticeHitVelocity=0;target.latticeHitDirection={0.0f,0.18f,-1.0f};target.latticeImpactLocal={};
'@
Replace-Once $gameCpp $resetNeedle $resetReplacement

$latticeStartNeedle = @'
    for(int i=0;i<TARGET_COUNT;++i){TargetState& target=state_.targets[i];
        if(!target.alive){target.tetherVisible=false;continue;}
'@
$latticeStartReplacement = @'
    for(int i=0;i<TARGET_COUNT;++i){TargetState& target=state_.targets[i];
        springScalar(target.latticeHitPulse,target.latticeHitVelocity,0.0f,6.0f,0.38f,step,target.latticeHitPulse,target.latticeHitVelocity);
        if(std::abs(target.latticeHitPulse)<0.002f&&std::abs(target.latticeHitVelocity)<0.002f){target.latticeHitPulse=0.0f;target.latticeHitVelocity=0.0f;}
        if(!target.alive){target.tetherVisible=false;continue;}
'@
Replace-Once $gameCpp $latticeStartNeedle $latticeStartReplacement

$nodeNeedle = @'
            desired+=pullDir*(visualPull*neck*(0.18f+armPattern*0.08f)+direct*0.12f+visualPull*shoulder*0.10f+collapse*facing*0.12f-visualPull*rearLag*0.13f);desired+=radial*(-taper);desired+=side*(stressWave*surface*(0.04f+cheek*0.04f));desired+=up*(std::sin(state_.time*10.0f+i*3.1f+n)*visualPull*surface*0.045f);
            if(ingest>0.001f&&livePin){
'@
$nodeReplacement = @'
            desired+=pullDir*(visualPull*neck*(0.18f+armPattern*0.08f)+direct*0.12f+visualPull*shoulder*0.10f+collapse*facing*0.12f-visualPull*rearLag*0.13f);desired+=radial*(-taper);desired+=side*(stressWave*surface*(0.04f+cheek*0.04f));desired+=up*(std::sin(state_.time*10.0f+i*3.1f+n)*visualPull*surface*0.045f);
            const float latticeHitPulse=clampf(target.latticeHitPulse,0.0f,1.0f);
            if(latticeHitPulse>0.001f&&surface>0.0f){
                Vec3 shotDirection=target.latticeHitDirection;if(lengthSq(shotDirection)>0.0001f)shotDirection=normalized(shotDirection);else shotDirection={0.0f,0.18f,-1.0f};
                const Vec3 impactOffset=rest-target.latticeImpactLocal;const float localDist=length(impactOffset);const float hitAge=1.0f-latticeHitPulse;
                const float waveRadius=hitAge*0.23f*3.4f,waveWidth=0.18f+hitAge*0.12f;
                const float waveBand=1.0f-smoothRange(std::abs(localDist-waveRadius),0.0f,waveWidth);
                const float nearImpact=1.0f-smoothRange(localDist,0.0f,0.42f);
                const float entryDent=-0.34f*nearImpact*latticeHitPulse;
                const float travellingRipple=std::sin(localDist*18.0f-hitAge*18.0f)*0.10f*waveBand;
                desired+=shotDirection*(entryDent+travellingRipple);
                desired+=outward*((0.10f*waveBand+0.06f*nearImpact)*latticeHitPulse);
            }
            if(ingest>0.001f&&livePin){
'@
Replace-Once $gameCpp $nodeNeedle $nodeReplacement

$hitNeedle = @'
            if(headshot&&target.grabbedPlayerId>=0)releaseTargetGrab(i);
            if(!damageSoulShell(i,headshot?headshotDamage(target):shotDamage)) continue;
'@
$hitReplacement = @'
            if(headshot&&target.grabbedPlayerId>=0)releaseTargetGrab(i);
            const Vec3 bulletTravel=b.pos-previous;
            const float bulletTravelSq=std::max(lengthSq(bulletTravel),0.000001f);
            const float impactT=clampf(dot3(shellCenter-previous,bulletTravel)/bulletTravelSq,0.0f,1.0f);
            const Vec3 impactWorld=previous+bulletTravel*impactT;
            Vec3 hitDirection=lengthSq(b.vel)>0.0001f?normalized(b.vel):(lengthSq(bulletTravel)>0.0001f?normalized(bulletTravel):Vec3{0.0f,0.18f,-1.0f});
            hitDirection.y=std::max(0.18f,hitDirection.y);
            target.latticeHitPulse=std::min(1.0f,target.latticeHitPulse+0.95f);
            target.latticeHitVelocity+=8.5f;
            target.latticeHitDirection=hitDirection;
            target.latticeImpactLocal=impactWorld-(target.pos+Vec3{0.0f,0.57f,0.0f});
            target.latticeImpactLocal.x=clampf(target.latticeImpactLocal.x,-0.3105f,0.3105f);
            target.latticeImpactLocal.y=clampf(target.latticeImpactLocal.y,-0.3105f,0.3105f);
            target.latticeImpactLocal.z=clampf(target.latticeImpactLocal.z,-0.3105f,0.3105f);
            if(!damageSoulShell(i,headshot?headshotDamage(target):shotDamage)) continue;
'@
Replace-Once $gameCpp $hitNeedle $hitReplacement

Invoke-Git diff --check

Write-Host "`nBLOB TRACKING PORT DIFF:"
& git diff --stat -- native/game/Game.hpp native/game/Game.cpp
& git diff -- native/game/Game.hpp native/game/Game.cpp
if ($LASTEXITCODE -ne 0) { throw "git diff failed" }

Write-Host "`nBUILDING RELEASE..."
& .\tools\dbdev.ps1 desktop-build -Configuration Release
if ($LASTEXITCODE -ne 0) { throw "desktop-build failed with exit code $LASTEXITCODE" }

Write-Host "`nRUNNING TESTS..."
& .\tools\dbdev.ps1 desktop-test -Configuration Release
if ($LASTEXITCODE -ne 0) { throw "desktop-test failed with exit code $LASTEXITCODE" }

Invoke-Git add native/game/Game.hpp native/game/Game.cpp
Invoke-Git commit -m "Restore localized blob tracking impact wave"
Invoke-Git push -u origin $TargetBranch

# The bootstrap branch exists only to deliver this self-applying port.
& git push origin --delete $BootstrapBranch *> $null

$commit = (& git rev-parse HEAD).Trim()
Write-Host "`nRESTORED_COMMIT=$commit"
Write-Host "WORKTREE=$Dest"
Write-Host "BRANCH=$TargetBranch"
Write-Host "`nLaunching the restored build..."
& .\tools\dbdev.ps1 desktop-run -Configuration Release
