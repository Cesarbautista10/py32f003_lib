#include <Wire.h>

// ═══════════════════════════════════════════════════════════
//              TEST CONTINUO - RECUPERACION DE BUS
//     Programa de test continuo con recuperacion automatica
// ═══════════════════════════════════════════════════════════

// Comandos básicos para dispositivos PY32F003
#define CMD_RELAY_OFF      0x00
#define CMD_RELAY_ON       0x01
#define CMD_NEO_RED        0x02
#define CMD_NEO_GREEN      0x03
#define CMD_NEO_BLUE       0x04
#define CMD_NEO_OFF        0x05
#define CMD_PA4_DIGITAL    0x07
#define CMD_ADC_PA0_HSB    0xD8
#define CMD_ADC_PA0_LSB    0xD9

// Estructura de dispositivo con estadísticas
struct DeviceStats {
  uint8_t address;
  uint8_t mux_channel;  // Nuevo campo para el canal del multiplexor
  uint32_t total_tests;
  uint32_t successful_tests;
  uint32_t failed_tests;
  uint32_t last_success_time;
  uint32_t last_failure_time;
  bool is_online;
  uint8_t consecutive_failures;
  uint16_t last_adc_value;
};

// Variables globales
DeviceStats devices[32];  // Máximo 32 dispositivos (ampliado desde 20)
int device_count = 0;
bool continuous_test = false;
bool continuous_sweep = false;  // Nueva variable para sweep continuo
uint32_t cycle_count = 0;
uint32_t total_bus_recoveries = 0;
uint32_t test_interval = 5;  // 1 segundo entre ciclos
uint32_t sweep_recovery_time = 500;  // 2 segundos de recuperación para sweep continuo
uint32_t command_delay = 25;    // Delay por defecto entre comandos

// Estadísticas de tiempo de ejecución
struct TimingStats {
  uint32_t total_time;
  uint32_t min_time;
  uint32_t max_time;
  uint32_t count;
  String operation_name;
};

TimingStats timing_stats[10];  // Para diferentes operaciones
int timing_count = 0;
uint32_t last_operation_time = 0;

// Configuración de pines I2C
const int SDA_PIN = 22;
const int SCL_PIN = 23;
const uint32_t I2C_SPEED = 100000;  // 100kHz para mayor velocidad

// Configuración del Multiplexor TCA9545A
#define MUX_ADDRESS 0x70  // Dirección del multiplexor
#define MUX_CHANNELS 4    // 4 canales disponibles (0-3)
bool mux_enabled = false;
uint8_t current_mux_channel = 0;

// Declaraciones de funciones
void initializeI2C();
void scanAndInitDevices();
void processCommand(String cmd);
void runTestCycle();
bool testSingleDeviceQuick(uint8_t addr);
bool attemptDeviceRecovery(uint8_t addr);
void forceI2CRecovery();
void runSingleTest();
void fastSweepTest();
void showStatistics();
void resetStatistics();
bool testDeviceConnection(uint8_t addr);
bool sendCommand(uint8_t addr, uint8_t cmd);
bool sendCommandFast(uint8_t addr, uint8_t cmd);
bool sendCommandSafe(uint8_t addr, uint8_t cmd);
void setAllLEDs(uint8_t led_cmd);
void setAllRelays(uint8_t relay_cmd);
void setAllDevicesState(uint8_t led_cmd, uint8_t relay_cmd);
void recordTiming(String operation, uint32_t execution_time);
void showTimingStatistics();
void resetTimingStatistics();
uint32_t startTiming();
void readAllPA4States();
void checkAllDevicesWithPA4();
uint8_t readPA4State(uint8_t addr);
uint16_t readADC_PA0(uint8_t addr);           // Nueva función para leer ADC PA0
void testADC_Profile();                       // Nueva función perfil ADC
void readAllADC_PA0States();                  // Nueva función leer todos los ADC
bool testADC_Support(uint8_t addr);           // Nueva función para probar soporte ADC
void identifyFirmwareVersion(uint8_t addr);   // Nueva función para identificar versión firmware
void scanFirmwareVersions();                  // Nueva función para escanear versiones de todos

// Funciones del multiplexor TCA9545A
bool initializeMux();
void selectMuxChannel(uint8_t channel);
bool testMuxConnection();
void scanAllMuxChannels();
void showMuxStatistics();
void fullMuxReset();  // Nueva función para reset completo del MUX

// Función de reset del microcontrolador
void resetMicrocontroller();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║       TEST CONTINUO - LOCKNODE V6         ║");
  Serial.println("║     Recuperacion Automatica de Bus        ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  
  // Inicializar I2C
  initializeI2C();
  
  // Probar multiplexor
  if (initializeMux()) {
    Serial.println("MUX: Multiplexor TCA9545A detectado y habilitado");
    mux_enabled = true;
  } else {
    Serial.println("MUX: Sin multiplexor - modo directo I2C");
    mux_enabled = false;
  }
  
  Serial.printf("I2C: SDA=GPIO%d, SCL=GPIO%d, Speed=%dHz\n", SDA_PIN, SCL_PIN, I2C_SPEED);
  Serial.println("SYSTEM: Capacidad máxima: 32 dispositivos");
  Serial.println();
  
  // Escaneo inicial - solo mux_scan si hay multiplexor
  if (mux_enabled) {
    Serial.println("SETUP: Ejecutando reset completo del multiplexor (mux_scan)...");
    fullMuxReset();  // Solo hacer mux_scan al arranque
  } else {
    Serial.println("SETUP: Sin multiplexor - escaneando dispositivos directamente...");
    scanAndInitDevices();  // Escaneo normal solo si no hay MUX
  }
  
  Serial.println("\nCOMANDOS DISPONIBLES:");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("'start' = Iniciar test continuo");
  Serial.println("'sweep_start' = Iniciar barrido continuo");
  Serial.println("'stop' = Detener test/barrido continuo");
  Serial.println("'scan' = Re-escanear dispositivos");
  Serial.println("'mux_scan' = RESET COMPLETO del multiplexor + recuperación de bus");
  Serial.println("'mux_scan_only' = Escaneo rápido de canales MUX (sin reset)");
  Serial.println("'mux_stats' = Mostrar estadísticas por canal MUX");
  Serial.println("'sweep' = Barrido rapido relay+LED (Verde=ON, Rojo=OFF)");
  Serial.println("'stats' = Mostrar estadisticas");
  Serial.println("'reset stats' = Reiniciar estadisticas");
  Serial.println("'interval <ms>' = Cambiar intervalo (ej: interval 500)");
  Serial.println("'sweep_recovery <ms>' = Tiempo de recuperación para sweep (ej: sweep_recovery 3000)");
  Serial.println("'single' = Test unico de todos los dispositivos");
  Serial.println("'recovery' = Forzar recuperacion de bus");
  Serial.println("'reset' = Reiniciar microcontrolador (emergencia)");
  Serial.println("━━━━━━━━━━━━━ TESTS GRUPALES ━━━━━━━━━━━━━━━━");
  Serial.println("'all red' = Todos los LEDs en ROJO");
  Serial.println("'all green' = Todos los LEDs en VERDE");
  Serial.println("'all blue' = Todos los LEDs en AZUL");
  Serial.println("'all off' = Apagar todos los LEDs");
  Serial.println("'relay on' = Encender todos los relays");
  Serial.println("'relay off' = Apagar todos los relays");
  Serial.println("'all on' = LEDs VERDE + Relays ON");
  Serial.println("'all shut' = LEDs OFF + Relays OFF");
  Serial.println("━━━━━━━━━━━━━ MEDICION DE TIEMPOS ━━━━━━━━━━━━");
  Serial.println("'timing' = Mostrar tiempos de ejecucion");
  Serial.println("'reset timing' = Reiniciar mediciones de tiempo");
  Serial.println("━━━━━━━━━━━━━ LECTURA DE ESTADOS ━━━━━━━━━━━━━");
  Serial.println("'read pa4' = Leer estado PA4 de todos los dispositivos");
  Serial.println("'check all' = Revision completa con respuestas PA4");
  Serial.println("━━━━━━━━━━━━━ LECTURA ADC PA0 ━━━━━━━━━━━━━━━━");
  Serial.println("'adc' = Leer ADC PA0 de todos los dispositivos (una vez)");
  Serial.println("'adc-profile' = Mostrar opciones del perfil ADC");
  Serial.println("'adc-loop' = Leer ADC PA0 continuo (usar 'stop' para parar)");
  Serial.println("'adc-single 0x08' = Leer ADC de un dispositivo específico");
  Serial.println("'adc-test' = Probar qué dispositivos soportan ADC");
  Serial.println("━━━━━━━━━━━━━ IDENTIFICACIÓN FIRMWARE ━━━━━━━━━━━━");
  Serial.println("'firmware-scan' = Identificar versión de firmware de todos");
  Serial.println("'firmware-id 0x08' = Identificar firmware de dispositivo específico");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println();
}

void loop() {
  // Procesar comandos serie
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.length() > 0) {
      processCommand(cmd);
    }
  }
  
  // Test continuo si está activado
  if (continuous_test) {
    runTestCycle();
    delay(test_interval);
  } else if (continuous_sweep) {
    fastSweepTest();
    
    // RECUPERACIÓN MEJORADA para sweep continuo
    Serial.printf("SWEEP: Iniciando recuperación de %lums...\n", sweep_recovery_time);
    delay(sweep_recovery_time);  // Tiempo de recuperación específico para sweep
    
    // Verificación del estado del bus cada 3 ciclos
    if (cycle_count % 3 == 0) {
      Serial.println("SWEEP: Verificando salud del bus I2C...");
      bool bus_healthy = true;
      
      // Test rápido con varios dispositivos
      for (int test_addr = 0x08; test_addr <= 0x10; test_addr++) {
        Wire.beginTransmission(test_addr);
        if (Wire.endTransmission() == 0) {
          // Si encontramos un dispositivo, testear comunicación
          Wire.beginTransmission(test_addr);
          Wire.write(CMD_PA4_DIGITAL);
          if (Wire.endTransmission() != 0) {
            bus_healthy = false;
            break;
          }
        }
        delay(5);
      }
      
      if (!bus_healthy) {
        Serial.println("SWEEP: Bus I2C comprometido - iniciando recuperación completa...");
        forceI2CRecovery();
        total_bus_recoveries++;
        delay(1500);  // Pausa extra larga tras recuperación
        Serial.println("SWEEP: Recuperación completada - continuando barrido...");
      } else {
        Serial.println("SWEEP: Bus I2C estable - continuando barrido...");
      }
    }
    
  } else {
    delay(1);
  }
}

void initializeI2C() {
  Serial.println("I2C: Inicializando bus I2C...");
  
  // Recuperacion preventiva al inicio
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(500);  // Pausa mas larga para estabilizacion
  
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeout(500);  // Timeout mucho mas conservador
  Wire.setClock(100000);  // Comenzar con velocidad muy segura
  
  Serial.println("I2C: Probando bus inicial...");
  
  // Test inicial del bus con mas tiempo
  int test_devices = 0;
  for (uint8_t addr = 0x08; addr <= 0x20; addr++) {  // Test rapido
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      test_devices++;
    }
    delay(10);  // Mas tiempo entre tests
  }
  
  if (test_devices > 0) {
    Serial.printf("I2C: Bus operativo - %d dispositivos detectados\n", test_devices);
    // NO aumentar velocidad - mantener 50kHz para maxima estabilidad
    Serial.println("I2C: Manteniendo velocidad conservadora 50kHz para estabilidad");
  } else {
    Serial.println("I2C: WARNING - Bus parece vacio, manteniendo modo seguro");
  }
}

void scanAndInitDevices() {
  if (mux_enabled) {
    Serial.println("SCAN: Usando multiplexor - escaneando todos los canales...");
    scanAllMuxChannels();
    return;
  }
  
  Serial.println("SCAN: Escaneando dispositivos (modo directo)...");
  
  device_count = 0;
  
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (device_count >= 32) {
      Serial.println("WARNING: Límite de 32 dispositivos alcanzado");
      break;
    }
    
    if (testDeviceConnection(addr)) {
      devices[device_count].address = addr;
      devices[device_count].mux_channel = 0; // Canal por defecto
      devices[device_count].total_tests = 0;
      devices[device_count].successful_tests = 0;
      devices[device_count].failed_tests = 0;
      devices[device_count].last_success_time = millis();
      devices[device_count].last_failure_time = 0;
      devices[device_count].is_online = true;
      devices[device_count].consecutive_failures = 0;
      devices[device_count].last_adc_value = 0;
      
      device_count++;
      Serial.printf("OK: Dispositivo 0x%02X inicializado\n", addr);
    }
    delay(2);
  }
  
  Serial.printf("STATUS: %d dispositivos encontrados\n", device_count);
}

void processCommand(String cmd) {
  Serial.printf("CMD: '%s'\n", cmd.c_str());
  
  if (cmd == "start") {
    continuous_test = true;
    continuous_sweep = false;  // Desactivar sweep si estaba activo
    cycle_count = 0;
    Serial.println("STATUS: Test continuo INICIADO");
    
  } else if (cmd == "sweep_start") {
    Serial.println("SWEEP_START: Preparando barrido continuo...");
    
    // Verificación previa del bus I2C
    Serial.println("SWEEP_START: Verificando estado del bus I2C...");
    Wire.beginTransmission(0x08);
    if (Wire.endTransmission() != 0) {
      Serial.println("SWEEP_START: Bus inestable - ejecutando recuperación preventiva...");
      forceI2CRecovery();
      delay(500);
    }
    
    // Verificar que hay dispositivos disponibles
    if (device_count == 0) {
      Serial.println("SWEEP_START: ERROR - No hay dispositivos detectados");
      Serial.println("SWEEP_START: Ejecute 'scan' primero para detectar dispositivos");
      return;
    }
    
    // Preparar todos los dispositivos en estado conocido
    Serial.printf("SWEEP_START: Inicializando %d dispositivos...\n", device_count);
    for (int i = 0; i < device_count; i++) {
      if (mux_enabled) {
        selectMuxChannel(devices[i].mux_channel);
        delay(5);
      }
      sendCommandSafe(devices[i].address, CMD_NEO_OFF);
      sendCommandSafe(devices[i].address, CMD_RELAY_OFF);
      delay(5);  // Tiempo entre dispositivos
    }
    
    Serial.println("SWEEP_START: Dispositivos inicializados - comenzando barrido continuo");
    continuous_sweep = true;
    continuous_test = false;  // Desactivar test si estaba activo
    cycle_count = 0;
    Serial.println("STATUS: Barrido continuo INICIADO con preparación completa");
    
  } else if (cmd == "stop") {
    continuous_test = false;
    continuous_sweep = false;
    Serial.println("STATUS: Test/Barrido continuo DETENIDO");
    
  } else if (cmd == "mux_scan") {
    Serial.println("MUX_SCAN: Ejecutando reset completo del multiplexor y recuperación del bus...");
    uint32_t start = startTiming();
    fullMuxReset();  // Usar la nueva función de reset completo
    recordTiming("FULL MUX RESET", millis() - start);
    
  } else if (cmd == "mux_scan_only") {
    Serial.println("MUX_SCAN_ONLY: Escaneo rápido sin reset completo...");
    uint32_t start = startTiming();
    scanAllMuxChannels();
    recordTiming("MUX SCAN ONLY", millis() - start);
    
  } else if (cmd == "mux_stats") {
    showMuxStatistics();
    
  } else if (cmd == "scan") {
    uint32_t start = startTiming();
    scanAndInitDevices();
    recordTiming("SCAN DEVICES", millis() - start);
    
  } else if (cmd == "sweep") {
    uint32_t start = startTiming();
    fastSweepTest();
    recordTiming("SWEEP TEST", millis() - start);
    
  } else if (cmd == "stats") {
    showStatistics();
    
  } else if (cmd == "reset stats") {
    resetStatistics();
    
  } else if (cmd.startsWith("interval ")) {
    uint32_t new_interval = cmd.substring(9).toInt();
    if (new_interval >= 100 && new_interval <= 60000) {
      test_interval = new_interval;
      Serial.printf("STATUS: Intervalo cambiado a %dms\n", test_interval);
    } else {
      Serial.println("ERROR: Intervalo debe ser entre 100-60000ms");
    }
    
  } else if (cmd.startsWith("sweep_recovery ")) {
    uint32_t new_recovery = cmd.substring(15).toInt();
    if (new_recovery >= 500 && new_recovery <= 10000) {
      sweep_recovery_time = new_recovery;
      Serial.printf("STATUS: Tiempo de recuperación de sweep cambiado a %lums\n", sweep_recovery_time);
    } else {
      Serial.println("ERROR: Tiempo de recuperación debe ser entre 500-10000ms");
    }
    
  } else if (cmd == "single") {
    uint32_t start = startTiming();
    runSingleTest();
    recordTiming("SINGLE TEST", millis() - start);
    
  } else if (cmd == "recovery") {
    uint32_t start = startTiming();
    forceI2CRecovery();
    recordTiming("BUS RECOVERY", millis() - start);
    
  } else if (cmd == "reset") {
    Serial.println("SISTEMA: Ejecutando reset del microcontrolador...");
    resetMicrocontroller(); // Esta función no retorna
    
  } else if (cmd == "all red") {
    uint32_t start = startTiming();
    setAllLEDs(CMD_NEO_RED);
    recordTiming("ALL RED", millis() - start);
    
  } else if (cmd == "all green") {
    uint32_t start = startTiming();
    setAllLEDs(CMD_NEO_GREEN);
    recordTiming("ALL GREEN", millis() - start);
    
  } else if (cmd == "all blue") {
    uint32_t start = startTiming();
    setAllLEDs(CMD_NEO_BLUE);
    recordTiming("ALL BLUE", millis() - start);
    
  } else if (cmd == "all off") {
    uint32_t start = startTiming();
    setAllLEDs(CMD_NEO_OFF);
    recordTiming("ALL OFF", millis() - start);
    
  } else if (cmd == "relay on") {
    uint32_t start = startTiming();
    setAllRelays(CMD_RELAY_ON);
    recordTiming("RELAY ON", millis() - start);
    
  } else if (cmd == "relay off") {
    uint32_t start = startTiming();
    setAllRelays(CMD_RELAY_OFF);
    recordTiming("RELAY OFF", millis() - start);
    
  } else if (cmd == "all on") {
    uint32_t start = startTiming();
    setAllDevicesState(CMD_NEO_GREEN, CMD_RELAY_ON);
    recordTiming("ALL ON", millis() - start);
    
  } else if (cmd == "all shut") {
    uint32_t start = startTiming();
    setAllDevicesState(CMD_NEO_OFF, CMD_RELAY_OFF);
    recordTiming("ALL SHUT", millis() - start);
    
  } else if (cmd == "timing") {
    showTimingStatistics();
    
  } else if (cmd == "reset timing") {
    resetTimingStatistics();
    
  } else if (cmd == "read pa4") {
    uint32_t start = startTiming();
    readAllPA4States();
    recordTiming("READ PA4", millis() - start);
    
  } else if (cmd == "check all") {
    uint32_t start = startTiming();
    checkAllDevicesWithPA4();
    recordTiming("CHECK ALL", millis() - start);
    
  } else if (cmd == "adc") {
    uint32_t start = startTiming();
    readAllADC_PA0States();
    recordTiming("READ ADC", millis() - start);
    
  } else if (cmd == "adc-profile") {
    testADC_Profile();
    
  } else if (cmd == "adc-loop") {
    Serial.println("Iniciando bucle continuo de lectura ADC...");
    Serial.println("Escriba 'stop' para detener el bucle");
    while (true) {
      readAllADC_PA0States();
      
      // Verificar si hay comando para parar
      if (Serial.available()) {
        String stop_cmd = Serial.readString();
        stop_cmd.trim();
        if (stop_cmd == "stop") {
          Serial.println("Bucle ADC detenido");
          break;
        }
      }
      
      delay(1000); // Delay de 1 segundo entre lecturas
    }
    
  } else if (cmd.startsWith("adc-single ")) {
    // Comando para leer ADC de un dispositivo específico
    // Formato: adc-single 0x08
    String addr_str = cmd.substring(11);
    addr_str.trim();
    
    uint8_t target_addr = 0;
    if (addr_str.startsWith("0x")) {
      target_addr = strtol(addr_str.c_str(), NULL, 16);
    } else {
      target_addr = addr_str.toInt();
    }
    
    if (target_addr > 0) {
      Serial.printf("Leyendo ADC del dispositivo 0x%02X:\n", target_addr);
      uint16_t adc_value = readADC_PA0(target_addr);
      
      if (adc_value == 0xFFFE) {
        Serial.println("❌ El dispositivo no soporta comandos ADC");
      } else if (adc_value == 0xFFFF) {
        Serial.println("⚠️ Error: No se pudo leer el ADC");
      } else {
        float voltage = (adc_value * 3.3) / 4095.0;
        Serial.printf("✅ Resultado: ADC=%d (0x%04X) = %.3fV\n", adc_value, adc_value, voltage);
      }
    } else {
      Serial.println("Error: Dirección inválida. Uso: adc-single 0x08");
    }
    
  } else if (cmd == "adc-test") {
    Serial.println("Probando soporte ADC en todos los dispositivos...");
    uint8_t devices_with_adc = 0;
    
    for (int i = 0; i < device_count; i++) {
      if (devices[i].is_online) {
        bool supports_adc = testADC_Support(devices[i].address);
        if (supports_adc) {
          devices_with_adc++;
        }
        delay(50);
      }
    }
    
    Serial.printf("\n📊 RESULTADO: %d/%d dispositivos soportan ADC\n", devices_with_adc, device_count);
    
  } else if (cmd == "firmware-scan") {
    scanFirmwareVersions();
    
  } else if (cmd.startsWith("firmware-id ")) {
    // Comando para identificar firmware de un dispositivo específico
    // Formato: firmware-id 0x08
    String addr_str = cmd.substring(12);
    addr_str.trim();
    
    uint8_t target_addr = 0;
    if (addr_str.startsWith("0x")) {
      target_addr = strtol(addr_str.c_str(), NULL, 16);
    } else {
      target_addr = addr_str.toInt();
    }
    
    if (target_addr > 0) {
      identifyFirmwareVersion(target_addr);
    } else {
      Serial.println("Error: Dirección inválida. Uso: firmware-id 0x08");
    }
    
  } else {
    Serial.println("ERROR: Comando no reconocido");
  }
}

void runTestCycle() {
  cycle_count++;
  uint32_t cycle_start = millis();
  
  Serial.printf("\n=== CICLO %lu ===\n", cycle_count);
  
  int online_devices = 0;
  int failed_devices = 0;
  int critical_failures = 0;
  
  for (int i = 0; i < device_count; i++) {
    DeviceStats* dev = &devices[i];
    dev->total_tests++;
    
    Serial.printf("TEST %d/%d: 0x%02X ", i+1, device_count, dev->address);
    
    bool success = testSingleDeviceQuick(dev->address);
    
    if (success) {
      dev->successful_tests++;
      dev->last_success_time = millis();
      dev->consecutive_failures = 0;
      
      if (!dev->is_online) {
        Serial.printf("RECOVERY: Dispositivo 0x%02X RECUPERADO", dev->address);
        dev->is_online = true;
      }
      
      Serial.println("OK");
      online_devices++;
      
    } else {
      dev->failed_tests++;
      dev->last_failure_time = millis();
      dev->consecutive_failures++;
      
      if (dev->is_online) {
        Serial.printf("FAIL: Dispositivo 0x%02X PERDIDO", dev->address);
        dev->is_online = false;
      }
      
      Serial.println("FAIL");
      failed_devices++;
      
      // Contar fallos criticos (mas de 5 consecutivos)
      if (dev->consecutive_failures >= 5) {
        critical_failures++;
      }
      
      // Recuperacion inmediata tras 3 fallos
      if (dev->consecutive_failures == 3) {
        Serial.printf("RECOVERY: Intentando recuperar 0x%02X...", dev->address);
        if (attemptDeviceRecovery(dev->address)) {
          Serial.println("OK");
          dev->consecutive_failures = 0;
          dev->is_online = true;
        } else {
          Serial.println("FAIL");
        }
      }
    }
    
    delay(1);  // Aumentar a 50ms entre dispositivos
  }
  
  // RECUPERACION CRITICA: Si mas del 70% fallan o hay muchos fallos criticos
  if (failed_devices > (device_count * 0.7) || critical_failures > 2) {
    Serial.println("CRITICAL: Fallos masivos detectados!");
    Serial.printf("CRITICAL: Failed:%d/%d Critical:%d\n", failed_devices, device_count, critical_failures);
    Serial.println("CRITICAL: Ejecutando recuperacion completa del bus...");
    
    forceI2CRecovery();
    total_bus_recoveries++;
    
    Serial.println("CRITICAL: Re-escaneando dispositivos tras recuperacion...");
    delay(100);
    scanAndInitDevices();
  }
  // RECUPERACION NORMAL: Si mas del 50% fallan
  else if (failed_devices > device_count / 2) {
    Serial.println("WARNING: Muchos fallos detectados, recuperacion ligera...");
    
    // Recuperacion ligera - solo reiniciar I2C
    Wire.end();
    delay(200);
    initializeI2C();
    total_bus_recoveries++;
  }
  
  uint32_t cycle_time = millis() - cycle_start;
  Serial.printf("CYCLE: %lu completado en %lums - Online:%d Fail:%d Critical:%d\n", 
    cycle_count, cycle_time, online_devices, failed_devices, critical_failures);
}

bool testSingleDeviceQuick(uint8_t addr) {
  // Test conservador para maxima estabilidad
  bool ping_ok = sendCommandFast(addr, CMD_PA4_DIGITAL);
  if (!ping_ok) return false;
  
  delay(5);  // Pausa entre comandos diferentes
  
  bool led_ok = sendCommandFast(addr, CMD_NEO_BLUE);
  delay(10);  // Tiempo para que el LED se active completamente
  bool led_off_ok = sendCommandFast(addr, CMD_NEO_OFF);
  
  delay(5);   // Pausa entre LED y relay
  
  bool relay_ok = sendCommandFast(addr, CMD_RELAY_ON);
  delay(10);  // Tiempo para que el relay se active completamente
  bool relay_off_ok = sendCommandFast(addr, CMD_RELAY_OFF);
  
  return ping_ok && led_ok && led_off_ok && relay_ok && relay_off_ok;
}

bool attemptDeviceRecovery(uint8_t addr) {
  Serial.printf("\nRECOVERY: Recuperando dispositivo 0x%02X...\n", addr);
  
  // 1. Limpieza con comandos seguros
  Serial.println("RECOVERY: Limpiando estado del dispositivo...");
  sendCommandSafe(addr, CMD_NEO_OFF);
  sendCommandSafe(addr, CMD_RELAY_OFF);
  delay(10);
  
  // 2. Test de conectividad basico
  Serial.println("RECOVERY: Probando conectividad...");
  if (testDeviceConnection(addr)) {
    Serial.println("RECOVERY: Dispositivo responde!");
    return true;
  }
  
  // 3. Recuperacion con multiples intentos
  Serial.println("RECOVERY: Multiples intentos de conexion...");
  for (int i = 0; i < 5; i++) {
    delay(100 * (i + 1));  // Pausa progresiva
    
    if (testDeviceConnection(addr)) {
      Serial.printf("RECOVERY: Dispositivo recuperado en intento %d\n", i + 1);
      return true;
    }
  }
  
  // 4. Ultimo intento con reset suave
  Serial.println("RECOVERY: Intento de reset suave...");
  Wire.beginTransmission(addr);
  Wire.write(CMD_PA4_DIGITAL);
  Wire.endTransmission();
  delay(500);
  
  bool final_result = testDeviceConnection(addr);
  Serial.printf("RECOVERY: Resultado final para 0x%02X: %s\n", 
    addr, final_result ? "EXITOSO" : "FALLIDO");
  
  return final_result;
}

void forceI2CRecovery() {
  Serial.println("RECOVERY: Forzando recuperacion COMPLETA del bus I2C...");
  
  // PASO 1: Detener I2C completamente
  Wire.end();
  delay(500);  // Pausa mas larga
  
  Serial.println("RECOVERY: Configurando pines en modo GPIO...");
  
  // PASO 2: Configurar pines como GPIO para recuperacion manual
  pinMode(SDA_PIN, OUTPUT);
  pinMode(SCL_PIN, OUTPUT);
  
  // PASO 3: Forzar ambos pines HIGH (estado idle)
  digitalWrite(SDA_PIN, HIGH);
  digitalWrite(SCL_PIN, HIGH);
  delay(100);
  
  // PASO 4: Generar multiples ciclos de clock para limpiar bus
  Serial.println("RECOVERY: Generando ciclos de limpieza...");
  for (int i = 0; i < 20; i++) {  // Mas ciclos
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(50);  // Pulsos mas lentos
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(50);
  }
  
  // PASO 5: Generar condicion de START
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(50);
  
  // PASO 6: Generar 9 pulsos de clock con SDA LOW (para limpiar datos pendientes)
  for (int i = 0; i < 9; i++) {
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(50);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(50);
  }
  
  // PASO 7: Generar condicion de STOP completa
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(50);
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(50);
  digitalWrite(SDA_PIN, HIGH);
  delayMicroseconds(100);
  
  // PASO 8: Configurar pines como INPUT_PULLUP (estado seguro)
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(200);
  
  Serial.println("RECOVERY: Re-inicializando I2C con configuracion segura...");
  
  // PASO 9: Re-inicializar I2C con configuracion mas conservadora
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeout(200);  // Timeout mas largo
  Wire.setClock(50000);  // Velocidad mas lenta para estabilidad
  
  delay(500);  // Pausa larga para estabilizacion
  
  Serial.println("RECOVERY: Bus I2C recuperado - Probando conexion...");
  
  // PASO 10: Test de conectividad basico
  int devices_found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      devices_found++;
    }
    delay(5);  // Delay entre tests
  }
  
  Serial.printf("RECOVERY: %d dispositivos detectados tras recuperacion\n", devices_found);
  
  if (devices_found > 0) {
    Serial.println("RECOVERY: Bus I2C operativo - Restaurando velocidad...");
    Wire.setClock(I2C_SPEED);  // Restaurar velocidad original
  } else {
    Serial.println("RECOVERY: WARNING - No se detectaron dispositivos");
    Serial.println("RECOVERY: Manteniendo configuracion conservadora");
  }
}

void runSingleTest() {
  Serial.println("\nSINGLE: Ejecutando test unico...");
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    Serial.printf("TEST %d/%d: 0x%02X ", i+1, device_count, addr);
    
    if (testSingleDeviceQuick(addr)) {
      Serial.println("OK");
    } else {
      Serial.println("FAIL");
    }
    delay(10);
  }
  
  Serial.println("SINGLE: Test unico completado");
}

void fastSweepTest() {
  Serial.println("\nSWEEP: Iniciando barrido ESTABLE con recuperación de bus...");
  uint32_t sweep_start = millis();
  
  // PRE-FASE: Verificación del estado del bus I2C
  Serial.println("PRE-FASE: Verificando estado del bus I2C...");
  Wire.beginTransmission(0x08);
  if (Wire.endTransmission() != 0) {
    Serial.println("WARNING: Bus I2C inestable - ejecutando recuperación preventiva...");
    forceI2CRecovery();
    delay(100);  // Pausa larga tras recuperación
  }
  
  // Fase 1: Todos AZUL (procesando) - Con recuperación entre dispositivos
  Serial.println("FASE 1: Todos AZUL (procesando) - modo ultra estable...");
  int azul_errors = 0;
  for (int i = 0; i < device_count; i++) {
    // Verificar MUX si está habilitado
    if (mux_enabled) {
      selectMuxChannel(devices[i].mux_channel);
      delay(10);  // Tiempo para estabilizar canal MUX
    }
    
    if (!sendCommandSafe(devices[i].address, CMD_NEO_BLUE)) {
      Serial.printf("ERROR: Dispositivo 0x%02X no responde en fase AZUL\n", devices[i].address);
      azul_errors++;
    }
    delay(10);  // Tiempo aumentado entre dispositivos para estabilidad
    
    // Recuperación preventiva cada 8 dispositivos
    if ((i + 1) % 8 == 0) {
      Serial.printf("SWEEP: Pausa de recuperación tras %d dispositivos...\n", i + 1);
      delay(5);
    }
  }
  
  // Verificar si hay muchos errores en fase AZUL
  if (azul_errors > device_count / 4) {
    Serial.printf("WARNING: %d errores en fase AZUL - pausando para recuperación...\n", azul_errors);
    delay(200);
  }
  
  delay(10);  // Pausa larga para visualizar azul y estabilizar bus
  
  // Fase 2: Barrido secuencial VERDE + RELAY ON
  Serial.println("FASE 2: Barrido VERDE + RELAY ON con recuperación...");
  int verde_errors = 0;
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    Serial.printf("SWEEP %d/%d: 0x%02X ", i+1, device_count, addr);
    
    // Verificar MUX si está habilitado
    if (mux_enabled) {
      selectMuxChannel(devices[i].mux_channel);
      delay(1);  // Tiempo para cambio de canal
    }
    
    // VERDE primero, luego RELAY ON (secuencial para maxima estabilidad)
    bool verde_ok = sendCommandSafe(addr, CMD_NEO_GREEN);
    delay(5);  // Pausa entre comandos del mismo dispositivo
    bool relay_ok = sendCommandSafe(addr, CMD_RELAY_ON);
    
    if (verde_ok && relay_ok) {
      Serial.println("VERDE+ON");
    } else {
      Serial.println("ERROR");
      verde_errors++;
    }
    delay(5);  // Tiempo visible aumentado para asegurar estabilidad
    
    // Recuperación preventiva cada 6 dispositivos en fase crítica
    if ((i + 1) % 6 == 0) {
      Serial.printf("SWEEP: Pausa de estabilización tras %d dispositivos...\n", i + 1);
      delay(5);
    }
  }
  
  delay(5);  // Pausa larga para ver todos verdes y estabilizar
  
  // Verificar errores antes de continuar
  if (verde_errors > device_count / 3) {
    Serial.printf("WARNING: %d errores en fase VERDE - recuperación de bus...\n", verde_errors);
    forceI2CRecovery();
    delay(500);
  }
  
  // Fase 3: Barrido secuencial ROJO + RELAY OFF
  Serial.println("FASE 3: Barrido ROJO + RELAY OFF con recuperación...");
  int rojo_errors = 0;
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    Serial.printf("SWEEP %d/%d: 0x%02X ", i+1, device_count, addr);
    
    // Verificar MUX si está habilitado
    if (mux_enabled) {
      selectMuxChannel(devices[i].mux_channel);
      delay(5);
    }
    
    // ROJO primero, luego RELAY OFF (secuencial)
    bool rojo_ok = sendCommandSafe(addr, CMD_NEO_RED);
    delay(5);  // Pausa entre comandos del mismo dispositivo
    bool relay_off_ok = sendCommandSafe(addr, CMD_RELAY_OFF);
    
    if (rojo_ok && relay_off_ok) {
      Serial.println("ROJO+OFF");
    } else {
      Serial.println("ERROR");
      rojo_errors++;
    }
    delay(5);   // Tiempo visible aumentado para estabilidad
    
    // Recuperación preventiva cada 6 dispositivos
    if ((i + 1) % 6 == 0) {
      delay(10);
    }
  }
  
  delay(10);  // Pausa larga para ver todos rojos
  
  // Fase 4: Todos OFF final con verificación
  Serial.println("FASE 4: Limpieza final con verificación...");
  int off_errors = 0;
  for (int i = 0; i < device_count; i++) {
    if (mux_enabled) {
      selectMuxChannel(devices[i].mux_channel);
      delay(5);
    }
    
    if (!sendCommandSafe(devices[i].address, CMD_NEO_OFF)) {
      Serial.printf("ERROR: No se pudo apagar LED 0x%02X\n", devices[i].address);
      off_errors++;
    }
    delay(10);   // Tiempo adecuado entre dispositivos
  }
  
  // FASE FINAL: Pausa de recuperación del bus
  Serial.println("FASE FINAL: Estabilizando bus I2C...");
  delay(100);  // Pausa para estabilización final
  
  uint32_t sweep_time = millis() - sweep_start;
  Serial.printf("SWEEP: Completado en %lums con recuperación mejorada\n", sweep_time);
  
  // Resumen de errores
  int total_errors = azul_errors + verde_errors + rojo_errors + off_errors;
  if (total_errors > 0) {
    Serial.printf("SWEEP: Errores detectados - AZUL:%d VERDE:%d ROJO:%d OFF:%d\n", 
      azul_errors, verde_errors, rojo_errors, off_errors);
    
    if (total_errors > device_count / 2) {
      Serial.println("SWEEP: CRÍTICO - Muchos errores detectados, se recomienda verificar hardware");
    }
  } else {
    Serial.println("SWEEP: PERFECTO - Sin errores detectados");
  }
  
  Serial.println("SWEEP: Modo ultra-estable activo - máxima confiabilidad de comunicación");
}

void showStatistics() {
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║              ESTADISTICAS                 ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  
  Serial.printf("CYCLES: Total ciclos ejecutados: %lu\n", cycle_count);
  Serial.printf("RECOVERY: Recuperaciones de bus: %lu\n", total_bus_recoveries);
  Serial.printf("INTERVAL: Intervalo actual: %lums\n", test_interval);
  Serial.println();
  
  Serial.println("ADDR  | TOTAL | SUCC | FAIL | RATE | STATUS | LAST_OK | LAST_FAIL");
  Serial.println("------|-------|------|------|------|--------|---------|----------");
  
  for (int i = 0; i < device_count; i++) {
    DeviceStats* dev = &devices[i];
    
    float success_rate = dev->total_tests > 0 ? 
      (float)dev->successful_tests / dev->total_tests * 100 : 0;
    
    uint32_t now = millis();
    uint32_t last_ok = dev->last_success_time > 0 ? 
      (now - dev->last_success_time) / 1000 : 9999;
    uint32_t last_fail = dev->last_failure_time > 0 ? 
      (now - dev->last_failure_time) / 1000 : 9999;
    
    String status = dev->is_online ? "ONLINE " : "OFFLINE";
    
    Serial.printf("0x%02X  | %5lu | %4lu | %4lu | %4.1f | %s | %4lus   | %4lus\n",
      dev->address, dev->total_tests, dev->successful_tests, dev->failed_tests,
      success_rate, status.c_str(), last_ok, last_fail);
  }
  Serial.println();
}

void resetStatistics() {
  for (int i = 0; i < device_count; i++) {
    devices[i].total_tests = 0;
    devices[i].successful_tests = 0;
    devices[i].failed_tests = 0;
    devices[i].consecutive_failures = 0;
    devices[i].last_success_time = millis();
    devices[i].last_failure_time = 0;
  }
  
  cycle_count = 0;
  total_bus_recoveries = 0;
  
  Serial.println("STATUS: Estadisticas reiniciadas");
}

bool testDeviceConnection(uint8_t addr) {
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() != 0) return false;
  
  Wire.beginTransmission(addr);
  Wire.write(CMD_PA4_DIGITAL);
  if (Wire.endTransmission() != 0) return false;
  
  delay(50);
  Wire.requestFrom(addr, (uint8_t)1);
  if (!Wire.available()) return false;
  
  Wire.read();
  return true;
}

bool sendCommand(uint8_t addr, uint8_t cmd) {
  for (int retry = 0; retry < 3; retry++) {  // Mas reintentos
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    uint8_t result = Wire.endTransmission();
    
    if (result == 0) {
      delay(command_delay);  // Aumentar a 50ms para dar mas tiempo
      Wire.requestFrom(addr, (uint8_t)1);
      if (Wire.available()) {
        Wire.read();
        return true;
      }
    }
    
    // Pausa progresiva mas larga entre reintentos
    delay(50 * (retry + 1));  // 50ms, 100ms, 150ms
    
    // Si falla, mostrar el error para debugging
    if (result != 0) {
      Serial.printf("[DEBUG] I2C Error 0x%02X: %d (retry %d)\n", addr, result, retry + 1);
    }
  }
  return false;
}

bool sendCommandFast(uint8_t addr, uint8_t cmd) {
  // Encontrar el canal del dispositivo si hay multiplexor
  if (mux_enabled) {
    for (int i = 0; i < device_count; i++) {
      if (devices[i].address == addr) {
        selectMuxChannel(devices[i].mux_channel);
        delay(1); // Tiempo mínimo para cambio de canal
        break;
      }
    }
  }
  
  // Version "rapida" pero ahora mas conservadora
  for (int attempt = 0; attempt < 3; attempt++) {  // Mas intentos
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    
    uint8_t result = Wire.endTransmission();
    if (result == 0) {
      delay(command_delay);  // Aumentar a 50ms minimo
      
      Wire.requestFrom(addr, (uint8_t)1);
      if (Wire.available()) {
        Wire.read();
        return true;
      }
    }
    
    // Pausa mas larga antes del retry
    delay(50 * (attempt + 1));  // 50ms, 100ms, 150ms
  }
  return false;
}

bool sendCommandSafe(uint8_t addr, uint8_t cmd) {
  // Encontrar el canal del dispositivo si hay multiplexor
  if (mux_enabled) {
    for (int i = 0; i < device_count; i++) {
      if (devices[i].address == addr) {
        selectMuxChannel(devices[i].mux_channel);
        delay(2); // Tiempo para cambio de canal
        break;
      }
    }
  }
  
  // Version ultra segura para recuperacion
  for (int attempt = 0; attempt < 5; attempt++) {  // Muchos mas intentos
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    
    uint8_t result = Wire.endTransmission();
    if (result == 0) {
      delay(command_delay);  // Delay muy largo para estabilidad maxima
      
      Wire.requestFrom(addr, (uint8_t)1);
      if (Wire.available()) {
        Wire.read();
        return true;
      }
    }
    
    // Pausa muy larga y progresiva entre reintentos
    delay(100 * (attempt + 1));  // 100ms, 200ms, 300ms, 400ms, 500ms
  }
  return false;
}

// ═══════════════════════════════════════════════════════════
//                    FUNCIONES DE TEST GRUPAL
// ═══════════════════════════════════════════════════════════

void setAllLEDs(uint8_t led_cmd) {
  String color_name;
  switch(led_cmd) {
    case CMD_NEO_RED:   color_name = "ROJO"; break;
    case CMD_NEO_GREEN: color_name = "VERDE"; break;
    case CMD_NEO_BLUE:  color_name = "AZUL"; break;
    case CMD_NEO_OFF:   color_name = "OFF"; break;
    default:            color_name = "DESCONOCIDO"; break;
  }
  
  Serial.printf("GROUP LED: Configurando todos los LEDs en %s...\n", color_name.c_str());
  uint32_t start_time = millis();
  
  int success_count = 0;
  int fail_count = 0;
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    Serial.printf("LED %d/%d: 0x%02X ", i+1, device_count, addr);
    
    if (sendCommandSafe(addr, led_cmd)) {
      Serial.println("OK");
      success_count++;
    } else {
      Serial.println("FAIL");
      fail_count++;
    }
    delay(1);  // Pausa entre dispositivos para estabilidad
  }
  
  uint32_t total_time = millis() - start_time;
  Serial.printf("GROUP LED: %s completado en %lums - Exitosos:%d Fallos:%d\n", 
    color_name.c_str(), total_time, success_count, fail_count);
}

void setAllRelays(uint8_t relay_cmd) {
  String state_name = (relay_cmd == CMD_RELAY_ON) ? "ENCENDER" : "APAGAR";
  
  Serial.printf("GROUP RELAY: %s todos los relays...\n", state_name.c_str());
  uint32_t start_time = millis();
  
  int success_count = 0;
  int fail_count = 0;
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    Serial.printf("RELAY %d/%d: 0x%02X ", i+1, device_count, addr);
    
    if (sendCommandSafe(addr, relay_cmd)) {
      Serial.println("OK");
      success_count++;
    } else {
      Serial.println("FAIL");
      fail_count++;
    }
    delay(1);  // Pausa entre dispositivos para estabilidad
  }
  
  uint32_t total_time = millis() - start_time;
  Serial.printf("GROUP RELAY: %s completado en %lums - Exitosos:%d Fallos:%d\n", 
    state_name.c_str(), total_time, success_count, fail_count);
}

void setAllDevicesState(uint8_t led_cmd, uint8_t relay_cmd) {
  String led_name, relay_name;
  
  switch(led_cmd) {
    case CMD_NEO_RED:   led_name = "ROJO"; break;
    case CMD_NEO_GREEN: led_name = "VERDE"; break;
    case CMD_NEO_BLUE:  led_name = "AZUL"; break;
    case CMD_NEO_OFF:   led_name = "OFF"; break;
    default:            led_name = "DESCONOCIDO"; break;
  }
  
  relay_name = (relay_cmd == CMD_RELAY_ON) ? "ON" : "OFF";
  
  Serial.printf("GROUP FULL: Configurando LED=%s + RELAY=%s...\n", led_name.c_str(), relay_name.c_str());
  uint32_t start_time = millis();
  
  int success_count = 0;
  int fail_count = 0;
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    Serial.printf("DEVICE %d/%d: 0x%02X ", i+1, device_count, addr);
    
    // Enviar comando LED primero
    bool led_ok = sendCommandSafe(addr, led_cmd);
    delay(command_delay);  // Pausa entre comandos del mismo dispositivo
    
    // Luego comando RELAY
    bool relay_ok = sendCommandSafe(addr, relay_cmd);
    
    if (led_ok && relay_ok) {
      Serial.println("OK");
      success_count++;
    } else {
      Serial.printf("FAIL (LED:%s RELAY:%s)\n", led_ok ? "OK" : "FAIL", relay_ok ? "OK" : "FAIL");
      fail_count++;
    }
    
    delay(75);  // Pausa mas larga entre dispositivos para comandos duales
  }
  
  uint32_t total_time = millis() - start_time;
  Serial.printf("GROUP FULL: LED=%s + RELAY=%s completado en %lums - Exitosos:%d Fallos:%d\n", 
    led_name.c_str(), relay_name.c_str(), total_time, success_count, fail_count);
}

// ═══════════════════════════════════════════════════════════
//                    SISTEMA DE MEDICION DE TIEMPOS
// ═══════════════════════════════════════════════════════════

uint32_t startTiming() {
  return millis();
}

void recordTiming(String operation, uint32_t execution_time) {
  last_operation_time = execution_time;
  
  // Buscar si ya existe esta operación
  int index = -1;
  for (int i = 0; i < timing_count; i++) {
    if (timing_stats[i].operation_name == operation) {
      index = i;
      break;
    }
  }
  
  // Si no existe, crear nueva entrada
  if (index == -1 && timing_count < 10) {
    index = timing_count;
    timing_stats[index].operation_name = operation;
    timing_stats[index].min_time = execution_time;
    timing_stats[index].max_time = execution_time;
    timing_stats[index].total_time = 0;
    timing_stats[index].count = 0;
    timing_count++;
  }
  
  // Actualizar estadísticas
  if (index >= 0) {
    timing_stats[index].total_time += execution_time;
    timing_stats[index].count++;
    
    if (execution_time < timing_stats[index].min_time) {
      timing_stats[index].min_time = execution_time;
    }
    if (execution_time > timing_stats[index].max_time) {
      timing_stats[index].max_time = execution_time;
    }
  }
  
  // Mostrar tiempo inmediato
  Serial.printf("⏱️  TIEMPO: %s ejecutado en %lums\n", operation.c_str(), execution_time);
}

void showTimingStatistics() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║                    ESTADISTICAS DE TIEMPO                  ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  
  if (timing_count == 0) {
    Serial.println("No hay datos de tiempo registrados.");
    Serial.println("Ejecuta algunos comandos para ver estadísticas de tiempo.");
    return;
  }
  
  Serial.printf("ULTIMA OPERACION: %lums\n", last_operation_time);
  Serial.println();
  
  Serial.println("OPERACION         | COUNT | MIN(ms) | MAX(ms) | AVG(ms) | TOTAL(ms)");
  Serial.println("------------------|-------|---------|---------|---------|----------");
  
  for (int i = 0; i < timing_count; i++) {
    TimingStats* stat = &timing_stats[i];
    uint32_t avg_time = stat->count > 0 ? stat->total_time / stat->count : 0;
    
    Serial.printf("%-17s | %5lu | %7lu | %7lu | %7lu | %9lu\n",
      stat->operation_name.c_str(),
      stat->count,
      stat->min_time,
      stat->max_time,
      avg_time,
      stat->total_time);
  }
  
  Serial.println();
  
  // Análisis de rendimiento
  Serial.println("═══ ANALISIS DE RENDIMIENTO ═══");
  
  for (int i = 0; i < timing_count; i++) {
    TimingStats* stat = &timing_stats[i];
    uint32_t avg_time = stat->count > 0 ? stat->total_time / stat->count : 0;
    
    Serial.printf("%s: ", stat->operation_name.c_str());
    
    if (avg_time < 100) {
      Serial.println("EXCELENTE (< 100ms)");
    } else if (avg_time < 500) {
      Serial.println("BUENO (< 500ms)");
    } else if (avg_time < 1000) {
      Serial.println("ACEPTABLE (< 1s)");
    } else if (avg_time < 5000) {
      Serial.println("LENTO (< 5s)");
    } else {
      Serial.println("MUY LENTO (> 5s)");
    }
  }
  Serial.println();
}

void resetTimingStatistics() {
  for (int i = 0; i < timing_count; i++) {
    timing_stats[i].total_time = 0;
    timing_stats[i].count = 0;
    timing_stats[i].min_time = 0;
    timing_stats[i].max_time = 0;
    timing_stats[i].operation_name = "";
  }
  
  timing_count = 0;
  last_operation_time = 0;
  
  Serial.println("STATUS: Estadísticas de tiempo reiniciadas");
}

// ═══════════════════════════════════════════════════════════
//                    LECTURA DE ESTADOS PA4
// ═══════════════════════════════════════════════════════════

uint8_t readPA4State(uint8_t addr) {
  // Función mejorada para leer el estado PA4 con máscara de 4 bits superiores
  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(addr);
    Wire.write(CMD_PA4_DIGITAL);
    
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
      if (attempt == 2) {
        Serial.printf("[PA4] Error de transmisión 0x%02X: código %d\n", addr, result);
        return 0xFF; // Valor de error
      }
      delay(10);
      continue;
    }
    
    delay(30); // Tiempo para que el dispositivo procese
    
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available()) {
      uint8_t raw_value = Wire.read();
      return raw_value; // Retornar valor completo para análisis
    } else {
      if (attempt == 2) {
        Serial.printf("[PA4] Sin respuesta de 0x%02X\n", addr);
        return 0xFF; // Valor de error
      }
      delay(10);
    }
  }
  return 0xFF; // Error tras todos los intentos
}

bool interpretPA4Value(uint8_t raw_value) {
  // Aplicar máscara de 4 bits superiores para obtener el estado real del PA4
  uint8_t pa4_state = raw_value & 0xF0; // Máscara de 4 bits: 11110000
  
  // Interpretar: si los 4 bits superiores son 0xF0, entonces HIGH
  // Si son 0x00, entonces LOW
  return (pa4_state == 0xF0);
}

// ═══════════════════════════════════════════════════════════
//                    LECTURA ADC PA0 (NUEVA FUNCIÓN)
// ═══════════════════════════════════════════════════════════

uint16_t readADC_PA0(uint8_t addr) {
  // Función para leer el ADC PA0 usando HSB y LSB
  uint8_t hsb_value = 0, lsb_value = 0;
  
  // Leer HSB (High Significant Byte)
  for (int attempt = 0; attempt < 2; attempt++) { // Reducido a 2 intentos para ser más rápido
    Wire.beginTransmission(addr);
    Wire.write(CMD_ADC_PA0_HSB);
    
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
      if (result == 4) {
        // NACK - El dispositivo no soporta este comando
        if (attempt == 1) {
          Serial.printf("[ADC] 0x%02X: Comando HSB no soportado (NACK)\n", addr);
          return 0xFFFE; // Valor especial para "comando no soportado"
        }
      } else {
        if (attempt == 1) {
          Serial.printf("[ADC] Error HSB transmisión 0x%02X: código %d\n", addr, result);
          return 0xFFFF; // Valor de error
        }
      }
      delay(5); // Delay más corto
      continue;
    }
    
    delay(20); // Tiempo reducido para que el dispositivo procese
    
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available()) {
      hsb_value = Wire.read();
      break; // Éxito
    } else {
      if (attempt == 1) {
        Serial.printf("[ADC] Sin respuesta HSB de 0x%02X\n", addr);
        return 0xFFFF; // Valor de error
      }
      delay(5);
    }
  }
  
  // Leer LSB (Low Significant Byte)  
  for (int attempt = 0; attempt < 2; attempt++) { // Reducido a 2 intentos
    Wire.beginTransmission(addr);
    Wire.write(CMD_ADC_PA0_LSB);
    
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
      if (result == 4) {
        // NACK - El dispositivo no soporta este comando
        if (attempt == 1) {
          Serial.printf("[ADC] 0x%02X: Comando LSB no soportado (NACK)\n", addr);
          return 0xFFFE; // Valor especial para "comando no soportado"
        }
      } else {
        if (attempt == 1) {
          Serial.printf("[ADC] Error LSB transmisión 0x%02X: código %d\n", addr, result);
          return 0xFFFF; // Valor de error
        }
      }
      delay(5);
      continue;
    }
    
    delay(20); // Tiempo reducido
    
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available()) {
      lsb_value = Wire.read();
      break; // Éxito
    } else {
      if (attempt == 1) {
        Serial.printf("[ADC] Sin respuesta LSB de 0x%02X\n", addr);
        return 0xFFFF; // Valor de error
      }
      delay(5);
    }
  }
  
  // Combinar HSB y LSB para formar valor de 16 bits
  uint16_t adc_value = (hsb_value << 8) | lsb_value;
  
  Serial.printf("[ADC] 0x%02X: HSB=0x%02X, LSB=0x%02X, ADC=%d (0x%04X)\n", 
                addr, hsb_value, lsb_value, adc_value, adc_value);
  
  return adc_value;
}

void readAllADC_PA0States() {
  // Función para leer ADC de todos los dispositivos conectados
  Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                    LECTURA ADC PA0 - TODOS LOS DISPOSITIVOS  ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  
  uint8_t devices_with_adc = 0;
  uint8_t devices_no_support = 0;
  uint8_t devices_error = 0;
  
  for (int i = 0; i < device_count; i++) {
    if (devices[i].is_online) {
      Serial.printf("Dispositivo 0x%02X: ", devices[i].address);
      
      uint16_t adc_value = readADC_PA0(devices[i].address);
      
      if (adc_value == 0xFFFE) {
        // Comando no soportado
        devices_no_support++;
        Serial.println("❌ ADC NO SOPORTADO");
        devices[i].last_adc_value = 0;
      } else if (adc_value == 0xFFFF) {
        // Error de comunicación
        devices_error++;
        Serial.println("⚠️ ERROR DE COMUNICACIÓN");
        devices[i].last_adc_value = 0;
      } else {
        // Lectura exitosa
        devices_with_adc++;
        devices[i].last_adc_value = adc_value;
        
        // Convertir a voltaje (asumiendo referencia de 3.3V y ADC de 12 bits)
        // Nota: valores altos sugieren posible problema de escalado en firmware
        float voltage = (adc_value * 3.3) / 4095.0;
        
        Serial.printf("✅ ADC: %d (0x%04X) = %.3fV\n", adc_value, adc_value, voltage);
      }
      
      delay(command_delay);
    }
  }
  
  // Mostrar resumen de resultados
  Serial.println("─────────────────────────────────────────────────────────────");
  Serial.printf("📊 RESUMEN: %d dispositivos online\n", device_count);
  Serial.printf("   ✅ Con ADC funcionando: %d\n", devices_with_adc);
  Serial.printf("   ❌ Sin soporte ADC: %d\n", devices_no_support);
  Serial.printf("   ⚠️ Errores de comunicación: %d\n", devices_error);
  
  if (devices_with_adc == 0) {
    Serial.println("\n⚠️ NOTA: Ningún dispositivo soporta comandos ADC PA0");
    Serial.println("   Verifique que el firmware de los dispositivos incluya:");
    Serial.println("   - CMD_ADC_PA0_HSB (0xD8)");
    Serial.println("   - CMD_ADC_PA0_LSB (0xD9)");
  }
  
  Serial.println("─────────────────────────────────────────────────────────────");
}

bool testADC_Support(uint8_t addr) {
  // Función rápida para probar si un dispositivo soporta comandos ADC
  Serial.printf("Probando soporte ADC en 0x%02X... ", addr);
  
  Wire.beginTransmission(addr);
  Wire.write(CMD_ADC_PA0_HSB);
  uint8_t result = Wire.endTransmission();
  
  if (result == 4) {
    Serial.println("❌ No soportado (NACK)");
    return false;
  } else if (result != 0) {
    Serial.printf("⚠️ Error de comunicación (código %d)\n", result);
    return false;
  }
  
  delay(20);
  Wire.requestFrom(addr, (uint8_t)1);
  if (Wire.available()) {
    Wire.read(); // Descartar valor
    Serial.println("✅ Soportado");
    return true;
  } else {
    Serial.println("⚠️ Sin respuesta");
    return false;
  }
}

void identifyFirmwareVersion(uint8_t addr) {
  // Función para identificar la versión de firmware probando comandos específicos
  Serial.printf("🔍 Identificando firmware de 0x%02X: ", addr);
  
  // Test 1: Probar comando ADC (firmware v5_4_lite)
  Wire.beginTransmission(addr);
  Wire.write(CMD_ADC_PA0_HSB);
  uint8_t adc_result = Wire.endTransmission();
  
  // Test 2: Probar comando PA4_DIGITAL (firmware v5+)
  Wire.beginTransmission(addr);
  Wire.write(CMD_PA4_DIGITAL);
  uint8_t pa4_result = Wire.endTransmission();
  
  // Test 3: Probar comando básico (cualquier firmware)
  Wire.beginTransmission(addr);
  Wire.write(CMD_RELAY_OFF);
  uint8_t basic_result = Wire.endTransmission();
  
  if (adc_result == 0) {
    Serial.print("✅ firmware_v5_4_lite (con ADC)");
  } else if (pa4_result == 0) {
    Serial.print("⚠️ firmware_v5.x (sin ADC)");
  } else if (basic_result == 0) {
    Serial.print("❌ firmware_v4.x o anterior (solo básico)");
  } else {
    Serial.print("❓ Versión desconocida o error de comunicación");
  }
  
  Serial.println();
  delay(50); // Pequeño delay entre tests
}

void scanFirmwareVersions() {
  // Función para escanear versiones de firmware de todos los dispositivos
  Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                  IDENTIFICACIÓN DE FIRMWARE                  ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  
  uint8_t v5_4_lite_count = 0;
  uint8_t v5_basic_count = 0;
  uint8_t v4_old_count = 0;
  uint8_t unknown_count = 0;
  
  for (int i = 0; i < device_count; i++) {
    if (devices[i].is_online) {
      // Identificar versión
      Serial.printf("Dispositivo 0x%02X: ", devices[i].address);
      
      // Test ADC (v5_4_lite)
      Wire.beginTransmission(devices[i].address);
      Wire.write(CMD_ADC_PA0_HSB);
      uint8_t adc_result = Wire.endTransmission();
      
      if (adc_result == 0) {
        Serial.println("✅ firmware_v5_4_lite (SOPORTE ADC)");
        v5_4_lite_count++;
      } else {
        // Test PA4 (v5.x sin ADC)
        Wire.beginTransmission(devices[i].address);
        Wire.write(CMD_PA4_DIGITAL);
        uint8_t pa4_result = Wire.endTransmission();
        
        if (pa4_result == 0) {
          Serial.println("⚠️ firmware_v5.x (sin ADC)");
          v5_basic_count++;
        } else {
          // Test básico (v4.x)
          Wire.beginTransmission(devices[i].address);
          Wire.write(CMD_RELAY_OFF);
          uint8_t basic_result = Wire.endTransmission();
          
          if (basic_result == 0) {
            Serial.println("❌ firmware_v4.x (solo comandos básicos)");
            v4_old_count++;
          } else {
            Serial.println("❓ Versión desconocida");
            unknown_count++;
          }
        }
      }
      
      delay(100); // Delay entre dispositivos
    }
  }
  
  // Mostrar resumen
  Serial.println("─────────────────────────────────────────────────────────────");
  Serial.printf("📊 RESUMEN DE FIRMWARE (%d dispositivos):\n", device_count);
  Serial.printf("   ✅ firmware_v5_4_lite (con ADC): %d\n", v5_4_lite_count);
  Serial.printf("   ⚠️ firmware_v5.x (sin ADC): %d\n", v5_basic_count);
  Serial.printf("   ❌ firmware_v4.x (básico): %d\n", v4_old_count);
  Serial.printf("   ❓ Desconocido/Error: %d\n", unknown_count);
  
  if (v5_4_lite_count == 0) {
    Serial.println("\n🚨 NINGÚN DISPOSITIVO tiene firmware_v5_4_lite");
    Serial.println("   Para usar ADC necesitas flashear firmware_v5_4_lite");
    Serial.println("   Comando: make PROJECT=firmware_v5_4_lite flash");
  } else {
    Serial.printf("\n🎯 %d dispositivos están listos para ADC\n", v5_4_lite_count);
  }
  
  Serial.println("─────────────────────────────────────────────────────────────");
}

void testADC_Profile() {
  // Perfil específico para test de ADC PA0
  Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                      PERFIL TEST ADC PA0                     ║");
  Serial.println("║   Comando: 'adc' para leer una vez, 'adc-loop' para continuo ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  
  // Mostrar dispositivos disponibles y probar soporte ADC
  Serial.printf("Dispositivos disponibles (%d):\n", device_count);
  
  uint8_t devices_with_adc = 0;
  for (int i = 0; i < device_count; i++) {
    if (devices[i].is_online) {
      Serial.printf("  - 0x%02X (Canal MUX: %d): ", devices[i].address, devices[i].mux_channel);
      bool supports_adc = testADC_Support(devices[i].address);
      if (supports_adc) {
        devices_with_adc++;
      }
      delay(100); // Pequeño delay entre tests
    }
  }
  
  Serial.println("\n📊 RESUMEN DE COMPATIBILIDAD ADC:");
  Serial.printf("   ✅ Dispositivos con ADC: %d/%d\n", devices_with_adc, device_count);
  
  if (devices_with_adc == 0) {
    Serial.println("\n⚠️ ADVERTENCIA: Ningún dispositivo soporta comandos ADC");
    Serial.println("   Los comandos ADC solo funcionarán con firmware que incluya:");
    Serial.println("   - CMD_ADC_PA0_HSB (0xD8) - Byte alto del ADC");
    Serial.println("   - CMD_ADC_PA0_LSB (0xD9) - Byte bajo del ADC");
  }
  
  Serial.println("\nComandos disponibles:");
  Serial.println("  adc          - Leer ADC una vez de todos los dispositivos");
  Serial.println("  adc-loop     - Leer ADC continuo (usar 'stop' para parar)");
  Serial.println("  adc-single   - Leer ADC de un dispositivo específico");
  Serial.println("  adc-test     - Probar soporte ADC en todos los dispositivos");
  Serial.println("  back         - Volver al menú principal");
}

void readAllPA4States() {
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║           LECTURA ESTADOS PA4             ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  
  uint32_t start_time = millis();
  int success_count = 0;
  int fail_count = 0;
  int high_count = 0;
  int low_count = 0;
  
  Serial.println("ADDR  | RAW_VAL | 4BIT | PA4_STATE | INTERPRETACION");
  Serial.println("------|---------|------|-----------|--------------------");
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    uint8_t raw_value = readPA4State(addr);
    
    String status, interpretation, pa4_state_str;
    
    if (raw_value == 0xFF) {
      status = "ERROR";
      pa4_state_str = "ERR";
      interpretation = "Sin comunicacion";
      fail_count++;
    } else {
      success_count++;
      
      // Aplicar máscara de 4 bits superiores
      uint8_t masked_value = raw_value & 0xF0;
      bool is_high = interpretPA4Value(raw_value);
      
      pa4_state_str = is_high ? "HIGH" : "LOW";
      
      // Interpretar el estado con la máscara
      if (masked_value == 0xF0) {
        interpretation = "HIGH (3.3V)";
        high_count++;
      } else if (masked_value == 0x00) {
        interpretation = "LOW (0V)";
        low_count++;
      } else {
        interpretation = String("Inusual: 0x") + String(masked_value, HEX);
      }
    }
    
    Serial.printf("0x%02X  | 0x%02X    | 0x%01X  | %-9s | %s\n", 
      addr, raw_value, (raw_value & 0xF0) >> 4, pa4_state_str.c_str(), interpretation.c_str());
    
    delay(command_delay); // Pausa entre lecturas para estabilidad
  }
  
  uint32_t total_time = millis() - start_time;
  Serial.println();
  Serial.printf("RESUMEN PA4: %d exitosos, %d fallos en %lums\n", 
    success_count, fail_count, total_time);
  Serial.printf("ESTADOS: %d HIGH, %d LOW\n", high_count, low_count);
  
  // Análisis de patrones
  if (success_count > 0) {
    Serial.println("\n═══ ANALISIS DE PATRONES ═══");
    float high_percentage = (float)high_count / success_count * 100;
    Serial.printf("Distribución: %.1f%% HIGH, %.1f%% LOW\n", 
      high_percentage, 100.0 - high_percentage);
      
    if (high_count == success_count) {
      Serial.println("PATRON: Todos los pines PA4 en estado HIGH");
    } else if (low_count == success_count) {
      Serial.println("PATRON: Todos los pines PA4 en estado LOW");
    } else {
      Serial.println("PATRON: Estados mixtos - verificar configuración de pines");
    }
  }
}

void checkAllDevicesWithPA4() {
  Serial.println("\n╔════════════════════════════════════════════════════════════╗");
  Serial.println("║              REVISION COMPLETA CON PA4                     ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  
  uint32_t start_time = millis();
  int total_success = 0;
  int total_fail = 0;
  
  Serial.println("ADDR  | PING | LED  | RELAY | PA4_VAL | PA4_INT    | ESTADO_GENERAL");
  Serial.println("------|------|------|-------|---------|------------|---------------");
  
  for (int i = 0; i < device_count; i++) {
    uint8_t addr = devices[i].address;
    
    // Test 1: Ping básico
    Wire.beginTransmission(addr);
    bool ping_ok = (Wire.endTransmission() == 0);
    
    // Test 2: Comando LED (azul temporal)
    bool led_ok = false;
    if (ping_ok) {
      led_ok = sendCommandFast(addr, CMD_NEO_BLUE);
      delay(10);
      sendCommandFast(addr, CMD_NEO_OFF); // Apagar inmediatamente
    }
    
    // Test 3: Comando Relay (on/off rápido)
    bool relay_ok = false;
    if (ping_ok) {
      relay_ok = sendCommandFast(addr, CMD_RELAY_ON);
      delay(10);
      sendCommandFast(addr, CMD_RELAY_OFF); // Apagar inmediatamente
    }
    
    // Test 4: Lectura PA4 con máscara
    uint8_t raw_pa4_value = readPA4State(addr);
    String pa4_interpretation;
    bool pa4_valid = false;
    
    if (raw_pa4_value == 0xFF) {
      pa4_interpretation = "ERROR";
    } else {
      pa4_valid = true;
      uint8_t masked_value = raw_pa4_value & 0xF0; // Aplicar máscara de 4 bits superiores
      bool is_high = interpretPA4Value(raw_pa4_value);
      
      if (masked_value == 0xF0) {
        pa4_interpretation = "HIGH";
      } else if (masked_value == 0x00) {
        pa4_interpretation = "LOW";
      } else {
        pa4_interpretation = String("0x") + String(masked_value, HEX);
      }
    }
    
    // Estado general con nueva lógica PA4
    String estado_general;
    if (ping_ok && led_ok && relay_ok && pa4_valid) {
      estado_general = "PERFECTO";
      total_success++;
    } else if (ping_ok && (led_ok || relay_ok) && pa4_valid) {
      estado_general = "PARCIAL";
      total_success++;
    } else if (ping_ok) {
      estado_general = "SOLO_PING";
      total_fail++;
    } else {
      estado_general = "OFFLINE";
      total_fail++;
    }
    
    Serial.printf("0x%02X  | %s | %s | %s  | 0x%02X    | %-10s | %s\n",
      addr,
      ping_ok ? "OK  " : "FAIL",
      led_ok ? "OK  " : "FAIL", 
      relay_ok ? "OK  " : "FAIL",
      raw_pa4_value,
      pa4_interpretation.c_str(),
      estado_general.c_str());
    
    delay(50); // Pausa entre dispositivos para estabilidad
  }
  
  uint32_t total_time = millis() - start_time;
  Serial.println();
  Serial.printf("REVISION COMPLETA: %d exitosos, %d con problemas en %lums\n", 
    total_success, total_fail, total_time);
  
  // Análisis detallado
  Serial.println("\n═══ ANALISIS DETALLADO ═══");
  float success_rate = device_count > 0 ? (float)total_success / device_count * 100 : 0;
  
  Serial.printf("Tasa de éxito: %.1f%% (%d/%d)\n", success_rate, total_success, device_count);
  
  if (success_rate >= 90) {
    Serial.println("RED: EXCELENTE - Sistema funcionando perfectamente");
  } else if (success_rate >= 75) {
    Serial.println("RED: BUENA - Algunos dispositivos con problemas menores");
  } else if (success_rate >= 50) {
    Serial.println("RED: REGULAR - Varios dispositivos requieren atención");
  } else {
    Serial.println("RED: CRITICA - Mayoría de dispositivos con problemas");
  }
}

// ═══════════════════════════════════════════════════════════
//            FUNCIONES DEL MULTIPLEXOR TCA9545A
// ═══════════════════════════════════════════════════════════

bool initializeMux() {
  Serial.println("MUX: Probando multiplexor TCA9545A...");
  
  // Probar conexión con el multiplexor
  Wire.beginTransmission(MUX_ADDRESS);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.printf("MUX: TCA9545A encontrado en 0x%02X\n", MUX_ADDRESS);
    
    // Probar selección de canales
    bool all_channels_ok = true;
    for (uint8_t ch = 0; ch < MUX_CHANNELS; ch++) {
      selectMuxChannel(ch);
      delay(10);
      
      Wire.beginTransmission(MUX_ADDRESS);
      Wire.write(1 << ch);
      if (Wire.endTransmission() != 0) {
        Serial.printf("MUX: ERROR en canal %d\n", ch);
        all_channels_ok = false;
      }
    }
    
    if (all_channels_ok) {
      Serial.println("MUX: Todos los canales operativos");
      selectMuxChannel(0); // Seleccionar canal 0 por defecto
      return true;
    } else {
      Serial.println("MUX: ERROR - Algunos canales fallan");
      return false;
    }
  } else {
    Serial.println("MUX: No se encontró multiplexor TCA9545A");
    return false;
  }
}

void selectMuxChannel(uint8_t channel) {
  if (channel > 3) {
    Serial.printf("MUX: ERROR - Canal %d inválido (0-3)\n", channel);
    return;
  }
  
  if (!mux_enabled) return;
  
  Wire.beginTransmission(MUX_ADDRESS);
  Wire.write(1 << channel);
  Wire.endTransmission();
  current_mux_channel = channel;
}

bool testMuxConnection() {
  if (!mux_enabled) return false;
  
  Wire.beginTransmission(MUX_ADDRESS);
  return (Wire.endTransmission() == 0);
}

void scanAllMuxChannels() {
  if (!mux_enabled) {
    Serial.println("MUX: Multiplexor no habilitado - ejecutando escaneo directo");
    scanAndInitDevices();
    return;
  }
  
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║           ESCANEO MULTIPLEXOR             ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  
  device_count = 0;
  
  for (uint8_t channel = 0; channel < MUX_CHANNELS; channel++) {
    Serial.printf("\nMUX: Escaneando canal %d...\n", channel);
    selectMuxChannel(channel);
    delay(50); // Tiempo para estabilizar canal
    
    int devices_found = 0;
    
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
      if (addr == MUX_ADDRESS) continue; // Saltar dirección del multiplexor
      
      Wire.beginTransmission(addr);
      uint8_t error = Wire.endTransmission();
      
      if (error == 0 && device_count < 32) {
        devices[device_count].address = addr;
        devices[device_count].mux_channel = channel;
        devices[device_count].total_tests = 0;
        devices[device_count].successful_tests = 0;
        devices[device_count].failed_tests = 0;
        devices[device_count].last_success_time = millis();
        devices[device_count].last_failure_time = 0;
        devices[device_count].is_online = true;
        devices[device_count].consecutive_failures = 0;
        devices[device_count].last_adc_value = 0;
        
        Serial.printf("OK: Canal %d - Dispositivo 0x%02X\n", channel, addr);
        device_count++;
        devices_found++;
      }
      delay(2);
    }
    
    Serial.printf("MUX: Canal %d - %d dispositivos encontrados\n", channel, devices_found);
  }
  
  Serial.printf("\nMUX: Total %d dispositivos en %d canales\n", device_count, MUX_CHANNELS);
}

void showMuxStatistics() {
  if (!mux_enabled) {
    Serial.println("MUX: Multiplexor no habilitado");
    showStatistics();
    return;
  }
  
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║         ESTADÍSTICAS MULTIPLEXOR          ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  
  Serial.printf("CYCLES: Total ciclos ejecutados: %lu\n", cycle_count);
  Serial.printf("RECOVERY: Recuperaciones de bus: %lu\n", total_bus_recoveries);
  Serial.printf("INTERVAL: Intervalo actual: %lums\n", test_interval);
  Serial.println();
  
  // Estadísticas por canal
  for (uint8_t ch = 0; ch < MUX_CHANNELS; ch++) {
    Serial.printf("\n=== CANAL MUX %d ===\n", ch);
    Serial.println("ADDR  | TOTAL | SUCC | FAIL | RATE | STATUS | LAST_OK | LAST_FAIL");
    Serial.println("------|-------|------|------|------|--------|---------|----------");
    
    bool channel_has_devices = false;
    
    for (int i = 0; i < device_count; i++) {
      DeviceStats* dev = &devices[i];
      
      if (dev->mux_channel != ch) continue;
      channel_has_devices = true;
      
      float success_rate = dev->total_tests > 0 ? 
        (float)dev->successful_tests / dev->total_tests * 100 : 0;
      
      uint32_t now = millis();
      uint32_t last_ok = dev->last_success_time > 0 ? 
        (now - dev->last_success_time) / 1000 : 9999;
      uint32_t last_fail = dev->last_failure_time > 0 ? 
        (now - dev->last_failure_time) / 1000 : 9999;
      
      String status = dev->is_online ? "ONLINE " : "OFFLINE";
      
      Serial.printf("0x%02X  | %5lu | %4lu | %4lu | %4.1f | %s | %4lus   | %4lus\n",
        dev->address, dev->total_tests, dev->successful_tests, dev->failed_tests,
        success_rate, status.c_str(), last_ok, last_fail);
    }
    
    if (!channel_has_devices) {
      Serial.println("(Sin dispositivos)");
    }
  }
  Serial.println();
}

// ═══════════════════════════════════════════════════════════
//        RESET COMPLETO DEL MULTIPLEXOR Y RECUPERACIÓN
// ═══════════════════════════════════════════════════════════

void fullMuxReset() {
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║      RESET COMPLETO DEL MULTIPLEXOR       ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  
  // PASO 1: Detener cualquier operación continua
  if (continuous_test || continuous_sweep) {
    Serial.println("RESET MUX: Deteniendo operaciones continuas...");
    continuous_test = false;
    continuous_sweep = false;
    delay(500);
  }
  
  // PASO 2: Recuperación completa del bus I2C
  Serial.println("RESET MUX: Iniciando recuperación completa del bus I2C...");
  forceI2CRecovery();
  total_bus_recoveries++;
  delay(1000);
  
  // PASO 3: Reset del multiplexor (desactivar todos los canales)
  Serial.println("RESET MUX: Reseteando multiplexor TCA9545A...");
  mux_enabled = false;  // Desactivar temporalmente
  
  // Intentar reset del multiplexor
  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(MUX_ADDRESS);
    Wire.write(0x00);  // Desactivar todos los canales
    uint8_t result = Wire.endTransmission();
    
    if (result == 0) {
      Serial.printf("RESET MUX: Multiplexor reseteado (intento %d)\n", attempt + 1);
      break;
    } else {
      Serial.printf("RESET MUX: Error en reset (intento %d) - código %d\n", attempt + 1, result);
      delay(200);
      
      if (attempt == 2) {
        Serial.println("RESET MUX: ERROR - No se pudo resetear el multiplexor");
        Serial.println("RESET MUX: Continuando con recuperación del bus...");
      }
    }
  }
  
  delay(500);  // Pausa tras reset del MUX
  
  // PASO 4: Limpiar configuración de dispositivos
  Serial.println("RESET MUX: Limpiando configuración de dispositivos...");
  device_count = 0;
  cycle_count = 0;
  current_mux_channel = 0;
  
  // Limpiar array de dispositivos
  for (int i = 0; i < 32; i++) {
    devices[i].address = 0;
    devices[i].mux_channel = 0;
    devices[i].total_tests = 0;
    devices[i].successful_tests = 0;
    devices[i].failed_tests = 0;
    devices[i].last_success_time = 0;
    devices[i].last_failure_time = 0;
    devices[i].is_online = false;
    devices[i].consecutive_failures = 0;
    devices[i].last_adc_value = 0;
  }
  
  // PASO 5: Re-inicializar el multiplexor
  Serial.println("RESET MUX: Re-inicializando multiplexor...");
  delay(500);
  
  if (initializeMux()) {
    Serial.println("RESET MUX: Multiplexor TCA9545A reinicializado exitosamente");
    mux_enabled = true;
    
    // PASO 6: Verificar todos los canales
    Serial.println("RESET MUX: Verificando operación de todos los canales...");
    bool all_channels_working = true;
    
    for (uint8_t ch = 0; ch < MUX_CHANNELS; ch++) {
      Serial.printf("RESET MUX: Probando canal %d...", ch);
      selectMuxChannel(ch);
      delay(50);
      
      // Test rápido del canal
      int test_responses = 0;
      for (uint8_t test_addr = 0x08; test_addr <= 0x20; test_addr++) {
        Wire.beginTransmission(test_addr);
        if (Wire.endTransmission() == 0) {
          test_responses++;
        }
        delay(2);
      }
      
      if (test_responses > 0) {
        Serial.printf(" OK (%d dispositivos detectados)\n", test_responses);
      } else {
        Serial.println(" Sin dispositivos");
      }
      
      delay(100);  // Pausa entre canales
    }
    
  } else {
    Serial.println("RESET MUX: WARNING - Multiplexor no detectado después del reset");
    Serial.println("RESET MUX: Cambiando a modo directo I2C...");
    mux_enabled = false;
  }
  
  // PASO 7: Escaneo completo de dispositivos
  Serial.println("RESET MUX: Iniciando escaneo completo de dispositivos...");
  delay(500);
  
  if (mux_enabled) {
    scanAllMuxChannels();
  } else {
    scanAndInitDevices();  // Escaneo directo si no hay MUX
  }
  
  // PASO 8: Inicializar todos los dispositivos encontrados
  if (device_count > 0) {
    Serial.printf("RESET MUX: Inicializando %d dispositivos encontrados...\n", device_count);
    
    for (int i = 0; i < device_count; i++) {
      if (mux_enabled) {
        selectMuxChannel(devices[i].mux_channel);
        delay(10);
      }
      
      // Inicializar en estado seguro
      sendCommandSafe(devices[i].address, CMD_NEO_OFF);
      delay(5);
      sendCommandSafe(devices[i].address, CMD_RELAY_OFF);
      delay(15);
      
      Serial.printf("RESET MUX: Dispositivo 0x%02X inicializado (Canal %d)\n", 
        devices[i].address, devices[i].mux_channel);
    }
  }
  
  // PASO 9: Verificación final del sistema
  Serial.println("RESET MUX: Ejecutando verificación final del sistema...");
  delay(200);
  
  Wire.beginTransmission(0x08);
  uint8_t final_bus_test = Wire.endTransmission();
  
  if (final_bus_test == 0) {
    Serial.println("RESET MUX: ✅ Bus I2C operativo");
  } else {
    Serial.println("RESET MUX: ⚠️  Bus I2C inestable - puede requerir atención");
  }
  
  if (mux_enabled && testMuxConnection()) {
    Serial.println("RESET MUX: ✅ Multiplexor operativo");
  } else if (mux_enabled) {
    Serial.println("RESET MUX: ⚠️  Multiplexor no responde");
  }
  
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║           RESET MUX COMPLETADO            ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.printf("RESULTADO: %d dispositivos listos en %s\n", 
    device_count, mux_enabled ? "modo multiplexor" : "modo directo");
  Serial.println("SISTEMA: Listo para operación normal");
  Serial.println();
}

// ═══════════════════════════════════════════════════════════
//            FUNCIÓN DE RESET DEL MICROCONTROLADOR
// ═══════════════════════════════════════════════════════════

void resetMicrocontroller() {
  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║        RESET DEL MICROCONTROLADOR         ║");
  Serial.println("╚═══════════════════════════════════════════╝");
  Serial.println("ADVERTENCIA: REINICIANDO MICROCONTROLADOR");
  Serial.println("Este comando reiniciará el ESP32 completamente");
  Serial.println("Se perderán todas las estadísticas actuales");
  Serial.println("La conexión serie se interrumpirá temporalmente");
  Serial.println("═══════════════════════════════════════════");
  Serial.println("Reiniciando en 3 segundos...");
  delay(1000);
  Serial.print("2...");
  delay(1000);
  Serial.print("1...");
  delay(1000);
  Serial.println("\nREINICIANDO AHORA!");
  Serial.flush(); // Asegurar que el mensaje se envíe antes del reset
  delay(100);
  ESP.restart(); // Reinicia el microcontrolador
}
