@echo off
setlocal EnableExtensions
rem ---------------------------------------------------------------------------------------------------------------
rem CI local: compila DebugGame, pasa los tests de Automation «Tortunabo», los de Python (uv run pytest) y, con
rem TN_CI_COOK=1, un BuildCookRun Shipping. Ejecutar con el editor CERRADO.
rem
rem Códigos de salida: 1 compilación, 2 tests de Unreal, 3 pytest, 4 cocinado; 0 = CI LOCAL OK.
rem Variables opcionales:
rem   UE_ROOT       carpeta del motor (por defecto C:\Program Files\Epic Games\UE_5.6)
rem   TN_CI_COOK    1 = también empaqueta Shipping en Saved\Packages
rem   TN_MIN_TESTS  tests de Unreal que tienen que haber pasado como mínimo (por defecto 100)
rem ---------------------------------------------------------------------------------------------------------------

if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.6"
if not defined TN_MIN_TESTS set "TN_MIN_TESTS=100"
set "ROOT=%~dp0..\.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "UPROJECT=%ROOT%\Tortunabo.uproject"
set "REPORT=%ROOT%\Saved\Automation\CI"

call "%~dp0build_check.bat" DebugGame || exit /b 1

rem El editor con el módulo en DebugGame; si esta instalación no trae el ejecutable DebugGame, el de siempre con -debug.
set "EDITOR_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe"
set "EDITOR_ARGS="
if not exist "%EDITOR_CMD%" (
  set "EDITOR_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
  set "EDITOR_ARGS=-debug"
)
if exist "%REPORT%" rmdir /s /q "%REPORT%"
"%EDITOR_CMD%" "%UPROJECT%" %EDITOR_ARGS% -ExecCmds="Automation RunTests Tortunabo; Quit" -ReportExportPath="%REPORT%" -nullrhi -unattended -nosplash -NoSteam
uv run python "%ROOT%\Scripts\ci\check_automation_report.py" "%REPORT%\index.json" --min-tests %TN_MIN_TESTS% || exit /b 2

pushd "%ROOT%"
uv run pytest -q
if errorlevel 1 (
  popd
  exit /b 3
)
popd

if "%TN_CI_COOK%"=="1" (
  call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="%UPROJECT%" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="%ROOT%\Saved\Packages" -unattended -utf8output || exit /b 4
)
echo CI LOCAL OK
