@echo off
echo Deploying FORGE64 plugins...

copy /Y "F:\forge64\build_win\Forge64_artefacts\Release\CLAP\FORGE64.clap" "C:\Program Files\Common Files\CLAP\FORGE64.clap" >nul 2>&1
robocopy "F:\forge64\build_win\Forge64_artefacts\Release\VST3\FORGE64.vst3" "C:\Program Files\Common Files\VST3\FORGE64.vst3" /E /IS /IT /R:1 /W:1 >nul 2>&1

if exist "%LOCALAPPDATA%\Programs\Common\CLAP\FORGE64.clap.old" del /F /Q "%LOCALAPPDATA%\Programs\Common\CLAP\FORGE64.clap.old" >nul 2>&1
if exist "%LOCALAPPDATA%\Programs\Common\CLAP\FORGE64.clap" ren "%LOCALAPPDATA%\Programs\Common\CLAP\FORGE64.clap" FORGE64.clap.old >nul 2>&1
copy /Y "F:\forge64\build_win\Forge64_artefacts\Release\CLAP\FORGE64.clap" "%LOCALAPPDATA%\Programs\Common\CLAP\FORGE64.clap" >nul 2>&1

robocopy "F:\forge64\build_win\Forge64_artefacts\Release\VST3\FORGE64.vst3" "%LOCALAPPDATA%\Programs\Common\VST3\FORGE64.vst3" /E /IS /IT /R:1 /W:1 >nul 2>&1

echo Done deploying FORGE64!

