#!/bin/bash
#
# LOCKNODE V6 - PROJECT GENERATOR
# Generador de nuevos proyectos basado en templates
#

set -o pipefail

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Rutas
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TOOLS_DIR="$(dirname "$SCRIPT_DIR")"
WORKSPACE_ROOT="$(dirname "$BUILD_TOOLS_DIR")"
TEMPLATES_DIR="$BUILD_TOOLS_DIR/templates"
CONFIGS_DIR="$BUILD_TOOLS_DIR/configs"

# Funciones de output
print_header() {
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $1"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
}

print_info() { echo -e "${BLUE}ℹ${NC} $1"; }
print_success() { echo -e "${GREEN}✅${NC} $1"; }
print_warning() { echo -e "${YELLOW}⚠${NC} $1"; }
print_error() { echo -e "${RED}❌${NC} $1"; }

# Templates disponibles
declare -A TEMPLATES=(
    ["esp32c6_basic"]="ESP32-C6 Proyecto básico con CDC-JTAG"
    ["esp32_basic"]="ESP32 Proyecto básico estándar"
    ["i2c_scanner"]="Scanner I2C con soporte TCA9545A"
    ["serial_bridge"]="Puente serie con comandos"
)

# Listar templates disponibles
list_templates() {
    print_header "TEMPLATES DISPONIBLES"
    
    local index=1
    for template in "${!TEMPLATES[@]}"; do
        echo -e "${GREEN}$index.${NC} ${YELLOW}$template${NC} - ${TEMPLATES[$template]}"
        ((index++))
    done
}

# Crear proyecto desde template
create_project() {
    local project_name="$1"
    local template_type="$2"
    local project_path="$WORKSPACE_ROOT/$project_name"
    
    if [[ -z "$project_name" ]]; then
        print_error "Nombre del proyecto requerido"
        return 1
    fi
    
    if [[ -z "$template_type" ]]; then
        template_type="esp32c6_basic"
    fi
    
    if [[ -d "$project_path" ]]; then
        print_error "El proyecto '$project_name' ya existe"
        return 1
    fi
    
    print_header "CREANDO PROYECTO: $project_name"
    print_info "Template: $template_type"
    print_info "Ubicación: $project_path"
    
    # Crear directorio del proyecto
    mkdir -p "$project_path"
    
    # Copiar template base
    case $template_type in
        "esp32c6_basic")
            create_esp32c6_project "$project_path" "$project_name"
            ;;
        "esp32_basic")
            create_esp32_project "$project_path" "$project_name"
            ;;
        "i2c_scanner")
            create_i2c_scanner_project "$project_path" "$project_name"
            ;;
        "serial_bridge")
            create_serial_bridge_project "$project_path" "$project_name"
            ;;
        *)
            print_error "Template '$template_type' no reconocido"
            rm -rf "$project_path"
            return 1
            ;;
    esac
    
    # Crear archivos adicionales
    create_project_files "$project_path" "$project_name" "$template_type"
    
    print_success "Proyecto '$project_name' creado exitosamente"
    print_info "Para compilar: cd $project_name && ../build_tools/master_build.sh compile $project_name"
}

# Crear proyecto ESP32-C6 básico
create_esp32c6_project() {
    local project_path="$1"
    local project_name="$2"
    
    # Copiar template
    cp "$TEMPLATES_DIR/esp32c6_project_template.ino" "$project_path/$project_name.ino"
    
    # Actualizar nombre del proyecto en el código
    sed -i "s/PROJECT TEMPLATE/$project_name/g" "$project_path/$project_name.ino"
    
    print_success "Template ESP32-C6 aplicado"
}

# Crear proyecto ESP32 estándar
create_esp32_project() {
    local project_path="$1" 
    local project_name="$2"
    
    cat > "$project_path/$project_name.ino" << EOF
/*
 * LOCKNODE V6 - $project_name
 * Proyecto ESP32 estándar
 */

#define SERIAL_BAUDRATE 115200
#define LED_BUILTIN 2

void setup() {
    Serial.begin(SERIAL_BAUDRATE);
    pinMode(LED_BUILTIN, OUTPUT);
    
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║         LOCKNODE V6 - $project_name        ║");  
    Serial.println("╚═══════════════════════════════════════════╝");
    
    Serial.printf("Chip: %s\\n", ESP.getChipModel());
    Serial.printf("Frecuencia CPU: %d MHz\\n", ESP.getCpuFreqMHz());
    Serial.println("Sistema inicializado");
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    
    Serial.println("Sistema ejecutándose...");
}
EOF
    
    print_success "Template ESP32 aplicado"
}

# Crear proyecto scanner I2C
create_i2c_scanner_project() {
    local project_path="$1"
    local project_name="$2"
    
    cat > "$project_path/$project_name.ino" << 'EOF'
/*
 * LOCKNODE V6 - I2C SCANNER
 * Scanner I2C con soporte para TCA9545A
 */

#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22
#define TCA9545A_ADDR 0x70

void setup() {
    Serial.begin(115200);
    Wire.begin(I2C_SDA, I2C_SCL);
    
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║        LOCKNODE V6 - I2C SCANNER         ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    
    Serial.println("Comandos disponibles:");
    Serial.println("'scan' - Escanear bus I2C");
    Serial.println("'tca'  - Escanear TCA9545A");
}

void loop() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toLowerCase();
        
        if (command == "scan") {
            scanI2C();
        } else if (command == "tca") {
            scanTCA9545A();
        }
    }
}

void scanI2C() {
    Serial.println("Escaneando bus I2C...");
    int devices = 0;
    
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Dispositivo encontrado: 0x%02X\n", addr);
            devices++;
        }
    }
    
    if (devices == 0) {
        Serial.println("No se encontraron dispositivos");
    } else {
        Serial.printf("Total: %d dispositivos\n", devices);
    }
}

void scanTCA9545A() {
    Serial.println("Escaneando TCA9545A...");
    
    for (int channel = 0; channel < 4; channel++) {
        // Seleccionar canal
        Wire.beginTransmission(TCA9545A_ADDR);
        Wire.write(1 << channel);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Canal %d activo\n", channel);
            scanI2C();
        }
    }
}
EOF
    
    print_success "Template I2C Scanner aplicado"
}

# Crear proyecto serial bridge
create_serial_bridge_project() {
    local project_path="$1"
    local project_name="$2"
    
    cat > "$project_path/$project_name.ino" << 'EOF'
/*
 * LOCKNODE V6 - SERIAL BRIDGE
 * Puente serie con sistema de comandos
 */

String inputBuffer = "";
bool commandReady = false;

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX2=16, TX2=17
    
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║       LOCKNODE V6 - SERIAL BRIDGE        ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    
    Serial.println("Comandos:");
    Serial.println("'bridge' - Modo puente transparente");
    Serial.println("'test'   - Enviar test a Serial2");
    Serial.println("'info'   - Información del sistema");
}

void loop() {
    // Procesar comandos del Serial principal
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            commandReady = true;
        } else {
            inputBuffer += c;
        }
    }
    
    if (commandReady) {
        processCommand(inputBuffer);
        inputBuffer = "";
        commandReady = false;
    }
    
    // Reenviar datos de Serial2 a Serial
    while (Serial2.available()) {
        Serial.write(Serial2.read());
    }
}

void processCommand(String cmd) {
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd == "bridge") {
        Serial.println("Modo puente activo. Ctrl+C para salir");
        bridgeMode();
    } else if (cmd == "test") {
        Serial2.println("TEST MESSAGE FROM ESP32");
        Serial.println("Test enviado a Serial2");
    } else if (cmd == "info") {
        showInfo();
    } else {
        Serial.println("Comando desconocido: " + cmd);
    }
}

void bridgeMode() {
    while (true) {
        // Serial -> Serial2
        while (Serial.available()) {
            Serial2.write(Serial.read());
        }
        
        // Serial2 -> Serial
        while (Serial2.available()) {
            Serial.write(Serial2.read());
        }
        
        // Salir con Ctrl+C (ASCII 3)
        if (Serial.available() && Serial.peek() == 3) {
            Serial.read();
            break;
        }
        
        delay(1);
    }
    Serial.println("\nSaliendo del modo puente");
}

void showInfo() {
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("CPU: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Memoria libre: %d bytes\n", ESP.getFreeHeap());
    Serial.println("Serial: 115200 bps");
    Serial.println("Serial2: 115200 bps (GPIO16/17)");
}
EOF
    
    print_success "Template Serial Bridge aplicado"
}

# Crear archivos adicionales del proyecto
create_project_files() {
    local project_path="$1"
    local project_name="$2"
    local template_type="$3"
    
    # README.md
    cat > "$project_path/README.md" << EOF
# $project_name

Proyecto generado automáticamente con LOCKNODE V6 Build Tools.

## Información del Proyecto
- **Nombre**: $project_name
- **Template**: $template_type
- **Fecha de creación**: $(date)
- **Generado por**: LOCKNODE V6 Project Generator

## Compilación

\`\`\`bash
# Compilar solamente
../build_tools/master_build.sh compile $project_name

# Compilar y cargar
../build_tools/master_build.sh upload $project_name

# Workflow completo
../build_tools/master_build.sh full $project_name
\`\`\`

## Estructura del Proyecto

\`\`\`
$project_name/
├── $project_name.ino    # Código principal
├── README.md           # Este archivo
└── platformio.ini      # Configuración PlatformIO (opcional)
\`\`\`

## Comandos Disponibles

Ejecuta el script master_build.sh desde el directorio raíz para ver todas las opciones disponibles.
EOF

    # platformio.ini (opcional)
    if [[ "$template_type" == *"esp32c6"* ]]; then
        cat > "$project_path/platformio.ini" << EOF
[env:esp32c6]
platform = espressif32
board = esp32c6dev
framework = arduino
monitor_speed = 115200
build_flags = 
    -DESP32C6=1
    -DARDUINO_ESP32C6_DEV=1
    -DCONFIG_IDF_TARGET_ESP32C6=1
    -DESP32C6_JTAG_ENABLED=1
EOF
    else
        cat > "$project_path/platformio.ini" << EOF
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
EOF
    fi
    
    print_success "Archivos del proyecto creados"
}

# Mostrar ayuda
show_help() {
    print_header "LOCKNODE V6 - PROJECT GENERATOR"
    echo
    echo "Uso: $0 [comando] [opciones]"
    echo
    echo "Comandos:"
    echo "  create <nombre> [template]  Crear nuevo proyecto"
    echo "  list                        Listar templates disponibles"  
    echo "  help                        Mostrar esta ayuda"
    echo
    echo "Templates disponibles:"
    for template in "${!TEMPLATES[@]}"; do
        echo "  - $template: ${TEMPLATES[$template]}"
    done
    echo
    echo "Ejemplos:"
    echo "  $0 create mi_proyecto esp32c6_basic"
    echo "  $0 create scanner i2c_scanner"
    echo "  $0 list"
}

# Función principal
main() {
    case "${1:-help}" in
        "create")
            create_project "$2" "$3"
            ;;
        "list")
            list_templates
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
