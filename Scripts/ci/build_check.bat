@echo off
setlocal EnableExtensions
rem ---------------------------------------------------------------------------------------------------------------
rem Compila TortunaboEditor (DebugGame de serie) desde cualquier carpeta y en cualquier máquina: rutas relativas al
rem propio script. Ejecutar con el editor CERRADO. Sustituye al build_check.bat viejo (Deprecado/, rutas de un solo PC).
rem
rem Uso: Scripts\ci\build_check.bat [DebugGame|Development]
rem Variables opcionales:
rem   UE_ROOT   carpeta del motor (por defecto C:\Program Files\Epic Games\UE_5.6)
rem Sale con el código de UBT (0 = bien) y enseña las líneas de error y aviso de C++.
rem ---------------------------------------------------------------------------------------------------------------

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=DebugGame"
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.6"
set "PROJECT_DIR=%~dp0..\.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
set "UPROJECT=%PROJECT_DIR%\Tortunabo.uproject"
set "BUILD_BAT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
set "BUILD_LOG=%TEMP%\tortunabo_build_%CONFIG%.txt"

if not exist "%BUILD_BAT%" (
  echo No esta %BUILD_BAT%: pon UE_ROOT con la carpeta de Unreal 5.6.
  exit /b 2
)

call "%BUILD_BAT%" TortunaboEditor Win64 %CONFIG% -Project="%UPROJECT%" -WaitMutex -NoHotReload > "%BUILD_LOG%" 2>&1
set "BUILD_EXIT=%ERRORLEVEL%"
findstr /i /c:"error C" /c:"warning C" /c:": error" /c:"cannot open" "%BUILD_LOG%"
echo BUILD EXIT CODE: %BUILD_EXIT% (%CONFIG%, registro completo en %BUILD_LOG%)
exit /b %BUILD_EXIT%
