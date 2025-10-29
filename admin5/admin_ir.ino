#include <Wire.h>

// ============================================================
//                I2C DEVICE MANAGER SIMPLE
//        Interfaz Serial para Scan y Cambio de Direcciones
// ============================================================

// Comandos básicos para verificar dispositivos compatibles
#define CMD_PA4_DIGITAL    0x07   // Comando de prueba para verificar compatibilidad
#define CMD_SET_I2C_ADDR   0x3D   // Establecer nueva dirección I2C
#define CMD_RESET_FACTORY  0x3E   // Reset factory (usar UID por defecto)
#define CMD_GET_I2C_STATUS 0x3F   // Obtener estado I2C (Flash vs UID)
#define CMD_RESET          0xFE   // Reset inmediato del sistema

// Declaraciones de función
void scanDevices();
void listDevices();
void pingDevice(uint8_t address);
void changeI2CAddress(uint8_t old_addr, uint8_t new_addr);
void factoryReset(uint8_t address);
void checkI2CStatus(uint8_t address);
void resetDevice(uint8_t address);
void debugResetDevice(uint8_t address);  // Nueva función de debug
void verifyAddressChange(uint8_t old_addr, uint8_t new_addr);  // Verificar cambio de dirección
void showMenu();
void processCommand(String cmd);
void parseChangeCommand(String cmd);
void parseChangeCommandShort(String cmd);
uint8_t parseAddress(String addr_str);
bool isCompatibleDevice(uint8_t address);
bool sendCommandWithTimeout(uint8_t address, uint8_t command, uint32_t timeout_ms = 1000);

// Estructura simple para dispositivos
struct Device {
  uint8_t address;
  bool is_compatible;
  bool is_online;
  String status;
};

Device found_devices[20];  // Máximo 20 dispositivos
int device_count = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Inicializar I2C con configuración estándar
  Wire.begin(6, 7);  // SDA=GPIO22, SCL=GPIO23 para ESP32-C3
  Wire.setTimeout(100);
  Wire.setClock(100000);  // 100kHz para mayor estabilidad
  
  printHeader();
  printMenu();
  
  // Realizar scan inicial automático
  Serial.println(">> Realizando scan inicial automático...\r\n");
  scanDevices();
  
  Serial.println("NOTA: Usa comandos rápidos: s, l, p 32, c 32 48, etc.\r");
  Serial.println("NOTA: Usa 'm' para ver el menú completo\r\n");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\r\n');
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
  Serial.println("\r\n===========================================\r");
  Serial.println("          I2C DEVICE MANAGER SIMPLE       \r");
  Serial.println("         Scan y Cambio de Direcciones     \r");
  Serial.println("===========================================\r");
  Serial.println("I2C: SDA=GPIO6, SCL=GPIO7, Clock=100kHz\r\n");
}

void printMenu() {
  Serial.println("COMANDOS DISPONIBLES:\r");
  Serial.println("===============================================\r");
  Serial.println("1. 's'              = Escanear dispositivos I2C\r");
  Serial.println("2. 'l'              = Ver dispositivos encontrados\r");
  Serial.println("3. 'p XX'           = Ping a dispositivo (hex o decimal)\r");
  Serial.println("4. 'c XX YY'        = Cambiar dirección XX->YY\r");
  Serial.println("5. 'f XX'           = Factory reset del dispositivo\r");
  Serial.println("6. 'st XX'          = Estado I2C del dispositivo\r");
  Serial.println("7. 'r XX'           = Reiniciar dispositivo\r");
  Serial.println("8. 'dr XX'          = Debug reset (diagnóstico detallado)\r");
  Serial.println("9. 'vc XX YY'       = Verificar cambio de dirección XX->YY\r");
  Serial.println("10. 'm'             = Mostrar este menú\r");
  Serial.println("===============================================\r");
  Serial.println("Formatos soportados:\r");
  Serial.println("  Hexadecimal: 0x20, 20h, 20\r");
  Serial.println("  Decimal:     32\r");
  Serial.println("===============================================\r");
  Serial.println("Ejemplos:\r");
  Serial.println("  s\r");
  Serial.println("  p 32        (ping a dirección decimal 32 = 0x20)\r");
  Serial.println("  p 0x20      (ping a dirección hex 0x20 = 32)\r");
  Serial.println("  c 32 48     (cambiar 0x20->0x30 en decimal)\r");
  Serial.println("  c 0x20 0x30 (cambiar 0x20->0x30 en hex)\r");
  Serial.println("  f 48        (factory reset a 0x30)\r");
  Serial.println("===============================================\r\n");
}

void processCommand(String cmd) {
  // Limpiar espacios extra
  cmd.trim();
  
  if (cmd == "s") {
    scanDevices();
    
  } else if (cmd == "l") {
    listDevices();
    
  } else if (cmd == "m") {
    printMenu();
    
  } else if (cmd.startsWith("p ")) {
    // Formato: "p XX"
    String addr_str = cmd.substring(2);
    addr_str.trim();
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      pingDevice(address);
    }
    
  } else if (cmd.startsWith("c ")) {
    // Formato: "c XX YY"
    parseChangeCommandShort(cmd);
    
  } else if (cmd.startsWith("f ")) {
    // Formato: "f XX"
    String addr_str = cmd.substring(2);
    addr_str.trim();
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      factoryReset(address);
    }
    
  } else if (cmd.startsWith("st ")) {
    // Formato: "st XX"
    String addr_str = cmd.substring(3);
    addr_str.trim();
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      checkI2CStatus(address);
    }
    
  } else if (cmd.startsWith("r ")) {
    // Formato: "r XX"
    String addr_str = cmd.substring(2);
    addr_str.trim();
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      resetDevice(address);
    }
    
  } else if (cmd.startsWith("dr ")) {
    // Formato: "dr XX" - Debug reset
    String addr_str = cmd.substring(3);
    addr_str.trim();
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      debugResetDevice(address);
    }
    
  } else if (cmd.startsWith("vc ")) {
    // Formato: "vc XX YY" - Verificar cambio
    int space_pos = cmd.indexOf(' ', 3);
    if (space_pos != -1) {
      String old_addr_str = cmd.substring(3, space_pos);
      String new_addr_str = cmd.substring(space_pos + 1);
      old_addr_str.trim();
      new_addr_str.trim();
      
      uint8_t old_addr = parseAddress(old_addr_str);
      uint8_t new_addr = parseAddress(new_addr_str);
      
      if (old_addr > 0 && new_addr > 0) {
        verifyAddressChange(old_addr, new_addr);
      }
    } else {
      Serial.println("ERROR: Formato incorrecto. Usar: vc XX YY\r");
    }
    
  // Compatibilidad con comandos largos
  } else if (cmd == "scan") {
    scanDevices();
    
  } else if (cmd == "list") {
    listDevices();
    
  } else if (cmd == "menu") {
    printMenu();
    
  } else if (cmd.startsWith("ping ")) {
    String addr_str = cmd.substring(5);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      pingDevice(address);
    }
    
  } else if (cmd.startsWith("change ")) {
    parseChangeCommand(cmd);
    
  } else if (cmd.startsWith("factory ")) {
    String addr_str = cmd.substring(8);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      factoryReset(address);
    }
    
  } else if (cmd.startsWith("status ")) {
    String addr_str = cmd.substring(7);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      checkI2CStatus(address);
    }
    
  } else if (cmd.startsWith("reset ")) {
    String addr_str = cmd.substring(6);
    uint8_t address = parseAddress(addr_str);
    if (address > 0) {
      resetDevice(address);
    }
    
  } else if (cmd.length() > 0) {
    Serial.println("ERROR: Comando no reconocido. Usa 'm' para ver comandos disponibles.\r");
  }
}

void scanDevices() {
  Serial.println(">> ESCANEANDO BUS I2C...\r");
  Serial.println("Rango: 0x08-0x77 (8-119 decimal)\r");
  Serial.println("===========================================\r");
  
  device_count = 0;
  bool found_any = false;
  
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      found_any = true;
      
      // Verificar si es compatible
      bool compatible = checkCompatibility(addr);
      
      // Guardar en array
      if (device_count < 20) {
        found_devices[device_count].address = addr;
        found_devices[device_count].is_compatible = compatible;
        found_devices[device_count].is_online = true;
        found_devices[device_count].status = compatible ? "Compatible" : "Genérico";
        device_count++;
      }
      
      // Mostrar resultado con decimal
      if (compatible) {
        Serial.printf("[OK] 0x%02X (%3dd) - COMPATIBLE (Firmware PY32F003)\r\n", addr, addr);
      } else {
        Serial.printf("[DEV] 0x%02X (%3dd) - Genérico (No compatible)\r\n", addr, addr);
      }
    }
    
    delay(5); // Pequeña pausa entre direcciones
  }
  
  Serial.println("===========================================\r");
  
  if (!found_any) {
    Serial.println("ERROR: No se encontraron dispositivos I2C\r");
    Serial.println("NOTA: Verificar conexiones SDA/SCL y alimentación\r");
  } else {
    Serial.printf("TOTAL: Total encontrados: %d dispositivos\r\n", device_count);
    
    // Contar compatibles
    int compatible_count = 0;
    for (int i = 0; i < device_count; i++) {
      if (found_devices[i].is_compatible) {
        compatible_count++;
      }
    }
    Serial.printf("COMPATIBLE: Compatibles: %d dispositivos\r\n", compatible_count);
    
    // Mostrar comandos rápidos sugeridos
    if (compatible_count > 0) {
      Serial.println("NOTA: Comandos rápidos disponibles:\r");
      for (int i = 0; i < device_count; i++) {
        if (found_devices[i].is_compatible) {
          uint8_t addr = found_devices[i].address;
          Serial.printf("   p %d      (ping)\r\n", addr);
          Serial.printf("   c %d XX   (cambiar dirección)\r\n", addr);
        }
      }
    }
  }
  
  Serial.println("\r");
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
  Serial.println("DISPOSITIVOS ENCONTRADOS:\r");
  Serial.println("===========================================\r");
  
  if (device_count == 0) {
    Serial.println("ERROR: No hay dispositivos. Usa 'scan' primero.\r");
    Serial.println("\r");
    return;
  }
  
  Serial.println("+------------------+------------+---------------------+\r");
  Serial.println("|  Direccion       |   Estado   |      Descripcion    |\r");
  Serial.println("+------------------+------------+---------------------+\r");
  
  for (int i = 0; i < device_count; i++) {
    String icon = found_devices[i].is_compatible ? "[OK]" : "[DEV]";
    String status = found_devices[i].is_online ? "ONLINE" : "OFFLINE";
    
    Serial.printf("|%s 0x%02X (%3d) | %-10s | %-19s |\r\n",
      icon.c_str(),
      found_devices[i].address,
      found_devices[i].address,
      status.c_str(),
      found_devices[i].status.c_str()
    );
  }
  
  Serial.println("+------------------+------------+---------------------+\r");
  Serial.println("\r");
}

void pingDevice(uint8_t address) {
  Serial.printf("PING: PING a dispositivo 0x%02X (%d decimal)\r\n", address, address);
  
  Wire.beginTransmission(address);
  uint8_t error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.printf("PONG: Dispositivo 0x%02X (%d) responde OK\r\n", address, address);
    
    // Verificar si es compatible
    if (checkCompatibility(address)) {
      Serial.printf("COMPATIBLE: Compatible con firmware PY32F003\r\n");
    } else {
      Serial.printf("DISPOSITIVO: Genérico (no compatible)\r\n");
    }
  } else {
    Serial.printf("ERROR: Dispositivo 0x%02X (%d) no responde\r\n", address, address);
  }
  
  Serial.println("\r");
}

void parseChangeCommand(String cmd) {
  // Formato: "change 0x20 0x30"
  int space1 = cmd.indexOf(' ', 7);
  int space2 = cmd.indexOf(' ', space1 + 1);
  
  if (space1 == -1 || space2 == -1) {
    Serial.println("ERROR: Formato incorrecto. Usar: change 0x20 0x30\r");
    Serial.println("\r");
    return;
  }
  
  String old_addr_str = cmd.substring(space1 + 1, space2);
  String new_addr_str = cmd.substring(space2 + 1);
  
  uint8_t old_addr = parseAddress(old_addr_str);
  uint8_t new_addr = parseAddress(new_addr_str);
  
  if (old_addr > 0 && new_addr > 0) {
    changeI2CAddress(old_addr, new_addr);
  }
}

uint8_t parseAddress(String addr_str) {
  addr_str.trim();
  
  uint8_t address = 0;
  
  // Soporte para múltiples formatos
  if (addr_str.startsWith("0x") || addr_str.startsWith("0X")) {
    // Formato hexadecimal: 0x20
    address = strtol(addr_str.c_str(), NULL, 16);
  } else if (addr_str.endsWith("h") || addr_str.endsWith("H")) {
    // Formato hexadecimal: 20h
    String hex_part = addr_str.substring(0, addr_str.length() - 1);
    address = strtol(hex_part.c_str(), NULL, 16);
  } else if (addr_str.toInt() > 127) {
    // Si es mayor a 127, asumir que es un error de formato
    Serial.printf("  Dirección inválida: %s (máximo decimal: 119 = 0x77)\r\n", addr_str.c_str());
    return 0;
  } else {
    // Asumir decimal si no hay prefijo
    int decimal_val = addr_str.toInt();
    if (decimal_val == 0 && addr_str != "0") {
      // Intentar como hexadecimal sin prefijo
      address = strtol(addr_str.c_str(), NULL, 16);
    } else {
      address = decimal_val;
    }
  }
  
  // Validar rango
  if (address < 0x08 || address > 0x77) {
    Serial.printf("  Dirección fuera de rango: %s (0x%02X)\r\n", addr_str.c_str(), address);
    Serial.printf("   Rango válido: 0x08-0x77 (8-119 decimal)\r\n");
    return 0;
  }
  
  // Mostrar conversión para claridad
  Serial.printf("ADDR: Dirección: %s → 0x%02X (%d decimal)\r\n", addr_str.c_str(), address, address);
  
  return address;
}

void parseChangeCommandShort(String cmd) {
  // Formato: "c XX YY"
  cmd.trim();
  
  // Buscar el primer espacio después de 'c'
  int space1 = cmd.indexOf(' ');
  if (space1 == -1) {
    Serial.println("  Formato incorrecto. Usar: c XX YY\r");
    Serial.println("   Ejemplos: c 32 48, c 0x20 0x30\r");
    Serial.println("\r");
    return;
  }
  
  // Buscar el segundo espacio
  int space2 = cmd.indexOf(' ', space1 + 1);
  if (space2 == -1) {
    Serial.println("  Formato incorrecto. Usar: c XX YY\r");
    Serial.println("   Ejemplos: c 32 48, c 0x20 0x30\r");
    Serial.println("\r");
    return;
  }
  
  String old_addr_str = cmd.substring(space1 + 1, space2);
  String new_addr_str = cmd.substring(space2 + 1);
  
  old_addr_str.trim();
  new_addr_str.trim();
  
  Serial.printf("DEBUG: Debug: cmd='%s', old='%s', new='%s'\r\n", cmd.c_str(), old_addr_str.c_str(), new_addr_str.c_str());
  
  uint8_t old_addr = parseAddress(old_addr_str);
  uint8_t new_addr = parseAddress(new_addr_str);
  
  if (old_addr > 0 && new_addr > 0) {
    changeI2CAddress(old_addr, new_addr);
  }
}

void changeI2CAddress(uint8_t old_addr, uint8_t new_addr) {
  Serial.printf("CHANGE: CAMBIANDO DIRECCIÓN I2C\r\n");
  Serial.printf("   Actual:  0x%02X (%d decimal)\r\n", old_addr, old_addr);
  Serial.printf("   Nueva:   0x%02X (%d decimal)\r\n", new_addr, new_addr);
  Serial.println("===============================================\r");
  
  // Verificar que el dispositivo esté disponible
  Wire.beginTransmission(old_addr);
  if (Wire.endTransmission() != 0) {
    Serial.printf("ERROR: Dispositivo 0x%02X (%d) no responde\r\n", old_addr, old_addr);
    Serial.println("\r");
    return;
  }
  
  // Verificar que sea compatible
  if (!checkCompatibility(old_addr)) {
    Serial.printf("ERROR: Dispositivo 0x%02X (%d) no es compatible\r\n", old_addr, old_addr);
    Serial.println("\r");
    return;
  }
  
  // NUEVO: Verificar estado I2C antes del cambio
  Serial.printf("DEBUG: Paso 0: Verificando estado I2C inicial...\r\n");
  Wire.beginTransmission(old_addr);
  Wire.write(CMD_GET_I2C_STATUS);
  uint8_t status_error = Wire.endTransmission();
  
  if (status_error == 0) {
    delay(50);
    Wire.requestFrom(old_addr, (uint8_t)1);
    if (Wire.available()) {
      uint8_t initial_status = Wire.read();
      Serial.printf("   Estado inicial: 0x%02X (%s)\r\n", 
                   initial_status & 0x0F, 
                   (initial_status & 0x0F) == 0x0F ? "FLASH" : 
                   (initial_status & 0x0F) == 0x0A ? "UID" : "DESCONOCIDO");
    }
  }
  
  Serial.printf("STEP: Paso 1: Activando modo cambio de dirección (CMD 0x3D)...\r\n");
  
  // Paso 1: Enviar comando para activar modo cambio de dirección
  Wire.beginTransmission(old_addr);
  Wire.write(CMD_SET_I2C_ADDR);  // 0x3D
  uint8_t activate_error = Wire.endTransmission();
  
  if (activate_error != 0) {
    Serial.printf("ERROR: No se pudo activar modo cambio (Error: %d)\r\n", activate_error);
    Serial.println("\r");
    return;
  }
  
  // IMPORTANTE: Esperar confirmación del dispositivo
  Serial.printf("RECEIVE: Paso 1.5: Esperando confirmación del dispositivo...\r\n");
  delay(100);  // Dar tiempo al dispositivo para procesar
  
  // Verificar que el dispositivo confirmó el modo
  Wire.requestFrom(old_addr, (uint8_t)1);
  if (Wire.available()) {
    uint8_t confirmation = Wire.read();
    Serial.printf("   Confirmación recibida: 0x%02X\r\n", confirmation);
    
    // Según tu código: RESP_I2C_ADDR_SET debería ser la respuesta esperada
    // Asumiendo que es 0x3D o similar basado en el patrón
    if ((confirmation & 0xF0) != 0x00) {  // Verificar que no sea error
      Serial.printf("[OK] Dispositivo confirmó activación del modo cambio\r\n");
    } else {
      Serial.printf("WARNING:  Advertencia: Confirmación inesperada, continuando...\r\n");
    }
  } else {
    Serial.printf("WARNING:  Advertencia: No hay confirmación del dispositivo\r\n");
  }
  
  Serial.printf("SEND: Paso 2: Enviando nueva dirección 0x%02X (%d)...\r\n", new_addr, new_addr);
  
  // Paso 2: Enviar la nueva dirección como un comando separado
  Wire.beginTransmission(old_addr);
  Wire.write(new_addr);  // La nueva dirección va como dato del próximo comando
  uint8_t addr_error = Wire.endTransmission();
  
  if (addr_error != 0) {
    Serial.printf("ERROR: No se pudo enviar nueva dirección (Error: %d)\r\n", addr_error);
    Serial.println("\r");
    return;
  }
  
  // Dar tiempo al dispositivo para procesar y guardar en FLASH
  Serial.printf("WAIT: Paso 2.5: Esperando guardado en FLASH...\r\n");
  delay(200);  // Tiempo para escritura en FLASH
  
  // NUEVO: Verificar estado I2C después del cambio (antes del reset)
  Serial.printf("DEBUG: Paso 3: Verificando estado I2C después del cambio...\r\n");
  Wire.beginTransmission(old_addr);
  Wire.write(CMD_GET_I2C_STATUS);
  status_error = Wire.endTransmission();
  
  if (status_error == 0) {
    delay(50);
    Wire.requestFrom(old_addr, (uint8_t)1);
    if (Wire.available()) {
      uint8_t post_change_status = Wire.read();
      Serial.printf("   Estado post-cambio: 0x%02X (%s)\r\n", 
                   post_change_status & 0x0F, 
                   (post_change_status & 0x0F) == 0x0F ? "FLASH" : 
                   (post_change_status & 0x0F) == 0x0A ? "UID" : "DESCONOCIDO");
                   
      if ((post_change_status & 0x0F) == 0x0F) {
        Serial.printf("[OK] Dirección guardada en FLASH correctamente\r\n");
      } else {
        Serial.printf("ERROR: Dirección NO se guardó en FLASH\r\n");
        Serial.printf("INFO: Posible problema en el firmware del microcontrolador\r\n");
      }
    }
  } else {
    Serial.printf("WARNING:  No se pudo verificar el estado post-cambio\r\n");
  }
  
  Serial.printf("[OK] Proceso de cambio de dirección completado\r\n");
  Serial.printf("WARNING:  CRÍTICO: Se requiere REINICIAR el dispositivo para aplicar cambios\r\n");
  Serial.printf("INFO: Comando sugerido: r %d\r\n", old_addr);
  Serial.printf("SUGGEST: Después del reset, verificar: vc %d %d\r\n", old_addr, new_addr);
  Serial.println("\r");
}

void factoryReset(uint8_t address) {
  Serial.printf(" FACTORY RESET en dispositivo 0x%02X (%d decimal)\r\n", address, address);
  Serial.println("Restaurando dirección basada en UID...\r");
  
  // Verificar disponibilidad
  Wire.beginTransmission(address);
  if (Wire.endTransmission() != 0) {
    Serial.printf("ERROR: Dispositivo 0x%02X (%d) no responde\r\n", address, address);
    Serial.println("\r");
    return;
  }
  
  // Enviar comando factory reset
  Wire.beginTransmission(address);
  Wire.write(CMD_RESET_FACTORY);
  if (Wire.endTransmission() == 0) {
    delay(50);
    
    Serial.printf("[OK] Factory reset ejecutado correctamente\r\n");
    Serial.printf("WARNING:  NOTA: Se requiere REINICIAR el dispositivo\r\n");
    Serial.printf("INFO: Usar: r %d (o reset 0x%02X)\r\n", address, address);
  } else {
    Serial.printf("ERROR: No se pudo ejecutar factory reset\r\n");
  }
  
  Serial.println("\r");
}

void checkI2CStatus(uint8_t address) {
  Serial.printf("TOTAL: ESTADO I2C del dispositivo 0x%02X (%d decimal)\r\n", address, address);
  
  Wire.beginTransmission(address);
  Wire.write(CMD_GET_I2C_STATUS);
  
  if (Wire.endTransmission() == 0) {
    delay(50);
    Wire.requestFrom(address, (uint8_t)1);
    
    if (Wire.available()) {
      uint8_t status = Wire.read();
      uint8_t status_clean = status & 0x0F;  // Solo bits inferiores
      
      Serial.printf("STEP: Respuesta: 0x%02X (%d decimal)\r\n", status_clean, status_clean);
      
      if (status_clean == 0x0F) {
        Serial.printf("[FLASH] Estado: Usando direccion desde FLASH (personalizada)\r\n");
      } else if (status_clean == 0x0A) {
        Serial.printf(" Estado: Usando dirección desde UID (factory)\r\n");
      } else {
        Serial.printf(" Estado: Desconocido (0x%02X = %d decimal)\r\n", status_clean, status_clean);
      }
    } else {
      Serial.printf("  No hay respuesta del dispositivo\r\n");
    }
  } else {
    Serial.printf("ERROR: No se puede comunicar con el dispositivo\r\n");
  }
  
  Serial.println("\r");
}

void resetDevice(uint8_t address) {
  Serial.printf("CHANGE: REINICIANDO dispositivo 0x%02X (%d decimal)...\r\n", address, address);
  
  // Paso 1: Verificar que el dispositivo responda antes del reset
  Serial.printf("DEBUG: Paso 1: Verificando comunicación inicial...\r\n");
  Wire.beginTransmission(address);
  uint8_t pre_reset_error = Wire.endTransmission();
  
  if (pre_reset_error != 0) {
    Serial.printf("ERROR: Dispositivo 0x%02X (%d) no responde (Error: %d)\r\n", address, address, pre_reset_error);
    Serial.printf("INFO: Posibles causas: dispositivo desconectado, dirección incorrecta\r\n");
    Serial.println("\r");
    return;
  }
  
  Serial.printf("[OK] Dispositivo responde correctamente\r\n");
  
  // Paso 2: Enviar comando de reset con verificación detallada
  Serial.printf("SEND: Paso 2: Enviando comando de reset (0xFE)...\r\n");
  Wire.beginTransmission(address);
  Wire.write(CMD_RESET);  // 0xFE
  uint8_t reset_error = Wire.endTransmission();
  
  if (reset_error != 0) {
    Serial.printf("ERROR: No se pudo enviar comando de reset (Error: %d)\r\n", reset_error);
    Serial.printf("� Error códigos: 1=datos muy largos, 2=NACK en dirección, 3=NACK en datos, 4=otro error\r\n");
    Serial.println("\r");
    return;
  }
  
  Serial.printf("[OK] Comando de reset enviado correctamente\r\n");
  
  // Paso 3: Esperar tiempo de reset del microcontrolador
  Serial.printf("WAIT: Paso 3: Esperando reset del microcontrolador...\r\n");
  Serial.printf("   - Delay de 100ms para procesamiento...\r\n");
  delay(100);  // Tiempo para que procese el comando
  
  Serial.printf("   - Esperando 3 segundos para reinicio completo...\r\n");
  delay(3000);  // Tiempo aumentado para reinicio completo
  
  // Paso 4: Verificación simple de disponibilidad
  Serial.printf("Paso 4: Verificando disponibilidad post-reset...\r\n");
  
  Wire.beginTransmission(address);
  uint8_t post_error = Wire.endTransmission();
  
  // Resultado final
  Serial.println("===============================================\r");
  if (post_error == 0) {
    Serial.printf("[OK] RESET EXITOSO: Dispositivo 0x%02X (%d) reiniciado y disponible\r\n", address, address);
    Serial.printf("INFO: Reset completado correctamente\r\n");
  } else {
    Serial.printf("[INFO] RESET ENVIADO: Comando de reset procesado\r\n");
    Serial.printf("NOTA: El dispositivo puede haber cambiado de direccion o estar reiniciando\r\n");
    Serial.printf("SUGERENCIA: Ejecutar 's' para escanear el bus y encontrar dispositivos\r\n");
  }
  
  Serial.println("\r");
}

void debugResetDevice(uint8_t address) {
  Serial.printf("[DEBUG] DEBUG RESET DETALLADO - Dispositivo 0x%02X (%d decimal)\r\n", address, address);
  Serial.println("================================================================\r");
  
  // Test 1: Comunicación básica
  Serial.printf("DEBUG: TEST 1: Comunicación básica...\r\n");
  Wire.beginTransmission(address);
  uint8_t basic_error = Wire.endTransmission();
  Serial.printf("   Resultado: %s (Error: %d)\r\n", basic_error == 0 ? "[OK] OK" : "  FALLO", basic_error);
  
  if (basic_error != 0) {
    Serial.printf("  DEBUG TERMINADO: No hay comunicación básica\r\n");
    return;
  }
  
  // Test 2: Verificar compatibilidad
  Serial.printf("DEBUG: TEST 2: Verificando compatibilidad (CMD 0x07)...\r\n");
  bool is_compatible = checkCompatibility(address);
  Serial.printf("   Resultado: %s\r\n", is_compatible ? "[OK] Compatible" : "  No compatible");
  
  // Test 3: Envío de comando reset con monitoreo
  Serial.printf("DEBUG: TEST 3: Enviando comando reset (0xFE) con monitoreo...\r\n");
  
  // Limpiar el bus I2C
  Wire.flush();
  delay(10);
  
  // Enviar comando byte por byte
  Wire.beginTransmission(address);
  Serial.printf("   - Transmisión iniciada a 0x%02X\r\n", address);
  
  size_t bytes_written = Wire.write(CMD_RESET);
  Serial.printf("   - Bytes escritos: %d (esperado: 1)\r\n", bytes_written);
  Serial.printf("   - Comando enviado: 0x%02X (decimal: %d)\r\n", CMD_RESET, CMD_RESET);
  
  uint8_t reset_error = Wire.endTransmission(true);  // true = send stop condition
  Serial.printf("   - Error endTransmission: %d\r\n", reset_error);
  Serial.printf("   - Stop condition enviado\r\n");
  
  if (reset_error != 0) {
    Serial.printf("  ERROR en envío de comando reset\r\n");
    Serial.printf("   Códigos de error:\r\n");
    Serial.printf("   0 = Éxito\r\n");
    Serial.printf("   1 = Datos demasiado largos para buffer\r\n");
    Serial.printf("   2 = NACK recibido en transmisión de dirección\r\n");
    Serial.printf("   3 = NACK recibido en transmisión de datos\r\n");
    Serial.printf("   4 = Otros errores\r\n");
    return;
  }
  
  Serial.printf("[OK] Comando reset enviado correctamente\r\n");
  
  // Test 4: Monitoreo de desconexión
  Serial.printf("DEBUG: TEST 4: Monitoreando desconexión del dispositivo...\r\n");
  for (int i = 1; i <= 10; i++) {
    delay(100);  // 100ms entre checks
    Wire.beginTransmission(address);
    uint8_t check_error = Wire.endTransmission();
    
    Serial.printf("   Monitoreo %2d (%3dms): %s (Error: %d)\r\n", 
                  i, i * 100, 
                  check_error == 0 ? "[ONLINE]" : "[OFFLINE]", 
                  check_error);
                  
    if (check_error != 0 && i > 2) {  // Después de 200ms, esperamos desconexión
      Serial.printf("[OK] Dispositivo se desconectó correctamente (reset iniciado)\r\n");
      break;
    }
  }
  
  // Test 5: Esperar reconexión
  Serial.printf("DEBUG: TEST 5: Esperando reconexión después del reset...\r\n");
  Serial.printf("   Esperando 3 segundos para boot completo...\r\n");
  delay(3000);
  
  bool reconnected = false;
  for (int attempt = 1; attempt <= 10; attempt++) {
    Wire.beginTransmission(address);
    uint8_t reconnect_error = Wire.endTransmission();
    
    Serial.printf("   Reconexion %2d: %s (Error: %d)\r\n", 
                  attempt, 
                  reconnect_error == 0 ? "[ONLINE]" : "[OFFLINE]", 
                  reconnect_error);
    
    if (reconnect_error == 0) {
      reconnected = true;
      Serial.printf("[OK] Dispositivo reconectado en intento %d\r\n", attempt);
      break;
    }
    
    delay(500);  // 500ms entre intentos de reconexión
  }
  
  // Resultado final del debug
  Serial.println("================================================================\r");
  if (reconnected) {
    Serial.printf("[OK] DEBUG RESET EXITOSO\r\n");
    Serial.printf("[OK] El dispositivo 0x%02X se resetó correctamente\r\n", address);
    
    // Test final de compatibilidad
    if (checkCompatibility(address)) {
      Serial.printf("[OK] Mantiene compatibilidad post-reset\r\n");
    } else {
      Serial.printf("WARNING:  Advertencia: Perdió compatibilidad (puede ser normal)\r\n");
    }
  } else {
    Serial.printf("  DEBUG RESET FALLIDO\r\n");
    Serial.printf("  El dispositivo 0x%02X no reconectó después del reset\r\n", address);
    Serial.printf("SUGGEST: Recomendaciones:\r\n");
    Serial.printf("   1. Verificar si cambió de dirección I2C (ejecutar 's')\r\n");
    Serial.printf("   2. Verificar alimentación del dispositivo\r\n");
    Serial.printf("   3. El firmware puede tener un bug en el reset\r\n");
    Serial.printf("   4. Intentar reset físico (desconectar/reconectar)\r\n");
  }
  
  Serial.println("\r");
}

void verifyAddressChange(uint8_t old_addr, uint8_t new_addr) {
  Serial.printf("DEBUG: VERIFICANDO CAMBIO DE DIRECCIÓN I2C\r\n");
  Serial.printf("   Dirección anterior: 0x%02X (%d decimal)\r\n", old_addr, old_addr);
  Serial.printf("   Dirección esperada: 0x%02X (%d decimal)\r\n", new_addr, new_addr);
  Serial.println("================================================================\r");
  
  // Test 1: Verificar que la dirección antigua YA NO responda
  Serial.printf("DEBUG: Test 1: Verificando que 0x%02X ya no responde...\r\n", old_addr);
  Wire.beginTransmission(old_addr);
  uint8_t old_error = Wire.endTransmission();
  
  if (old_error == 0) {
    Serial.printf("  PROBLEMA: La dirección antigua 0x%02X aún responde\r\n", old_addr);
    Serial.printf("   Esto indica que el cambio NO se aplicó correctamente\r\n");
  } else {
    Serial.printf("[OK] Correcto: La dirección antigua 0x%02X ya no responde\r\n", old_addr);
  }
  
  // Test 2: Verificar que la dirección nueva SÍ responda
  Serial.printf("DEBUG: Test 2: Verificando que 0x%02X responde...\r\n", new_addr);
  Wire.beginTransmission(new_addr);
  uint8_t new_error = Wire.endTransmission();
  
  if (new_error == 0) {
    Serial.printf("[OK] Perfecto: La dirección nueva 0x%02X responde correctamente\r\n", new_addr);
    
    // Test 3: Verificar compatibilidad en la nueva dirección
    Serial.printf("DEBUG: Test 3: Verificando compatibilidad en nueva dirección...\r\n");
    if (checkCompatibility(new_addr)) {
      Serial.printf("[OK] Excelente: Dispositivo mantiene compatibilidad en 0x%02X\r\n", new_addr);
    } else {
      Serial.printf("WARNING:  Advertencia: Dispositivo no pasa test de compatibilidad en 0x%02X\r\n", new_addr);
    }
    
    // Test 4: Verificar estado I2C en nueva dirección
    Serial.printf("DEBUG: Test 4: Verificando estado I2C en nueva dirección...\r\n");
    Wire.beginTransmission(new_addr);
    Wire.write(CMD_GET_I2C_STATUS);
    
    if (Wire.endTransmission() == 0) {
      delay(50);
      Wire.requestFrom(new_addr, (uint8_t)1);
      
      if (Wire.available()) {
        uint8_t status = Wire.read();
        uint8_t status_clean = status & 0x0F;
        Serial.printf("   Estado I2C: 0x%02X (%s)\r\n", 
                     status_clean, 
                     status_clean == 0x0F ? "FLASH (personalizada)" : 
                     status_clean == 0x0A ? "UID (factory)" : "DESCONOCIDO");
        
        if (status_clean == 0x0F) {
          Serial.printf("[OK] Perfecto: Usando dirección personalizada desde FLASH\r\n");
        } else {
          Serial.printf("  Problema: No está usando la dirección de FLASH\r\n");
        }
      }
    }
    
  } else {
    Serial.printf("  PROBLEMA: La dirección nueva 0x%02X NO responde\r\n", new_addr);
    Serial.printf("   Error: %d\r\n", new_error);
  }
  
  // Resultado final
  Serial.println("================================================================\r");
  
  bool old_offline = (old_error != 0);
  bool new_online = (new_error == 0);
  
  if (old_offline && new_online) {
    Serial.printf("[OK] CAMBIO DE DIRECCION EXITOSO\r\n");
    Serial.printf("[OK] El dispositivo cambió correctamente de 0x%02X a 0x%02X\r\n", old_addr, new_addr);
  } else if (!old_offline && new_online) {
    Serial.printf("WARNING:  CAMBIO PARCIAL\r\n");
    Serial.printf("WARNING:  El dispositivo responde en AMBAS direcciones (0x%02X y 0x%02X)\r\n", old_addr, new_addr);
    Serial.printf("INFO: Esto puede indicar un problema en el firmware\r\n");
  } else if (old_offline && !new_online) {
    Serial.printf("  DISPOSITIVO PERDIDO\r\n");
    Serial.printf("  El dispositivo no responde en ninguna dirección\r\n");
    Serial.printf("INFO: Ejecutar 's' para escanear todas las direcciones\r\n");
  } else {
    Serial.printf("  CAMBIO FALLIDO\r\n");
    Serial.printf("  El dispositivo sigue en la dirección antigua 0x%02X\r\n", old_addr);
    Serial.printf("INFO: El proceso de cambio no funcionó correctamente\r\n");
  }
  
  Serial.println("\r");
}
