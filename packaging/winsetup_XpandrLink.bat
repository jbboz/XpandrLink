@echo off
cd /d "%~dp0"
echo.
echo Clearing the "downloaded from the internet" block for XpandrLink
echo Location: %cd%
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -Path '.' -Recurse -File | Unblock-File"
echo.
echo Done. XpandrLink.exe will now run normally, and any DAW should load
echo the VST3 without a SmartScreen block.
echo.
pause
