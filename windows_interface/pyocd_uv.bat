@echo off
REM Script para ejecutar pyocd con UV en Windows
REM Esto evita problemas de compatibilidad con Python 3.13

REM Verificar que UV esté instalado
where uv >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: UV no esta instalado
    echo Instala UV con: powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
    exit /b 1
)

REM Ejecutar pyocd con UV usando Python 3.12
echo Ejecutando pyocd con UV (Python 3.12)...
uv run pyocd %*
