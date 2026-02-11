@echo off
REM Script para iniciar LockNode Agent con UV
REM Asegura que siempre se use Python 3.11

echo ========================================
echo   LockNode Agent - Inicio con UV
echo ========================================
echo.

REM Verificar si uv está instalado
where uv >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] UV no está instalado.
    echo.
    echo Por favor instala UV primero:
    echo   powershell -c "irm https://astral.sh/uv/install.ps1 | iex"
    echo.
    pause
    exit /b 1
)

echo [OK] UV detectado
echo.

REM Verificar si el proyecto está sincronizado
if not exist ".venv" (
    echo [INFO] Primera ejecución - Sincronizando proyecto...
    echo [INFO] Esto instalará Python 3.11 y todas las dependencias...
    echo.
    uv sync
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo [ERROR] Falló la sincronización del proyecto
        pause
        exit /b 1
    )
)

echo [INFO] Iniciando LockNode Agent (Python 3.11)...
echo [INFO] El agente escuchará en: https://localhost:5000
echo.
echo Presiona Ctrl+C para detener el agente
echo ========================================
echo.

REM Ejecutar el agente con uv
uv run python locknode_agent_https.py

pause
