#!/bin/bash
#
# INSTALL TOOLS FOR LOCKNODE V6
# Script para instalar todas las herramientas necesarias
#

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

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

# Detectar distribución de Linux
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo $ID
    elif [ -f /etc/redhat-release ]; then
        echo "rhel"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    else
        echo "unknown"
    fi
}

# Instalar herramientas básicas
install_basic_tools() {
    print_header "INSTALANDO HERRAMIENTAS BÁSICAS"
    
    local distro=$(detect_distro)
    
    case $distro in
        "ubuntu"|"debian")
            print_info "Detectado: Ubuntu/Debian"
            sudo apt update
            sudo apt install -y curl wget build-essential git python3 python3-pip
            ;;
        "fedora"|"rhel"|"centos")
            print_info "Detectado: Fedora/RHEL/CentOS"
            sudo dnf install -y curl wget gcc gcc-c++ make git python3 python3-pip
            ;;
        "arch"|"manjaro")
            print_info "Detectado: Arch/Manjaro"
            sudo pacman -Sy curl wget base-devel git python python-pip
            ;;
        *)
            print_warning "Distribución no detectada, instalación manual requerida"
            return 1
            ;;
    esac
    
    print_success "Herramientas básicas instaladas"
}

# Instalar Arduino CLI
install_arduino_cli() {
    print_header "INSTALANDO ARDUINO CLI"
    
    # Crear directorio bin local si no existe
    mkdir -p ~/bin
    
    # Descargar Arduino CLI
    print_info "Descargando Arduino CLI..."
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh
    
    # Agregar ~/bin al PATH si no está
    if [[ ":$PATH:" != *":$HOME/bin:"* ]]; then
        echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
        export PATH="$HOME/bin:$PATH"
    fi
    
    # También verificar si se instaló en el directorio actual
    if [ -f "./bin/arduino-cli" ]; then
        print_info "Arduino CLI encontrado en ./bin/, moviendo a ~/bin/"
        mv ./bin/arduino-cli ~/bin/
        rm -rf ./bin
        export PATH="$HOME/bin:$PATH"
    fi
    
    # Verificar instalación
    if command -v arduino-cli &> /dev/null || [ -f "$HOME/bin/arduino-cli" ]; then
        # Asegurar que usamos la ruta correcta
        ARDUINO_CLI_PATH="$HOME/bin/arduino-cli"
        if ! command -v arduino-cli &> /dev/null; then
            export PATH="$HOME/bin:$PATH"
        fi
        
        print_success "Arduino CLI instalado"
        
        # Configurar Arduino CLI
        print_info "Configurando Arduino CLI..."
        $ARDUINO_CLI_PATH config init --dest-dir ~/.arduino15 || arduino-cli config init --dest-dir ~/.arduino15
        
        # Instalar core ESP32
        print_info "Instalando ESP32 core..."
        $ARDUINO_CLI_PATH config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json || \
        arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
        
        $ARDUINO_CLI_PATH core update-index || arduino-cli core update-index
        $ARDUINO_CLI_PATH core install esp32:esp32 || arduino-cli core install esp32:esp32
        
        print_success "ESP32 core instalado"
        print_info "PATH actualizado. Reinicia la terminal o ejecuta: source ~/.bashrc"
    else
        print_error "Error instalando Arduino CLI"
        print_info "Intenta manualmente: export PATH=\"\$HOME/bin:\$PATH\""
        return 1
    fi
}

# Instalar ARM GCC Toolchain
install_arm_gcc() {
    print_header "INSTALANDO ARM GCC TOOLCHAIN"
    
    local distro=$(detect_distro)
    
    case $distro in
        "ubuntu"|"debian")
            sudo apt install -y gcc-arm-none-eabi binutils-arm-none-eabi
            ;;
        "fedora"|"rhel"|"centos")
            sudo dnf install -y arm-none-eabi-gcc-cs arm-none-eabi-binutils-cs
            ;;
        "arch"|"manjaro")
            sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils
            ;;
        *)
            print_info "Instalando ARM GCC manualmente..."
            # Descargar ARM GCC desde ARM
            cd /tmp
            wget -q https://developer.arm.com/-/media/Files/downloads/gnu/12.3.rel1/binrel/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz
            sudo tar -xf arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz -C /opt/
            echo 'export PATH="/opt/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi/bin:$PATH"' | sudo tee -a /etc/environment
            export PATH="/opt/arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi/bin:$PATH"
            ;;
    esac
    
    if command -v arm-none-eabi-gcc &> /dev/null; then
        print_success "ARM GCC Toolchain instalado"
    else
        print_warning "ARM GCC necesita configuración manual del PATH"
    fi
}

# Instalar PyOCD
install_pyocd() {
    print_header "INSTALANDO PYOCD"
    
    print_info "Instalando PyOCD via pip..."
    pip3 install --user pyocd
    
    # Agregar ~/.local/bin al PATH si no está
    if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
        export PATH="$HOME/.local/bin:$PATH"
    fi
    
    if command -v pyocd &> /dev/null; then
        print_success "PyOCD instalado"
        
        # Instalar paquetes para diferentes targets
        print_info "Instalando paquetes PyOCD..."
        pyocd pack install py32f003xx || print_warning "Paquete PY32F003 no disponible"
        
    else
        print_warning "PyOCD instalado pero no en PATH. Reinicia la terminal."
    fi
}

# Configurar permisos USB
setup_usb_permissions() {
    print_header "CONFIGURANDO PERMISOS USB"
    
    print_info "Agregando usuario al grupo dialout..."
    sudo usermod -a -G dialout $USER
    
    print_info "Configurando reglas udev para dispositivos ESP32 y ARM debuggers..."
    
    # Crear archivo de reglas udev
    sudo tee /etc/udev/rules.d/99-locknode.rules > /dev/null << 'EOF'
# ESP32-C6 CDC-JTAG
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", ATTR{idProduct}=="1001", MODE="0666", GROUP="dialout"

# ARM Debuggers (ST-Link, J-Link, etc.)
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", MODE="0666", GROUP="dialout"
SUBSYSTEM=="usb", ATTR{idVendor}=="1366", MODE="0666", GROUP="dialout"

# Generic USB-Serial
KERNEL=="ttyACM*", MODE="0666", GROUP="dialout"
KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"
EOF
    
    sudo udevadm control --reload-rules
    sudo udevadm trigger
    
    print_success "Permisos USB configurados"
    print_warning "Necesitas cerrar sesión y volver a entrar para aplicar permisos de grupo"
}

# Verificar instalación
verify_installation() {
    print_header "VERIFICANDO INSTALACIÓN"
    
    local errors=0
    
    # Arduino CLI
    if command -v arduino-cli &> /dev/null; then
        local version=$(arduino-cli version | head -n1)
        print_success "Arduino CLI: $version"
    elif [ -f "$HOME/bin/arduino-cli" ]; then
        local version=$($HOME/bin/arduino-cli version | head -n1)
        print_success "Arduino CLI: $version (en ~/bin/)"
        print_warning "Ejecuta: source ~/.bashrc o export PATH=\"\$HOME/bin:\$PATH\""
    else
        print_error "Arduino CLI no instalado correctamente"
        errors=$((errors + 1))
    fi
    
    # Make
    if command -v make &> /dev/null; then
        local version=$(make --version | head -n1)
        print_success "Make: $version"
    else
        print_error "Make no instalado correctamente"
        errors=$((errors + 1))
    fi
    
    # ARM GCC
    if command -v arm-none-eabi-gcc &> /dev/null; then
        local version=$(arm-none-eabi-gcc --version | head -n1)
        print_success "ARM GCC: $version"
    else
        print_error "ARM GCC no instalado correctamente"
        errors=$((errors + 1))
    fi
    
    # PyOCD
    if command -v pyocd &> /dev/null; then
        local version=$(pyocd --version)
        print_success "PyOCD: $version"
    else
        print_error "PyOCD no instalado correctamente"
        errors=$((errors + 1))
    fi
    
    # ESP32 Core
    if arduino-cli core list | grep -q esp32:esp32; then
        print_success "ESP32 Core instalado"
    else
        print_error "ESP32 Core no instalado"
        errors=$((errors + 1))
    fi
    
    echo
    if [ $errors -eq 0 ]; then
        print_success "¡TODAS LAS HERRAMIENTAS INSTALADAS CORRECTAMENTE!"
        print_info "Ya puedes usar ./build_and_flash.sh"
    else
        print_error "Se encontraron $errors errores en la instalación"
        return 1
    fi
}

# Menú de instalación
show_install_menu() {
    print_header "INSTALADOR DE HERRAMIENTAS - LOCKNODE V6"
    echo
    echo -e "${GREEN}Opciones de instalación:${NC}"
    echo
    echo "1) 🔧 Instalar TODO (recomendado)"
    echo "2) 📦 Instalar herramientas básicas"
    echo "3) 🎯 Instalar Arduino CLI + ESP32"
    echo "4) 🛠️ Instalar ARM GCC Toolchain"
    echo "5) 🔌 Instalar PyOCD"
    echo "6) 🔐 Configurar permisos USB"
    echo "7) ✅ Verificar instalación"
    echo "8) ❓ Ayuda"
    echo "9) 🚪 Salir"
    echo
}

# Instalar todo
install_all() {
    print_header "INSTALACIÓN COMPLETA - LOCKNODE V6"
    
    local errors=0
    
    install_basic_tools || errors=$((errors + 1))
    echo
    install_arduino_cli || errors=$((errors + 1))
    echo
    install_arm_gcc || errors=$((errors + 1))
    echo
    install_pyocd || errors=$((errors + 1))
    echo
    setup_usb_permissions || errors=$((errors + 1))
    echo
    
    if [ $errors -eq 0 ]; then
        print_success "¡INSTALACIÓN COMPLETA EXITOSA!"
        echo
        verify_installation
    else
        print_error "Se encontraron $errors errores durante la instalación"
        return 1
    fi
}

# Función principal
main() {
    if [[ -n "$1" ]]; then
        # Modo comando directo
        case "$1" in
            "all")
                install_all
                ;;
            "basic")
                install_basic_tools
                ;;
            "arduino")
                install_arduino_cli
                ;;
            "arm-gcc")
                install_arm_gcc
                ;;
            "pyocd")
                install_pyocd
                ;;
            "permissions")
                setup_usb_permissions
                ;;
            "verify")
                verify_installation
                ;;
            *)
                echo "Comando desconocido: $1"
                echo "Comandos disponibles: all, basic, arduino, arm-gcc, pyocd, permissions, verify"
                exit 1
                ;;
        esac
    else
        # Modo menú interactivo
        while true; do
            show_install_menu
            read -p "Selecciona una opción (1-9): " choice
            echo
            
            case $choice in
                1)
                    install_all
                    ;;
                2)
                    install_basic_tools
                    ;;
                3)
                    install_arduino_cli
                    ;;
                4)
                    install_arm_gcc
                    ;;
                5)
                    install_pyocd
                    ;;
                6)
                    setup_usb_permissions
                    ;;
                7)
                    verify_installation
                    ;;
                8)
                    echo "INSTALADOR DE HERRAMIENTAS - LOCKNODE V6"
                    echo
                    echo "Este script instala todas las herramientas necesarias para:"
                    echo "- Desarrollo ESP32-C6 con Arduino CLI"
                    echo "- Desarrollo PY32F003 con ARM GCC + PyOCD"
                    echo "- Configuración de permisos USB/serie"
                    echo
                    echo "Uso directo:"
                    echo "  $0 all         # Instalar todo"
                    echo "  $0 verify      # Solo verificar"
                    echo
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
    fi
}

main "$@"
