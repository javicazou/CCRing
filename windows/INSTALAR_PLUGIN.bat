@echo off
REM Copia URing.vst3 a la carpeta de plugins VST3 de Windows.
REM Uso: clic derecho -> "Ejecutar como administrador"
setlocal
cd /d "%~dp0"
set "DST=%CommonProgramFiles%\VST3\URing.vst3"
if exist "%DST%" rmdir /s /q "%DST%"
xcopy "URing.vst3" "%DST%\" /E /I /Y >nul || (
  echo [X] No se pudo copiar. Ejecuta este archivo como administrador.
  pause & exit /b 1
)
echo [OK] URing instalado en %DST%
echo      En Ableton: Preferencias -^> Plug-ins -^> Re-escanear.
pause
