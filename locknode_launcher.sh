#!/bin/bash
#
# LOCKNODE V6 - QUICK LAUNCHER
# Script de inicio rápido para todas las herramientas
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TOOLS="$SCRIPT_DIR/build_tools"

# Colores
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

print_header() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $1"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
}

show_menu() {
    print_header "LOCKNODE V6 - QUICK LAUNCHER"
    echo
    echo -e "${GREEN}Herramientas disponibles:${NC}"
    echo
    echo "1) 🔧 Master Build System     - Sistema principal de compilación"
    echo "2) 📦 Project Generator       - Crear nuevos proyectos"
    echo "3) 🚀 ESP32-C6 Smart Compiler - Compilador inteligente ESP32-C6"
    echo "4) ⚡ Quick Build & Upload    - Compilación y carga rápida"
    echo "5) 💻 Install New PC          - Instalador para nuevas PCs"
    echo "6) 📋 Show Project Structure  - Mostrar estructura del workspace"
    echo "7) ❓ Help                    - Ayuda general"
    echo "8) 🚪 Exit                    - Salir"
    echo
}

show_project_structure() {
    print_header "ESTRUCTURA DEL WORKSPACE"
    echo
    tree -L 2 "$SCRIPT_DIR" 2>/dev/null || find "$SCRIPT_DIR" -maxdepth 2 -type d | sort
}

main() {
    if [[ -n "$1" ]]; then
        # Modo comando directo
        case "$1" in
            "build"|"master")
                "$BUILD_TOOLS/master_build.sh" "${@:2}"
                ;;
            "generate"|"new")
                "$BUILD_TOOLS/scripts/project_generator.sh" "${@:2}"
                ;;
            "esp32c6")
                "$BUILD_TOOLS/scripts/esp32c6_smart.sh" "${@:2}"
                ;;
            "quick")
                "$BUILD_TOOLS/scripts/build_and_upload.sh" "${@:2}"
                ;;
            "install")
                "$BUILD_TOOLS/installers/install_new_pc.sh" "${@:2}"
                ;;
            "structure")
                show_project_structure
                ;;
            "help")
                echo "LOCKNODE V6 Quick Launcher"
                echo
                echo "Uso directo:"
                echo "  $0 build [opciones]     # Master Build System"
                echo "  $0 generate [opciones]  # Project Generator"
                echo "  $0 esp32c6 [opciones]   # ESP32-C6 Smart Compiler"
                echo "  $0 quick [opciones]     # Quick Build & Upload"
                echo "  $0 install [opciones]   # Install New PC"
                echo "  $0 structure            # Show structure"
                echo
                echo "O ejecuta sin argumentos para el menú interactivo"
                ;;
            *)
                echo "Comando desconocido: $1"
                echo "Usa '$0 help' para ver comandos disponibles"
                exit 1
                ;;
        esac
    else
        # Modo menú interactivo
        while true; do
            show_menu
            read -p "Selecciona una opción (1-8): " choice
            echo
            
            case $choice in
                1)
                    "$BUILD_TOOLS/master_build.sh"
                    ;;
                2)
                    "$BUILD_TOOLS/scripts/project_generator.sh"
                    ;;
                3)
                    cd test_continuous_project 2>/dev/null || echo "Directorio test_continuous_project no encontrado"
                    "$BUILD_TOOLS/scripts/esp32c6_smart.sh"
                    cd - > /dev/null
                    ;;
                4)
                    cd test_continuous_project 2>/dev/null || echo "Directorio test_continuous_project no encontrado"
                    "$BUILD_TOOLS/scripts/build_and_upload.sh"
                    cd - > /dev/null
                    ;;
                5)
                    "$BUILD_TOOLS/installers/install_new_pc.sh"
                    ;;
                6)
                    show_project_structure
                    ;;
                7)
                    echo "LOCKNODE V6 Build Tools"
                    echo
                    echo "Sistema completo de desarrollo para ESP32-C6 con CDC-JTAG"
                    echo "Incluye compilación inteligente, generación de proyectos,"
                    echo "instalación automatizada y herramientas de desarrollo."
                    echo
                    ;;
                8)
                    echo -e "${GREEN}¡Hasta luego!${NC}"
                    exit 0
                    ;;
                *)
                    echo "Opción inválida"
                    ;;
            esac
            
            echo
            read -p "Presiona Enter para continuar..."
            echo
        done
    fi
}

main "$@"
