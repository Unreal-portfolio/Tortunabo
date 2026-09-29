@echo off
"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" TortunaboEditor Win64 Development "C:\Users\mokiu\Documents\Unreal Projects\Tortunabo\Tortunabo.uproject" -WaitMutex -NoHotReload > "%TEMP%\ue_build_out.txt" 2>&1
findstr /i "error C warning C : error cannot" "%TEMP%\ue_build_out.txt" > "%TEMP%\ue_build_errors.txt"
echo BUILD EXIT CODE: %errorlevel%
type "%TEMP%\ue_build_errors.txt"
