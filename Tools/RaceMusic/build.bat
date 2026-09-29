@echo off
rem Compila los arneses de audio fuera del motor (MSVC de Visual Studio 2022): render.exe (musica de la carrera) y shell.exe (golpes del caparazon).
rem Uso: Tools\RaceMusic\build.bat   y luego, por ejemplo:  render.exe scenario ronda.wav   |   shell.exe golpes.wav
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul
set HERE=%~dp0
set AUD=%HERE%..\..\Source\Tortunabo\Private\Audio
cd /d "%HERE%"
if not exist obj mkdir obj
cl /nologo /std:c++20 /O2 /EHsc /utf-8 /W4 /wd4100 /I "%AUD%" render.cpp /Fe:render.exe /Fo:obj\render_
cl /nologo /std:c++20 /O2 /EHsc /utf-8 /W4 /wd4100 /I "%AUD%" shell.cpp /Fe:shell.exe /Fo:obj\shell_
