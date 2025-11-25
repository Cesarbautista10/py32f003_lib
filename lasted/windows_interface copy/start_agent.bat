@echo off
REM Script para ejecutar el agente LockNode en Windows
REM Uso: start_agent.bat [http|https]

echo ========================================================
echo    LockNode Agent - Iniciador Windows
echo ========================================================
echo.

REM Verificar si Python está instalado
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python no esta instalado
    echo Por favor instala Python 3.8 o superior desde python.org
    pause
    exit /b 1
)

REM Verificar argumentos
set MODE=%1
if "%MODE%"=="" set MODE=http

REM Verificar si las dependencias están instaladas
python -c "import flask" >nul 2>&1
if errorlevel 1 (
    echo.
    echo Las dependencias no estan instaladas.
    echo Instalando dependencias...
    echo.
    pip install -r requirements.txt
    if errorlevel 1 (
        echo.
        echo ERROR: No se pudieron instalar las dependencias
        pause
        exit /b 1
    )
)

echo.
echo Modo seleccionado: %MODE%
echo.

REM Ejecutar el agente según el modo
if /i "%MODE%"=="https" (
    echo Verificando certificados SSL...
    if not exist "certs\cert.pem" (
        echo.
        echo ERROR: Certificados SSL no encontrados
        echo Genera los certificados con: bash generate_ssl_certs.sh
        echo O ejecuta en modo HTTP: start_agent.bat http
        pause
        exit /b 1
    )
    echo Iniciando agente HTTPS...
    python locknode_agent_https.py
) else (
    echo Iniciando agente HTTP...
    python locknode_agent.py
)

if errorlevel 1 (
    echo.
    echo ERROR: El agente termino con errores
    pause
    exit /b 1
)
