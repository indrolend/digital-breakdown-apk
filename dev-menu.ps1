$ErrorActionPreference = "Stop"

function Pause-Menu {
  Write-Host ""
  Read-Host "Press Enter to continue"
}

function Show-Menu {
  Clear-Host
  Write-Host "====================================="
  Write-Host " Digital Breakdown Dev Menu"
  Write-Host "====================================="
  Write-Host ""
  Write-Host "1. Build / install / run APK"
  Write-Host "2. Copy debug log to clipboard"
  Write-Host "3. Make context pack"
  Write-Host "4. Git status"
  Write-Host "5. Git pull"
  Write-Host "6. Git add / commit / push"
  Write-Host "7. Switch runtime: stylo-v2.mjs"
  Write-Host "8. Open project status doc"
  Write-Host "9. Open latest log"
  Write-Host "10. Exit"
  Write-Host ""
}

while ($true) {
  Show-Menu
  $choice = Read-Host "Choose"

  switch ($choice) {
    "1" {
      .\scripts\build-install-run.ps1
      Pause-Menu
    }

    "2" {
      .\scripts\copy-debug-log.ps1
      Pause-Menu
    }

    "3" {
      .\scripts\make-context-pack.ps1
      Pause-Menu
    }

    "4" {
      git status
      Pause-Menu
    }

    "5" {
      git pull
      Pause-Menu
    }

    "6" {
      git status
      Write-Host ""
      $msg = Read-Host "Commit message"

      if ([string]::IsNullOrWhiteSpace($msg)) {
        Write-Host "Commit cancelled: empty message."
        Pause-Menu
        continue
      }

      git add .
      git commit -m "$msg"

      if ($LASTEXITCODE -eq 0) {
        git push
      }

      Pause-Menu
    }

    "7" {
      .\scripts\use-runtime.ps1 stylo-v2.mjs
      Pause-Menu
    }

    "8" {
      notepad ".\docs\MOBILE_PORT_STATUS.md"
      Pause-Menu
    }

    "9" {
      if (Test-Path ".\logs\latest-logcat.txt") {
        notepad ".\logs\latest-logcat.txt"
      } else {
        Write-Host "No latest log found yet."
      }
      Pause-Menu
    }

    "10" {
      break
    }

    default {
      Write-Host "Invalid choice."
      Start-Sleep -Seconds 1
    }
  }
}
