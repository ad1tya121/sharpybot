@echo off
cd /d "%~dp0assistant"
start "" python app.py
timeout /t 2 >nul
start "" frontend\index.html
