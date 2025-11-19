@echo off

where /q pwsh
IF ERRORLEVEL 1 (
    powershell -noprofile -nologo -executionpolicy bypass %~dp0\zip.ps1 %*
) ELSE (
    pwsh -noprofile -nologo -executionpolicy bypass %~dp0\zip.ps1 %*
)
