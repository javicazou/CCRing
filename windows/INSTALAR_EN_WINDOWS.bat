@echo off
REM ===========================================================================
REM  URing - compilacion e instalacion en Windows (VST3, 64 bits)
REM  Requisitos (gratis):
REM    1. Visual Studio 2022 Community con "Desarrollo para el escritorio con C++"
REM       https://visualstudio.microsoft.com/es/vs/community/
REM    2. Git para Windows: https://git-scm.com/download/win
REM  Uso: clic derecho sobre este archivo -> "Ejecutar como administrador"
REM ===========================================================================
setlocal
cd /d "%~dp0\.."

where cmake >nul 2>nul
if errorlevel 1 (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set VSPATH=%%i
)
if defined VSPATH set "PATH=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
where cmake >nul 2>nul
if errorlevel 1 (
  echo [X] No se encontro CMake. Instala Visual Studio 2022 con "Desarrollo para el escritorio con C++".
  pause & exit /b 1
)
where git >nul 2>nul
if errorlevel 1 (
  echo [X] No se encontro Git. Instalalo desde https://git-scm.com/download/win
  pause & exit /b 1
)

echo == Configurando (la primera vez descarga JUCE, unos minutos)...
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 || (pause & exit /b 1)
echo == Compilando URing VST3...
cmake --build build-win --config Release --target URing_VST3 URing_Standalone || (pause & exit /b 1)

set "SRC=build-win\URing_artefacts\Release\VST3\URing.vst3"
set "DST=%CommonProgramFiles%\VST3\URing.vst3"
if not exist "%SRC%" ( echo [X] No se genero %SRC% & pause & exit /b 1 )
echo == Instalando en %DST%
if exist "%DST%" rmdir /s /q "%DST%"
xcopy "%SRC%" "%DST%\" /E /I /Y >nul || (
  echo [X] No se pudo copiar. Ejecuta este archivo como administrador.
  pause & exit /b 1
)
echo.
echo [OK] URing instalado. En Ableton: Preferencias -^> Plug-ins -^> Re-escanear.
echo      Tambien queda la app suelta en build-win\URing_artefacts\Release\Standalone\URing.exe
pause
