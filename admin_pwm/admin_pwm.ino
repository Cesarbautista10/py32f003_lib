// ═══════════════════════════════════════════════════════════
// ADMIN SIMPLE - Solo SCAN y TOGGLE RELAY
// Version minimalista para control basico I2C
// ═══════════════════════════════════════════════════════════

#include <Wire.h>

// Configuracion I2C - Wire1 con GPIO2 (SDA) y GPIO3 (SCL)
#define WIRE Wire1
#define I2C_SDA 2
#define I2C_SCL 3
#define I2C_FREQ 100000  // 100 kHz - SEGURO para inicialización (luego se puede subir a 400kHz)

// ═══════════════════════════════════════════════════════════
//           COMANDOS CRÍTICOS DE SEGURIDAD (0xA0-0xAF)
// ═══════════════════════════════════════════════════════════
#define CMD_RELAY_OFF      0xA0   // Apagar relé PA2 y PB5
#define CMD_RELAY_ON       0xA1   // Encender relé PA2 y PB5
#define CMD_RELAY_TOGGLE   0xA6   // Pulso/Disparo relé PA2 y PB5

// ═══════════════════════════════════════════════════════════
//           COMANDOS NEOPIXEL
// ═══════════════════════════════════════════════════════════
#define CMD_RED            0x02   // NeoPixels rojos
#define CMD_GREEN          0x03   // NeoPixels verdes
#define CMD_BLUE           0x04   // NeoPixels azules
#define CMD_OFF            0x05   // NeoPixels apagados + PWM OFF
#define CMD_WHITE          0x08   // NeoPixels blancos

// ═══════════════════════════════════════════════════════════
//           COMANDOS PWM / BUZZER
// ═══════════════════════════════════════════════════════════
#define CMD_PWM_OFF        0x20   // PWM OFF - Silencio
#define CMD_PWM_25         0x21   // PWM 25% - Tono grave 200Hz
#define CMD_PWM_50         0x22   // PWM 50% - Tono medio 500Hz
#define CMD_PWM_75         0x23   // PWM 75% - Tono agudo 1000Hz
#define CMD_PWM_100        0x24   // PWM 100% - Tono muy agudo 2000Hz

// Comandos deshabilitados - Causan problemas de estabilidad I2C
/*
// Notas musicales
#define CMD_TONE_DO        0x25   // Tono DO (261Hz)
#define CMD_TONE_RE        0x26   // Tono RE (294Hz)
#define CMD_TONE_MI        0x27   // Tono MI (330Hz)
#define CMD_TONE_FA        0x28   // Tono FA (349Hz)
#define CMD_TONE_SOL       0x29   // Tono SOL (392Hz)
#define CMD_TONE_LA        0x2A   // Tono LA (440Hz)
#define CMD_TONE_SI        0x2B   // Tono SI (494Hz)

// Alertas del sistema
#define CMD_SUCCESS        0x2C   // Success - Tono positivo 800Hz
#define CMD_OK             0x2D   // OK - Confirmación 1000Hz
#define CMD_WARNING        0x2E   // Warning - Advertencia 1200Hz
#define CMD_ALERT          0x2F   // Alert - Alerta crítica 1500Hz
*/

// ═══════════════════════════════════════════════════════════
//           COMANDOS LECTURA DIGITAL
// ═══════════════════════════════════════════════════════════
#define CMD_PA4_DIGITAL    0x07   // Leer PA0 como entrada digital
#define RESP_PA4_DIGITAL   0x09   // Respuesta PA4 digital

// ═══════════════════════════════════════════════════════════
//           COMANDOS I2C ADDRESS MANAGEMENT
// ═══════════════════════════════════════════════════════════
#define CMD_SET_I2C_ADDR   0x3D   // Establecer nueva dirección I2C (se guarda en Flash)
#define CMD_RESET_FACTORY  0x3E   // Reset factory (usar UID por defecto)
#define CMD_GET_I2C_STATUS 0x3F   // Obtener estado I2C (Flash vs UID)

#define RESP_I2C_ADDR_SET  0x0D   // Nueva dirección I2C establecida
#define RESP_FACTORY_RESET 0x0E   // Reset factory ejecutado (usar UID)
#define RESP_I2C_FROM_FLASH 0x0F  // I2C usando dirección desde Flash
#define RESP_I2C_FROM_UID  0x0A   // I2C usando dirección desde UID

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

// Flag para reimprimir menú cuando se conecta Serial
bool menu_shown = false;
uint32_t last_serial_check = 0;
bool i2c_safe_mode = false;  // Modo seguro si I2C está bloqueado

void setup() {
  // ═══════════════════════════════════════════════════════════
  // PASO 1: INICIALIZAR USB/CDC PRIMERO (CRÍTICO EN RP2040/RP2350)
  // ═══════════════════════════════════════════════════════════
  Serial.begin(115200);
  
  #if defined(ARDUINO_ARCH_RP2040)
    // RP2040/RP2350: Esperar a que USB enumere ANTES de tocar I2C
    // Esto evita que I2C bloquee IRQs y mate el CDC
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000) {
      delay(10);
    }
    delay(200);  // Margen extra de seguridad
    
    Serial.println("\n[RP2040/RP2350] USB CDC listo");
  #else
    delay(500);
  #endif
  
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║   ADMIN PWM - I2C Control            ║");
  Serial.println("║   Scan + Relay + PWM/Buzzer          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  
  // ═══════════════════════════════════════════════════════════
  // PASO 2: AHORA SÍ INICIALIZAR I2C (USB ya está vivo)
  // ═══════════════════════════════════════════════════════════
  Serial.println("\n[INFO] Inicializando I2C...");
  
  // Verificar que SDA/SCL no estén bloqueados ANTES de WIRE.begin()
  Serial.println("[CHECK] Verificando bus I2C...");
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  delay(10);
  
  bool sda_ok = digitalRead(I2C_SDA);
  bool scl_ok = digitalRead(I2C_SCL);
  
  Serial.printf("  SDA (GPIO%d): %s\n", I2C_SDA, sda_ok ? "HIGH ✓" : "LOW ✗ BLOQUEADO");
  Serial.printf("  SCL (GPIO%d): %s\n", I2C_SCL, scl_ok ? "HIGH ✓" : "LOW ✗ BLOQUEADO");
  
  if (!sda_ok || !scl_ok) {
    Serial.println("\n[ERROR] Bus I2C bloqueado!");
    Serial.println("CAUSA: Dispositivo esclavo colgado o sin pull-ups");
    Serial.println("SOLUCION:");
    Serial.println("  1. Desconecta todos los dispositivos I2C");
    Serial.println("  2. Verifica resistencias pull-up (4.7k ohm)");
    Serial.println("  3. Resetea el RP2350");
    Serial.println("\n[MODO SEGURO] I2C NO inicializado");
    i2c_safe_mode = true;
    // NO return - continuar para procesar comandos seriales
  } else {
    Serial.println("[OK] Bus I2C libre");
    i2c_safe_mode = false;
  }
  
  // Solo inicializar I2C si el bus está libre
  if (!i2c_safe_mode) {
    // Inicializar I2C con Wire1
    // RP2040 / RP2350: configurar pines ANTES de begin()
    WIRE.setSDA(I2C_SDA);
    WIRE.setSCL(I2C_SCL);
    WIRE.begin();
    WIRE.setClock(I2C_FREQ);
  
    Serial.println("[OK] I2C inicializado sin bloquear USB");
    Serial.printf("     Wire1 - SDA:GPIO%d SCL:GPIO%d @ %d kHz\n", I2C_SDA, I2C_SCL, I2C_FREQ/1000);
  
  }  // Fin de if (!i2c_safe_mode)
  
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
  // Reimprimir menú si se reconecta el Serial Monitor
  #if defined(ARDUINO_ARCH_RP2040)
    if (Serial && !menu_shown && millis() - last_serial_check > 1000) {
      showMenu();
      menu_shown = true;
      last_serial_check = millis();
    }
    if (!Serial) {
      menu_shown = false;
      last_serial_check = millis();
    }
  #endif
  
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
  } else if (cmd == "menu" || cmd == "m") {
    showMenu();
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
  } else if (cmd.startsWith("pwm ")) {
    pwmCommand(cmd);
  /*
  // Comandos deshabilitados
  } else if (cmd.startsWith("note ")) {
    playNote(cmd);
  } else if (cmd.startsWith("alert ")) {
    playAlert(cmd);
  */
  } else if (cmd == "silence" || cmd == "pwmoff") {
    pwmOff();
  } else if (cmd.startsWith("neo ")) {
    neoCommand(cmd);
  } else if (cmd.startsWith("red ")) {
    neoRed(cmd);
  } else if (cmd.startsWith("green ")) {
    neoGreen(cmd);
  } else if (cmd.startsWith("blue ")) {
    neoBlue(cmd);
  } else if (cmd.startsWith("white ")) {
    neoWhite(cmd);
  } else if (cmd == "neooff") {
    neoOff();
  } else if (cmd.startsWith("ch ")) {
    changeI2CAddress(cmd);
  } else if (cmd == "help" || cmd == "h") {
    showHelp();
  } else {
    Serial.println("ERROR: Comando desconocido. Usa 'help' o 'menu' para ver comandos.");
  }
}

void showMenu() {
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════╗");
  Serial.println("║   ADMIN PWM - I2C Control            ║");
  Serial.println("║   Scan + Relay + PWM/Buzzer          ║");
  Serial.println("╚═══════════════════════════════════════╝");
  Serial.println("[OK] I2C inicializado sin bloquear USB");
  Serial.printf("     SDA:GPIO%d SCL:GPIO%d @ %d kHz\n", I2C_SDA, I2C_SCL, I2C_FREQ/1000);
  Serial.println("\nComandos disponibles:");
  Serial.println("  s / menu       - Scan dispositivos / mostrar menu");
  Serial.println("  loop           - Scan continuo (cada 1 seg)");
  Serial.println("  stop           - Detener scan continuo");
  Serial.println("  t XX           - Pulso relay (XX = addr hex)");
  Serial.println("  on XX          - Encender relay");
  Serial.println("  off XX         - Apagar relay");
  Serial.println("  pwm XX NIVEL   - PWM (25, 50, 75, 100, off)");
  Serial.println("  silence        - Apagar PWM en todos");
  Serial.println("  neo XX COLOR   - NeoPixel (red, green, blue, white, off)");
  Serial.println("  ch XX YY       - Cambiar dir I2C (XX=old, YY=new)");
  Serial.println("  help           - Mostrar ayuda completa");
  Serial.println("\nEjemplos:");
  Serial.println("  s              (scan de dispositivos)");
  Serial.println("  menu           (mostrar este menu)");
  Serial.println("  pwm 20 50      (PWM 50% en 0x20)");
  Serial.println("  silence        (apagar PWM en todos)");
  Serial.println("  neo 20 red     (NeoPixel rojo en 0x20)");
  Serial.println("");
}

void scanDevices() {
  // Verificar si estamos en modo seguro
  if (i2c_safe_mode) {
    Serial.println("[ERROR] I2C en MODO SEGURO - comando no disponible");
    Serial.println("Desconecta dispositivos I2C y resetea el dispositivo");
    return;
  }
  
  // Evitar scans simultáneos
  if (scan_in_progress) {
    Serial.println("⚠ Scan ya en progreso, esperando...");
    return;
  }
  
  scan_in_progress = true;
  Serial.println("\n━━━ SCAN I2C + TOGGLE ━━━");
  
  device_count = 0;
  memset(found_devices, 0, sizeof(found_devices));
  
  WIRE.setTimeout(50);  // Timeout corto para velocidad
  
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    WIRE.beginTransmission(addr);
    uint8_t error = WIRE.endTransmission();
    
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
  // Verificar si estamos en modo seguro
  if (i2c_safe_mode) {
    Serial.println("[ERROR] I2C en MODO SEGURO - comando no disponible");
    return;
  }
  
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
  WIRE.beginTransmission(address);
  WIRE.write(CMD_PA4_DIGITAL);
  uint8_t error = WIRE.endTransmission();
  
  if (error != 0) {
    updateDigitalStats(address, false, 0);
    return false;
  }
  
  // Esperar procesamiento
  delay(5);
  
  // Leer respuesta
  WIRE.setTimeout(50);
  uint8_t bytesRead = WIRE.requestFrom(address, (uint8_t)1);
  
  if (bytesRead == 1) {
    uint8_t response = WIRE.read();
    
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
  WIRE.setTimeout(50);
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = found_devices[i];
    uint8_t state = 0;
    readPA0Digital(addr, &state);
    delay(read_test_interval / device_count);  // Distribuir tiempo entre dispositivos
  }
}

bool sendCommand(uint8_t address, uint8_t command) {
  WIRE.beginTransmission(address);
  WIRE.write(command);
  uint8_t error = WIRE.endTransmission();
  
  if (error != 0) return false;
  
  // Esperar respuesta con timeout más generoso
  delay(20);  // Dar tiempo al esclavo para procesar
  WIRE.setTimeout(100);
  uint8_t bytesRead = WIRE.requestFrom(address, (uint8_t)1);
  if (bytesRead == 1) {
    WIRE.read();  // Leer y descartar respuesta
    return true;
  }
  
  return false;
}

// Función para leer respuesta del dispositivo
uint8_t readResponse(uint8_t address) {
  delay(10);  // Pequeño delay para que el dispositivo prepare respuesta
  WIRE.setTimeout(100);
  uint8_t bytesRead = WIRE.requestFrom(address, (uint8_t)1);
  if (bytesRead == 1) {
    return WIRE.read();
  }
  return 0xFF;  // Error: no se recibió respuesta
}

// Versión rápida: enviar comando SIN esperar respuesta
bool sendCommandFast(uint8_t address, uint8_t command) {
  WIRE.beginTransmission(address);
  WIRE.write(command);
  uint8_t error = WIRE.endTransmission();
  
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
  // Verificar si estamos en modo seguro
  if (i2c_safe_mode) {
    Serial.println("[ERROR] I2C en MODO SEGURO - comando no disponible");
    return;
  }
  
  WIRE.setTimeout(50);
  
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

// ═══════════════════════════════════════════════════════════
//           FUNCIONES PWM / BUZZER
// ═══════════════════════════════════════════════════════════

void pwmOff() {
  Serial.println("\n━━━ APAGAR PWM (TODOS) ━━━");
  int count = 0;
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = found_devices[i];
    if (sendCommand(addr, CMD_PWM_OFF)) {
      Serial.printf("  [OK] 0x%02X - PWM apagado\n", addr);
      count++;
    } else {
      Serial.printf("  [FAIL] 0x%02X\n", addr);
    }
    delay(10);
  }
  Serial.printf("\nTotal: %d/%d dispositivos\n", count, device_count);
}

void pwmCommand(String cmd) {
  // Formato: "pwm XX NIVEL"
  // XX = dirección hex, NIVEL = 25, 50, 75, 100, off
  cmd.trim();
  int space = cmd.indexOf(' ', 4);
  
  if (space == -1) {
    Serial.println("ERROR: Formato incorrecto");
    Serial.println("Uso: pwm XX NIVEL");
    Serial.println("Ejemplo: pwm 20 50  (PWM 50% en 0x20)");
    Serial.println("Niveles: 25, 50, 75, 100, off");
    return;
  }
  
  String addr_str = cmd.substring(4, space);
  String level_str = cmd.substring(space + 1);
  addr_str.trim();
  level_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  uint8_t pwm_cmd = CMD_PWM_OFF;
  String level_name = "OFF";
  
  if (level_str == "off" || level_str == "0") {
    pwm_cmd = CMD_PWM_OFF;
    level_name = "OFF";
  } else if (level_str == "25") {
    pwm_cmd = CMD_PWM_25;
    level_name = "25% (200Hz)";
  } else if (level_str == "50") {
    pwm_cmd = CMD_PWM_50;
    level_name = "50% (500Hz)";
  } else if (level_str == "75") {
    pwm_cmd = CMD_PWM_75;
    level_name = "75% (1000Hz)";
  } else if (level_str == "100") {
    pwm_cmd = CMD_PWM_100;
    level_name = "100% (2000Hz)";
  } else {
    Serial.println("ERROR: Nivel invalido. Usa: 25, 50, 75, 100, off");
    return;
  }
  
  if (sendCommand(address, pwm_cmd)) {
    Serial.printf("[OK] PWM %s en 0x%02X\n", level_name.c_str(), address);
  } else {
    Serial.printf("[FAIL] No se pudo configurar PWM en 0x%02X\n", address);
  }
}

/*
// Funciones deshabilitadas - Causan problemas de estabilidad I2C
void playNote(String cmd) {
  // Formato: "note XX NOTA"
  // XX = dirección hex, NOTA = do, re, mi, fa, sol, la, si
  cmd.trim();
  int space = cmd.indexOf(' ', 5);
  
  if (space == -1) {
    Serial.println("ERROR: Formato incorrecto");
    Serial.println("Uso: note XX NOTA");
    Serial.println("Ejemplo: note 20 la  (Nota LA en 0x20)");
    Serial.println("Notas: do, re, mi, fa, sol, la, si");
    return;
  }
  
  String addr_str = cmd.substring(5, space);
  String note_str = cmd.substring(space + 1);
  addr_str.trim();
  note_str.trim();
  note_str.toLowerCase();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  uint8_t note_cmd = 0;
  String note_name = "";
  int freq = 0;
  
  if (note_str == "do") {
    note_cmd = CMD_TONE_DO;
    note_name = "DO";
    freq = 261;
  } else if (note_str == "re") {
    note_cmd = CMD_TONE_RE;
    note_name = "RE";
    freq = 294;
  } else if (note_str == "mi") {
    note_cmd = CMD_TONE_MI;
    note_name = "MI";
    freq = 330;
  } else if (note_str == "fa") {
    note_cmd = CMD_TONE_FA;
    note_name = "FA";
    freq = 349;
  } else if (note_str == "sol") {
    note_cmd = CMD_TONE_SOL;
    note_name = "SOL";
    freq = 392;
  } else if (note_str == "la") {
    note_cmd = CMD_TONE_LA;
    note_name = "LA";
    freq = 440;
  } else if (note_str == "si") {
    note_cmd = CMD_TONE_SI;
    note_name = "SI";
    freq = 494;
  } else {
    Serial.println("ERROR: Nota invalida. Usa: do, re, mi, fa, sol, la, si");
    return;
  }
  
  if (sendCommand(address, note_cmd)) {
    Serial.printf("[OK] Nota %s (%dHz) en 0x%02X\n", note_name.c_str(), freq, address);
  } else {
    Serial.printf("[FAIL] No se pudo reproducir nota en 0x%02X\n", address);
  }
}

void playAlert(String cmd) {
  // Formato: "alert XX TIPO"
  // XX = dirección hex, TIPO = success, ok, warning, alert
  cmd.trim();
  int space = cmd.indexOf(' ', 6);
  
  if (space == -1) {
    Serial.println("ERROR: Formato incorrecto");
    Serial.println("Uso: alert XX TIPO");
    Serial.println("Ejemplo: alert 20 ok  (Alerta OK en 0x20)");
    Serial.println("Tipos: success, ok, warning, alert");
    return;
  }
  
  String addr_str = cmd.substring(6, space);
  String alert_str = cmd.substring(space + 1);
  addr_str.trim();
  alert_str.trim();
  alert_str.toLowerCase();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  uint8_t alert_cmd = 0;
  String alert_name = "";
  int freq = 0;
  
  if (alert_str == "success") {
    alert_cmd = CMD_SUCCESS;
    alert_name = "SUCCESS";
    freq = 800;
  } else if (alert_str == "ok") {
    alert_cmd = CMD_OK;
    alert_name = "OK";
    freq = 1000;
  } else if (alert_str == "warning") {
    alert_cmd = CMD_WARNING;
    alert_name = "WARNING";
    freq = 1200;
  } else if (alert_str == "alert") {
    alert_cmd = CMD_ALERT;
    alert_name = "ALERT";
    freq = 1500;
  } else {
    Serial.println("ERROR: Tipo invalido. Usa: success, ok, warning, alert");
    return;
  }
  
  if (sendCommand(address, alert_cmd)) {
    Serial.printf("[OK] Alerta %s (%dHz) en 0x%02X\n", alert_name.c_str(), freq, address);
  } else {
    Serial.printf("[FAIL] No se pudo reproducir alerta en 0x%02X\n", address);
  }
}
*/

// ═══════════════════════════════════════════════════════════
//           FUNCIONES NEOPIXEL
// ═══════════════════════════════════════════════════════════

void neoCommand(String cmd) {
  // Formato: "neo XX COLOR"
  // XX = dirección hex, COLOR = red, green, blue, white, off
  cmd.trim();
  int space = cmd.indexOf(' ', 4);
  
  if (space == -1) {
    Serial.println("ERROR: Formato incorrecto");
    Serial.println("Uso: neo XX COLOR");
    Serial.println("Ejemplo: neo 20 red  (NeoPixel rojo en 0x20)");
    Serial.println("Colores: red, green, blue, white, off");
    return;
  }
  
  String addr_str = cmd.substring(4, space);
  String color_str = cmd.substring(space + 1);
  addr_str.trim();
  color_str.trim();
  color_str.toLowerCase();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  uint8_t neo_cmd = 0;
  String color_name = "";
  
  if (color_str == "red") {
    neo_cmd = CMD_RED;
    color_name = "ROJO";
  } else if (color_str == "green") {
    neo_cmd = CMD_GREEN;
    color_name = "VERDE";
  } else if (color_str == "blue") {
    neo_cmd = CMD_BLUE;
    color_name = "AZUL";
  } else if (color_str == "white") {
    neo_cmd = CMD_WHITE;
    color_name = "BLANCO";
  } else if (color_str == "off") {
    neo_cmd = CMD_OFF;
    color_name = "APAGADO";
  } else {
    Serial.println("ERROR: Color invalido. Usa: red, green, blue, white, off");
    return;
  }
  
  if (sendCommand(address, neo_cmd)) {
    Serial.printf("[OK] NeoPixel %s en 0x%02X\n", color_name.c_str(), address);
  } else {
    Serial.printf("[FAIL] No se pudo configurar NeoPixel en 0x%02X\n", address);
  }
}

void neoRed(String cmd) {
  String addr_str = cmd.substring(4);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_RED)) {
    Serial.printf("[OK] NeoPixel ROJO en 0x%02X\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo configurar NeoPixel en 0x%02X\n", address);
  }
}

void neoGreen(String cmd) {
  String addr_str = cmd.substring(6);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_GREEN)) {
    Serial.printf("[OK] NeoPixel VERDE en 0x%02X\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo configurar NeoPixel en 0x%02X\n", address);
  }
}

void neoBlue(String cmd) {
  String addr_str = cmd.substring(5);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_BLUE)) {
    Serial.printf("[OK] NeoPixel AZUL en 0x%02X\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo configurar NeoPixel en 0x%02X\n", address);
  }
}

void neoWhite(String cmd) {
  String addr_str = cmd.substring(6);
  addr_str.trim();
  
  uint8_t address = parseHex(addr_str);
  if (address == 0) {
    Serial.println("ERROR: Direccion invalida");
    return;
  }
  
  if (sendCommand(address, CMD_WHITE)) {
    Serial.printf("[OK] NeoPixel BLANCO en 0x%02X\n", address);
  } else {
    Serial.printf("[FAIL] No se pudo configurar NeoPixel en 0x%02X\n", address);
  }
}

void neoOff() {
  Serial.println("\n━━━ APAGAR NEOPIXEL (TODOS) ━━━");
  int count = 0;
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = found_devices[i];
    if (sendCommand(addr, CMD_OFF)) {
      Serial.printf("  [OK] 0x%02X - NeoPixel apagado\n", addr);
      count++;
    } else {
      Serial.printf("  [FAIL] 0x%02X\n", addr);
    }
    delay(10);
  }
  Serial.printf("\nTotal: %d/%d dispositivos\n", count, device_count);
}

// ═══════════════════════════════════════════════════════════
//           FUNCIÓN: CAMBIAR DIRECCIÓN I2C
// ═══════════════════════════════════════════════════════════
void changeI2CAddress(String cmd) {
  // Formato: "ch XX YY" donde XX=dirección actual, YY=nueva dirección
  cmd.trim();
  int firstSpace = cmd.indexOf(' ');
  int secondSpace = cmd.indexOf(' ', firstSpace + 1);
  
  if (firstSpace == -1 || secondSpace == -1) {
    Serial.println("[ERROR] Formato: ch XX YY");
    Serial.println("        XX = dirección actual (hex)");
    Serial.println("        YY = nueva dirección (hex)");
    return;
  }
  
  String oldAddrStr = cmd.substring(firstSpace + 1, secondSpace);
  String newAddrStr = cmd.substring(secondSpace + 1);
  
  oldAddrStr.trim();
  newAddrStr.trim();
  
  // Convertir direcciones de hexadecimal
  uint8_t oldAddr = (uint8_t)strtol(oldAddrStr.c_str(), NULL, 16);
  uint8_t newAddr = (uint8_t)strtol(newAddrStr.c_str(), NULL, 16);
  
  // Validar rangos
  if (oldAddr < 0x08 || oldAddr > 0x77) {
    Serial.printf("[ERROR] Dirección actual 0x%02X fuera de rango (0x08-0x77)\n", oldAddr);
    return;
  }
  
  if (newAddr < 0x08 || newAddr > 0x77) {
    Serial.printf("[ERROR] Nueva dirección 0x%02X fuera de rango (0x08-0x77)\n", newAddr);
    return;
  }
  
  Serial.println("\n━━━ CAMBIAR DIRECCIÓN I2C ━━━");
  Serial.printf("Dispositivo: 0x%02X → 0x%02X\n", oldAddr, newAddr);
  Serial.println("⚠️  ADVERTENCIA: Esta operación es PERMANENTE (guarda en Flash)");
  Serial.println("");
  
  // PROTOCOLO CORRECTO según firmware i2c_slave/main.c:
  // 1. Enviar 0x3D (CMD_SET_I2C_ADDR) - activa modo "esperar nueva dirección"
  // 2. El firmware responde 0x0D y espera el siguiente byte como nueva dirección
  // 3. Enviar la nueva dirección como siguiente comando
  // 4. Firmware guarda en Flash y responde 0x0D
  // 5. Requiere reset para aplicar
  
  Serial.print("Paso 1: Activando modo cambio dirección... ");
  
  WIRE.beginTransmission(oldAddr);
  WIRE.write(CMD_SET_I2C_ADDR);  // 0x3D
  uint8_t error = WIRE.endTransmission();
  
  if (error != 0) {
    Serial.printf("[FAIL] Error I2C: %d\n", error);
    Serial.println("⚠️  El dispositivo no respondió. Verifica:");
    Serial.println("   - Dirección correcta (usa 'scan' para confirmar)");
    Serial.println("   - Dispositivo conectado y funcionando");
    return;
  }
  
  delay(50);  // Delay importante: firmware necesita procesar comando
  
  // Leer respuesta
  uint8_t bytesRead = WIRE.requestFrom(oldAddr, (uint8_t)1);
  uint8_t response = 0xFF;
  if (bytesRead == 1) {
    response = WIRE.read();
  }
  
  if (response == 0x0D) {  // RESP_I2C_ADDR_SET
    Serial.println("[OK] Modo activado");
  } else {
    Serial.printf("[WARN] Respuesta: 0x%02X (esperaba 0x0D)\n", response);
    // Continuamos, puede funcionar igual
  }
  
  delay(50);  // Delay adicional antes del siguiente comando
  
  // Paso 2: Enviar nueva dirección
  Serial.printf("Paso 2: Enviando nueva dirección 0x%02X... ", newAddr);
  
  WIRE.beginTransmission(oldAddr);
  WIRE.write(newAddr);  // Nueva dirección como dato
  error = WIRE.endTransmission();
  
  if (error != 0) {
    Serial.printf("[FAIL] Error I2C: %d\n", error);
    Serial.println("⚠️  No se pudo completar el cambio");
    return;
  }
  
  delay(100);  // Delay largo: escritura Flash puede tomar tiempo
  
  // Leer confirmación
  bytesRead = WIRE.requestFrom(oldAddr, (uint8_t)1);
  response = 0xFF;
  if (bytesRead == 1) {
    response = WIRE.read();
  }
  
  if (response == 0x0D) {  // RESP_I2C_ADDR_SET
    Serial.println("[OK] ✅ Guardado en Flash");
  } else {
    Serial.printf("[WARN] Respuesta: 0x%02X\n", response);
  }
  
  Serial.println("");
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║  ✅ CAMBIO DE DIRECCIÓN COMPLETADO        ║");
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println("");
  Serial.println("⚠️  IMPORTANTE - PRÓXIMOS PASOS:");
  Serial.println("   1. La nueva dirección está GUARDADA EN FLASH");
  Serial.println("   2. DEBES RESETEAR el dispositivo para aplicar");
  Serial.println("   3. Opciones de reset:");
  Serial.println("      • Desconecta y reconecta el dispositivo");
  Serial.println("      • Usa el botón de reset físico");
  Serial.println("      • Cicla la alimentación");
  Serial.printf("   4. Después del reset: dispositivo en 0x%02X\n", newAddr);
  Serial.println("");
  Serial.println("Verificación:");
  Serial.println("   1. Resetea el dispositivo");
  Serial.println("   2. Ejecuta: scan");
  Serial.printf("   3. Busca la dirección 0x%02X en la lista\n", newAddr);
  Serial.println("");
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
  Serial.println("  t XX           - Pulso relay PB5 (10ms)");
  Serial.println("  on XX          - Encender relay PB5");
  Serial.println("  off XX         - Apagar relay PB5");
  Serial.println("");
  Serial.println("PWM / BUZZER:");
  Serial.println("  pwm XX NIVEL   - PWM en PA2 (25, 50, 75, 100, off)");
  Serial.println("  silence        - Apagar PWM en todos los dispositivos");
  Serial.println("");
  Serial.println("NEOPIXEL:");
  Serial.println("  neo XX COLOR   - Control NeoPixel (red, green, blue, white, off)");
  Serial.println("  red XX         - NeoPixel rojo");
  Serial.println("  green XX       - NeoPixel verde");
  Serial.println("  blue XX        - NeoPixel azul");
  Serial.println("  white XX       - NeoPixel blanco");
  Serial.println("  neooff         - Apagar NeoPixel en todos");
  Serial.println("");
  Serial.println("I2C ADDRESS:");
  Serial.println("  ch XX YY       - Cambiar dirección I2C");
  Serial.println("                   XX = dirección actual (hex)");
  Serial.println("                   YY = nueva dirección (hex)");
  Serial.println("                   (se guarda en Flash, requiere reset)");
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
  Serial.println("  pwm 20 50      → PWM 50% (500Hz) en 0x20");
  Serial.println("  silence        → Apagar PWM en todos");
  Serial.println("  neo 20 red     → NeoPixel rojo en 0x20");
  Serial.println("  red 20         → NeoPixel rojo en 0x20");
  Serial.println("  neooff         → Apagar NeoPixel en todos");
  Serial.println("  t 20           → Pulso en 0x20");
  Serial.println("  on 32          → Encender en 0x32");
  Serial.println("  off 20         → Apagar relay en 0x20");
  Serial.println("  ch 19 25       → Cambiar 0x19 a 0x25");
  Serial.println("");
}
