#!/bin/bash
# Script para ejecutar pyocd con UV
# Esto asegura usar Python 3.12 del entorno UV

# Verificar que UV esté instalado
if ! command -v uv &> /dev/null; then
    echo "ERROR: UV no está instalado"
    echo "Instala UV con: curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
fi

# Ejecutar pyocd con UV usando Python 3.12
echo "Ejecutando pyocd con UV (Python 3.12)..."
uv run pyocd "$@"
