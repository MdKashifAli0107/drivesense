@echo off
echo ==========================================================
echo    Starting DriveSense Web Server from Windows VS Code...
echo ==========================================================
start http://192.168.1.3:8080
ssh -t kashif@192.168.1.3 "cd /home/kashif/drivesense && ./scripts/run_browser.sh"
