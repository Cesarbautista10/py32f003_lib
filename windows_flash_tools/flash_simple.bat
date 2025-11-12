@echo off

echo FLASH PY32F003X4
echo.

if "%1"=="" (
    echo Archivos .hex disponibles:
    dir /b *.hex 2>nul
    echo.
    echo Uso: %0 archivo.hex
    pause
    exit /b 1
)

if not exist "%1" (
    echo ERROR: Archivo no encontrado: %1
    pause
    exit /b 1
)

echo Programando: %1

echo Borrando...
python -m pyocd erase -t py32f003x4 --chip --config Misc/pyocd.yaml

echo Cargando...
python -m pyocd load "%1" -t py32f003x4 --config Misc/pyocd.yaml

echo LISTO!
