#!/bin/bash
#
# LOCKNODE V6 - MASTER BUILD SYSTEM
# Sistema maestro de compilación, carga y gestión de firmware
# Gestiona múltiples proyectos y versiones
#

set -o pipefail

# Configuración de colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m'

# Configuración del proyecto
WORKSPACE_ROOT="/media/mr/firmware/personal/locknode_v6"
BUILD_TOOLS_DIR="$WORKSPACE_ROOT/build_tools"
BUILD_OUTPUT_DIR="$BUILD_TOOLS_DIR/output"
PROJECTS_DIR="$WORKSPACE_ROOT"

# Declarar array de proyectos
declare -A PROJECTS

# Detectar proyectos automáticamente
detect_projects() {
    local projects_found=()
    
    # Buscar directorios con archivos .ino
    while IFS= read -r -d '' dir; do
        local project_name=$(basename "$dir")
        if [[ -n "$(find "$dir" -maxdepth 1 -name "*.ino" 2>/dev/null)" ]]; then
            PROJECTS["$project_name"]="$dir"
            projects_found+=("$project_name")
        fi
    done < <(find "$PROJECTS_DIR" -maxdepth 1 -type d -not -path "$PROJECTS_DIR" -not -name ".*" -not -name "build_tools" -not -name "Libraries" -not -name "Misc" -not -name "Build" -not -name "venv" -print0 2>/dev/null)
    
    # Proyectos específicos adicionales (en case no se detecten automáticamente)
    PROJECTS["firmware_v5"]="${PROJECTS["firmware_v5"]:-$PROJECTS_DIR/firmware_v5}"
    PROJECTS["test_continuous"]="${PROJECTS["test_continuous"]:-$PROJECTS_DIR/test_continuous_project}"
    PROJECTS["traffic_router"]="${PROJECTS["traffic_router"]:-$PROJECTS_DIR/traffic_router_manager}"
}

# Inicializar detección de proyectos
detect_projects

# Configuraciones de boards
declare -A BOARD_CONFIGS=(
    ["esp32c6"]="esp32:esp32:esp32c6:CDCOnBoot=cdc,JTAGAdapter=builtin"
    ["esp32"]="esp32:esp32:esp32"
    ["esp32s3"]="esp32:esp32:esp32s3"
)

# Funciones de output
print_header() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} ${WHITE}$1${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_subheader() {
    echo -e "${PURPLE}┌─ $1${NC}"
}

print_info() { echo -e "${BLUE}ℹ${NC} $1"; }
print_success() { echo -e "${GREEN}✅${NC} $1"; }
print_warning() { echo -e "${YELLOW}⚠${NC} $1"; }
print_error() { echo -e "${RED}❌${NC} $1"; }

# Crear directorios de trabajo
setup_workspace() {
    print_info "Configurando workspace..."
    
    mkdir -p "$BUILD_OUTPUT_DIR"
    mkdir -p "$BUILD_OUTPUT_DIR/firmware_v5"
    mkdir -p "$BUILD_OUTPUT_DIR/test_continuous"
    mkdir -p "$BUILD_OUTPUT_DIR/traffic_router"
    mkdir -p "$BUILD_OUTPUT_DIR/logs"
    
    print_success "Workspace configurado en: $BUILD_TOOLS_DIR"
}

# Detectar puerto serie automáticamente
detect_serial_port() {
    local ports=("/dev/ttyACM0" "/dev/ttyUSB0" "/dev/cu.usbserial*" "/dev/cu.usbmodem*")
    
    for port_pattern in "${ports[@]}"; do
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

# Listar proyectos disponibles
list_projects() {
    print_subheader "PROYECTOS DISPONIBLES"
    
    local index=1
    for project in "${!PROJECTS[@]}"; do
        local path="${PROJECTS[$project]}"
        if [[ -d "$path" ]]; then
            print_success "$index. $project -> $path"
        else
            print_warning "$index. $project -> $path [NO EXISTE]"
        fi
        ((index++))
    done
}

# Compilar proyecto específico
compile_project() {
    local project_name="$1"
    local board_type="${2:-esp32c6}"
    local project_path="${PROJECTS[$project_name]}"
    
    if [[ -z "$project_path" ]] || [[ ! -d "$project_path" ]]; then
        print_error "Proyecto '$project_name' no encontrado"
        return 1
    fi
    
    local board_fqbn="${BOARD_CONFIGS[$board_type]}"
    if [[ -z "$board_fqbn" ]]; then
        print_error "Configuración de board '$board_type' no encontrada"
        return 1
    fi
    
    print_subheader "COMPILANDO: $project_name ($board_type)"
    print_info "Proyecto: $project_path"
    print_info "Board: $board_fqbn"
    
    cd "$project_path"
    
    # Encontrar archivo .ino
    local ino_file
    ino_file=$(find . -name "*.ino" -maxdepth 1 | head -1)
    
    if [[ -z "$ino_file" ]]; then
        print_error "No se encontró archivo .ino en $project_path"
        return 1
    fi
    
    ino_file=$(basename "$ino_file")
    print_info "Archivo: $ino_file"
    
    # Crear configuración platform.local.txt si es ESP32-C6
    if [[ "$board_type" == "esp32c6" ]]; then
        setup_esp32c6_config
    fi
    
    # Compilar
    local log_file="$BUILD_OUTPUT_DIR/logs/${project_name}_compile_$(date +%Y%m%d_%H%M%S).log"
    
    print_info "Compilando... (log: $log_file)"
    
    if arduino-cli compile --fqbn "$board_fqbn" "$ino_file" 2>&1 | tee "$log_file"; then
        print_success "Compilación exitosa: $project_name"
        
        # Copiar binarios al output
        local output_dir="$BUILD_OUTPUT_DIR/$project_name"
        if [[ -d "build" ]]; then
            cp -r build/* "$output_dir/" 2>/dev/null || true
        fi
        
        return 0
    else
        print_error "Error en compilación: $project_name"
        return 1
    fi
}

# Configuración específica para ESP32-C6
setup_esp32c6_config() {
    local arduino_home="$HOME/.arduino15"
    local core_path="$arduino_home/packages/esp32/hardware/esp32/3.2.0"
    local platform_file="$core_path/platform.local.txt"
    
    if [[ -d "$core_path" ]]; then
        print_info "Configurando ESP32-C6 CDC-JTAG..."
        cat > "$platform_file" << 'EOF'
# ESP32-C6 CDC-JTAG Configuration
compiler.cpp.extra_flags=-DESP32C6=1 -DARDUINO_ESP32C6_DEV=1 -DCONFIG_IDF_TARGET_ESP32C6=1 -DESP32C6_JTAG_ENABLED=1
EOF
        print_success "Configuración CDC-JTAG aplicada"
    fi
}

# Cargar firmware al dispositivo
upload_firmware() {
    local project_name="$1"
    local board_type="${2:-esp32c6}"
    local port="${3:-$(detect_serial_port)}"
    
    local project_path="${PROJECTS[$project_name]}"
    local board_fqbn="${BOARD_CONFIGS[$board_type]}"
    
    print_subheader "CARGANDO FIRMWARE: $project_name"
    print_info "Puerto: $port"
    print_info "Board: $board_fqbn"
    
    cd "$project_path"
    
    local ino_file
    ino_file=$(find . -name "*.ino" -maxdepth 1 | head -1)
    ino_file=$(basename "$ino_file")
    
    local log_file="$BUILD_OUTPUT_DIR/logs/${project_name}_upload_$(date +%Y%m%d_%H%M%S).log"
    
    if arduino-cli upload -p "$port" --fqbn "$board_fqbn" "$ino_file" 2>&1 | tee "$log_file"; then
        print_success "Firmware cargado exitosamente: $project_name"
        print_info "Monitor serie: picocom $port -b 115200"
        return 0
    else
        print_error "Error al cargar firmware: $project_name"
        return 1
    fi
}

# Conectar al monitor serie
connect_monitor() {
    local port="${1:-$(detect_serial_port)}"
    local baudrate="${2:-115200}"
    
    print_subheader "MONITOR SERIE"
    print_info "Puerto: $port"
    print_info "Baudrate: $baudrate"
    print_warning "Para salir: Ctrl+A, Ctrl+X"
    
    if command -v picocom &> /dev/null; then
        picocom "$port" -b "$baudrate"
    elif command -v screen &> /dev/null; then
        screen "$port" "$baudrate"
    else
        print_error "No se encontró picocom ni screen"
        return 1
    fi
}

# Workflow completo
full_workflow() {
    local project_name="$1"
    local board_type="${2:-esp32c6}"
    
    print_header "WORKFLOW COMPLETO: $project_name"
    
    if compile_project "$project_name" "$board_type"; then
        if upload_firmware "$project_name" "$board_type"; then
            print_success "¿Conectar al monitor serie? (y/n)"
            read -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                connect_monitor
            fi
        fi
    fi
}

# Gestión de versiones y backups
backup_firmware() {
    local project_name="$1"
    local version_tag="$2"
    
    if [[ -z "$version_tag" ]]; then
        version_tag="backup_$(date +%Y%m%d_%H%M%S)"
    fi
    
    local backup_dir="$BUILD_OUTPUT_DIR/backups/$project_name/$version_tag"
    mkdir -p "$backup_dir"
    
    local project_path="${PROJECTS[$project_name]}"
    
    print_subheader "CREANDO BACKUP: $project_name -> $version_tag"
    
    # Copiar código fuente
    cp -r "$project_path"/* "$backup_dir/"
    
    # Copiar binarios si existen
    if [[ -d "$BUILD_OUTPUT_DIR/$project_name" ]]; then
        cp -r "$BUILD_OUTPUT_DIR/$project_name" "$backup_dir/build_output"
    fi
    
    print_success "Backup creado: $backup_dir"
}

# Información del sistema
show_system_info() {
    print_header "INFORMACIÓN DEL SISTEMA"
    
    print_info "Workspace: $WORKSPACE_ROOT"
    print_info "Build Tools: $BUILD_TOOLS_DIR"
    print_info "Output: $BUILD_OUTPUT_DIR"
    
    # Arduino CLI
    if command -v arduino-cli &> /dev/null; then
        print_success "Arduino CLI: $(arduino-cli version | head -1)"
    else
        print_error "Arduino CLI no encontrado"
    fi
    
    # Puerto serie
    local port=$(detect_serial_port)
    if [[ -e "$port" ]]; then
        print_success "Puerto serie: $port"
    else
        print_warning "Puerto serie: $port [NO CONECTADO]"
    fi
    
    # Proyectos
    list_projects
    
    # Espacio en disco
    print_subheader "ESPACIO EN DISCO"
    df -h "$WORKSPACE_ROOT" | tail -1
}

# Menú interactivo
interactive_menu() {
    while true; do
        print_header "LOCKNODE V6 - MASTER BUILD SYSTEM"
        echo
        echo "1) Listar proyectos"
        echo "2) Compilar proyecto"
        echo "3) Cargar firmware"
        echo "4) Workflow completo (compile + upload + monitor)"
        echo "5) Monitor serie"
        echo "6) Backup proyecto"
        echo "7) Información del sistema"
        echo "8) Configurar workspace"
        echo "9) Salir"
        echo
        read -p "Selecciona una opción (1-9): " choice
        echo
        
        case $choice in
            1)
                list_projects
                ;;
            2)
                list_projects
                echo
                read -p "Nombre del proyecto: " project
                read -p "Tipo de board [esp32c6]: " board
                board=${board:-esp32c6}
                compile_project "$project" "$board"
                ;;
            3)
                list_projects
                echo
                read -p "Nombre del proyecto: " project
                read -p "Tipo de board [esp32c6]: " board
                read -p "Puerto serie [auto]: " port
                board=${board:-esp32c6}
                port=${port:-$(detect_serial_port)}
                upload_firmware "$project" "$board" "$port"
                ;;
            4)
                list_projects
                echo
                read -p "Nombre del proyecto: " project
                read -p "Tipo de board [esp32c6]: " board
                board=${board:-esp32c6}
                full_workflow "$project" "$board"
                ;;
            5)
                connect_monitor
                ;;
            6)
                list_projects
                echo
                read -p "Nombre del proyecto: " project
                read -p "Tag de versión [auto]: " tag
                backup_firmware "$project" "$tag"
                ;;
            7)
                show_system_info
                ;;
            8)
                setup_workspace
                ;;
            9)
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
}

# Mostrar ayuda
show_help() {
    print_header "LOCKNODE V6 - MASTER BUILD SYSTEM"
    echo
    echo "Uso: $0 [comando] [opciones]"
    echo
    echo "Comandos disponibles:"
    echo "  list                    Listar proyectos disponibles"
    echo "  compile <proyecto> [board]  Compilar proyecto"
    echo "  upload <proyecto> [board] [port]  Cargar firmware"
    echo "  full <proyecto> [board]     Workflow completo"
    echo "  monitor [port] [baudrate]   Conectar monitor serie"
    echo "  backup <proyecto> [tag]     Crear backup"
    echo "  info                    Información del sistema"
    echo "  setup                   Configurar workspace"
    echo "  menu                    Menú interactivo"
    echo "  help                    Mostrar esta ayuda"
    echo
    echo "Ejemplos:"
    echo "  $0 list"
    echo "  $0 compile test_continuous esp32c6"
    echo "  $0 upload test_continuous esp32c6 /dev/ttyACM0"
    echo "  $0 full test_continuous"
    echo "  $0 menu"
    echo
    echo "Proyectos disponibles:"
    for project in "${!PROJECTS[@]}"; do
        echo "  - $project"
    done
}

# Función principal
main() {
    case "${1:-menu}" in
        "list")
            list_projects
            ;;
        "compile")
            compile_project "$2" "$3"
            ;;
        "upload")
            upload_firmware "$2" "$3" "$4"
            ;;
        "full")
            full_workflow "$2" "$3"
            ;;
        "monitor")
            connect_monitor "$2" "$3"
            ;;
        "backup")
            backup_firmware "$2" "$3"
            ;;
        "info")
            show_system_info
            ;;
        "setup")
            setup_workspace
            ;;
        "menu")
            interactive_menu
            ;;
        "help"|"--help"|"-h")
            show_help
            ;;
        *)
            print_error "Comando desconocido: $1"
            show_help
            exit 1
            ;;
    esac
}

# Ejecutar si no se está sourcing
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
