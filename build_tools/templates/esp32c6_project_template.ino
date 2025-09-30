/*
 * LOCKNODE V6 - ESP32-C6 PROJECT TEMPLATE
 * Template básico para nuevos proyectos ESP32-C6
 * Incluye configuración CDC-JTAG y funciones básicas
 */

#ifdef ESP32C6_JTAG_ENABLED
#include "hal/usb_serial_jtag_ll.h"
#endif

// Configuración básica
#define SERIAL_BAUDRATE 115200
#define LED_BUILTIN 8  // ESP32-C6 built-in LED

// Variables globales
String inputCommand = "";
bool commandReady = false;

void setup() {
    // Inicializar comunicación serie
    Serial.begin(SERIAL_BAUDRATE);
    
    // Configurar LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Esperar conexión serie
    while (!Serial && millis() < 5000) {
        delay(100);
    }
    
    // Mostrar información inicial
    showProjectInfo();
    showAvailableCommands();
    
    Serial.println("Sistema inicializado correctamente");
    Serial.println("Ingresa comando:");
}

void loop() {
    // Procesar comandos serie
    processSerialInput();
    
    if (commandReady) {
        processCommand(inputCommand);
        inputCommand = "";
        commandReady = false;
        Serial.println("\nIngresa comando:");
    }
    
    // Parpadeo LED para mostrar que está vivo
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        lastBlink = millis();
    }
}

void processSerialInput() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (inputCommand.length() > 0) {
                commandReady = true;
                inputCommand.trim();
            }
        } else {
            inputCommand += c;
        }
    }
}

void processCommand(String command) {
    command.toLowerCase();
    
    if (command == "help") {
        showAvailableCommands();
        
    } else if (command == "info") {
        showProjectInfo();
        
    } else if (command == "status") {
        showSystemStatus();
        
#ifdef ESP32C6_JTAG_ENABLED
    } else if (command == "jtag") {
        showJTAGInfo();
#endif
        
    } else if (command == "reset") {
        Serial.println("Reiniciando sistema...");
        delay(1000);
        ESP.restart();
        
    } else if (command == "blink") {
        Serial.println("Test de LED...");
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
            digitalWrite(LED_BUILTIN, LOW);
            delay(200);
        }
        Serial.println("Test completado");
        
    } else {
        Serial.println("Comando desconocido: " + command);
        Serial.println("Usa 'help' para ver comandos disponibles");
    }
}

void showProjectInfo() {
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║     LOCKNODE V6 - PROJECT TEMPLATE       ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("Revisión: %d\n", ESP.getChipRevision());
    Serial.printf("Frecuencia CPU: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Memoria Flash: %d bytes\n", ESP.getFlashChipSize());
    Serial.printf("Memoria libre: %d bytes\n", ESP.getFreeHeap());
    
#ifdef ESP32C6_JTAG_ENABLED
    Serial.println("JTAG: CDC-JTAG Integrado habilitado");
#else
    Serial.println("JTAG: No habilitado");
#endif
    
    Serial.println("═══════════════════════════════════════════");
}

void showAvailableCommands() {
    Serial.println("\n📋 COMANDOS DISPONIBLES:");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("'help'   - Mostrar esta ayuda");
    Serial.println("'info'   - Información del sistema");
    Serial.println("'status' - Estado del sistema");
    Serial.println("'reset'  - Reiniciar sistema");
    Serial.println("'blink'  - Test del LED");
    
#ifdef ESP32C6_JTAG_ENABLED
    Serial.println("'jtag'   - Información JTAG");
#endif
    
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
}

void showSystemStatus() {
    Serial.println("📊 ESTADO DEL SISTEMA:");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.printf("Uptime: %lu ms\n", millis());
    Serial.printf("Memoria libre: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Temperatura: %.1f°C\n", temperatureRead());
    Serial.printf("Voltaje: %.2fV\n", analogReadMilliVolts(A0) / 1000.0);
    Serial.printf("LED Estado: %s\n", digitalRead(LED_BUILTIN) ? "ON" : "OFF");
    
    // Estado del puerto serie
    Serial.println("Puerto serie: Conectado");
    Serial.printf("Baudrate: %d\n", SERIAL_BAUDRATE);
    
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━");
}

#ifdef ESP32C6_JTAG_ENABLED
void showJTAGInfo() {
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║       CDC-JTAG INTEGRADO ESP32-C6        ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    Serial.println("JTAG: USB-Serial-JTAG Controller integrado");
    Serial.println("CARACTERÍSTICAS:");
    Serial.println("  • Puerto serie y JTAG en un solo USB");
    Serial.println("  • No requiere hardware externo");
    Serial.println("  • Debugging nativo sin adaptadores");
    Serial.println("  • Switching automático entre modos");
    
    Serial.println("\nINFORMACIÓN DEL CHIP:");
    Serial.printf("  • Modelo: %s\n", ESP.getChipModel());
    Serial.printf("  • Revisión: %d\n", ESP.getChipRevision());
    Serial.printf("  • Frecuencia CPU: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("  • Memoria Flash: %d bytes\n", ESP.getFlashChipSize());
    
    Serial.println("\nVENTAJAS CDC-JTAG:");
    Serial.println("  ✓ Un solo cable USB para todo");
    Serial.println("  ✓ Debugging sin hardware adicional");
    Serial.println("  ✓ Puerto serie siempre disponible");
    Serial.println("  ✓ Compatible con OpenOCD nativo");
    
    Serial.println("\nCOMANDOS PARA DEBUGGING:");
    Serial.println("  openocd -f board/esp32c6-builtin.cfg");
    Serial.println("  (gdb) target remote :3333");
    Serial.println("═══════════════════════════════════════════");
}
#endif
