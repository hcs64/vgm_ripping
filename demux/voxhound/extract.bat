@echo off
set mydir=%~dp0
:loop
if "%~f1"=="" goto end

"%mydir%voxhound.exe" -r "%~f1" "%~dpn1.VAG"
start /D "%mydir%MFAudiov11\" /WAIT MFAudio.exe /IF22050 /IC1 /IH0 /OTWAVU /OF22050 /OC1 "%~dpn1.VAG" "%~dpn1.WAV"
del "%~dpn1.VAG"
shift
goto loop

:end