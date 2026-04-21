// ═══════════════════════════════════════════════════════════
//        MAP CONTROLLER EXAMPLE - Complete Device Manager
//         Demonstrates all MapController functionality
//       Compatible with PY32F003 firmware v6.0.0_lite
// ═══════════════════════════════════════════════════════════
//
// ⚠️ IMPORTANT v6.0.0_lite CHANGES:
// - Relay commands moved to 0xA0-0xAF for security
//   CMD_RELAY_OFF = 0xA0 (was 0x00)
//   CMD_RELAY_ON  = 0xA1 (was 0x01)
//   CMD_TOGGLE    = 0xA6 (was 0x06)
//
// 🎵 NEW FEATURES:
// - Musical tones: DO-SI (0x25-0x2B)
// - System alerts: SUCCESS/OK/WARNING/ALERT (0x2C-0x2F)
// - PWM gradual: 100Hz-1000Hz (0x10-0x1F)
// - New test modes: MUSICAL_SCALE, ALERTS, PWM_GRADUAL
//
// ═══════════════════════════════════════════════════════════

#include <Arduino.h>
#include <Wire.h>
#include <MapController.h>

// MapController class with full functionality (MapCRUD alias available for command compatibility)
MapController deviceManager;

// Global variables for demo
unsigned long lastTestTime = 0;
bool demoMode = false;
int demoStep = 0;

// Function declarations
void showBanner();
void showMenu();
void processCommand(String cmd);
void runDemo();
void printDeviceStatus();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  showBanner();
  
  // Initialize the device manager with optimized I2C settings
  deviceManager.begin(Wire, 21, 22, 100000); // ESP32: SDA=21, SCL=22, 100kHz
  deviceManager.initializeI2C(1); // print=1 to show initialization
  
  // Initialize device mapping system
  deviceManager.initializeMapping(1);
  
  // Perform initial scan
  Serial.println(">> Realizando scan inicial automatico...");
  deviceManager.scanDevices(1);
  
  showMenu();
  Serial.print("\nMapController> ");
}

void loop() {
  // Handle continuous testing if active
  if (deviceManager.isContinuousTestActive()) {
    deviceManager.runContinuousTest();
    delay(deviceManager.getTestInterval());
  }
  
  // Run demo mode if active
  if (demoMode) {
    runDemo();
  }
  
  // Process user commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    if (cmd.length() > 0) {
      Serial.printf("CMD: %s\n", cmd.c_str());
      processCommand(cmd);
      Serial.print("\nMapController> ");
    }
  }
  
  delay(10);
}

void showBanner() {
  Serial.println("\n===========================================");
  Serial.println("    MAP CONTROLLER - Complete Example     ");
  Serial.println("   I2C Device Manager & Testing System   ");
  Serial.println("  Compatible with PY32F003 v6.0.0_lite  ");
  Serial.println("===========================================");
  Serial.println("Features:");
  Serial.println("✓ Device scanning & compatibility check");
  Serial.println("✓ Continuous testing with statistics");
  Serial.println("✓ I2C address management");
  Serial.println("✓ PWM/Buzzer control + Musical tones 🎵");
  Serial.println("✓ System alerts (SUCCESS/OK/WARN/ALERT) 🔔");
  Serial.println("✓ NeoPixel control");
  Serial.println("✓ Sensor reading (PA4, ADC)");
  Serial.println("✓ Device information & UID");
  Serial.println("✓ Security: Relay commands 0xA0-0xAF ⚠️");
  Serial.println("===========================================\n");
}

void showMenu() {
  Serial.println("\nCOMANDOS DISPONIBLES:");
  Serial.println("===============================================");
  Serial.println("=== GESTION BASICA ===");
  Serial.println("'scan' / 's'        = Escanear dispositivos I2C");
  Serial.println("'list' / 'l'        = Ver dispositivos encontrados");
  Serial.println("'ping 0x20'         = Ping a dispositivo especifico");
  Serial.println("'stats'             = Ver estadisticas detalladas");
  Serial.println("'menu' / 'm'        = Mostrar este menu");
  Serial.println("");
  Serial.println("=== DEMO & TESTING ===");
  Serial.println("'demo'              = Iniciar modo demo automatico");
  Serial.println("'stop-demo'         = Detener modo demo");
  Serial.println("'test-start pa4'    = Test continuo lectura PA4");
  Serial.println("'test-start toggle' = Test continuo toggle");
  Serial.println("'test-start neo'    = Test continuo NeoPixels");
  Serial.println("'test-start pwm'    = Test continuo PWM/buzzer");
  Serial.println("'test-start music'  = 🎵 Test escala musical (v6.0.0)");
  Serial.println("'test-start alerts' = 🔔 Test alertas sistema (v6.0.0)");
  Serial.println("'test-start gradual'= 📈 Test PWM gradual (v6.0.0)");
  Serial.println("'test-stop'         = Detener test continuo");
  Serial.println("'interval 1000'     = Cambiar intervalo test (ms)");
  Serial.println("");
  Serial.println("=== CONTROL DE DISPOSITIVOS ===");
  Serial.println("'relay 0x20 on'     = Encender rele");
  Serial.println("'relay 0x20 off'    = Apagar rele");
  Serial.println("'toggle 0x20'       = Pulso de rele (toggle)");
  Serial.println("'neo 0x20 red'      = NeoPixels rojos");
  Serial.println("'neo 0x20 1'        = Neo slot 1 (RED preset guardado)");
  Serial.println("'neo 0x20 white'    = NeoPixels blancos");
  Serial.println("'neo 0x20 off'      = Apagar NeoPixels");
  Serial.println("'pwm 0x20 25'       = PWM 25% (200Hz)");
  Serial.println("'pwm 0x20 100'      = PWM 100% (2kHz)");
  Serial.println("'pwm 0x20 off'      = PWM silencio");
  Serial.println("'tone 0x20 do'      = 🎵 Tocar nota DO (261Hz)");
  Serial.println("'tone 0x20 la'      = 🎵 Tocar nota LA (440Hz)");
  Serial.println("'alert 0x20 ok'     = 🔔 Alerta OK (1000Hz)");
  Serial.println("'alert 0x20 warn'   = ⚠️ Alerta WARNING (1200Hz)");
  Serial.println("");
  Serial.println("=== SENSORES ===");
  Serial.println("'pa4 0x20'          = Leer entrada digital PA4");
  Serial.println("'adc 0x20'          = Leer ADC PA0");
  Serial.println("");
  Serial.println("=== INFORMACION ===");
  Serial.println("'info 0x20'         = Informacion del dispositivo");
  Serial.println("'uid 0x20'          = UID unico del dispositivo");
  Serial.println("'test-all 0x20'     = Test completo de comandos");
  Serial.println("'test-both 0x20'    = Test rápido (solo toggle+pa4)");
  Serial.println("'test-both 0x20'    = Test rápido (solo toggle + pa4)");
  Serial.println("");
  Serial.println("=== GESTION I2C ===");
  Serial.println("'change 0x20 0x30'  = Cambiar direccion I2C");
  Serial.println("'factory 0x20'      = Factory reset a UID");
  Serial.println("'reset 0x20'        = Reiniciar dispositivo");
  Serial.println("'status 0x20'       = Estado I2C (Flash/UID)");
  Serial.println("'save_color <id|addr> <slot> <r> <g> <b>' = Guardar color default por slot");
  Serial.println("===============================================");
  Serial.println("Ejemplos:");
  Serial.println("  scan               (escanear bus)");
  Serial.println("  demo               (modo demostracion)");
  Serial.println("  test-start toggle  (test automatico)");
  Serial.println("  relay 0x20 on      (encender rele)");
  Serial.println("  neo 0x20 blue      (NeoPixels azules)");
  Serial.println("  neo 0x20 1         (Neo slot 1 = RED preset)");
  Serial.println("===============================================");
}

void processCommand(String cmd) {
  // Accept common typos/aliases used in field.
  if (cmd.startsWith("save_colo ")) {
    cmd = "save_color " + cmd.substring(10);
  }
  if (cmd.startsWith("assing ")) {
    cmd = "assign " + cmd.substring(7);
  }

  // Basic management commands
  if (cmd == "scan" || cmd == "s") {
    deviceManager.scanDevices(1);
    
  } else if (cmd == "list" || cmd == "l") {
    deviceManager.listDevices(1);
    
  } else if (cmd == "menu" || cmd == "m") {
    showMenu();

  // Mapping commands (CRUD)
  } else if (cmd == "map" || cmd == "ls") {
    deviceManager.cmdShowMapping(1);

  } else if (cmd == "automap" || cmd == "am") {
    deviceManager.cmdAutoMap(1);

  } else if (cmd.startsWith("assign ")) {
    deviceManager.cmdAssign(cmd, 1);

  } else if (cmd.startsWith("mv ")) {
    deviceManager.cmdMove(cmd, 1);

  } else if (cmd.startsWith("rm ")) {
    deviceManager.cmdRemove(cmd, 1);

  } else if (cmd == "clear map") {
    deviceManager.clearMapping(1);

  } else if (cmd == "save") {
    deviceManager.saveMapping(1);

  } else if (cmd == "load") {
    deviceManager.loadMapping(1);
    
  } else if (cmd == "stats") {
    deviceManager.showStatistics(1);
    
  } else if (cmd.startsWith("ping ")) {
    String addr_str = cmd.substring(5);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.pingDevice(address, 1);
    }
    
  // Demo and testing commands
  } else if (cmd == "demo") {
    demoMode = true;
    demoStep = 0;
    Serial.println("[OK] Modo demo iniciado. Usa 'stop-demo' para detener.");
    
  } else if (cmd == "stop-demo") {
    demoMode = false;
    Serial.println("[OK] Modo demo detenido.");
    
  } else if (cmd == "test-start pa4") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_PA4_ONLY);
    Serial.println("[OK] Test continuo PA4 iniciado");
    
  } else if (cmd == "test-start toggle") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_TOGGLE_ONLY);
    Serial.println("[OK] Test continuo toggle iniciado");
    
  } else if (cmd == "test-start neo") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_NEO_CYCLE);
    Serial.println("[OK] Test continuo NeoPixels iniciado");
    
  } else if (cmd == "test-start pwm") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_PWM_TONES);
    Serial.println("[OK] Test continuo PWM iniciado");
    
  } else if (cmd == "test-start music") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_MUSICAL_SCALE);
    Serial.println("[OK] 🎵 Test escala musical DO-SI iniciado (v6.0.0_lite)");
    
  } else if (cmd == "test-start alerts") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_ALERTS);
    Serial.println("[OK] 🔔 Test alertas sistema iniciado (v6.0.0_lite)");
    
  } else if (cmd == "test-start gradual") {
    deviceManager.startContinuousTest(MapController::TEST_MODE_PWM_GRADUAL);
    Serial.println("[OK] 📈 Test PWM gradual 100Hz-1000Hz iniciado (v6.0.0_lite)");
    
  } else if (cmd == "test-stop") {
    deviceManager.stopContinuousTest();
    Serial.println("[OK] Test continuo detenido");
    
  } else if (cmd.startsWith("interval ")) {
    String interval_str = cmd.substring(9);
    uint32_t new_interval = interval_str.toInt();
    deviceManager.setTestInterval(new_interval);
    Serial.printf("[OK] Intervalo cambiado a %d ms\n", new_interval);
    
  // Device control commands
  } else if (cmd.startsWith("relay ")) {
    parseRelayCommand(cmd);
    
  } else if (cmd.startsWith("toggle ")) {
    String addr_str = cmd.substring(7);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.cmdToggle(address, 1);
    }
    
  } else if (cmd.startsWith("neo ")) {
    parseNeoCommand(cmd);
    
  } else if (cmd.startsWith("pwm ")) {
    parsePWMCommand(cmd);
    
  } else if (cmd.startsWith("tone ")) {
    parseToneCommand(cmd);
    
  } else if (cmd.startsWith("alert ")) {
    parseAlertCommand(cmd);
    
  // Sensor commands
  } else if (cmd.startsWith("pa4 ")) {
    String addr_str = cmd.substring(4);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.cmdReadPA4(address, 1);
    }
    
  } else if (cmd.startsWith("adc ")) {
    String addr_str = cmd.substring(4);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.cmdReadADC(address, 1);
    }
    
  // Information commands
  } else if (cmd.startsWith("info ")) {
    String addr_str = cmd.substring(5);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.showDeviceInfo(address, 1);
    }
    
  } else if (cmd.startsWith("uid ")) {
    String addr_str = cmd.substring(4);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.showDeviceUID(address, 1);
    }
    
  } else if (cmd.startsWith("test-all ")) {
    String addr_str = cmd.substring(9);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.testAllCommands(address, 1);
    }
    
  } else if (cmd.startsWith("test-both ")) {
    String addr_str = cmd.substring(10);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.testBothCommands(address, 1);
    }
    
  } else if (cmd.startsWith("test-both ")) {
    String addr_str = cmd.substring(10);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.testBothCommands(address, 1);
    }
    
  // I2C management commands
  } else if (cmd.startsWith("change ")) {
    parseChangeCommand(cmd);
    
  } else if (cmd.startsWith("factory ")) {
    String addr_str = cmd.substring(8);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.factoryReset(address, 1);
    }
    
  } else if (cmd.startsWith("reset ")) {
    String addr_str = cmd.substring(6);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.resetDevice(address, 1);
    }
    
  } else if (cmd.startsWith("status ")) {
    String addr_str = cmd.substring(7);
    uint8_t address = deviceManager.parseAddress(addr_str);
    if (address > 0) {
      deviceManager.checkI2CStatus(address, 1);
    }

  } else if (cmd.startsWith("save_color ")) {
    // Accepts mapped ID or direct I2C address: save_color <id|addr> <slot 1..3> <r> <g> <b>
    deviceManager.cmdSaveColor(cmd, 1);
    
  } else if (cmd.length() > 0) {
    Serial.println("ERROR: Comando no reconocido. Usa 'menu' para ver comandos disponibles.");
  }
}

void parseRelayCommand(String cmd) {
  // Parse: "relay 0x20 on/off"
  int first_space = cmd.indexOf(' ');
  int second_space = cmd.indexOf(' ', first_space + 1);
  
  if (first_space == -1 || second_space == -1) {
    Serial.println("ERROR: Formato: relay <address> on/off");
    return;
  }
  
  String addr_str = cmd.substring(first_space + 1, second_space);
  String action = cmd.substring(second_space + 1);
  action.toLowerCase();
  
  uint8_t address = deviceManager.parseAddress(addr_str);
  if (address == 0) return;
  
  if (action == "on") {
    deviceManager.cmdRelay(address, true, 1);
  } else if (action == "off") {
    deviceManager.cmdRelay(address, false, 1);
  } else {
    Serial.println("ERROR: Usar 'on' o 'off'");
  }
}

void parseNeoCommand(String cmd) {
  // Parse: "neo 0x20 red/green/blue/white/off" or "neo 0x20 1/2/3/4/0"
  int first_space = cmd.indexOf(' ');
  int second_space = cmd.indexOf(' ', first_space + 1);
  
  if (first_space == -1 || second_space == -1) {
    Serial.println("ERROR: Formato: neo <address> red/green/blue/white/off o 1/2/3/4/0");
    return;
  }
  
  String addr_str = cmd.substring(first_space + 1, second_space);
  String color = cmd.substring(second_space + 1);
  color.toLowerCase();
  
  uint8_t address = deviceManager.parseAddress(addr_str);
  if (address == 0) return;
  
  if (color == "red") {
    deviceManager.cmdNeoRed(address, 1);
  } else if (color == "1") {
    deviceManager.cmdNeoRed(address, 1);
  } else if (color == "green") {
    deviceManager.cmdNeoGreen(address, 1);
  } else if (color == "2") {
    deviceManager.cmdNeoGreen(address, 1);
  } else if (color == "blue") {
    deviceManager.cmdNeoBlue(address, 1);
  } else if (color == "3") {
    deviceManager.cmdNeoBlue(address, 1);
  } else if (color == "white") {
    deviceManager.cmdNeoWhite(address, 1);
  } else if (color == "4") {
    deviceManager.cmdNeoWhite(address, 1);
  } else if (color == "off") {
    deviceManager.cmdNeoOff(address, 1);
  } else if (color == "0") {
    deviceManager.cmdNeoOff(address, 1);
  } else {
    Serial.println("ERROR: Usar red/green/blue/white/off o 1/2/3/4/0");
  }
}

void parsePWMCommand(String cmd) {
  // Parse: "pwm 0x20 off/25/50/75/100"
  int first_space = cmd.indexOf(' ');
  int second_space = cmd.indexOf(' ', first_space + 1);
  
  if (first_space == -1 || second_space == -1) {
    Serial.println("ERROR: Formato: pwm <address> off/25/50/75/100");
    return;
  }
  
  String addr_str = cmd.substring(first_space + 1, second_space);
  String level = cmd.substring(second_space + 1);
  level.toLowerCase();
  
  uint8_t address = deviceManager.parseAddress(addr_str);
  if (address == 0) return;
  
  if (level == "off" || level == "0") {
    deviceManager.cmdPWMOff(address, 1);
  } else if (level == "25") {
    deviceManager.cmdPWM25(address, 1);
  } else if (level == "50") {
    deviceManager.cmdPWM50(address, 1);
  } else if (level == "75") {
    deviceManager.cmdPWM75(address, 1);
  } else if (level == "100") {
    deviceManager.cmdPWM100(address, 1);
  } else {
    Serial.println("ERROR: Usar off/25/50/75/100");
  }
}

void parseToneCommand(String cmd) {
  // Parse: "tone 0x20 do/re/mi/fa/sol/la/si"
  int first_space = cmd.indexOf(' ');
  int second_space = cmd.indexOf(' ', first_space + 1);
  
  if (first_space == -1 || second_space == -1) {
    Serial.println("ERROR: Formato: tone <address> do/re/mi/fa/sol/la/si");
    return;
  }
  
  String addr_str = cmd.substring(first_space + 1, second_space);
  String note = cmd.substring(second_space + 1);
  note.toLowerCase();
  
  uint8_t address = deviceManager.parseAddress(addr_str);
  if (address == 0) return;
  
  if (note == "do") {
    deviceManager.cmdToneDo(address, 1);
  } else if (note == "re") {
    deviceManager.cmdToneRe(address, 1);
  } else if (note == "mi") {
    deviceManager.cmdToneMi(address, 1);
  } else if (note == "fa") {
    deviceManager.cmdToneFa(address, 1);
  } else if (note == "sol") {
    deviceManager.cmdToneSol(address, 1);
  } else if (note == "la") {
    deviceManager.cmdToneLa(address, 1);
  } else if (note == "si") {
    deviceManager.cmdToneSi(address, 1);
  } else {
    Serial.println("ERROR: Usar do/re/mi/fa/sol/la/si");
  }
}

void parseAlertCommand(String cmd) {
  // Parse: "alert 0x20 success/ok/warn/critical"
  int first_space = cmd.indexOf(' ');
  int second_space = cmd.indexOf(' ', first_space + 1);
  
  if (first_space == -1 || second_space == -1) {
    Serial.println("ERROR: Formato: alert <address> success/ok/warn/critical");
    return;
  }
  
  String addr_str = cmd.substring(first_space + 1, second_space);
  String alert = cmd.substring(second_space + 1);
  alert.toLowerCase();
  
  uint8_t address = deviceManager.parseAddress(addr_str);
  if (address == 0) return;
  
  if (alert == "success") {
    deviceManager.cmdSuccess(address, 1);
  } else if (alert == "ok") {
    deviceManager.cmdOk(address, 1);
  } else if (alert == "warn" || alert == "warning") {
    deviceManager.cmdWarning(address, 1);
  } else if (alert == "critical" || alert == "alert") {
    deviceManager.cmdAlert(address, 1);
  } else {
    Serial.println("ERROR: Usar success/ok/warn/critical");
  }
}

void parseChangeCommand(String cmd) {
  // Parse: "change 0x20 0x30"
  int first_space = cmd.indexOf(' ');
  int second_space = cmd.indexOf(' ', first_space + 1);
  
  if (first_space == -1 || second_space == -1) {
    Serial.println("ERROR: Formato: change <old_address> <new_address>");
    return;
  }
  
  String old_str = cmd.substring(first_space + 1, second_space);
  String new_str = cmd.substring(second_space + 1);
  
  uint8_t old_addr = deviceManager.parseAddress(old_str);
  uint8_t new_addr = deviceManager.parseAddress(new_str);
  
  if (old_addr > 0 && new_addr > 0) {
    deviceManager.changeI2CAddress(old_addr, new_addr, 1);
  }
}

void runDemo() {
  static unsigned long lastDemoTime = 0;
  
  if (millis() - lastDemoTime < 3000) return; // 3 second intervals
  lastDemoTime = millis();
  
  if (deviceManager.getDeviceCount() == 0) {
    Serial.println("[DEMO] No hay dispositivos para demostrar");
    demoMode = false;
    return;
  }
  
  // Get first compatible device
  const MapController::DeviceInfo* devices = deviceManager.getDevices();
  uint8_t demo_addr = 0;
  
  for (int i = 0; i < deviceManager.getDeviceCount(); i++) {
    if (devices[i].is_compatible) {
      demo_addr = devices[i].address;
      break;
    }
  }
  
  if (demo_addr == 0) {
    Serial.println("[DEMO] No hay dispositivos compatibles para demostrar");
    demoMode = false;
    return;
  }
  
  Serial.printf("[DEMO] Paso %d - Dispositivo 0x%02X\n", demoStep + 1, demo_addr);
  
  switch (demoStep % 8) {
    case 0:
      Serial.println("[DEMO] NeoPixels rojos");
      deviceManager.cmdNeoRed(demo_addr, 1);
      break;
    case 1:
      Serial.println("[DEMO] NeoPixels verdes");
      deviceManager.cmdNeoGreen(demo_addr, 1);
      break;
    case 2:
      Serial.println("[DEMO] NeoPixels azules");
      deviceManager.cmdNeoBlue(demo_addr, 1);
      break;
    case 3:
      Serial.println("[DEMO] NeoPixels blancos");
      deviceManager.cmdNeoWhite(demo_addr, 1);
      break;
    case 4:
      Serial.println("[DEMO] NeoPixels OFF");
      deviceManager.cmdNeoOff(demo_addr, 1);
      break;
    case 5:
      Serial.println("[DEMO] Toggle rele");
      deviceManager.cmdToggle(demo_addr, 1);
      break;
    case 6:
      Serial.println("[DEMO] Lectura PA4");
      deviceManager.cmdReadPA4(demo_addr, 1);
      break;
    case 7:
      Serial.println("[DEMO] Lectura ADC");
      deviceManager.cmdReadADC(demo_addr, 1);
      break;
  }
  
  demoStep++;
}



void showHelp() {
  Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                    FAST COMMANDS (≤2 chars)                 ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println("⚡ SYSTEM FAST:");
  Serial.println("  sc = scan       m = menu        am = automap    h = help");  
  Serial.println("  c = clear       r = reboot");
  Serial.println("⚡ ADC FAST:");
  Serial.println("  a1 = adc 1      a2 = adc 2      a3 = adc 3      a4 = adc 4");
  Serial.println("  a5 = adc 5      aa = adc all");
  Serial.println("⚡ RELAY FAST:");
  Serial.println("  f1 = fire 1     f2 = fire 2     f3 = fire 3     f4 = fire 4");
  Serial.println("  f5 = fire 5     ro = relay off all");
  Serial.println("⚡ NEOPIXEL FAST:");
  Serial.println("  nr = red all    ng = green all  nb = blue all   no = off all");
  Serial.println("⚡ FLASH DEBUG FAST (PRIORITY):");
  Serial.println("  fs <id> <data> = flash save     fr <id> = flash read");
  Serial.println("  Example: fs 1 25, fs 1 0x25, fr 1");
  Serial.println("⚡ ARRAY CONTROL FAST (NEW):");
  Serial.println("  sa <n> = sense array n          sh <n> = show array n");
  Serial.println("  fa <n> = fill array n (TEST!)   all = show all arrays");
  Serial.println("  Example: sa 0, sh 1, fa 2, all (n = 0-3)");
  Serial.println("⚡ I2C MANAGEMENT FAST:");
  Serial.println("  si <id> <addr> = set i2c        rf <id> = reset factory");
  Serial.println("  is <id> = i2c status            dr <id> = debug reset");
  Serial.println("  Example: si 1 25, rf 1, is 1, dr 1");
  Serial.println("⚡ COLOR PRESET (NEW):");
  Serial.println("  save_color <id|addr> <slot> <r> <g> <b>");
  Serial.println("  slot: 1=red, 2=green, 3=blue");
  Serial.println("  Example: save_color 1 1 0x00 0xff 0x60");
  Serial.println("           save_color 0x20 1 0xff 0x22 0x00");

  Serial.println("\n╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                    STANDARD COMMANDS                        ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println("SYSTEM:");
  Serial.println("  scan, s         - Scan for I2C devices");
  Serial.println("  reboot          - Reboot the ESP32");
  Serial.println("  help, h, ?      - Show this help");
  Serial.println("  clear, cls      - Clear screen");
  Serial.println("\nMAPPING (CRUD):");
  Serial.println("  map, ls         - (List) Show current ID-address mapping");
  Serial.println("  automap         - (Create) Auto-assign scanned devices to free IDs");
  Serial.println("  assign <id> <addr> - (Create/Update) Assign an address to an ID");
  Serial.println("  mv <from> <to>  - (Update) Move/Swap a device from one ID to another");
  Serial.println("  rm <id>         - (Delete) Remove a device from an ID");
  Serial.println("  clear map       - (Delete) Clear all mappings from memory and storage");
  Serial.println("  save            - Save current mapping to persistent storage");
  Serial.println("  load            - Load mapping from persistent storage");
  Serial.println("\nSENSE & RELAY CONTROL (uses ID mapping):");
  Serial.println("  sense           - Read PA4 state from all mapped devices");
  Serial.println("  mask            - Get 4-byte bitmask of PA4 states for mapped devices");
  Serial.println("  sense_array <n> - Get detailed mask for array n (0-3: IDs 1-8, 9-16, 17-24, 25-32)");
  Serial.println("  show_array <n>  - Show detailed view of array n with bit positions");
  Serial.println("  fill_array <n>  - Test fill array n (briefly activates relays - USE WITH CARE!)");
  Serial.println("  show_all        - Overview of all 4 arrays with their current masks");
  Serial.println("  f <id>          - Send momentary relay pulse to a device by ID");
  Serial.println("  t <id>          - Toggle relay state on a device by ID");
  Serial.println("  roff            - Turn off all mapped relays");
  Serial.println("  shut            - Shutdown all mapped devices (LEDs and relays off)");
  Serial.println("\nNEOPIXEL CONTROL (uses ID mapping):");
  Serial.println("  neo red [id]    - Set NeoPixel(s) to red");
  Serial.println("  neo green [id]  - Set NeoPixel(s) to green");  
  Serial.println("  neo blue [id]   - Set NeoPixel(s) to blue");
  Serial.println("  neo off [id]    - Turn off NeoPixel(s)");
  Serial.println("  neo <addr> <slot> - Slot rapido: 1=red, 2=green, 3=blue, 4=white, 0=off");
  Serial.println("  red, green, blue, off - Quick color commands for all");
  Serial.println("\nADC READING (uses ID mapping - 12-bit, 3.3V reference):");
  Serial.println("  adc <id>        - Read ADC value from specific device by ID");
  Serial.println("  adcall, aa      - Read ADC values from all mapped devices");
  Serial.println("  Note: ADC is 12-bit (0-4095) with 3.3V reference, automatically masked");
  Serial.println("\nDEBUG FLASH COMMANDS (NEW):");
  Serial.println("  flash_save <id> <data> - Save data byte to device flash (hex or decimal)");
  Serial.println("  flash_read <id>        - Read data byte from device flash");
  Serial.println("  Examples: flash_save 1 0x25, flash_save 1 37, flash_read 1");
  Serial.println("\nI2C ADDRESS MANAGEMENT (NEW):");
  Serial.println("  set_i2c <id> <addr>    - Change device I2C address (0x08-0x77)");
  Serial.println("  reset_factory <id>     - Reset device to factory I2C address");
  Serial.println("  i2c_status <id>        - Get device I2C configuration status");
  Serial.println("  Examples: set_i2c 1 0x25, reset_factory 1, i2c_status 1");
  Serial.println("  -----Address changes require device restart (~2s)");
  Serial.println("  save_color <id|addr> <slot> <r> <g> <b> - Save Neo default color slot");
  Serial.println("    slot: 1=red, 2=green, 3=blue (RGB decimal o 0x00..0xFF)");
  Serial.println("    Example: save_color 1 1 0x00 0xff 0x60");
  Serial.println("             save_color 0x20 1 0xff 0x22 0x00");
  Serial.println("\nDIAGNOSTIC COMMANDS:");
  Serial.println("  testcmds        - Test all command values (0x00-0xFF) to find supported ones");
  Serial.println("════════════════════════════════════════════════════════════════");
}
