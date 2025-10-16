@echo off

echo INSTALANDO PYOCD...

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Python no encontrado
    echo Instala desde: https://python.org
    pause
    exit /b 1
)

python -m pip install --user pyocd

echo Instalando paquete PY32F0xx...
pyocd pack install Puya.PY32F0xx_DFP

echo LISTO!
echo.
echo Uso: flash_simple.bat archivo.hex
pause
