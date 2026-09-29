@echo off
setlocal EnableExtensions
rem ---------------------------------------------------------------------------------------------------------------
rem Localización, pasos 1 y 2: RECOGER los textos del juego (código y assets) y EXPORTAR los .po de cada idioma.
rem
rem Docs/Localizacion.md. Ejecutar con el editor CERRADO (los comandos usan el mismo proyecto). No hace commits ni toca nada
rem fuera de Content/Localization/Game.
rem
rem Variables opcionales:
rem   UE_ROOT   carpeta del motor (por defecto C:\Program Files\Epic Games\UE_5.6)
rem ---------------------------------------------------------------------------------------------------------------

set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"
set "UPROJECT=%PROJECT_DIR%\Tortunabo.uproject"
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.6"
set "EDITOR_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if not exist "%EDITOR_CMD%" (
  echo No esta %EDITOR_CMD%
  echo Pon UE_ROOT con la carpeta del motor 5.6.
  exit /b 1
)
if not exist "%UPROJECT%" (
  echo No esta %UPROJECT%
  exit /b 1
)

tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>NUL | find /I "UnrealEditor.exe" >NUL
if not errorlevel 1 (
  echo El editor esta abierto. Cierralo y vuelve a ejecutar este script ^(no lo cierro yo^).
  exit /b 1
)

echo [1/2] Recogiendo textos (Source/Tortunabo y Content) y exportando los .po a Content\Localization\Game\^<cultura^>\Game.po ...
"%EDITOR_CMD%" "%UPROJECT%" -run=GatherText -config="Config/Localization/Game_Gather.ini;Config/Localization/Game_Export.ini" -unattended -nopause -nosplash -NoShaderCompile -stdout -FullStdOutLogOutput
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
  echo.
  echo Fallo la recogida o la exportacion ^(codigo %RESULT%^). Mira Saved\Logs\Tortunabo.log ^(busca LogGatherTextCommandlet^).
  exit /b %RESULT%
)

echo.
echo [2/2] Hecho. Traducir los .po de Content\Localization\Game\^<cultura^>\ ^(Docs/Localizacion.md^) y luego ejecutar Scripts\localization_import_compile.bat
echo Informes: Content\Localization\Game\Game.csv ^(palabras^) y Game_Conflicts.txt ^(textos con la misma clave y distinto origen; debe estar vacio^).
exit /b 0
