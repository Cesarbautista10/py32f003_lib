// ═══════════════════════════════════════════════════════════
// ADMIN SIMPLE - Solo SCAN y TOGGLE RELAY
// Version minimalista para control basico I2C
// ═══════════════════════════════════════════════════════════

#include <Wire.h>

// Configuracion I2C - GPIO6 (SDA) y GPIO7 (SCL)
#define I2C_SDA 6
#define I2C_SCL 7
#define I2C_FREQ 400000  // 400 kHz - velocidad máxima

// ═══════════════════════════════════════════════════════════
//           COMANDOS CRÍTICOS DE SEGURIDAD (0xA0-0xAF)
// ═══════════════════════════════════════════════════════════
#define CMD_RELAY_OFF      0xA0   // Apagar relé PA2 y PB5
#define CMD_RELAY_ON       0xA1   // Encender relé PA2 y PB5
#define CMD_RELAY_TOGGLE   0xA6   // Pulso/Disparo relé PA2 y PB5

// ═══════════════════════════════════════════════════════════
//           COMANDOS LECTURA DIGITAL
// ═══════════════════════════════════════════════════════════
#define CMD_PA4_DIGITAL    0x07   // Leer PA0 como entrada digital
#define RESP_PA4_DIGITAL   0x09   // Respuesta PA4 digital

// Variables globales
uint8_t found_devices[128];
uint8_t device_count = 0;
bool scan_loop_active = false;  // Bandera para scan continuo
uint32_t last_scan_time = 0;    // Timestamp del último scan
uint32_t scan_interval = 1000;  // Intervalo entre scans (1 segundo)
bool scan_in_progress = false;  // Bandera para evitar scans simultáneos
#define DEVICE_DELAY 30         // Delay óptimo entre dispositivos (ms)
struct DeviceError {
  uint8_t address;
  uint32_t error_count;
  uint32_t success_count;
  uint32_t last_error_time;
};
DeviceError device_errors[128];
int device_error_count = 0;

// Test mode
bool test_mode_active = false;
uint32_t test_interval = 100;  // Intervalo de test (100ms por defecto)
uint32_t test_start_time = 0;
uint32_t test_duration = 0;

// PA0 Digital reading stats
struct DigitalReadStats {
  uint8_t address;
  uint32_t read_count;
  uint32_t fail_count;
  uint8_t last_state;  // 0=LOW, 1=HIGH
  uint32_t last_read_time;
};
DigitalReadStats digital_stats[128];
int digital_stats_count = 0;

// Test mode para lecturas digitales
bool read_test_active = false;
uint32_t read_test_interval = 100;
uint32_t read_test_start_time = 0;
uint32_t read_test_duration = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║   ADMIN SIMPLE - I2C Control         ║");
  Serial.println("║   Scan + Relay Control               ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  // Inicializar I2C con pines personalizados
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(I2C_FREQ);
  
  Serial.printf("\nI2C Iniciado - SDA:GPIO%d SCL:GPIO%d\n", I2C_SDA, I2C_SCL);
  Serial.println("\nComandos disponibles:");
  Serial.println("  s              - Scan dispositivos I2C");
  Serial.println("  loop           - Scan continuo (cada 1 seg)");
  Serial.println("  stop           - Detener scan continuo");
  Serial.println("  t XX           - Pulso relay (XX = addr hex)");
  Serial.println("  on XX          - Encender relay");
  Serial.println("  off XX         - Apagar relay");
  Serial.println("  help           - Mostrar ayuda");
  Serial.println("\nEjemplos:");
  Serial.println("  s              (scan de dispositivos)");
  Serial.println("  loop           (scan continuo)");
  Serial.println("  stop           (detener scan)");
  Serial.println("  t 32           (toggle relay en 0x20)");
  Serial.println("  off 32         (apagar relay en 0x20)");
  Serial.println("\n");
}

void loop() {
  // Procesar comandos desde Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd.length() > 0) {
      processCommand(cmd);
    }
  }
  
  // Ejecutar scan continuo si está activo
  if (scan_loop_active && !scan_in_progress) {
    if (millis() - last_scan_time >= scan_interval) {
      scanDevices();
      last_scan_time = millis();
    }
  }
  
  // Ejecutar test asíncrono si está activo
  if (test_mode_active) {
    if (millis() - last_scan_time >= test_interval) {
      testDevices();
      last_scan_time = millis();
    }
    // Verificar si el test debe terminar
    if (test_duration > 0 && (millis() - test_start_time >= test_duration)) {
      stopTest();
      showTestResults();
    }
  }
  
  // Ejecutar test de lectura digital si está activo
  if (read_test_active) {
    if (millis() - last_scan_time >= read_test_interval) {
      testReadDigital();
      last_scan_time = millis();
    }
    // Verificar si el test debe terminar
    if (read_test_duration > 0 && (millis() - read_test_start_time >= read_test_duration)) {
      stopReadTest();
    }
  }
}

void processCommand(String cmd) {
  if (cmd == "s" || cmd == "scan") {
    scanDevices();
  } else if (cmd == "loop") {
    startScanLoop();
  } else if (cmd == "stop") {
    stopScanLoop();
  } else if (cmd.startsWith("test ")) {
    startTest(cmd);
  } else if (cmd == "stoptest") {
    stopTest();
    showTestResults();
  } else if (cmd == "errors") {
    showTestResults();
  } else if (cmd.startsWith("read ")) {
    readDigital(cmd);
  } else if (cmd == "dstats") {
    showDigitalStats();
  } else if (cmd.startsWith("readtest ")) {
    startReadTest(cmd);
  } else if (cmd == "stopread") {
    stopReadTest();
  } else if (cmd.startsWith("t ")) {
    toggleRelay(cmd);
  } else if (cmd.startsWith("on ")) {
    relayOn(cmd);
  } else if (cmd.startsWith("off ")) {
    relayOff(cmd);
  } else if (cmd == "help" || cmd == "h") {
    showHelp();
  } else {
    Serial.println("ERROR: Comando desconocido. Usa 'help' para ver comandos.");
  }
}

void scanDevices() {
  // Evitar scans simultáneos
  if (scan_in_progress) {
    Serial.println("⚠ Scan ya en progreso, esperando...");
    return;
  }
  
  scan_in_progress = true;
  Serial.println("\n━━━ SCAN I2C + TOGGLE ━━━");
  
  device_count = 0;
  memset(found_devices, 0, sizeof(found_devices));
  
  Wire.setTimeout(50);  // Timeout corto para velocidad
  
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      found_devices[device_count++] = addr;
      Serial.printf("  [%02d] 0x%02X (%3d) → ", device_count, addr, addr);
      
      // Enviar toggle automático SIN esperar respuesta
      if (sendCommandFast(addr, CMD_RELAY_TOGGLE)) {
        Serial.println("TOGGLE ✓");
      } else {
        Serial.println("TOGGLE ✗");
      }
      
      delay(DEVICE_DELAY); // Delay optimizado: 30ms
    }
  }
  
  Serial.println("━━━━━━━━━━━━━━━━");
  Serial.printf("Total: %d dispositivos\n\n", device_count);
  
  if (device_count == 0) {
    Serial.println("⚠ No se encontraron dispositivos I2C");
    Serial.println("  Verifica conexiones y pull-ups\n");
  }
  
  scan_in_progress = false;
}

void toggleRelay(String cmd) {
  String addr_str = cmd.substring(2);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_RELAY_TOGGLE)) {
    Serial.printf("[OK] Pulso relay en 0x%02X (10ms)\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo enviar pulso a 0x%02X\n", address);
  }
}

void relayOn(String cmd) {
  String addr_str = cmd.substring(3);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_RELAY_ON)) {
    Serial.printf("[OK] Relay ON en 0x%02X\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo encender relay en 0x%02X\n", address);
  }
}

void relayOff(String cmd) {
  String addr_str = cmd.substring(4);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_RELAY_OFF)) {
    Serial.printf("[OK] Relay OFF en 0x%02X\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo apagar relay en 0x%02X\n", address);
  }
}

// ═══════════════════════════════════════════════════════════
//           LECTURA DIGITAL PA0
// ═══════════════════════════════════════════════════════════

void readDigital(String cmd) {
  String addr_str = cmd.substring(5);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  uint8_t state = 0;
  bool success = readPA0Digital(address, &state);
  
  if (success) {
    Serial.printf("[OK] 0x%02X - PA0 Digital: %s\n", address, state ? "HIGH" : "LOW");
  } else {
    Serial.printf("[FAIL] No se pudo leer PA0 digital en 0x%02X\n", address);
  }
}

bool readPA0Digital(uint8_t address, uint8_t* state) {
  // Enviar comando de lectura PA0 digital
  Wire.beginTransmission(address);
  Wire.write(CMD_PA4_DIGITAL);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) {
    updateDigitalStats(address, false, 0);
    return false;
  }
  
  // Esperar procesamiento
  delay(5);
  
  // Leer respuesta (1 byte)
  Wire.setTimeout(50);
  uint8_t bytesRead = Wire.requestFrom(address, (uint8_t)1);
  
  if (bytesRead == 1) {
    uint8_t response = Wire.read();
    
    // El estado digital está en el nibble alto (bit 7-4)
    // 0xF0 = HIGH, 0x00 = LOW
    *state = (response & 0xF0) ? 1 : 0;
    
    updateDigitalStats(address, true, *state);
    return true;
  }
  
  updateDigitalStats(address, false, 0);
  return false;
}

void updateDigitalStats(uint8_t address, bool success, uint8_t state) {
  // Buscar dispositivo en el array
  int index = -1;
  for (int i = 0; i < digital_stats_count; i++) {
    if (digital_stats[i].address == address) {
      index = i;
      break;
    }
  }
  
  // Si no existe, agregarlo
  if (index == -1 && digital_stats_count < 128) {
    index = digital_stats_count++;
    digital_stats[index].address = address;
    digital_stats[index].read_count = 0;
    digital_stats[index].fail_count = 0;
    digital_stats[index].last_state = 0;
    digital_stats[index].last_read_time = 0;
  }
  
  if (index >= 0) {
    digital_stats[index].read_count++;
    if (!success) {
      digital_stats[index].fail_count++;
    } else {
      digital_stats[index].last_state = state;
    }
    digital_stats[index].last_read_time = millis();
  }
}

void showDigitalStats() {
  if (digital_stats_count == 0) {
    Serial.println("No hay estadisticas de lectura digital");
    return;
  }
  
  Serial.println("\n╔════════════════════════════════════════════════════════╗");
  Serial.println("║      ESTADISTICAS DE LECTURA DIGITAL PA0             ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  Serial.println("║ Addr │ Lecturas │ Fallos │ Tasa OK │ Estado │  Tiempo ║");
  Serial.println("╠════════════════════════════════════════════════════════╣");
  
  for (int i = 0; i < digital_stats_count; i++) {
    DigitalReadStats* stats = &digital_stats[i];
    
    float success_rate = 0.0;
    if (stats->read_count > 0) {
      success_rate = ((float)(stats->read_count - stats->fail_count) / stats->read_count) * 100.0;
    }
    
    uint32_t time_since = (millis() - stats->last_read_time) / 1000;  // segundos
    
    Serial.printf("║ 0x%02X │ %8lu │ %6lu │ %6.1f%% │ %4s   │ %4lus   ║\n",
                  stats->address,
                  stats->read_count,
                  stats->fail_count,
                  success_rate,
                  stats->last_state ? "HIGH" : "LOW",
                  time_since);
  }
  
  Serial.println("╚════════════════════════════════════════════════════════╝");
}

// ═══════════════════════════════════════════════════════════
//           TEST DE LECTURA DIGITAL CONTINUA
// ═══════════════════════════════════════════════════════════

void startReadTest(String cmd) {
  // Formato: "readtest <intervalo_ms> <duracion_ms>"
  // Ejemplo: "readtest 100 10000" = leer cada 100ms durante 10 segundos
  cmd.trim();
  int space1 = cmd.indexOf(' ', 9);
  
  if (space1 == -1) {
    Serial.println("ERROR: Formato incorrecto");
    Serial.println("Uso: readtest <intervalo_ms> <duracion_ms>");
    Serial.println("Ejemplo: readtest 100 10000  (leer cada 100ms por 10 seg)");
    return;
  }
  
  String interval_str = cmd.substring(9, space1);
  String duration_str = cmd.substring(space1 + 1);
  
  read_test_interval = interval_str.toInt();
  read_test_duration = duration_str.toInt();
  
  // Validar rango
  if (read_test_interval < 30) read_test_interval = 30;
  if (read_test_interval > 1000) read_test_interval = 1000;
  if (read_test_duration < 1000) read_test_duration = 1000;
  
  // Reset contadores de lectura digital
  digital_stats_count = 0;
  memset(digital_stats, 0, sizeof(digital_stats));
  
  read_test_active = true;
  read_test_start_time = millis();
  last_scan_time = millis();
  
  Serial.println("\n[READ TEST MODE ACTIVADO]");
  Serial.printf("Intervalo: %lu ms\n", read_test_interval);
  Serial.printf("Duración: %lu ms (%.1f seg)\n", read_test_duration, read_test_duration / 1000.0);
  Serial.println("Dispositivos a leer: ");
  for (int i = 0; i < device_count; i++) {
    Serial.printf("  0x%02X\n", found_devices[i]);
  }
  Serial.println("Usa 'stopread' para detener antes\n");
}

void stopReadTest() {
  read_test_active = false;
  uint32_t elapsed = millis() - read_test_start_time;
  Serial.println("\n[READ TEST MODE DETENIDO]");
  Serial.printf("Tiempo transcurrido: %.2f segundos\n", elapsed / 1000.0);
  showDigitalStats();
}

void testReadDigital() {
  Wire.setTimeout(50);
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = found_devices[i];
    uint8_t state = 0;
    readPA0Digital(addr, &state);
    delay(read_test_interval / device_count);  // Distribuir tiempo entre dispositivos
  }
}

bool sendCommand(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(command);
  uint8_t error = Wire.endTransmission();
  
  if (error != 0) return false;
  
  // Esperar respuesta con timeout más generoso
  delay(20);  // Dar tiempo al esclavo para procesar
  Wire.setTimeout(100);
  uint8_t bytesRead = Wire.requestFrom(address, (uint8_t)1);
  if (bytesRead == 1) {
    Wire.read();  // Leer y descartar respuesta
    return true;
  }
  
  return false;
}

// Versión rápida: enviar comando SIN esperar respuesta
bool sendCommandFast(uint8_t address, uint8_t command) {
  Wire.beginTransmission(address);
  Wire.write(command);
  uint8_t error = Wire.endTransmission();
  
  // Actualizar contador de errores
  updateDeviceError(address, error == 0);
  
  return (error == 0);  // Solo verificar que se envió
}

void updateDeviceError(uint8_t address, bool success) {
  // Buscar dispositivo en el array
  int index = -1;
  for (int i = 0; i < device_error_count; i++) {
    if (device_errors[i].address == address) {
      index = i;
      break;
    }
  }
  
  // Si no existe, agregarlo
  if (index == -1 && device_error_count < 128) {
    index = device_error_count++;
    device_errors[index].address = address;
    device_errors[index].error_count = 0;
    device_errors[index].success_count = 0;
    device_errors[index].last_error_time = 0;
  }
  
  // Actualizar contadores
  if (index != -1) {
    if (success) {
      device_errors[index].success_count++;
    } else {
      device_errors[index].error_count++;
      device_errors[index].last_error_time = millis();
    }
  }
}

void startTest(String cmd) {
  // Formato: "test <intervalo_ms> <duracion_ms>"
  // Ejemplo: "test 50 5000" = test cada 50ms durante 5 segundos
  cmd.trim();
  int space1 = cmd.indexOf(' ', 5);
  
  if (space1 == -1) {
    Serial.println("ERROR: Formato incorrecto");
    Serial.println("Uso: test <intervalo_ms> <duracion_ms>");
    Serial.println("Ejemplo: test 50 5000  (test cada 50ms por 5 seg)");
    return;
  }
  
  String interval_str = cmd.substring(5, space1);
  String duration_str = cmd.substring(space1 + 1);
  
  test_interval = interval_str.toInt();
  test_duration = duration_str.toInt();
  
  // Validar rango
  if (test_interval < 30) test_interval = 30;
  if (test_interval > 1000) test_interval = 1000;
  if (test_duration < 1000) test_duration = 1000;
  
  // Reset contadores
  device_error_count = 0;
  memset(device_errors, 0, sizeof(device_errors));
  
  test_mode_active = true;
  test_start_time = millis();
  last_scan_time = millis();
  
  Serial.println("\n[TEST MODE ACTIVADO]");
  Serial.printf("Intervalo: %lu ms\n", test_interval);
  Serial.printf("Duración: %lu ms (%.1f seg)\n", test_duration, test_duration / 1000.0);
  Serial.println("Usa 'stoptest' para detener antes\n");
}

void stopTest() {
  test_mode_active = false;
  uint32_t elapsed = millis() - test_start_time;
  Serial.println("\n[TEST MODE DETENIDO]");
  Serial.printf("Tiempo transcurrido: %.2f segundos\n\n", elapsed / 1000.0);
}

void testDevices() {
  Wire.setTimeout(50);
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = found_devices[i];
    sendCommandFast(addr, CMD_RELAY_TOGGLE);
    delay(test_interval / device_count);  // Distribuir tiempo entre dispositivos
  }
}

void showTestResults() {
  Serial.println("\n╔═══════════════════════════════════════════════════╗");
  Serial.println("║           RESULTADOS DE TEST I2C                 ║");
  Serial.println("╚═══════════════════════════════════════════════════╝\n");
  
  if (device_error_count == 0) {
    Serial.println("No hay datos de test disponibles\n");
    return;
  }
  
  uint32_t total_success = 0;
  uint32_t total_errors = 0;
  
  Serial.println("Addr  | Success | Errors | Tasa Éxito | Último Error");
  Serial.println("------|---------|--------|------------|-------------");
  
  for (int i = 0; i < device_error_count; i++) {
    uint8_t addr = device_errors[i].address;
    uint32_t success = device_errors[i].success_count;
    uint32_t errors = device_errors[i].error_count;
    uint32_t total = success + errors;
    float rate = total > 0 ? (success * 100.0 / total) : 0;
    
    total_success += success;
    total_errors += errors;
    
    Serial.printf("0x%02X  | %7lu | %6lu | %6.2f%%   | ", 
                  addr, success, errors, rate);
    
    if (errors > 0) {
      uint32_t since_error = (millis() - device_errors[i].last_error_time) / 1000;
      Serial.printf("%lu seg\n", since_error);
    } else {
      Serial.println("Nunca");
    }
  }
  
  Serial.println("------|---------|--------|------------|-------------");
  uint32_t grand_total = total_success + total_errors;
  float overall_rate = grand_total > 0 ? (total_success * 100.0 / grand_total) : 0;
  Serial.printf("TOTAL | %7lu | %6lu | %6.2f%%   |\n\n", 
                total_success, total_errors, overall_rate);
}

uint8_t parseHex(String hex_str) {
  hex_str.trim();
  
  // Convertir de hexadecimal
  char* endptr;
  long addr = strtol(hex_str.c_str(), &endptr, 16);
  
  if (*endptr != '\0' || addr < 0x08 || addr > 0x77) {
    return 0;
  }
  
  return (uint8_t)addr;
}

void startScanLoop() {
  scan_loop_active = true;
  last_scan_time = millis();
  Serial.println("\n[OK] Scan continuo ACTIVADO");
  Serial.printf("     Intervalo: %lu ms\n", scan_interval);
  Serial.println("     Usa 'stop' para detener\n");
}

void stopScanLoop() {
  scan_loop_active = false;
  Serial.println("\n[OK] Scan continuo DETENIDO\n");
}

void showHelp() {
  Serial.println("\n╔═══════════════════════════════════════╗");
  Serial.println("║        COMANDOS DISPONIBLES          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  Serial.println("");
  Serial.println("SCAN:");
  Serial.println("  s              - Escanear dispositivos I2C");
  Serial.println("  loop           - Scan continuo (1 seg)");
  Serial.println("  stop           - Detener scan continuo");
  Serial.println("");
  Serial.println("TEST I2C:");
  Serial.println("  test INT DUR   - Test asíncrono");
  Serial.println("                   INT = intervalo (30-1000ms)");
  Serial.println("                   DUR = duración (ms)");
  Serial.println("  stoptest       - Detener test y mostrar resultados");
  Serial.println("  errors         - Mostrar estadísticas de errores");
  Serial.println("");
  Serial.println("LECTURA DIGITAL PA0:");
  Serial.println("  read XX        - Leer estado digital PA0");
  Serial.println("  dstats         - Estadísticas de lecturas PA0");
  Serial.println("  readtest I D   - Test continuo de lectura PA0");
  Serial.println("                   I = intervalo (30-1000ms)");
  Serial.println("                   D = duración (ms)");
  Serial.println("  stopread       - Detener test de lectura");
  Serial.println("");
  Serial.println("RELAY:");
  Serial.println("  t XX           - Pulso relay PA2/PB5 (10ms)");
  Serial.println("  on XX          - Encender relay PA2/PB5");
  Serial.println("  off XX         - Apagar relay PA2/PB5");
  Serial.println("");
  Serial.println("FORMATO:");
  Serial.println("  XX = Direccion I2C en hexadecimal");
  Serial.println("");
  Serial.println("EJEMPLOS:");
  Serial.println("  s              → Scan completo");
  Serial.println("  loop           → Scan automatico cada 1s");
  Serial.println("  stop           → Detener scan automatico");
  Serial.println("  test 50 10000  → Test cada 50ms por 10 seg");
  Serial.println("  stoptest       → Detener test y ver errores");
  Serial.println("  errors         → Ver estadísticas");
  Serial.println("  read 20        → Leer PA0 digital en 0x20");
  Serial.println("  readtest 50 10000 → Test lectura cada 50ms por 10s");
  Serial.println("  stopread       → Detener test de lectura");
  Serial.println("  dstats         → Ver estadísticas PA0");
  Serial.println("  t 20           → Pulso en 0x20");
  Serial.println("  on 32          → Encender en 0x32");
  Serial.println("  off 20         → Apagar relay en 0x20");
  Serial.println("");
}
