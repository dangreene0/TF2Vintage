@echo off

rem This is the location of the script
SET scriptdir=%~dp0

echo Script location: "%scriptdir:~0,-1%"
if exist tf2vintage\ (echo Found existing tf2vintage location, continuing with update.) else (goto ERRORNOINSTALL)

echo Do not close this window until the game has fully downloaded. It is a quiet download...
powershell -Command "(New-Object Net.WebClient).DownloadFile('https://nightly.link/TF2V/TF2Vintage/workflows/release/3.6/tf2vintage.exe.zip', 'tf2vintage.zip')"

echo Preserving gameinfo
cd tf2vintage
rename gameinfo.txt temp-gameinfo.txt
cd ..

echo Extracting SFX out of zip file
powershell Expand-Archive tf2vintage.zip -DestinationPath .
goto SELFEXTRACT

:SELFEXTRACT
echo Opening SFX and silently extracting
call tf2vintage.exe -y -gm
goto CLEANUP

:CLEANUP

cd tf2vintage 
del /q gameinfo.txt
rename temp-gameinfo.txt gameinfo.txt
cd ..

rem Garbage collection
del /q tf2vintage.zip
del /q tf2vintage.exe

echo Restarting Steam to apply update...
call "TF2V Sourcemod Junctioner.bat"

echo Update has finished, have a good day!
pause
exit /b

:ERRORNOINSTALL
echo No existing tf2vintage location was found, please manually install the game first.
echo https://nightly.link/TF2V/TF2Vintage/workflows/release/3.6/tf2vintage.zip
pause
exit /b
