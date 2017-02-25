@echo off
set mydir=%~dp0
:loop
if "%~f1"=="" goto end

start /D "%mydir%MFAudiov11\" /WAIT MFAudio.exe /OTRAWC "%~dpn1.WAV" "%~dpn1.VAG"
"%mydir%voxhound.exe" -w "%~f1" "%~dpn1.VAG"

del "%~dpn1.VAG"
shift
goto loop

:end