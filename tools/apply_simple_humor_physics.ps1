$ErrorActionPreference = "Stop"

$BaseCommit = "82e64fac1e6badb4c4b7b7a4bddecce52e33000c"
$TargetBranch = "experiment/simple-humor-physics"
$BootstrapBranch = "experiment/simple-humor-physics-bootstrap"

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
if ($LASTEXITCODE -ne 0 -or -not $Repo) { throw "Run this from digital-breakdown-apk." }
Set-Location $Repo
Invoke-Git fetch origin --prune

& git ls-remote --exit-code --heads origin $TargetBranch *> $null
if ($LASTEXITCODE -eq 0) { throw "origin/$TargetBranch already exists; refusing to overwrite it." }

$baseDest = Join-Path $HOME "Projects\digital-breakdown-humor-physics"
$Dest = $baseDest
$ordinal = 2
while (Test-Path $Dest) { $Dest = "$baseDest-$ordinal"; $ordinal++ }

Invoke-Git worktree add --detach $Dest $BaseCommit
Set-Location $Dest
Invoke-Git switch -c $TargetBranch

$gameHpp = Join-Path $Dest "native\game\Game.hpp"
$gameCpp = Join-Path $Dest "native\game\Game.cpp"
$renderer = Join-Path $Dest "native-desktop\DesktopRenderer.cpp"

$hppNeedle = @'
    float hitDirectionLocal = 0.0f;
    float vacuumPullAmount = 0.0f;
'@
$hppReplacement = @'
    float hitDirectionLocal = 0.0f;
    // Intentionally cheap humor physics. This is a deterministic visual body
    // response, not a general rigid-body solver: impacts inject angular energy,
    // grounded enemies wobble upright, and loose/slurpable bodies tumble longer.
    float humorTiltX = 0.0f;
    float humorTiltZ = 0.0f;
    float humorSpin = 0.0f;
    float humorAngularX = 0.0f;
    float humorAngularZ = 0.0f;
    float humorSpinVelocity = 0.0f;
    float humorLimp = 0.0f;
    float vacuumPullAmount = 0.0f;
'@
Replace-Once $gameHpp $hppNeedle $hppReplacement

$impactNeedle = @'
    Vec3 away=normalized(Vec3{t.pos.x-state_.player.pos.x,0.0f,t.pos.z-state_.player.pos.z});
    t.vel.x+=away.x*2.4f; t.vel.z+=away.z*2.4f; t.vel.y=std::max(t.vel.y,1.2f);
'@
$impactReplacement = @'
    Vec3 away=normalized(Vec3{t.pos.x-state_.player.pos.x,0.0f,t.pos.z-state_.player.pos.z});
    t.vel.x+=away.x*2.4f; t.vel.z+=away.z*2.4f; t.vel.y=std::max(t.vel.y,1.2f);
    const float humorImpulse=clampf(2.6f+amount*1.8f+(t.brute?0.8f:0.0f),2.6f,7.5f);
    t.humorAngularX+=away.z*humorImpulse;
    t.humorAngularZ-=away.x*humorImpulse;
    const float spinSign=((index*17+state_.frame)%2)==0?1.0f:-1.0f;
    t.humorSpinVelocity+=spinSign*(1.8f+amount*1.35f);
    t.humorLimp=std::max(t.humorLimp,t.slurpable?1.0f:0.72f);
'@
Replace-Once $gameCpp $impactNeedle $impactReplacement

$updateNeedle = @'
        gameplay::updateLooseSoulMotion(t, dt);
        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);
        t.visibility = 1.0f;
'@
$updateReplacement = @'
        gameplay::updateLooseSoulMotion(t, dt);
        t.hitFlash = std::max(0.0f, t.hitFlash - TARGET_HITFLASH_DECAY_PER_FRAME);
        const float humorSpeed=std::sqrt(t.vel.x*t.vel.x+t.vel.z*t.vel.z);
        const float humorTargetLimp=t.slurpable?1.0f:clampf(t.hitFlash*0.85f+std::abs(t.vel.y)*0.09f+humorSpeed*0.025f,0.0f,0.82f);
        t.humorLimp+=(humorTargetLimp-t.humorLimp)*std::min(1.0f,dt*(humorTargetLimp>t.humorLimp?14.0f:4.2f));
        const bool looseHumor=t.slurpable||std::abs(t.vel.y)>0.12f||t.humorLimp>0.58f;
        const float humorSpring=looseHumor?1.2f:14.0f;
        const float humorDamping=looseHumor?1.7f:8.5f;
        t.humorAngularX+=(-t.humorTiltX*humorSpring-t.humorAngularX*humorDamping)*dt;
        t.humorAngularZ+=(-t.humorTiltZ*humorSpring-t.humorAngularZ*humorDamping)*dt;
        t.humorSpinVelocity*=std::exp(-(looseHumor?1.1f:6.0f)*dt);
        if(looseHumor){
            const float stupidTorque=std::sin(state_.time*5.7f+static_cast<float>(i)*1.91f)*(0.35f+0.8f*t.humorLimp);
            t.humorAngularX+=stupidTorque*dt;
            t.humorAngularZ+=std::cos(state_.time*4.3f+static_cast<float>(i))*0.55f*t.humorLimp*dt;
        }
        t.humorTiltX=clampf(t.humorTiltX+t.humorAngularX*dt,-1.45f,1.45f);
        t.humorTiltZ=clampf(t.humorTiltZ+t.humorAngularZ*dt,-1.45f,1.45f);
        t.humorSpin=std::fmod(t.humorSpin+t.humorSpinVelocity*dt,DB_PI*2.0f);
        t.visibility = 1.0f;
'@
Replace-Once $gameCpp $updateNeedle $updateReplacement

$rendererNeedle = @'
    const Quat rootQ=quaternionFromEulerXYZ(
        motionPitch+impactPitch+(target.attackTimer>0?windup*0.08f-reach*(0.16f+low*0.05f):0),
        target.visualYaw+PI,
        motionRoll+impactRoll+(target.attackTimer>0?side*(strike*0.18f-windup*0.24f):0));
'@
$rendererReplacement = @'
    const float humor=clampf(target.humorLimp,0.0f,1.0f);
    const float stupidBounce=std::sin(time*11.0f+target.phase)*0.055f*humor;
    const Quat rootQ=quaternionFromEulerXYZ(
        motionPitch+impactPitch+target.humorTiltX+stupidBounce+(target.attackTimer>0?windup*0.08f-reach*(0.16f+low*0.05f):0),
        target.visualYaw+PI+target.humorSpin,
        motionRoll+impactRoll+target.humorTiltZ+(target.attackTimer>0?side*(strike*0.18f-windup*0.24f):0));
'@
Replace-Once $renderer $rendererNeedle $rendererReplacement

Invoke-Git diff --check
Write-Host "`nHUMOR PHYSICS DIFF:"
& git diff --stat -- native/game/Game.hpp native/game/Game.cpp native-desktop/DesktopRenderer.cpp
if ($LASTEXITCODE -ne 0) { throw "git diff failed" }

Write-Host "`nBUILDING RELEASE..."
& .\tools\dbdev.ps1 desktop-build -Configuration Release
if ($LASTEXITCODE -ne 0) { throw "desktop-build failed with exit code $LASTEXITCODE" }

Write-Host "`nRUNNING TESTS..."
& .\tools\dbdev.ps1 desktop-test -Configuration Release
if ($LASTEXITCODE -ne 0) { throw "desktop-test failed with exit code $LASTEXITCODE" }

Invoke-Git add native/game/Game.hpp native/game/Game.cpp native-desktop/DesktopRenderer.cpp
Invoke-Git commit -m "Add intentionally cheap humor body physics"
Invoke-Git push -u origin $TargetBranch

& git push origin --delete $BootstrapBranch *> $null

$commit = (& git rev-parse HEAD).Trim()
Write-Host "`nHUMOR_PHYSICS_COMMIT=$commit"
Write-Host "WORKTREE=$Dest"
Write-Host "BRANCH=$TargetBranch"
Write-Host "`nLaunching..."
& .\tools\dbdev.ps1 desktop-run -Configuration Release
