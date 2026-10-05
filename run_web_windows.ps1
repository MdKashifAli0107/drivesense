# DriveSense Windows VS Code Browser Launcher
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Starting DriveSense Web Server on Ubuntu VM...        " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Open the Windows browser
Start-Process "http://192.168.1.3:8080"
Write-Host "[+] Opened Windows browser at http://192.168.1.3:8080" -ForegroundColor Green

# 2. Start the DriveSense server via SSH
Write-Host "[+] Connecting to Ubuntu VM (192.168.1.3)..." -ForegroundColor Yellow
ssh -t kashif@192.168.1.3 "cd /home/kashif/drivesense && ./scripts/run_browser.sh"
