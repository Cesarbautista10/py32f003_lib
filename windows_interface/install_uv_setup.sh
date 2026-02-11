#!/bin/bash
# Script de instalación y verificación de UV para LockNode Agent
# Ejecutar con: bash install_uv_setup.sh

echo "========================================"
echo "  Instalación UV - LockNode Agent"
echo "========================================"
echo ""

# Verificar si uv ya está instalado
if command -v uv &> /dev/null; then
    echo "[✓] UV ya está instalado"
    uv --version
    echo ""
else
    echo "[→] Instalando UV..."
    echo ""
    
    # Instalar UV
    curl -LsSf https://astral.sh/uv/install.sh | sh
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "[✓] UV instalado correctamente"
        
        # Agregar UV al PATH para esta sesión
        export PATH="$HOME/.cargo/bin:$PATH"
        
        echo "[i] Por favor, cierra y abre la terminal para que UV esté disponible"
        echo "[i] O ejecuta: source \$HOME/.cargo/env"
        echo ""
    else
        echo ""
        echo "[✗] Error al instalar UV"
        exit 1
    fi
fi

# Verificar Python
echo "========================================"
echo "  Verificación de Python"
echo "========================================"
echo ""

if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version)
    echo "[✓] Python detectado: $PYTHON_VERSION"
else
    echo "[i] Python no detectado, pero UV lo instalará automáticamente"
fi

echo ""
echo "========================================"
echo "  Sincronización del Proyecto"
echo "========================================"
echo ""

# Agregar UV al PATH por si se acaba de instalar
export PATH="$HOME/.cargo/bin:$PATH"

# Verificar que uv esté disponible
if command -v uv &> /dev/null; then
    echo "[→] Sincronizando proyecto con UV..."
    echo "[i] Esto instalará Python 3.11 y todas las dependencias"
    echo ""
    
    # Ir al directorio windows_interface
    cd "$(dirname "$0")"
    
    # Sincronizar proyecto
    uv sync
    
    if [ $? -eq 0 ]; then
        echo ""
        echo "========================================"
        echo "  ✓ Instalación Completa"
        echo "========================================"
        echo ""
        echo "El proyecto está listo para usarse."
        echo ""
        echo "Para ejecutar el agente:"
        echo "  ./start_agent_uv.sh"
        echo ""
        echo "O manualmente:"
        echo "  uv run python locknode_agent_https.py"
        echo ""
        echo "Ver versión de Python:"
        echo "  uv run python --version"
        echo ""
    else
        echo ""
        echo "[✗] Error al sincronizar el proyecto"
        exit 1
    fi
else
    echo "[!] UV no está disponible en esta sesión"
    echo "[i] Por favor, cierra y abre la terminal, luego ejecuta:"
    echo "    cd windows_interface"
    echo "    uv sync"
fi
