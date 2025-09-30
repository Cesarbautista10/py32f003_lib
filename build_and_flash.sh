#!/bin/bash
#
# BUILD AND FLASH SCRIPT - LOCKNODE V6
# Script automatizado para compilar y flashear ESP32-C6 y PY32F003
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuraciones
# Nota: Usando esp32 estándar ya que esp32c6 no está disponible en la versión instalada
ESP32C6_FQBN="esp32:esp32:esp32"
ESP32C6_PORT="/dev/ttyACM0"
ESP32C6_BAUDRATE="921600"

# Detectar Arduino CLI
detect_arduino_cli() {
    if command -v arduino-cli &> /dev/null; then
        echo "arduino-cli"
    elif [ -f "$HOME/bin/arduino-cli" ]; then
        echo "$HOME/bin/arduino-cli"
    else
        echo ""
    fi
}

ARDUINO_CLI=$(detect_arduino_cli)

print_header() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $1"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Verificar disponibilidad de puerto
check_port() {
    local port=$1
    if [ ! -e "$port" ]; then
        print_error "Puerto $port no existe"
        return 1
    fi
    
    # Verificar si el puerto está ocupado
    if lsof "$port" > /dev/null 2>&1; then
        print_warning "Puerto $port está ocupado, intentando liberar..."
        local pid=$(lsof -t "$port")
        if [ -n "$pid" ]; then
            kill "$pid" 2>/dev/null || true
            sleep 1
        fi
    fi
    
    return 0
}

# Compilar ESP32-C6
compile_esp32c6() {
    local project_dir=$1
    local sketch_name=$2
    
    print_header "COMPILANDO ESP32-C6: $sketch_name"
    
    if [ ! -d "$project_dir" ]; then
        print_error "Directorio $project_dir no existe"
        return 1
    fi
    
    if [ ! -f "$project_dir/$sketch_name.ino" ]; then
        print_error "Archivo $sketch_name.ino no encontrado en $project_dir"
        return 1
    fi
    
    cd "$project_dir"
    print_info "Compilando con FQBN: $ESP32C6_FQBN"
    
    if [ -z "$ARDUINO_CLI" ]; then
        print_error "Arduino CLI no encontrado. Ejecuta ./install_tools.sh arduino"
        return 1
    fi
    
    if $ARDUINO_CLI compile --fqbn "$ESP32C6_FQBN" "$sketch_name.ino"; then
        print_success "Compilación ESP32-C6 exitosa"
        return 0
    else
        print_error "Error en compilación ESP32-C6"
        return 1
    fi
}

# Flashear ESP32-C6
flash_esp32c6() {
    local project_dir=$1
    local sketch_name=$2
    
    print_header "FLASHEANDO ESP32-C6: $sketch_name"
    
    if ! check_port "$ESP32C6_PORT"; then
        return 1
    fi
    
    cd "$project_dir"
    print_info "Flasheando en puerto: $ESP32C6_PORT"
    
    if [ -z "$ARDUINO_CLI" ]; then
        print_error "Arduino CLI no encontrado. Ejecuta ./install_tools.sh arduino"
        return 1
    fi
    
    if $ARDUINO_CLI upload --fqbn "$ESP32C6_FQBN" -p "$ESP32C6_PORT" "$sketch_name.ino"; then
        print_success "Flash ESP32-C6 exitoso"
        return 0
    else
        print_error "Error en flash ESP32-C6"
        return 1
    fi
}

# Compilar PY32F003
compile_py32f003() {
    local project_name=$1
    
    print_header "COMPILANDO PY32F003: $project_name"
    
    cd "$SCRIPT_DIR"
    print_info "Compilando proyecto: $project_name"
    
    if make PROJECT="$project_name" clean; then
        print_success "Clean exitoso"
    else
        print_warning "Clean falló, continuando..."
    fi
    
    if make PROJECT="$project_name"; then
        print_success "Compilación PY32F003 exitosa"
        return 0
    else
        print_error "Error en compilación PY32F003"
        return 1
    fi
}

# Flashear PY32F003
flash_py32f003() {
    local project_name=$1
    
    print_header "FLASHEANDO PY32F003: $project_name"
    
    cd "$SCRIPT_DIR"
    print_info "Flasheando proyecto: $project_name"
    
    if make PROJECT="$project_name" flash; then
        print_success "Flash PY32F003 exitoso"
        return 0
    else
        print_error "Error en flash PY32F003"
        return 1
    fi
}

# Compilar y flashear PY32F003 en un comando
build_and_flash_py32f003() {
    local project_name=$1
    
    print_header "BUILD & FLASH PY32F003: $project_name"
    
    cd "$SCRIPT_DIR"
    print_info "Compilando y flasheando: $project_name"
    
    if make PROJECT="$project_name" flash; then
        print_success "Build & Flash PY32F003 exitoso"
        return 0
    else
        print_error "Error en Build & Flash PY32F003"
        return 1
    fi
}

# Monitor serie ESP32-C6
monitor_esp32c6() {
    print_header "MONITOR SERIE ESP32-C6"
    
    if ! check_port "$ESP32C6_PORT"; then
        return 1
    fi
    
    print_info "Conectando a $ESP32C6_PORT a $ESP32C6_BAUDRATE baud"
    print_info "Presiona Ctrl+C para salir del monitor"
    
    if [ -z "$ARDUINO_CLI" ]; then
        print_error "Arduino CLI no encontrado. Ejecuta ./install_tools.sh arduino"
        return 1
    fi
    
    $ARDUINO_CLI monitor -p "$ESP32C6_PORT" -c baudrate="$ESP32C6_BAUDRATE"
}

# Mostrar menú
show_menu() {
    print_header "BUILD & FLASH TOOL - LOCKNODE V6"
    echo
    echo -e "${GREEN}Proyectos disponibles:${NC}"
    echo
    echo "ESP32-C6:"
    echo "  1) 🔧 Compilar can_bus_c6_v2"
    echo "  2) 🚀 Flash can_bus_c6_v2"
    echo "  3) ⚡ Compilar + Flash can_bus_c6_v2"
    echo "  4) 📱 Monitor Serie ESP32-C6"
    echo
    echo "PY32F003:"
    echo "  5) 🔧 Compilar firmware_v5_4_lite"
    echo "  6) 🚀 Flash firmware_v5_4_lite"
    echo "  7) ⚡ Compilar + Flash firmware_v5_4_lite"
    echo
    echo "Ambos:"
    echo "  8) 🎯 Compilar + Flash TODO (ESP32-C6 + PY32F003)"
    echo
    echo "Herramientas:"
    echo "  9) 📋 Verificar herramientas"
    echo " 10) 🔍 Listar puertos serie"
    echo " 11) ❓ Ayuda"
    echo " 12) 🚪 Salir"
    echo
}

# Verificar herramientas
check_tools() {
    print_header "VERIFICANDO HERRAMIENTAS"
    
    # Arduino CLI
    if command -v arduino-cli &> /dev/null; then
        local version=$(arduino-cli version | head -n1)
        print_success "Arduino CLI: $version"
    elif [ -f "$HOME/bin/arduino-cli" ]; then
        local version=$($HOME/bin/arduino-cli version | head -n1)
        print_success "Arduino CLI: $version (en ~/bin/)"
        print_warning "Ejecuta: source ~/.bashrc o export PATH=\"\$HOME/bin:\$PATH\""
    else
        print_error "Arduino CLI no instalado"
    fi
    
    # Make
    if command -v make &> /dev/null; then
        local version=$(make --version | head -n1)
        print_success "Make: $version"
    else
        print_error "Make no instalado"
    fi
    
    # PyOCD o OpenOCD
    if command -v pyocd &> /dev/null; then
        local version=$(pyocd --version)
        print_success "PyOCD: $version"
    elif command -v openocd &> /dev/null; then
        local version=$(openocd --version | head -n1)
        print_success "OpenOCD: $version"
    else
        print_warning "PyOCD/OpenOCD no encontrado (necesario para PY32F003)"
    fi
    
    # ARM GCC
    if command -v arm-none-eabi-gcc &> /dev/null; then
        local version=$(arm-none-eabi-gcc --version | head -n1)
        print_success "ARM GCC: $version"
    else
        print_warning "ARM GCC no encontrado (necesario para PY32F003)"
    fi
}

# Listar puertos serie
list_ports() {
    print_header "PUERTOS SERIE DISPONIBLES"
    
    print_info "Puertos ACM (ESP32-C6 CDC-JTAG):"
    ls -la /dev/ttyACM* 2>/dev/null || print_warning "No hay puertos ACM"
    
    print_info "Puertos USB:"
    ls -la /dev/ttyUSB* 2>/dev/null || print_warning "No hay puertos USB"
    
    print_info "Procesos usando puertos:"
    lsof /dev/tty* 2>/dev/null | grep -E "(ACM|USB)" || print_info "Ningún puerto en uso"
}

# Compilar y flashear todo
build_and_flash_all() {
    print_header "COMPILAR + FLASH TODO"
    
    local errors=0
    
    # ESP32-C6
    print_info "=== PROCESANDO ESP32-C6 ==="
    if compile_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"; then
        if ! flash_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"; then
            errors=$((errors + 1))
        fi
    else
        errors=$((errors + 1))
    fi
    
    echo
    
    # PY32F003
    print_info "=== PROCESANDO PY32F003 ==="
    if ! build_and_flash_py32f003 "firmware_v5_4_lite"; then
        errors=$((errors + 1))
    fi
    
    echo
    
    if [ $errors -eq 0 ]; then
        print_success "¡TODO COMPILADO Y FLASHEADO EXITOSAMENTE!"
    else
        print_error "Se encontraron $errors errores durante el proceso"
        return 1
    fi
}

# Función principal
main() {
    if [[ -n "$1" ]]; then
        # Modo comando directo
        case "$1" in
            "esp32c6-compile")
                compile_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"
                ;;
            "esp32c6-flash")
                flash_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"
                ;;
            "esp32c6-all")
                compile_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2" && \
                flash_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"
                ;;
            "py32f003-compile")
                compile_py32f003 "firmware_v5_4_lite"
                ;;
            "py32f003-flash")
                flash_py32f003 "firmware_v5_4_lite"
                ;;
            "py32f003-all")
                build_and_flash_py32f003 "firmware_v5_4_lite"
                ;;
            "all")
                build_and_flash_all
                ;;
            "monitor")
                monitor_esp32c6
                ;;
            "check")
                check_tools
                ;;
            "ports")
                list_ports
                ;;
            *)
                echo "Comando desconocido: $1"
                echo "Comandos disponibles:"
                echo "  esp32c6-compile, esp32c6-flash, esp32c6-all"
                echo "  py32f003-compile, py32f003-flash, py32f003-all"
                echo "  all, monitor, check, ports"
                exit 1
                ;;
        esac
    else
        # Modo menú interactivo
        while true; do
            show_menu
            read -p "Selecciona una opción (1-12): " choice
            echo
            
            case $choice in
                1)
                    compile_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"
                    ;;
                2)
                    flash_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"
                    ;;
                3)
                    compile_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2" && \
                    flash_esp32c6 "$SCRIPT_DIR/can_bus_c6_v2" "can_bus_c6_v2"
                    ;;
                4)
                    monitor_esp32c6
                    ;;
                5)
                    compile_py32f003 "firmware_v5_4_lite"
                    ;;
                6)
                    flash_py32f003 "firmware_v5_4_lite"
                    ;;
                7)
                    build_and_flash_py32f003 "firmware_v5_4_lite"
                    ;;
                8)
                    build_and_flash_all
                    ;;
                9)
                    check_tools
                    ;;
                10)
                    list_ports
                    ;;
                11)
                    echo "BUILD & FLASH TOOL - LOCKNODE V6"
                    echo
                    echo "Este script automatiza la compilación y flasheo de:"
                    echo "- ESP32-C6: Usando Arduino CLI con CDC-JTAG"
                    echo "- PY32F003: Usando Makefile y PyOCD/OpenOCD"
                    echo
                    echo "Uso directo:"
                    echo "  $0 esp32c6-all     # Compilar + flash ESP32-C6"
                    echo "  $0 py32f003-all    # Compilar + flash PY32F003"
                    echo "  $0 all             # Compilar + flash ambos"
                    echo "  $0 monitor         # Monitor serie ESP32-C6"
                    echo
                    ;;
                12)
                    print_success "¡Hasta luego!"
                    exit 0
                    ;;
                *)
                    print_error "Opción inválida"
                    ;;
            esac
            
            echo
            read -p "Presiona Enter para continuar..."
            echo
        done
    fi
}

main "$@"
