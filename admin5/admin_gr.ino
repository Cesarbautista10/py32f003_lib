#include <Wire.h>

// ============================================================
//         I2C DEVICE MANAGER COMPLETO (admin_gr)
//    Combina funcionalidades de admin_ir + admin5
//    - Gestión de direcciones I2C (cambio, factory reset)
//    - Test continuo con estadísticas
//    - Comando completo de dispositivos PY32F003
// ============================================================

// Comandos básicos para dispositivos PY32F003
#define CMD_RELAY_OFF      0x00   // Apagar relé
#define CMD_RELAY_ON       0x01   // Encender relé
#define CMD_NEO_RED        0x02   // NeoPixels rojos
#define CMD_NEO_GREEN      0x03   // NeoPixels verdes
#define CMD_NEO_BLUE       0x04   // NeoPixels azules
#define CMD_NEO_OFF        0x05   // Apagar NeoPixels
#define CMD_TOGGLE         0x06   // Toggle relé
#define CMD_PA4_DIGITAL    0x07   // Leer PA4 digital

// Comandos I2C management
#define CMD_SET_I2C_ADDR   0x3D   // Establecer nueva dirección I2C
#define CMD_RESET_FACTORY  0x3E   // Reset factory (usar UID por defecto)
#define CMD_GET_I2C_STATUS 0x3F   // Obtener estado I2C (Flash vs UID)

// Comandos ADC
#define CMD_ADC_PA0_HSB    0xD8   // ADC PA0 High bits
#define CMD_ADC_PA0_LSB    0xD9   // ADC PA0 Low bits
#define CMD_ADC_PA0_I2C    0xDA   // ADC PA0 escalado a dirección I2C

// Comandos de sistema
#define CMD_RESET          0xFE   // Reset inmediato del sistema
#define CMD_WATCHDOG_RESET 0xFF   // Reset por watchdog

// Estructura completa de dispositivo con estadísticas
struct DeviceInfo {
  uint8_t address;
  bool is_compatible;
  bool is_online;
  String status;
  
  // Estadísticas de test continuo
  uint32_t total_tests;
  uint32_t successful_tests;
  uint32_t failed_tests;
  uint32_t last_success_time;
  uint32_t last_failure_time;
  uint8_t consecutive_failures;
  uint16_t last_adc_value;
  uint8_t last_pa4_state;
};

// Variables globales
DeviceInfo found_devices[32];  // Máximo 32 dispositivos
int device_count = 0;
bool continuous_test = false;
uint32_t cycle_count = 0;
uint32_t total_bus_recoveries = 0;
uint32_t test_interval = 2000;  // 2 segundos entre ciclos
uint32_t command_delay = 50;    // 50ms entre comandos

// Modos de test continuo
enum TestMode {
  TEST_MODE_PA4_ONLY,      // Solo lectura PA4
  TEST_MODE_TOGGLE_ONLY,   // Solo toggle
  TEST_MODE_BOTH           // Toggle + PA4
};
TestMode current_test_mode = TEST_MODE_PA4_ONLY;

// Declaraciones de función - Gestión básica
void scanDevices();
void listDevices();
void pingDevice(uint8_t address);
void showMenu();
void processCommand(String cmd);
uint8_t parseAddress(String addr_str);
bool checkCompatibility(uint8_t address);

// Declaraciones de función - Gestión de direcciones I2C
void changeI2CAddress(uint8_t old_addr, uint8_t new_addr);
void factoryReset(uint8_t address);
void checkI2CStatus(uint8_t address);
void resetDevice(uint8_t address);
void verifyAddressChange(uint8_t old_addr, uint8_t new_addr);

// Declaraciones de función - Test continuo
void startContinuousTest();
void stopContinuousTest();
void runContinuousTest();
void testDevice(uint8_t index);
void showStatistics();
void resetStatistics();

// Declaraciones de función - Comandos de dispositivo
bool sendBasicCommand(uint8_t address, uint8_t command);
uint16_t readADC(uint8_t address);
uint8_t readPA4Digital(uint8_t address);
void testAllCommands(uint8_t address);

// Lectura robusta con reintentos
bool i2cReadByteWithRetry(uint8_t address, uint8_t command, uint8_t* out,
                          uint8_t attempts = 2, uint32_t wait_ms = 40);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Inicializar I2C con configuración optimizada
  Wire.begin(21, 22);  // ESP32 (clásico): SDA=GPIO21, SCL=GPIO22. Para ESP32-C3 usa Wire.begin(6, 7)
  Wire.setTimeout(100);
  Wire.setClock(100000);  // 100kHz para mayor estabilidad
  
  printHeader();
  printMenu();
  
  // Realizar scan inicial automático
  Serial.println(">> Realizando scan inicial automatico...\r\n");
  scanDevices();
  
  Serial.println("NOTA: Usa 'm' para ver menu completo o 'help' para ayuda\r\n");
}

void loop() {
  // Procesar test continuo si está activo
  if (continuous_test) {
    runContinuousTest();
    delay(test_interval);
  }
  
  // Procesar comandos del usuario
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd.length() > 0) {
      Serial.printf("CMD: %s\r\n", cmd.c_str());
      processCommand(cmd);
    }
  }
  
  delay(10);
}

void printHeader() {
  Serial.println("\r\n===========================================");
  Serial.println("    I2C DEVICE MANAGER COMPLETO (GR)    ");
  Serial.println("  Gestion + Test Continuo + Comandos    ");
  Serial.println("===========================================");
  Serial.println("I2C: SDA=GPIO6, SCL=GPIO7, Clock=100kHz\r\n");
}

void printMenu() {
  Serial.println("COMANDOS DISPONIBLES:");
  Serial.println("===============================================");
  Serial.println("=== GESTION BASICA ===");
  Serial.println("1. 's'              = Escanear dispositivos I2C");
  Serial.println("2. 'l'              = Ver dispositivos encontrados");
  Serial.println("3. 'p XX'           = Ping a dispositivo");
  Serial.println("4. 'stat'           = Ver estadisticas detalladas");
  Serial.println("");
  Serial.println("=== GESTION DE DIRECCIONES ===");
  Serial.println("5. 'c XX YY'        = Cambiar direccion XX->YY");
  Serial.println("6. 'f XX'           = Factory reset del dispositivo");
  Serial.println("7. 'st XX'          = Estado I2C del dispositivo");
  Serial.println("8. 'r XX'           = Reiniciar dispositivo");
  Serial.println("9. 'vc XX YY'       = Verificar cambio XX->YY");
  Serial.println("");
  Serial.println("=== TEST CONTINUO ===");
  Serial.println("10. 'start'         = Iniciar test (modo actual)");
  Serial.println("11. 'start pa4'     = Test solo lectura PA4");
  Serial.println("12. 'start toggle'  = Test solo toggle");
  Serial.println("13. 'start both'    = Test toggle + PA4");
  Serial.println("14. 'stop'          = Detener test continuo");
  Serial.println("15. 'interval XX'   = Cambiar intervalo (ms)");
  Serial.println("16. 'reset-stats'   = Resetear estadisticas");
  Serial.println("");
  Serial.println("=== COMANDOS DE DISPOSITIVO ===");
  Serial.println("17. 'relay XX on'   = Encender rele");
  Serial.println("18. 'relay XX off'  = Apagar rele");
  Serial.println("19. 'toggle XX'     = Toggle rele (pulso)");
  Serial.println("20. 'neo XX red'    = NeoPixels rojos");
  Serial.println("21. 'neo XX green'  = NeoPixels verdes");
  Serial.println("22. 'neo XX blue'   = NeoPixels azules");
  Serial.println("23. 'neo XX off'    = Apagar NeoPixels");
  Serial.println("24. 'adc XX'        = Leer ADC PA0");
  Serial.println("25. 'pa4 XX'        = Leer PA4 digital");
  Serial.println("26. 'test XX'       = Test todos los comandos");
  Serial.println("");
  Serial.println("27. 'm' / 'menu'    = Mostrar este menu");
  Serial.println("28. 'help'          = Ayuda detallada");
  Serial.println("===============================================");
  Serial.println("Ejemplos:");
  Serial.println("  s                 (escanear)");
  Serial.println("  p 32              (ping a 0x20)");
  Serial.println("  c 32 48           (cambiar 0x20->0x30)");
  Serial.println("  relay 32 on       (encender rele en 0x20)");
  Serial.println("  start pa4         (test solo lectura PA4)");
  Serial.println("  start toggle      (test solo toggle)");
  Serial.println("  start both        (test toggle + PA4)");
  Serial.println("===============================================\r\n");
}

void processCommand(String cmd) {
  cmd.trim();
  
  // Comandos básicos de gestión
  if (cmd == "s" || cmd == "scan") {
    scanDevices();
    
  } else if (cmd == "l" || cmd == "list") {
    listDevices();
    
  } else if (cmd == "m" || cmd == "menu") {
    printMenu();
    
  } else if (cmd == "stat" || cmd == "statistics") {
    showStatistics();
    
  } else if (cmd == "help") {
    printHelp();
    
  // Test continuo
  } else if (cmd == "start" || cmd == "start pa4") {
    current_test_mode = TEST_MODE_PA4_ONLY;
    startContinuousTest();
    
  } else if (cmd == "start toggle") {
    current_test_mode = TEST_MODE_TOGGLE_ONLY;
    startContinuousTest();
    
  } else if (cmd == "start both") {
    current_test_mode = TEST_MODE_BOTH;
    startContinuousTest();
    
  } else if (cmd == "stop") {
    stopContinuousTest();
    
  } else if (cmd == "reset-stats") {
    resetStatistics();
    
  } else if (cmd.startsWith("interval ")) {
    String interval_str = cmd.substring(9);
    uint32_t new_interval = interval_str.toInt();
    if (new_interval >= 100 && new_interval <= 60000) {
      test_interval = new_interval;
      Serial.printf("[OK] Intervalo cambiado a %d ms\r\n", test_interval);
    } else {
      Serial.println("ERROR: Intervalo debe estar entre 100-60000 ms\r");
    }
    
  // Comandos con direcciones
  } else if (cmd.startsWith("p ")) {
    String addr_str = cmd.substring(2);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      pingDevice(address);
    }
    
  } else if (cmd.startsWith("c ")) {
    parseChangeCommand(cmd);
    
  } else if (cmd.startsWith("f ")) {
    String addr_str = cmd.substring(2);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      factoryReset(address);
    }
    
  } else if (cmd.startsWith("st ")) {
    String addr_str = cmd.substring(3);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      checkI2CStatus(address);
    }
    
  } else if (cmd.startsWith("r ")) {
    String addr_str = cmd.substring(2);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      resetDevice(address);
    }
    
  } else if (cmd.startsWith("vc ")) {
    parseVerifyCommand(cmd);
    
  // Comandos de dispositivo
  } else if (cmd.startsWith("relay ")) {
    parseRelayCommand(cmd);
    
  } else if (cmd.startsWith("toggle ")) {
    String addr_str = cmd.substring(7);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      if (sendBasicCommand(address, CMD_TOGGLE)) {
        Serial.printf("[OK] Toggle ejecutado en 0x%02X\r\n", address);
      } else {
        Serial.printf("ERROR: No se pudo ejecutar toggle en 0x%02X\r\n", address);
      }
    }
    
  } else if (cmd.startsWith("neo ")) {
    parseNeoCommand(cmd);
    
  } else if (cmd.startsWith("adc ")) {
    String addr_str = cmd.substring(4);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      uint16_t adc_value = readADC(address);
      Serial.printf("[ADC] Dispositivo 0x%02X: %d (0x%03X)\r\n", address, adc_value, adc_value);
    }
    
  } else if (cmd.startsWith("pa4 ")) {
    String addr_str = cmd.substring(4);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      uint8_t pa4_state = readPA4Digital(address);
      Serial.printf("[PA4] Dispositivo 0x%02X: %s (%d)\r\n", 
                   address, pa4_state ? "HIGH" : "LOW", pa4_state);
    }
    
  } else if (cmd.startsWith("test ")) {
    String addr_str = cmd.substring(5);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      testAllCommands(address);
    }
    
  } else if (cmd.length() > 0) {
    Serial.println("ERROR: Comando no reconocido. Usa 'm' para ver comandos disponibles.");
  }
  
  Serial.println("");
}

void scanDevices() {
  Serial.println(">> ESCANEANDO BUS I2C...");
  Serial.println("Rango: 0x08-0x77 (8-119 decimal)");
  Serial.println("===========================================");
  
  device_count = 0;
  bool found_any = false;
  
  // Limpiar array de dispositivos
  memset(found_devices, 0, sizeof(found_devices));
  
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      found_any = true;
      
      // Verificar compatibilidad
      bool compatible = checkCompatibility(addr);
      
      // Guardar en array
      if (device_count < 32) {
        found_devices[device_count].address = addr;
        found_devices[device_count].is_compatible = compatible;
        found_devices[device_count].is_online = true;
        found_devices[device_count].status = compatible ? "Compatible PY32F003" : "Generico";
        found_devices[device_count].total_tests = 0;
        found_devices[device_count].successful_tests = 0;
        found_devices[device_count].failed_tests = 0;
        found_devices[device_count].consecutive_failures = 0;
        device_count++;
      }
      
      // Mostrar resultado
      if (compatible) {
        Serial.printf("[OK] 0x%02X (%3d) - COMPATIBLE (PY32F003)\r\n", addr, addr);
      } else {
        Serial.printf("[DEV] 0x%02X (%3d) - Generico\r\n", addr, addr);
      }
    }
    
    delay(5);
  }
  
  Serial.println("===========================================");
  
  if (!found_any) {
    Serial.println("ERROR: No se encontraron dispositivos I2C");
    Serial.println("NOTA: Verificar conexiones SDA/SCL y alimentacion");
  } else {
    Serial.printf("TOTAL: %d dispositivos encontrados\r\n", device_count);
    
    int compatible_count = 0;
    for (int i = 0; i < device_count; i++) {
      if (found_devices[i].is_compatible) {
        compatible_count++;
      }
    }
    Serial.printf("COMPATIBLE: %d dispositivos PY32F003\r\n", compatible_count);
  }
  
  Serial.println("");
}

bool checkCompatibility(uint8_t address) {
  // Enviar comando PA4_DIGITAL para verificar compatibilidad
  Wire.beginTransmission(address);
  Wire.write(CMD_PA4_DIGITAL);
  
  if (Wire.endTransmission() == 0) {
    delay(50);
    Wire.requestFrom(address, (uint8_t)1);
    
    if (Wire.available()) {
      uint8_t response = Wire.read();
      // Un dispositivo compatible debe responder algo diferente a 0x00 o 0xFF
      return (response != 0x00 && response != 0xFF);
    }
  }
  
  return false;
}

void listDevices() {
  Serial.println("DISPOSITIVOS ENCONTRADOS:");
  Serial.println("===========================================");
  
  if (device_count == 0) {
    Serial.println("ERROR: No hay dispositivos. Usa 'scan' primero.");
    return;
  }
  
  Serial.println("+----------+--------+-------------+----------+----------+");
  Serial.println("| Direccion| Estado | Descripcion | Exitos   | Fallos   |");
  Serial.println("+----------+--------+-------------+----------+----------+");
  
  for (int i = 0; i < device_count; i++) {
    String icon = found_devices[i].is_compatible ? "[OK]" : "[DEV]";
    String status = found_devices[i].is_online ? "ONLINE" : "OFFLINE";
    
    Serial.printf("|%s 0x%02X | %-6s | %-11s | %-8d | %-8d |\r\n",
      icon.c_str(),
      found_devices[i].address,
      status.c_str(),
      found_devices[i].is_compatible ? "PY32F003" : "Generico",
      found_devices[i].successful_tests,
      found_devices[i].failed_tests
    );
  }
  
  Serial.println("+----------+--------+-------------+----------+----------+");
  Serial.println("");
}

void pingDevice(uint8_t address) {
  Serial.printf("PING: 0x%02X (%d decimal)\r\n", address, address);
  
  Wire.beginTransmission(address);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.printf("[OK] Dispositivo responde\r\n");
    
    if (checkCompatibility(address)) {
      Serial.printf("COMPATIBLE: PY32F003 firmware\r\n");
    } else {
      Serial.printf("DISPOSITIVO: Generico (no compatible)\r\n");
    }
  } else {
    Serial.printf("ERROR: No responde (Error: %d)\r\n", error);
  }
}

uint8_t parseAddress(String addr_str) {
  addr_str.trim();
  addr_str.toLowerCase();
  
  uint8_t address = 0;
  
  if (addr_str.startsWith("0x")) {
    address = strtol(addr_str.c_str() + 2, NULL, 16);
  } else if (addr_str.endsWith("h")) {
    String hex_part = addr_str.substring(0, addr_str.length() - 1);
    address = strtol(hex_part.c_str(), NULL, 16);
  } else {
    address = addr_str.toInt();
  }
  
  if (address < 0x08 || address > 0x77) {
    Serial.printf("ERROR: Direccion 0x%02X fuera de rango (0x08-0x77)\r\n", address);
    return 0;
  }
  
  return address;
}

// Continuaré con las funciones de gestión de direcciones I2C en la siguiente parte...

void printHelp() {
  Serial.println("AYUDA DETALLADA - I2C DEVICE MANAGER:");
  Serial.println("===============================================");
  Serial.println("FORMATOS DE DIRECCION:");
  Serial.println("  Decimal:     32");
  Serial.println("  Hexadecimal: 0x20, 20h, 20");
  Serial.println("");
  Serial.println("GESTION BASICA:");
  Serial.println("  s              - Escanea bus I2C (0x08-0x77)");
  Serial.println("  l              - Lista dispositivos encontrados");
  Serial.println("  p 32           - Ping a dispositivo en direccion 32");
  Serial.println("");
  Serial.println("CAMBIO DE DIRECCIONES:");
  Serial.println("  c 32 48        - Cambiar dispositivo 32 -> 48");
  Serial.println("  f 48           - Factory reset (usar UID)");
  Serial.println("  st 32          - Estado I2C (Flash/UID)");
  Serial.println("  r 32           - Reiniciar dispositivo");
  Serial.println("");
  Serial.println("TEST CONTINUO:");
  Serial.println("  start pa4      - Test solo lectura PA4");
  Serial.println("  start toggle   - Test solo toggle");
  Serial.println("  start both     - Test toggle + PA4");
  Serial.println("  stop           - Detiene test automatico");
  Serial.println("  interval 1000  - Cambia intervalo a 1000ms");
  Serial.println("");
  Serial.println("COMANDOS DE DISPOSITIVO:");
  Serial.println("  relay 32 on    - Enciende rele");
  Serial.println("  relay 32 off   - Apaga rele");
  Serial.println("  toggle 32      - Toggle rele (pulso)");
  Serial.println("  neo 32 red     - NeoPixels rojos");
  Serial.println("  adc 32         - Lee ADC PA0");
  Serial.println("  pa4 32         - Lee entrada digital PA4");
  Serial.println("===============================================");
}

// Funciones de gestión de direcciones I2C (copiadas de admin_ir)
void changeI2CAddress(uint8_t old_addr, uint8_t new_addr) {
  Serial.printf("CAMBIO DE DIRECCION I2C: 0x%02X -> 0x%02X\r\n", old_addr, new_addr);
  
  // Verificar que el dispositivo origen existe y es compatible
  if (!checkCompatibility(old_addr)) {
    Serial.printf("ERROR: Dispositivo 0x%02X no es compatible\r\n", old_addr);
    return;
  }
  
  // Paso 1: Activar modo cambio de dirección
  Wire.beginTransmission(old_addr);
  Wire.write(CMD_SET_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: No se pudo activar modo cambio");
    return;
  }
  
  delay(100);
  
  // Paso 2: Enviar nueva dirección
  Wire.beginTransmission(old_addr);
  Wire.write(new_addr);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: No se pudo enviar nueva direccion");
    return;
  }
  
  delay(200);  // Tiempo para guardar en Flash
  
  // Paso 3: Reset del dispositivo
  Wire.beginTransmission(old_addr);
  Wire.write(CMD_RESET);
  Wire.endTransmission();  // Ignorar error porque el dispositivo se resetea
  
  Serial.println("Esperando reset del dispositivo...");
  delay(3000);
  
  // Verificar cambio
  Wire.beginTransmission(new_addr);
  if (Wire.endTransmission() == 0) {
    Serial.printf("[OK] CAMBIO EXITOSO: Dispositivo ahora en 0x%02X\r\n", new_addr);
  } else {
    Serial.println("[INFO] Comando enviado. Ejecutar 's' para verificar");
  }
}

void factoryReset(uint8_t address) {
  Serial.printf("FACTORY RESET: 0x%02X -> direccion UID\r\n", address);
  
  Wire.beginTransmission(address);
  Wire.write(CMD_RESET_FACTORY);
  if (Wire.endTransmission() == 0) {
    Serial.println("[OK] Comando factory reset enviado");
    Serial.println("NOTA: Ejecutar 's' para escanear nueva direccion");
  } else {
    Serial.println("ERROR: No se pudo enviar comando factory reset");
  }
}

void checkI2CStatus(uint8_t address) {
  Serial.printf("ESTADO I2C: 0x%02X\r\n", address);
  
  Wire.beginTransmission(address);
  Wire.write(CMD_GET_I2C_STATUS);
  
  if (Wire.endTransmission() == 0) {
    delay(50);
    Wire.requestFrom(address, (uint8_t)1);
    
    if (Wire.available()) {
      uint8_t status = Wire.read();
      uint8_t status_clean = status & 0x0F;
      
      Serial.printf("Estado: 0x%02X -> ", status_clean);
      if (status_clean == 0x0F) {
        Serial.println("FLASH (direccion personalizada)");
      } else if (status_clean == 0x0A) {
        Serial.println("UID (direccion factory)");
      } else {
        Serial.println("DESCONOCIDO");
      }
    }
  } else {
    Serial.println("ERROR: No se pudo obtener estado");
  }
}

void resetDevice(uint8_t address) {
  Serial.printf("RESET DISPOSITIVO: 0x%02X\r\n", address);
  
  Wire.beginTransmission(address);
  Wire.write(CMD_RESET);
  Wire.endTransmission();  // Ignorar error
  
  Serial.println("[OK] Comando de reset enviado");
  Serial.println("SUGERENCIA: Ejecutar 's' para verificar");
}

// Funciones de test continuo
void startContinuousTest() {
  if (device_count == 0) {
    Serial.println("ERROR: No hay dispositivos. Ejecutar 'scan' primero");
    return;
  }
  
  continuous_test = true;
  cycle_count = 0;
  
  String mode_name = "";
  switch(current_test_mode) {
    case TEST_MODE_PA4_ONLY:
      mode_name = "Solo lectura PA4";
      break;
    case TEST_MODE_TOGGLE_ONLY:
      mode_name = "Solo toggle";
      break;
    case TEST_MODE_BOTH:
      mode_name = "Toggle + PA4";
      break;
  }
  
  Serial.printf("[OK] Test continuo iniciado - Modo: %s\r\n", mode_name.c_str());
  Serial.printf("     Dispositivos: %d, Intervalo: %dms\r\n", device_count, test_interval);
}

void stopContinuousTest() {
  continuous_test = false;
  Serial.printf("[OK] Test continuo detenido (ciclos ejecutados: %d)\r\n", cycle_count);
}

void runContinuousTest() {
  cycle_count++;
  Serial.printf("=== CICLO %d ===\r\n", cycle_count);
  
  for (int i = 0; i < device_count; i++) {
    if (found_devices[i].is_compatible) {
      testDevice(i);
    }
  }
  
  // Mostrar resumen cada 10 ciclos
  if (cycle_count % 10 == 0) {
    showStatistics();
  }
}

void testDevice(uint8_t index) {
  uint8_t addr = found_devices[index].address;
  found_devices[index].total_tests++;
  
  // Test básico de comunicación
  Wire.beginTransmission(addr);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    bool toggle_ok = false;
    uint8_t pa4_state = 0;
    
    // Ejecutar comandos según el modo seleccionado
    switch(current_test_mode) {
      case TEST_MODE_PA4_ONLY:
        // Solo lectura PA4
        pa4_state = readPA4Digital(addr);
        found_devices[index].last_pa4_state = pa4_state;
        Serial.printf("[%d] 0x%02X: OK (PA4:%s)\r\n", 
                     index, addr, 
                     pa4_state ? "HIGH" : "LOW");
        break;
        
      case TEST_MODE_TOGGLE_ONLY:
        // Solo toggle
        toggle_ok = sendBasicCommand(addr, CMD_TOGGLE);
        Serial.printf("[%d] 0x%02X: OK (Toggle:%s)\r\n", 
                     index, addr, 
                     toggle_ok ? "OK" : "FAIL");
        break;
        
      case TEST_MODE_BOTH:
        // Toggle + PA4
        toggle_ok = sendBasicCommand(addr, CMD_TOGGLE);
        delay(100);  // Delay para estabilización después del toggle
        pa4_state = readPA4Digital(addr);
        found_devices[index].last_pa4_state = pa4_state;
        Serial.printf("[%d] 0x%02X: OK (Toggle:%s, PA4:%s)\r\n", 
                     index, addr, 
                     toggle_ok ? "OK" : "FAIL",
                     pa4_state ? "HIGH" : "LOW");
        break;
    }
    
    found_devices[index].successful_tests++;
    found_devices[index].last_success_time = millis();
    found_devices[index].consecutive_failures = 0;
    found_devices[index].is_online = true;
    
  } else {
    found_devices[index].failed_tests++;
    found_devices[index].last_failure_time = millis();
    found_devices[index].consecutive_failures++;
    found_devices[index].is_online = false;
    
    Serial.printf("[%d] 0x%02X: FAIL (Error:%d)\r\n", index, addr, error);
  }
}

void showStatistics() {
  Serial.println("\n=== ESTADISTICAS ===");
  Serial.printf("Ciclos ejecutados: %d\r\n", cycle_count);
  Serial.printf("Recuperaciones de bus: %d\r\n", total_bus_recoveries);
  
  for (int i = 0; i < device_count; i++) {
    if (found_devices[i].total_tests > 0) {
      float success_rate = (float)found_devices[i].successful_tests / found_devices[i].total_tests * 100.0;
      
      Serial.printf("0x%02X: %d tests, %.1f%% exito, %d fallos consecutivos\r\n",
                   found_devices[i].address,
                   found_devices[i].total_tests,
                   success_rate,
                   found_devices[i].consecutive_failures);
    }
  }
  Serial.println("==================\n");
}

void resetStatistics() {
  for (int i = 0; i < device_count; i++) {
    found_devices[i].total_tests = 0;
    found_devices[i].successful_tests = 0;
    found_devices[i].failed_tests = 0;
    found_devices[i].consecutive_failures = 0;
  }
  cycle_count = 0;
  total_bus_recoveries = 0;
  Serial.println("[OK] Estadisticas reseteadas");
}

// Funciones de comandos de dispositivo
bool sendBasicCommand(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(command);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    delay(command_delay);
    return true;
  }
  return false;
}

// Helper: lectura robusta de un byte con reintentos y pequeños delays
bool i2cReadByteWithRetry(uint8_t address, uint8_t command, uint8_t* out,
                          uint8_t attempts, uint32_t wait_ms) {
  for (uint8_t i = 0; i < attempts; i++) {
    Wire.beginTransmission(address);
    Wire.write(command);
    uint8_t err = Wire.endTransmission(true); // enviar STOP
    if (err == 0) {
      delay(wait_ms); // dar tiempo al esclavo para preparar la respuesta
      uint8_t req = Wire.requestFrom(address, (uint8_t)1, (uint8_t)true); // 1 byte, STOP
      if (req == 1 && Wire.available()) {
        *out = Wire.read();
        return true;
      }
    }
    // pequeño backoff antes de reintentar
    delay(10);
  }
  return false;
}

uint16_t readADC(uint8_t address) {
  uint16_t adc_value = 0;
  uint8_t byte_val = 0;

  // Leer HSB (bits 11-8 en nibble bajo; nibble alto puede contener PA4)
  if (i2cReadByteWithRetry(address, CMD_ADC_PA0_HSB, &byte_val, 3, 40)) {
    uint8_t hsb = byte_val & 0x0F; // limpiar PA4 de nibble alto
    adc_value = (hsb << 8);
  } else {
    // Fallo en HSB, retornar 0 para indicar lectura inválida
    return 0;
  }

  // Leer LSB (bits 7-0 completos)
  if (i2cReadByteWithRetry(address, CMD_ADC_PA0_LSB, &byte_val, 3, 40)) {
    adc_value |= byte_val;
  } else {
    // Si falla LSB, devolver solo HSB desplazado (de lo contrario 0)
    // Aquí preferimos indicar fallo completo
    return 0;
  }

  return adc_value;
}

uint8_t readPA4Digital(uint8_t address) {
  uint8_t byte_val = 0;
  if (!i2cReadByteWithRetry(address, CMD_PA4_DIGITAL, &byte_val, 3, 30)) {
    return 0; // fallo -> asumir LOW
  }
  // PA4 está en los bits superiores (7-4)
  return (byte_val & 0xF0) ? 1 : 0;
}

void testAllCommands(uint8_t address) {
  Serial.printf("TEST COMPLETO: 0x%02X\r\n", address);
  Serial.println("=========================");
  
  // // Test relé
  // Serial.print("Rele ON: ");
  // if (sendBasicCommand(address, CMD_RELAY_ON)) {
  //   Serial.println("[OK]");
  //   delay(500);
  //   Serial.print("Rele OFF: ");
  //   if (sendBasicCommand(address, CMD_RELAY_OFF)) {
  //     Serial.println("[OK]");
  //   } else {
  //     Serial.println("[FAIL]");
  //   }
  // } else {
  //   Serial.println("[FAIL]");
  // }
  
  // Test Toggle
  Serial.print("Toggle: ");
  if (sendBasicCommand(address, CMD_TOGGLE)) {
    Serial.println("[OK]");
    delay(100);
  } else {
    Serial.println("[FAIL]");
  }
  
  // Test NeoPixels
  Serial.print("Neo RED: ");
  if (sendBasicCommand(address, CMD_NEO_RED)) {
    Serial.println("[OK]");
    delay(500);
    Serial.print("Neo OFF: ");
    if (sendBasicCommand(address, CMD_NEO_OFF)) {
      Serial.println("[OK]");
    } else {
      Serial.println("[FAIL]");
    }
  } else {
    Serial.println("[FAIL]");
  }
  
  // // Test ADC
  // Serial.print("ADC PA0: ");
  // uint16_t adc = readADC(address);
  // Serial.printf("%d (0x%03X)\r\n", adc, adc);
  
  // Test PA4
  Serial.print("PA4 Digital: ");
  uint8_t pa4 = readPA4Digital(address);
  Serial.printf("%s (%d)\r\n", pa4 ? "HIGH" : "LOW", pa4);
  
  Serial.println("=========================");
}

// Funciones de parsing de comandos
void parseChangeCommand(String cmd) {
  // Acepta: "c OLD NEW" con uno o varios espacios, en decimal o hex (0xNN o NNh)
  // Ejemplos válidos: c 32 48 | c 0x20 0x30 | c 20h 30h | c   32    48
  String rest = cmd.substring(1); // quitar 'c'
  rest.trim();                    // "110 20"

  int sep = rest.indexOf(' ');
  if (sep == -1) {
    Serial.println("ERROR: Formato: c OLD_ADDR NEW_ADDR");
    return;
  }

  String old_str = rest.substring(0, sep);
  String new_str = rest.substring(sep + 1);
  old_str.trim();
  new_str.trim();

  if (new_str.length() == 0) {
    Serial.println("ERROR: Falta NEW_ADDR. Formato: c OLD_ADDR NEW_ADDR");
    return;
  }

  uint8_t old_addr = parseAddress(old_str);
  uint8_t new_addr = parseAddress(new_str);

  if (old_addr > 0 && new_addr > 0) {
    changeI2CAddress(old_addr, new_addr);
  }
}

void parseVerifyCommand(String cmd) {
  // Acepta: "vc OLD NEW" con uno o varios espacios
  String rest = cmd.substring(2); // quitar 'vc'
  rest.trim();

  int sep = rest.indexOf(' ');
  if (sep == -1) {
    Serial.println("ERROR: Formato: vc OLD_ADDR NEW_ADDR");
    return;
  }

  String old_str = rest.substring(0, sep);
  String new_str = rest.substring(sep + 1);
  old_str.trim();
  new_str.trim();

  if (new_str.length() == 0) {
    Serial.println("ERROR: Falta NEW_ADDR. Formato: vc OLD_ADDR NEW_ADDR");
    return;
  }

  uint8_t old_addr = parseAddress(old_str);
  uint8_t new_addr = parseAddress(new_str);

  if (old_addr > 0 && new_addr > 0) {
    verifyAddressChange(old_addr, new_addr);
  }
}

void parseRelayCommand(String cmd) {
  // Acepta: "relay ADDR on/off" con uno o varios espacios
  String rest = cmd.substring(6); // quitar 'relay '
  rest.trim();
  int sep = rest.indexOf(' ');
  if (sep == -1) {
    Serial.println("ERROR: Formato: relay ADDR on/off");
    return;
  }
  String addr_str = rest.substring(0, sep);
  String action = rest.substring(sep + 1);
  action.trim();
  action.toLowerCase();
  
  uint8_t address = parseAddress(addr_str);
  if (address == 0) return;
  
  if (action == "on") {
    if (sendBasicCommand(address, CMD_RELAY_ON)) {
      Serial.printf("[OK] Rele encendido en 0x%02X\r\n", address);
    } else {
      Serial.printf("ERROR: No se pudo encender rele en 0x%02X\r\n", address);
    }
  } else if (action == "off") {
    if (sendBasicCommand(address, CMD_RELAY_OFF)) {
      Serial.printf("[OK] Rele apagado en 0x%02X\r\n", address);
    } else {
      Serial.printf("ERROR: No se pudo apagar rele en 0x%02X\r\n", address);
    }
  } else {
    Serial.println("ERROR: Usar 'on' o 'off'");
  }
}

void parseNeoCommand(String cmd) {
  // Acepta: "neo ADDR red/green/blue/off" con uno o varios espacios
  String rest = cmd.substring(4); // quitar 'neo '
  rest.trim();
  int sep = rest.indexOf(' ');
  if (sep == -1) {
    Serial.println("ERROR: Formato: neo ADDR red/green/blue/off");
    return;
  }
  String addr_str = rest.substring(0, sep);
  String color = rest.substring(sep + 1);
  color.trim();
  color.toLowerCase();
  
  uint8_t address = parseAddress(addr_str);
  if (address == 0) return;
  
  uint8_t neo_cmd = 0;
  String color_name = "";
  
  if (color == "red") {
    neo_cmd = CMD_NEO_RED;
    color_name = "rojos";
  } else if (color == "green") {
    neo_cmd = CMD_NEO_GREEN;
    color_name = "verdes";
  } else if (color == "blue") {
    neo_cmd = CMD_NEO_BLUE;
    color_name = "azules";
  } else if (color == "off") {
    neo_cmd = CMD_NEO_OFF;
    color_name = "apagados";
  } else {
    Serial.println("ERROR: Usar red/green/blue/off");
    return;
  }
  
  if (sendBasicCommand(address, neo_cmd)) {
    Serial.printf("[OK] NeoPixels %s en 0x%02X\r\n", color_name.c_str(), address);
  } else {
    Serial.printf("ERROR: No se pudo cambiar NeoPixels en 0x%02X\r\n", address);
  }
}

void verifyAddressChange(uint8_t old_addr, uint8_t new_addr) {
  Serial.printf("VERIFICANDO CAMBIO: 0x%02X -> 0x%02X\r\n", old_addr, new_addr);
  
  // Verificar que antigua no responde
  Wire.beginTransmission(old_addr);
  bool old_offline = (Wire.endTransmission() != 0);
  
  // Verificar que nueva sí responde
  Wire.beginTransmission(new_addr);
  bool new_online = (Wire.endTransmission() == 0);
  
  if (old_offline && new_online) {
    Serial.printf("[OK] CAMBIO EXITOSO: 0x%02X -> 0x%02X\r\n", old_addr, new_addr);
  } else if (!old_offline && new_online) {
    Serial.println("WARNING: Ambas direcciones responden");
  } else if (old_offline && !new_online) {
    Serial.println("ERROR: Dispositivo perdido");
  } else {
    Serial.println("ERROR: Cambio fallido");
  }
}