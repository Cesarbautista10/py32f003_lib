#!/bin/bash
#
# Script de compilación y carga rápida para ESP32-C6
# Workflow completo: compile -> upload -> connect
#

set -o pipefail

# Colores
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_info() {
    echo -e "${YELLOW}ℹ${NC} $1"
}

print_success() {
    echo -e "${GREEN}✅${NC} $1"
}

print_error() {
    echo -e "${RED}❌${NC} $1"
}

# Función principal
main() {
    case "${1:-build}" in
        "build"|"compile")
            print_info "Compilando proyecto..."
            ./esp32c6_smart.sh compile
            ;;
        "upload"|"flash")
            print_info "Compilando y cargando firmware..."
            ./esp32c6_smart.sh upload
            ;;
        "connect"|"monitor")
            print_info "Conectando al monitor serie..."
            ./esp32c6_smart.sh connect
            ;;
        "full"|"all")
            print_info "Workflow completo: compile -> upload -> connect"
            if ./esp32c6_smart.sh upload; then
                print_success "Firmware cargado exitosamente"
                print_info "Conectando al monitor en 2 segundos..."
                sleep 2
                ./esp32c6_smart.sh connect
            else
                print_error "Error en la carga del firmware"
                exit 1
            fi
            ;;
        "test")
            print_info "Enviando comando de test al ESP32-C6..."
            timeout 10 bash -c '
                if command -v picocom &> /dev/null; then
                    echo "jtag" | picocom /dev/ttyACM0 -b 115200 -q --exit-after 2000
                elif command -v screen &> /dev/null; then
                    echo "jtag" | screen /dev/ttyACM0 115200
                else
                    echo "Instala picocom o screen"
                fi
            '
            ;;
        "info")
            ./esp32c6_smart.sh info
            ;;
        "help"|"--help"|"-h")
            echo "Script de desarrollo rápido ESP32-C6 CDC-JTAG"
            echo ""
            echo "Uso: $0 [comando]"
            echo ""
            echo "Comandos disponibles:"
            echo "  build, compile   Compilar solamente"
            echo "  upload, flash    Compilar y cargar firmware"
            echo "  connect, monitor Conectar al monitor serie" 
            echo "  full, all        Workflow completo (upload + monitor)"
            echo "  test             Enviar comando 'jtag' y ver respuesta"
            echo "  info             Información del sistema"
            echo "  help             Mostrar esta ayuda"
            echo ""
            echo "Ejemplos:"
            echo "  $0                # Compilar (por defecto)"
            echo "  $0 upload         # Compilar y cargar"
            echo "  $0 full           # Cargar y conectar"
            echo "  $0 test           # Probar comando JTAG"
            ;;
        *)
            print_error "Comando desconocido: $1"
            echo "Usa '$0 help' para ver comandos disponibles"
            exit 1
            ;;
    esac
}

# Verificar que existe el script principal
if [[ ! -f "esp32c6_smart.sh" ]]; then
    print_error "No se encontró esp32c6_smart.sh en el directorio actual"
    exit 1
fi

# Ejecutar función principal
main "$@"
