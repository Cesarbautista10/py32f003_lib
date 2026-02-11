#!/bin/bash
# Script para iniciar LockNode Agent con UV
# Asegura que siempre se use Python 3.11

echo "========================================"
echo "  LockNode Agent - Inicio con UV"
echo "========================================"
echo ""

# Verificar si uv está instalado
if ! command -v uv &> /dev/null; then
    echo "[ERROR] UV no está instalado."
    echo ""
    echo "Por favor instala UV primero:"
    echo "  curl -LsSf https://astral.sh/uv/install.sh | sh"
    echo ""
    exit 1
fi

echo "[OK] UV detectado"
echo ""

# Verificar si el proyecto está sincronizado
if [ ! -d ".venv" ]; then
    echo "[INFO] Primera ejecución - Sincronizando proyecto..."
    echo "[INFO] Esto instalará Python 3.11 y todas las dependencias..."
    echo ""
    uv sync
    if [ $? -ne 0 ]; then
        echo ""
        echo "[ERROR] Falló la sincronización del proyecto"
        exit 1
    fi
fi

echo "[INFO] Iniciando LockNode Agent (Python 3.11)..."
echo "[INFO] El agente escuchará en: https://localhost:5000"
echo ""
echo "Presiona Ctrl+C para detener el agente"
echo "========================================"
echo ""

# Ejecutar el agente con uv
uv run python locknode_agent_https.py
