#!/bin/bash
#
# ESP32-C6 Project Installer
# Instalador completo para nuevas PCs
# Descarga y configura todo lo necesario
#

set -e

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Información del proyecto
PROJECT_REPO="https://github.com/Cesarbautista10/locknode_v6.git"
PROJECT_DIR="locknode_v6"
PROJECT_SUBDIR="test_continuous_project"

print_header() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $1"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_info() { echo -e "${BLUE}ℹ${NC} $1"; }
print_success() { echo -e "${GREEN}✅${NC} $1"; }
print_warning() { echo -e "${YELLOW}⚠${NC} $1"; }
print_error() { echo -e "${RED}❌${NC} $1"; }

# Detectar sistema operativo
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

# Instalar dependencias del sistema
install_system_deps() {
    local os=$(detect_os)
    
    print_info "Instalando dependencias del sistema para: $os"
    
    case $os in
        "linux")
            # Detectar distribución
            if command -v apt-get &> /dev/null; then
                print_info "Detectado sistema basado en Debian/Ubuntu"
                sudo apt-get update
                sudo apt-get install -y curl git picocom screen build-essential python3 python3-pip
                
                # Agregar usuario al grupo dialout para acceso a puertos serie
                sudo usermod -a -G dialout $USER
                print_warning "Necesitarás cerrar sesión y volver a entrar para aplicar permisos de puerto serie"
                
            elif command -v dnf &> /dev/null; then
                print_info "Detectado sistema basado en Fedora/RHEL"
                sudo dnf install -y curl git picocom screen gcc gcc-c++ python3 python3-pip
                sudo usermod -a -G dialout $USER
                
            elif command -v pacman &> /dev/null; then
                print_info "Detectado sistema basado en Arch"
                sudo pacman -S --needed curl git picocom screen base-devel python python-pip
                sudo usermod -a -G uucp $USER
                
            else
                print_warning "Distribución Linux no reconocida. Instala manualmente:"
                print_warning "- curl, git, picocom, screen, build-essential, python3"
            fi
            ;;
            
        "macos")
            if command -v brew &> /dev/null; then
                print_info "Usando Homebrew para instalar dependencias"
                brew install curl git picocom screen python3
            else
                print_info "Instalando Homebrew..."
                /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                brew install curl git picocom screen python3
            fi
            ;;
            
        "windows")
            print_warning "Sistema Windows detectado"
            print_info "Instala manualmente:"
            print_info "1. Git: https://git-scm.com/download/win"
            print_info "2. Python: https://www.python.org/downloads/"
            print_info "3. Arduino CLI: https://github.com/arduino/arduino-cli/releases"
            print_warning "O usa WSL2 con Ubuntu para mejor experiencia"
            ;;
    esac
}

# Instalar Arduino CLI
install_arduino_cli() {
    if command -v arduino-cli &> /dev/null; then
        print_success "Arduino CLI ya está instalado: $(arduino-cli version | head -1)"
        return 0
    fi
    
    print_info "Instalando Arduino CLI..."
    
    local os=$(detect_os)
    case $os in
        "linux"|"macos")
            curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
            
            # Mover a PATH
            if [[ -f "bin/arduino-cli" ]]; then
                sudo mv bin/arduino-cli /usr/local/bin/
                rmdir bin 2>/dev/null || true
            fi
            ;;
        "windows")
            print_warning "Descarga Arduino CLI manualmente desde:"
            print_warning "https://github.com/arduino/arduino-cli/releases"
            return 1
            ;;
    esac
    
    # Verificar instalación
    if command -v arduino-cli &> /dev/null; then
        print_success "Arduino CLI instalado: $(arduino-cli version | head -1)"
    else
        print_error "Error al instalar Arduino CLI"
        return 1
    fi
}

# Configurar Arduino CLI
configure_arduino_cli() {
    print_info "Configurando Arduino CLI..."
    
    # Crear configuración
    arduino-cli config init --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    
    # Actualizar índices
    print_info "Actualizando índices de placas..."
    arduino-cli core update-index
    
    # Instalar core ESP32
    print_info "Instalando ESP32 core..."
    arduino-cli core install esp32:esp32
    
    print_success "Arduino CLI configurado correctamente"
}

# Clonar proyecto
clone_project() {
    print_info "Clonando proyecto desde repositorio..."
    
    if [[ -d "$PROJECT_DIR" ]]; then
        print_warning "El directorio $PROJECT_DIR ya existe"
        read -p "¿Deseas actualizarlo? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            cd "$PROJECT_DIR"
            git pull
            cd ..
        fi
    else
        git clone "$PROJECT_REPO" "$PROJECT_DIR"
    fi
    
    if [[ -d "$PROJECT_DIR/$PROJECT_SUBDIR" ]]; then
        print_success "Proyecto clonado correctamente"
    else
        print_error "Error: No se encontró el subdirectorio del proyecto"
        return 1
    fi
}

# Crear script de inicio rápido
create_launcher() {
    local launcher_path="$PROJECT_DIR/$PROJECT_SUBDIR/launch.sh"
    
    print_info "Creando script de inicio rápido..."
    
    cat > "$launcher_path" << 'EOF'
#!/bin/bash
# Lanzador rápido para ESP32-C6

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "🚀 ESP32-C6 Quick Launcher"
echo "Directorio: $SCRIPT_DIR"
echo

if [[ ! -f "esp32c6_smart.sh" ]]; then
    echo "❌ Script principal no encontrado"
    exit 1
fi

case "${1:-menu}" in
    "menu")
        echo "Opciones disponibles:"
        echo "1) Compilar y subir firmware"
        echo "2) Solo conectar al puerto serie"
        echo "3) Mostrar información del sistema"
        echo "4) Salir"
        echo
        read -p "Selecciona una opción (1-4): " choice
        
        case $choice in
            1) ./esp32c6_smart.sh upload ;;
            2) ./esp32c6_smart.sh connect ;;
            3) ./esp32c6_smart.sh info ;;
            4) exit 0 ;;
            *) echo "Opción inválida" ;;
        esac
        ;;
    *)
        ./esp32c6_smart.sh "$@"
        ;;
esac
EOF

    chmod +x "$launcher_path"
    print_success "Script de inicio creado: $launcher_path"
}

# Verificar instalación
verify_installation() {
    print_header "VERIFICANDO INSTALACIÓN"
    
    local errors=0
    
    # Verificar Arduino CLI
    if command -v arduino-cli &> /dev/null; then
        print_success "Arduino CLI: $(arduino-cli version | head -1)"
    else
        print_error "Arduino CLI no encontrado"
        ((errors++))
    fi
    
    # Verificar Git
    if command -v git &> /dev/null; then
        print_success "Git: $(git --version)"
    else
        print_error "Git no encontrado"
        ((errors++))
    fi
    
    # Verificar herramientas serie
    if command -v picocom &> /dev/null; then
        print_success "Picocom disponible"
    elif command -v screen &> /dev/null; then
        print_success "Screen disponible"
    else
        print_warning "No se encontró picocom ni screen"
    fi
    
    # Verificar core ESP32
    if arduino-cli core list | grep -q esp32:esp32; then
        print_success "ESP32 core instalado"
    else
        print_error "ESP32 core no encontrado"
        ((errors++))
    fi
    
    # Verificar proyecto
    if [[ -f "$PROJECT_DIR/$PROJECT_SUBDIR/$PROJECT_SUBDIR.ino" ]]; then
        print_success "Proyecto encontrado"
    else
        print_error "Archivo del proyecto no encontrado"
        ((errors++))
    fi
    
    if [[ $errors -eq 0 ]]; then
        print_success "¡Instalación completada correctamente!"
        echo
        print_info "Para usar el proyecto:"
        echo "  cd $PROJECT_DIR/$PROJECT_SUBDIR"
        echo "  ./launch.sh"
        echo
    else
        print_error "Se encontraron $errors errores en la instalación"
        return 1
    fi
}

# Función principal
main() {
    print_header "INSTALADOR ESP32-C6 PARA NUEVA PC"
    
    print_info "Este script instalará todo lo necesario para el desarrollo ESP32-C6"
    print_warning "Se requieren permisos de sudo para algunas operaciones"
    echo
    
    read -p "¿Continuar con la instalación? (y/n): " -n 1 -r
    echo
    echo
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Instalación cancelada"
        exit 0
    fi
    
    # Paso 1: Dependencias del sistema
    install_system_deps
    
    # Paso 2: Arduino CLI
    install_arduino_cli
    
    # Paso 3: Configurar Arduino CLI
    configure_arduino_cli
    
    # Paso 4: Clonar proyecto
    clone_project
    
    # Paso 5: Crear launcher
    create_launcher
    
    # Paso 6: Verificar
    verify_installation
}

# Ejecutar si no se está sourcing
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
