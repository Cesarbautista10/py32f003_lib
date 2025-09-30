#!/bin/bash
#
# ESP32-C6 Smart Compiler & Installer
# Auto-detección de rutas y configuración inteligente
# Soporte para instalación en nuevas PCs
#

# Configuración de errores más inteligente
set -o pipefail  # Los pipes fallan si algún comando falla

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Función para imprimir con colores
print_header() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $1"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_success() {
    echo -e "${GREEN}✅${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_error() {
    echo -e "${RED}❌${NC} $1"
}

# Variables globales
PROJECT_NAME="test_continuous_project"
BOARD_FQBN="esp32:esp32:esp32c6:CDCOnBoot=cdc,JTAGAdapter=builtin"
ESP32_CORE_VERSIONS=("3.2.0" "3.1.0" "3.0.0" "2.0.17" "2.0.16")
POSSIBLE_PORTS=("/dev/ttyACM0" "/dev/ttyUSB0" "/dev/cu.usbserial*" "/dev/cu.usbmodem*")

# Función para detectar el sistema operativo
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Función para detectar el directorio home de Arduino
detect_arduino_home() {
    local os=$(detect_os)
    
    case $os in
        "linux")
            echo "$HOME/.arduino15"
            ;;
        "macos")
            echo "$HOME/Library/Arduino15"
            ;;
        "windows")
            echo "$USERPROFILE/AppData/Local/Arduino15"
            ;;
        *)
            echo "$HOME/.arduino15"
            ;;
    esac
}

# Función para detectar todas las versiones del core ESP32
detect_esp32_core_paths() {
    local arduino_home=$(detect_arduino_home)
    local found_paths=()
    
    # Buscar versiones conocidas primero
    for version in "${ESP32_CORE_VERSIONS[@]}"; do
        local core_path="$arduino_home/packages/esp32/hardware/esp32/$version"
        if [[ -d "$core_path" ]]; then
            found_paths+=("$core_path")
        fi
    done
    
    # Buscar versiones adicionales sin duplicados
    if [[ -d "$arduino_home/packages/esp32/hardware/esp32" ]]; then
        for dir in "$arduino_home/packages/esp32/hardware/esp32"/*; do
            if [[ -d "$dir" ]] && [[ ! " ${found_paths[@]} " =~ " ${dir} " ]]; then
                found_paths+=("$dir")
            fi
        done
    fi
    
    # Imprimir solo las rutas, sin colores
    printf '%s\n' "${found_paths[@]}"
}

# Función para seleccionar el core ESP32 más reciente
select_esp32_core() {
    local cores_output
    cores_output=$(detect_esp32_core_paths)
    local cores=()
    
    # Leer línea por línea y filtrar solo las rutas válidas
    while IFS= read -r line; do
        if [[ -d "$line" ]]; then
            cores+=("$line")
        fi
    done <<< "$cores_output"
    
    if [[ ${#cores[@]} -eq 0 ]]; then
        print_error "No se encontró ningún core ESP32 instalado"
        print_warning "Ejecuta: ./$(basename $0) install"
        exit 1
    fi
    
    # Usar el primer core encontrado (más reciente)
    echo "${cores[0]}"
}

# Función para detectar puerto serie
# Detecta el puerto serie disponible (versión silenciosa)
detect_serial_port_silent() {
    for port_pattern in "${POSSIBLE_PORTS[@]}"; do
        # Expandir patrones con wildcard
        if [[ "$port_pattern" == *"*"* ]]; then
            for port in $port_pattern; do
                if [[ -e "$port" ]]; then
                    echo "$port"
                    return 0
                fi
            done
        else
            if [[ -e "$port_pattern" ]]; then
                echo "$port_pattern"
                return 0
            fi
        fi
    done
    
    echo "/dev/ttyACM0"  # Default
}

# Detecta el puerto serie disponible (versión con mensajes)
detect_serial_port() {
    print_info "Detectando puerto serie..."
    
    for port_pattern in "${POSSIBLE_PORTS[@]}"; do
        # Expandir patrones con wildcard
        if [[ "$port_pattern" == *"*"* ]]; then
            for port in $port_pattern; do
                if [[ -e "$port" ]]; then
                    print_success "Puerto encontrado: $port"
                    echo "$port"
                    return 0
                fi
            done
        else
            if [[ -e "$port_pattern" ]]; then
                print_success "Puerto encontrado: $port_pattern"
                echo "$port_pattern"
                return 0
            fi
        fi
    done
    
    print_warning "No se detectó puerto automáticamente"
    echo "/dev/ttyACM0"  # Default
}

# Función para verificar si arduino-cli está instalado
check_arduino_cli() {
    if ! command -v arduino-cli &> /dev/null; then
        print_error "arduino-cli no está instalado"
        print_warning "Ejecuta: ./$(basename $0) install"
        exit 1
    fi
    
    local version=$(arduino-cli version | grep -oP 'arduino-cli\s+\K[\d.]+' | head -1)
    print_success "arduino-cli encontrado: v$version"
}

# Función para verificar si el core ESP32 está instalado
check_esp32_core() {
    print_info "Verificando core ESP32..."
    
    local cores=$(arduino-cli core list | grep esp32:esp32 || true)
    if [[ -z "$cores" ]]; then
        print_error "Core ESP32 no está instalado en arduino-cli"
        print_warning "Ejecuta: ./$(basename $0) install"
        exit 1
    fi
    
    print_success "Core ESP32 instalado: $cores"
}

# Función de instalación completa
install_dependencies() {
    print_header "INSTALADOR AUTOMÁTICO ESP32-C6"
    
    local os=$(detect_os)
    print_info "Sistema detectado: $os"
    
    # Instalar arduino-cli si no existe
    if ! command -v arduino-cli &> /dev/null; then
        print_info "Instalando arduino-cli..."
        
        case $os in
            "linux")
                curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
                sudo mv bin/arduino-cli /usr/local/bin/
                ;;
            "macos")
                if command -v brew &> /dev/null; then
                    brew install arduino-cli
                else
                    print_warning "Homebrew no encontrado. Instalando manualmente..."
                    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
                    sudo mv bin/arduino-cli /usr/local/bin/
                fi
                ;;
            "windows")
                print_warning "En Windows, descarga arduino-cli desde:"
                print_warning "https://github.com/arduino/arduino-cli/releases"
                exit 1
                ;;
        esac
        
        print_success "arduino-cli instalado"
    fi
    
    # Configurar arduino-cli
    print_info "Configurando arduino-cli..."
    arduino-cli config init --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    
    # Actualizar índices
    print_info "Actualizando índices..."
    arduino-cli core update-index
    
    # Instalar core ESP32
    print_info "Instalando core ESP32..."
    arduino-cli core install esp32:esp32
    
    # Instalar herramientas adicionales si es necesario
    case $os in
        "linux")
            print_info "Instalando herramientas adicionales..."
            sudo apt-get update
            sudo apt-get install -y picocom screen cu
            
            # Permisos para puerto serie
            sudo usermod -a -G dialout $USER
            print_warning "Reinicia la sesión para aplicar permisos de puerto serie"
            ;;
        "macos")
            if command -v brew &> /dev/null; then
                brew install picocom screen
            fi
            ;;
    esac
    
    print_success "Instalación completada"
    print_info "Ahora puedes ejecutar: ./$(basename $0) upload"
}

# Función principal de compilación
compile_project() {
    print_header "COMPILADOR INTELIGENTE ESP32-C6"
    
    # Verificaciones previas
    check_arduino_cli
    check_esp32_core
    
    # Detectar rutas automáticamente
    local esp32_core_path=$(select_esp32_core)
    print_success "Core ESP32 seleccionado: $esp32_core_path"
    
    # Verificar archivo del proyecto
    if [[ ! -f "$PROJECT_NAME.ino" ]]; then
        print_error "No se encontró $PROJECT_NAME.ino en el directorio actual"
        print_info "Directorio actual: $(pwd)"
        exit 1
    fi
    
    # Copiar configuración local si existe
    if [[ -f "platform.local.txt" ]]; then
        print_info "Copiando configuración platform.local.txt..."
        cp platform.local.txt "$esp32_core_path/platform.local.txt"
    fi
    
    # Compilar
    print_info "Compilando con configuración CDC-JTAG..."
    
    arduino-cli compile \
        --fqbn "$BOARD_FQBN" \
        --build-property "build.cdc_on_boot=1" \
        --build-property "compiler.cpp.extra_flags=-DARDUINO_ESP32C6_DEV=1 -DESP32C6=1 -DCONFIG_IDF_TARGET_ESP32C6=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DARDUINO_USB_MODE=1 -DESP32C6_JTAG_ENABLED=1" \
        "$PROJECT_NAME.ino"
    
    if [[ $? -eq 0 ]]; then
        print_success "Compilación exitosa"
        return 0
    else
        print_error "Error en la compilación"
        return 1
    fi
}

# Función para subir firmware
upload_firmware() {
    local port=$(detect_serial_port_silent)
    
    print_info "Cargando firmware al puerto: $port"
    
    arduino-cli upload \
        -p "$port" \
        --fqbn "$BOARD_FQBN" \
        "$PROJECT_NAME.ino"
    
    if [[ $? -eq 0 ]]; then
        print_success "Firmware cargado exitosamente"
        print_info "Para conectar: picocom $port -b 115200"
        print_info "Comando JTAG disponible: 'jtag'"
        print_info "Para salir de picocom: Ctrl+A, Ctrl+X"
    else
        print_error "Error al cargar el firmware"
        return 1
    fi
}

# Función para conectar al puerto serie
connect_serial() {
    local port=$(detect_serial_port_silent)
    
    if command -v picocom &> /dev/null; then
        print_info "Conectando con picocom al puerto: $port"
        picocom "$port" -b 115200
    elif command -v screen &> /dev/null; then
        print_info "Conectando con screen al puerto: $port"
        screen "$port" 115200
    else
        print_error "No se encontró picocom ni screen"
        print_warning "Instala: sudo apt-get install picocom screen"
        exit 1
    fi
}

# Función para mostrar ayuda
show_help() {
    echo -e "${CYAN}ESP32-C6 Smart Compiler & Installer${NC}"
    echo
    echo "Uso: $0 [comando]"
    echo
    echo "Comandos disponibles:"
    echo -e "  ${GREEN}install${NC}     Instalar dependencias (arduino-cli, cores, herramientas)"
    echo -e "  ${GREEN}compile${NC}     Solo compilar el proyecto"  
    echo -e "  ${GREEN}upload${NC}      Compilar y subir firmware"
    echo -e "  ${GREEN}connect${NC}     Conectar al puerto serie"
    echo -e "  ${GREEN}info${NC}        Mostrar información del sistema"
    echo -e "  ${GREEN}help${NC}        Mostrar esta ayuda"
    echo
    echo "Ejemplos:"
    echo "  $0 install           # Instalar todo en una PC nueva"
    echo "  $0 upload            # Compilar y subir"
    echo "  $0 connect           # Solo conectar al puerto serie"
    echo
}

# Función para mostrar información del sistema
show_info() {
    print_header "INFORMACIÓN DEL SISTEMA"
    
    print_info "Sistema operativo: $(detect_os)"
    print_info "Directorio Arduino: $(detect_arduino_home)"
    print_info "Puerto serie detectado: $(detect_serial_port)"
    
    if command -v arduino-cli &> /dev/null; then
        print_info "Arduino CLI: $(arduino-cli version | head -1)"
    else
        print_warning "Arduino CLI: No instalado"
    fi
    
    echo
    print_info "Cores ESP32 disponibles:"
    detect_esp32_core_paths | while read path; do
        if [[ -n "$path" ]]; then
            local version=$(basename "$path")
            echo "  - v$version: $path"
        fi
    done
    
    echo
    print_info "Archivos del proyecto:"
    if [[ -f "$PROJECT_NAME.ino" ]]; then
        print_success "$PROJECT_NAME.ino encontrado"
    else
        print_warning "$PROJECT_NAME.ino no encontrado"
    fi
}

# Función principal
main() {
    case "${1:-}" in
        "install")
            install_dependencies
            ;;
        "compile")
            compile_project
            ;;
        "upload")
            compile_project && upload_firmware
            ;;
        "connect")
            connect_serial
            ;;
        "info")
            show_info
            ;;
        "help"|"--help"|"-h")
            show_help
            ;;
        "")
            show_help
            ;;
        *)
            print_error "Comando desconocido: $1"
            show_help
            exit 1
            ;;
    esac
}

# Ejecutar función principal
main "$@"
