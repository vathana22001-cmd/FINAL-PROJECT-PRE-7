@echo off
setlocal
title FLEETFLOW
"%~dp0build\Release\FLEETFLOW.exe" "%~dp0data\database.json"
if errorlevel 1 (
  echo.
  echo FLEETFLOW stopped with error code %errorlevel%.
  pause
)
endlocal
