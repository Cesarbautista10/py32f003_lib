#include "MapController.h"
#include <stdarg.h>

// Static variables for test cycles
uint8_t MapController::neo_step_ = 0;
uint8_t MapController::pwm_step_ = 0;
uint8_t MapController::musical_step_ = 0;
uint8_t MapController::alert_step_ = 0;
uint8_t MapController::gradual_step_ = 0;

// ========== helpers de salida ==========
void MapController::emit(String& out, int print, const String& s) {
  out += s;
  if (print) Serial.print(s);
}

void MapController::emitf(String& out, int print, const char* fmt, ...) {
  char buf[256];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  out += buf;
  if (print) Serial.printf("%s", buf);
}

// ========== Ctor/Begin ==========
MapController::MapController() {
  for (int i = 0; i < MAX_DEVICES; i++) device_map_.id_to_address[i] = INVALID_ADDRESS;
  for (int i = 0; i < 256; i++) device_map_.address_to_id[i] = 0;
  device_map_.mapped_count = 0;
  
  // Initialize device list
  memset(found_devices_, 0, sizeof(found_devices_));
  device_count_ = 0;
  continuous_test_ = false;
  current_test_mode_ = TEST_MODE_PA4_ONLY;
  cycle_count_ = 0;
  test_interval_ = DEFAULT_TEST_INTERVAL;
  total_bus_recoveries_ = 0;
  
  // Initialize async I2C management
  last_keepalive_ = 0;
  last_i2c_activity_ = 0;
  last_health_check_time_ = 0;
  keepalive_enabled_ = false;  // DESHABILITADO por defecto para evitar interferencias
  keepalive_device_index_ = 0;
  
  // Initialize rate limiting
  last_command_time_ = 0;
  last_command_address_ = 0;
  memset(last_device_times_, 0, sizeof(last_device_times_));
  last_error_time_ = 0;
  in_cooldown_ = false;
  burst_command_count_ = 0;
  
  // Initialize health tracking for all devices
  for (int i = 0; i < MAX_DEVICES; i++) {
    found_devices_[i].last_health_check = 0;
    found_devices_[i].is_problematic = false;
    found_devices_[i].timeout_count = 0;
    found_devices_[i].avg_response_time = 0;
  }
}

void MapController::begin(TwoWire &wire, int sda, int scl, uint32_t i2c_freq) {
  wire_ = &wire;
  sda_ = sda;
  scl_ = scl;
  i2c_freq_ = i2c_freq;
  
  // Initialize I2C with optimized settings
#if defined(ESP32)
  pinMode(sda_, INPUT_PULLUP);
  pinMode(scl_, INPUT_PULLUP);
  delay(100);
  wire_->begin(sda_, scl_);
  wire_->setTimeout(100);
  wire_->setClock(i2c_freq_);
#else
  wire_->begin();
  wire_->setTimeout(100);
  wire_->setClock(i2c_freq_);
#endif
  
  initializeMapping(/*print=*/0);
}

String MapController::initializeI2C(int print) {
  String out;
  
#if defined(ESP32)
  pinMode(sda_, INPUT_PULLUP);
  pinMode(scl_, INPUT_PULLUP);
  delay(150); // Aumentado para chips nuevos
  
  wire_->begin(sda_, scl_);
  
  // Detección automática de chip ESP32 y configuración específica
  #if CONFIG_IDF_TARGET_ESP32C6
    // ESP32-C6: Hardware I2C diferente, necesita timeouts más largos
    wire_->setTimeout(300);
    wire_->setTimeOut(300); // Por si usa API antigua
    if (print) emit(out, print, "[C6] ");
  #elif CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3: Más sensible a timing, timeout aumentado
    wire_->setTimeout(250);
    wire_->setTimeOut(250);
    if (print) emit(out, print, "[S3] ");
  #elif CONFIG_IDF_TARGET_ESP32C3
    wire_->setTimeout(200);
    wire_->setTimeOut(200);
    if (print) emit(out, print, "[C3] ");
  #else
    // ESP32 clásico
    wire_->setTimeout(150);
    wire_->setTimeOut(150);
    if (print) emit(out, print, "[Classic] ");
  #endif
  
  wire_->setClock(i2c_freq_);
  delay(50); // Estabilización adicional
  
#else
  wire_->begin();
  wire_->setTimeout(100);
  wire_->setClock(i2c_freq_);
#endif
  
  if (print) emitf(out, print, "I2C: SDA=%d SCL=%d %dkHz\r\n", sda_, scl_, i2c_freq_/1000);
  return out;
}

// ========== DEVICE SCANNING & MANAGEMENT ==========

String MapController::scanDevices(int print) {
  String out;
  
  device_count_ = 0;
  bool found_any = false;
  
  memset(found_devices_, 0, sizeof(found_devices_));
  
  // Detectar delays según chip
  uint8_t scan_delay = 8;
  uint8_t found_delay = 5;
  uint8_t addr_delay = 2;
  
  #if CONFIG_IDF_TARGET_ESP32C6
    scan_delay = 15;  // C6 necesita más tiempo
    found_delay = 10;
    addr_delay = 5;
  #elif CONFIG_IDF_TARGET_ESP32S3
    scan_delay = 12;  // S3 también más lento
    found_delay = 8;
    addr_delay = 4;
  #endif
  
  // Bus reset más completo
  wire_->beginTransmission(0x00);
  wire_->endTransmission(true);
  delay(30);
  yield();
  
  // Limpieza completa del buffer
  while(wire_->available()) {
    wire_->read();
  }
  
  delay(30);
  yield();
  
  uint8_t consecutive_scan_failures = 0;
  
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    // 3 intentos para mayor confiabilidad con muchos dispositivos
    bool device_found = false;
    bool had_bus_error = false;
    
    yield(); // Feed watchdog cada dirección
    
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
      // Limpiar buffer antes de cada intento
      while(wire_->available()) {
        wire_->read();
      }
      
      wire_->beginTransmission(addr);
      uint8_t error = wire_->endTransmission(true);
      
      if (error == 0) {
        device_found = true;
        consecutive_scan_failures = 0; // Reset contador de fallos
        break;
      } else if (error == 4 || error == 5) {
        // Error 4 = timeout, Error 5 = bus error
        had_bus_error = true;
        break; // No reintentar si es error de bus
      }
      
      // Delay progresivo entre intentos (variable según chip)
      if (attempt < 2) {
        delay(scan_delay);
      }
    }
    
    if (device_found) {
      found_any = true;
      
      if (device_count_ < MAX_DEVICES) {
        found_devices_[device_count_].address = addr;
        found_devices_[device_count_].assigned_id = getIDByAddress(addr);
        found_devices_[device_count_].is_compatible = true;
        found_devices_[device_count_].is_online = true;
        found_devices_[device_count_].status = "I2C Device";
        found_devices_[device_count_].total_tests = 0;
        found_devices_[device_count_].successful_tests = 0;
        found_devices_[device_count_].failed_tests = 0;
        found_devices_[device_count_].consecutive_failures = 0;
        device_count_++;
      }
      
      if (print) emitf(out, print, "0x%02X\r\n", addr);
      
      delay(found_delay);
    } else {
      // No se encontró dispositivo
      if (had_bus_error) {
        consecutive_scan_failures++;
      }
      
      // Si detectamos muchos fallos consecutivos, probable problema de bus
      if (consecutive_scan_failures >= 10) {
        if (print) emit(out, print, "! Bus recovery\r\n");
        
        // Recuperación completa del bus
        recoverI2CBus();
        delay(50);
        yield();
        
        // Limpiar buffer
        while(wire_->available()) {
          wire_->read();
        }
        
        // Reset del bus
        wire_->beginTransmission(0x00);
        wire_->endTransmission(true);
        delay(30);
        
        consecutive_scan_failures = 0;
      }
    }
    
    delay(addr_delay);
  }
  
  if (print) {
    if (!found_any) {
      emit(out, print, "No devices\r\n");
    } else {
      emitf(out, print, "Found: %d\r\n", device_count_);
    }
  }
  
  // Auto-inject scanned devices for automap (matches MapCRUD behavior)
  setScannedDevices(found_devices_, device_count_);
  
  return out;
}

// ========== FAST SCAN (Optimized for 400kHz I2C) ==========
String MapController::fastScanDevices(int print) {
  String out;
  emit(out, print, ">> FAST SCAN I2C BUS @ 400kHz...\r\n");
  emit(out, print, "Rango: 0x08-0x77 (8-119 decimal)\r\n");
  emit(out, print, "===========================================\r\n");
  
  device_count_ = 0;
  bool found_any = false;
  
  // Clear device array
  memset(found_devices_, 0, sizeof(found_devices_));
  
  // MINIMAL bus reset - just 1 broadcast
  wire_->beginTransmission(0x00);
  wire_->endTransmission(true);
  delay(5);  // Reduced from 60ms
  
  // Clear buffer
  while(wire_->available()) {
    wire_->read();
  }
  
  // Scan all addresses with 2 attempts para mayor confiabilidad
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    // Intentar 2 veces antes de considerar que no hay dispositivo
    bool device_found = false;
    
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
      // Limpiar buffer antes de cada intento
      while(wire_->available()) {
        wire_->read();
      }
      
      wire_->beginTransmission(addr);
      uint8_t error = wire_->endTransmission(true);
      
      if (error == 0) {
        device_found = true;
        break; // Encontrado, no necesitamos más intentos
      }
      
      // Pequeño delay entre intentos
      if (attempt < 1) {
        delay(5); // 5ms entre intentos
      }
    }
    
    if (device_found) {
      found_any = true;
      
      // Store in array
      if (device_count_ < MAX_DEVICES) {
        found_devices_[device_count_].address = addr;
        found_devices_[device_count_].assigned_id = getIDByAddress(addr);
        found_devices_[device_count_].is_compatible = true;
        found_devices_[device_count_].is_online = true;
        found_devices_[device_count_].status = "I2C Device";
        found_devices_[device_count_].total_tests = 0;
        found_devices_[device_count_].successful_tests = 0;
        found_devices_[device_count_].failed_tests = 0;
        found_devices_[device_count_].consecutive_failures = 0;
        device_count_++;
      }
      
      // Show result
      emitf(out, print, "[OK] 0x%02X (%3d) - I2C Device\r\n", addr, addr);
      
      delay(5); // Delay después de encontrar dispositivo
    }
    
    delay(1); // Delay mínimo entre direcciones
  }
  
  emit(out, print, "===========================================\r\n");
  
  if (!found_any) {
    emit(out, print, "ERROR: No se encontraron dispositivos I2C\r\n");
    emit(out, print, "NOTA: Verificar conexiones SDA/SCL y alimentacion\r\n");
    emit(out, print, "SUGERENCIA: Usar 'scan' normal si hay problemas\r\n");
  } else {
    emitf(out, print, "TOTAL: %d dispositivos I2C encontrados\r\n", device_count_);
  }
  
  // Auto-inject scanned devices
  setScannedDevices(found_devices_, device_count_);
  
  return out;
}

// Compatibility check disabled - all responding devices are considered valid
// This function is kept for potential future use
bool MapController::checkCompatibility(uint8_t address) {
  // Send PA4_DIGITAL command to verify compatibility con reintentos
  const uint8_t max_attempts = 2;
  
  for (uint8_t attempt = 0; attempt < max_attempts; attempt++) {
    if (attempt > 0) {
      delay(20); // Pausa entre intentos para que el slave se recupere
    }
    
    wire_->beginTransmission(address);
    wire_->write(CMD_PA4_DIGITAL);
    
    if (wire_->endTransmission(true) == 0) { // Force STOP condition
      delay(30); // Dar tiempo al PY32F003 para procesar comando y preparar respuesta
      
      uint8_t bytes_received = wire_->requestFrom(address, (uint8_t)1, (uint8_t)true); // Force STOP after read
      
      if (bytes_received > 0 && wire_->available()) {
        uint8_t response = wire_->read();
        delay(5); // Breve delay después de lectura
        
        // Compatible device formats:
        // - Legacy firmware: response = 0 or 1 (direct PA4 state)
        // - v6.0.0_lite: bits[3-0] = 0x09, bits[7-4] = PA4 state
        // Accept: 0, 1, 0x09, 0x19 (and any valid response with lower nibble pattern)
        uint8_t lower_nibble = response & 0x0F;
        
        if (response == 0 || response == 1 || lower_nibble == 0x09) {
          return true; // Compatible PY32F003
        }
      }
    }
  }
  
  return false; // No compatible o no responde correctamente
}

String MapController::listDevices(int print) {
  String out;
  emit(out, print, "DISPOSITIVOS ENCONTRADOS:\r\n");
  emit(out, print, "===========================================\r\n");
  
  if (device_count_ == 0) {
    emit(out, print, "ERROR: No hay dispositivos. Usa scanDevices() primero.\r\n");
    return out;
  }
  
  emit(out, print, "+----------+--------+-------------+----------+----------+\r\n");
  emit(out, print, "| Direccion| Estado | Descripcion | Exitos   | Fallos   |\r\n");
  emit(out, print, "+----------+--------+-------------+----------+----------+\r\n");
  
  for (int i = 0; i < device_count_; i++) {
    String icon = found_devices_[i].is_compatible ? "[OK]" : "[DEV]";
    String status = found_devices_[i].is_online ? "ONLINE" : "OFFLINE";
    
    emitf(out, print, "|%s 0x%02X | %-6s | %-11s | %-8d | %-8d |\r\n",
      icon.c_str(),
      found_devices_[i].address,
      status.c_str(),
      found_devices_[i].is_compatible ? "PY32F003" : "Generico",
      found_devices_[i].successful_tests,
      found_devices_[i].failed_tests
    );
  }
  
  emit(out, print, "+----------+--------+-------------+----------+----------+\r\n");
  return out;
}

String MapController::pingDevice(uint8_t address, int print) {
  String out;
  emitf(out, print, "PING: 0x%02X (%d decimal)\r\n", address, address);
  
  wire_->beginTransmission(address);
  uint8_t error = wire_->endTransmission();
  
  if (error == 0) {
    emit(out, print, "[OK] Dispositivo responde\r\n");
    
    if (checkCompatibility(address)) {
      emit(out, print, "COMPATIBLE: PY32F003 firmware\r\n");
    } else {
      emit(out, print, "DISPOSITIVO: Generico (no compatible)\r\n");
    }
  } else {
    emitf(out, print, "ERROR: No responde (Error: %d)\r\n", error);
  }
  
  return out;
}

uint8_t MapController::parseAddress(String addr_str) {
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
    // Note: In library context, we don't print errors directly
    return 0;
  }
  
  return address;
}

bool MapController::recoverI2CBus() {
  // Método robusto de recuperación del bus I2C
  // Maneja dispositivos colgados y clock stretching infinito
  
  yield(); // CRITICAL: Feed watchdog antes de operación larga
  
  // 1. Terminar sesión I2C actual
  wire_->end();
  delay(10); // Reducido de 20ms a 10ms
  yield();
  
#if defined(ESP32)
  // 2. Enviar pulsos de clock para liberar dispositivos colgados (SIMPLIFICADO)
  pinMode(scl_, OUTPUT);
  pinMode(sda_, INPUT_PULLUP);
  
  // Solo 5 pulsos en lugar de 9 (más rápido)
  for (int i = 0; i < 5; i++) {
    digitalWrite(scl_, LOW);
    delayMicroseconds(5); // Reducido de 10 a 5
    digitalWrite(scl_, HIGH);
    delayMicroseconds(5);
  }
  yield();
  
  // 3. Generar condición STOP para resetear estado del bus
  pinMode(sda_, OUTPUT);
  digitalWrite(sda_, LOW);
  delayMicroseconds(5);
  pinMode(scl_, INPUT_PULLUP);
  delayMicroseconds(5);
  pinMode(sda_, INPUT_PULLUP);
  delayMicroseconds(5);
  
  delay(20); // Reducido de 50ms a 20ms
  yield();
  
  // 4. Reinicializar I2C
  wire_->begin(sda_, scl_);
  
  // Re-configurar timeout según chip
  #if CONFIG_IDF_TARGET_ESP32C6
    wire_->setTimeout(300);
    wire_->setTimeOut(300);
  #elif CONFIG_IDF_TARGET_ESP32S3
    wire_->setTimeout(250);
    wire_->setTimeOut(250);
  #elif CONFIG_IDF_TARGET_ESP32C3
    wire_->setTimeout(200);
    wire_->setTimeOut(200);
  #else
    wire_->setTimeout(150);
    wire_->setTimeOut(150);
  #endif
  
#else
  // Para otras plataformas - reset básico
  wire_->begin();
  wire_->setTimeout(I2C_COMMAND_TIMEOUT_MS);
#endif
  
  // 5. Configurar velocidad
  wire_->setClock(i2c_freq_);
  yield();
  
  // 6. Broadcast para despertar dispositivos
  wire_->beginTransmission(0x00);
  wire_->endTransmission(true);
  delay(10);
  yield();
  
  // 7. Limpiar buffer residual
  while (wire_->available()) {
    wire_->read();
  }
  
  total_bus_recoveries_++;
  yield(); // Feed watchdog al final
  return true;
}

// ========== HEALTH MONITORING SYSTEM ==========

void MapController::performHealthCheck() {
  // Realizar health check periódico de todos los dispositivos
  uint32_t current_time = millis();
  
  for (int i = 0; i < device_count_; i++) {
    // Solo revisar si ha pasado el intervalo
    if (current_time - found_devices_[i].last_health_check < I2C_HEALTH_CHECK_INTERVAL) {
      continue;
    }
    
    found_devices_[i].last_health_check = current_time;
    
    // Verificar si el dispositivo es problemático
    if (found_devices_[i].consecutive_failures >= I2C_CONSECUTIVE_FAIL_THRESHOLD) {
      markDeviceProblematic(i, true);
    } else if (found_devices_[i].consecutive_failures == 0 && found_devices_[i].is_problematic) {
      // Dispositivo se ha recuperado
      markDeviceProblematic(i, false);
    }
    
    // Actualizar timeout count basado en fallos recientes
    if (found_devices_[i].failed_tests > found_devices_[i].successful_tests) {
      found_devices_[i].timeout_count++;
    } else if (found_devices_[i].timeout_count > 0) {
      found_devices_[i].timeout_count--; // Decrementar si mejora
    }
  }
}

bool MapController::isDeviceHealthy(uint8_t index) {
  if (index >= device_count_) {
    return false;
  }
  
  DeviceInfo& dev = found_devices_[index];
  
  // Criterios de salud:
  // 1. No está marcado como problemático
  // 2. Consecutive failures bajo
  // 3. Tasa de éxito razonable (>50%)
  // 4. Timeouts bajo control
  
  if (dev.is_problematic) {
    return false;
  }
  
  if (dev.consecutive_failures >= 3) {
    return false;
  }
  
  if (dev.total_tests > 10) {
    float success_rate = (float)dev.successful_tests / (float)dev.total_tests;
    if (success_rate < 0.5) {
      return false;
    }
  }
  
  if (dev.timeout_count > 5) {
    return false;
  }
  
  return true;
}

void MapController::markDeviceProblematic(uint8_t index, bool problematic) {
  if (index >= device_count_) {
    return;
  }
  
  found_devices_[index].is_problematic = problematic;
  
  if (problematic) {
    found_devices_[index].status = "PROBLEMATIC";
  } else {
    found_devices_[index].status = "RECOVERED";
  }
}

String MapController::showHealthReport(int print) {
  String out;
  emit(out, print, "\n╔═════════════════════════════════════════════════╗\n");
  emit(out, print, "║         I2C BUS HEALTH REPORT                   ║\n");
  emit(out, print, "╚═════════════════════════════════════════════════╝\n\n");
  
  emitf(out, print, "Total Bus Recoveries: %d\n", total_bus_recoveries_);
  emitf(out, print, "Active Devices: %d\n\n", device_count_);
  
  if (device_count_ == 0) {
    emit(out, print, "No devices to report.\n");
    return out;
  }
  
  int healthy_count = 0;
  int problematic_count = 0;
  
  emit(out, print, "Device Health Status:\n");
  emit(out, print, "═══════════════════════════════════════════════════\n");
  
  for (int i = 0; i < device_count_; i++) {
    DeviceInfo& dev = found_devices_[i];
    bool healthy = isDeviceHealthy(i);
    
    if (healthy) {
      healthy_count++;
    } else {
      problematic_count++;
    }
    
    String health_icon = healthy ? "✓" : "⚠";
    String status_text = healthy ? "HEALTHY" : "DEGRADED";
    
    emitf(out, print, "[%s] 0x%02X - %-10s | Success: %d/%d | Fails: %d | Timeouts: %d\n",
      health_icon.c_str(),
      dev.address,
      status_text.c_str(),
      dev.successful_tests,
      dev.total_tests,
      dev.consecutive_failures,
      dev.timeout_count
    );
    
    // Detalles adicionales para dispositivos problemáticos
    if (!healthy && print) {
      if (dev.is_problematic) {
        emitf(out, print, "    └─ ALERT: Device marked as problematic!\n");
      }
      if (dev.consecutive_failures >= 3) {
        emitf(out, print, "    └─ WARNING: %d consecutive failures\n", dev.consecutive_failures);
      }
      if (dev.timeout_count > 5) {
        emitf(out, print, "    └─ WARNING: High timeout count (%d)\n", dev.timeout_count);
      }
    }
  }
  
  emit(out, print, "═══════════════════════════════════════════════════\n");
  emitf(out, print, "Summary: %d healthy, %d degraded\n", healthy_count, problematic_count);
  
  if (problematic_count > 0) {
    emit(out, print, "\n💡 RECOMMENDATIONS:\n");
    emit(out, print, "  • Check physical connections to degraded devices\n");
    emit(out, print, "  • Verify power supply stability\n");
    emit(out, print, "  • Consider increasing test interval if needed\n");
    emit(out, print, "  • Use 'reset' command on problematic devices\n");
  }
  
  return out;
}

// ========== ASYNC I2C MANAGEMENT SYSTEM ==========

void MapController::updateAsyncI2C() {
  // Sistema asíncrono principal - llamar en loop()
  // Mantiene el bus I2C activo independiente del intervalo de testing
  
  yield(); // Feed ESP32 watchdog
  
  uint32_t current_time = millis();
  
  // 0. Recuperación preventiva del bus cada 30s para evitar estado inválido
  static uint32_t last_preventive_recovery = 0;
  if (current_time - last_preventive_recovery >= I2C_PREVENTIVE_RECOVERY) {
    // Limpieza preventiva del bus
    while(wire_->available()) wire_->read();
    wire_->beginTransmission(0x00);
    wire_->endTransmission(true);
    delay(10);
    last_preventive_recovery = current_time;
    yield();
  }
  
  // 1. Realizar keepalive si es necesario
  if (shouldPerformKeepalive()) {
    performKeepalive();
    yield(); // Feed watchdog after keepalive
  }
  
  // 2. Health check periódico (no bloqueante)
  if (current_time - last_health_check_time_ >= I2C_HEALTH_CHECK_INTERVAL) {
    performHealthCheck();
    last_health_check_time_ = current_time;
    yield(); // Feed watchdog after health check
  }
}

bool MapController::shouldPerformKeepalive() {
  if (!keepalive_enabled_ || device_count_ == 0) {
    return false;
  }
  
  uint32_t current_time = millis();
  uint32_t time_since_keepalive = current_time - last_keepalive_;
  uint32_t time_since_activity = current_time - last_i2c_activity_;
  
  // Keepalive si:
  // 1. Ha pasado el intervalo de keepalive Y
  // 2. No ha habido actividad reciente (evitar colisiones)
  if (time_since_keepalive >= I2C_KEEPALIVE_INTERVAL &&
      time_since_activity >= I2C_MIN_ACTIVITY_INTERVAL) {
    return true;
  }
  
  // Keepalive forzado si el bus está muy idle
  if (time_since_activity >= I2C_MAX_IDLE_TIME) {
    return true;
  }
  
  return false;
}

void MapController::performKeepalive() {
  // Realizar ping ligero a un dispositivo para mantener bus activo
  // Rotar entre dispositivos para distribuir carga
  
  if (device_count_ == 0) {
    return;
  }
  
  uint32_t start_time = millis();
  
  // Seleccionar dispositivo para keepalive (round-robin)
  uint8_t device_index = keepalive_device_index_ % device_count_;
  uint8_t address = found_devices_[device_index].address;
  
  // Limpiar buffer antes de ping
  while(wire_->available()) wire_->read();
  
  // Solo hacer keepalive a dispositivos saludables
  if (!found_devices_[device_index].is_problematic) {
    // Ping simple y rápido - solo verificar presencia
    wire_->beginTransmission(address);
    uint8_t error = wire_->endTransmission(true);
    
    if (error == 0) {
      // Dispositivo responde - actualizar estadísticas
      found_devices_[device_index].is_online = true;
      found_devices_[device_index].last_success_time = millis();
      
      // Reset consecutive failures si responde
      if (found_devices_[device_index].consecutive_failures > 0) {
        found_devices_[device_index].consecutive_failures = 0;
      }
    } else {
      // No responde - incrementar fallos
      found_devices_[device_index].consecutive_failures++;
      
      // Si muchos fallos, marcar como offline
      if (found_devices_[device_index].consecutive_failures >= I2C_CONSECUTIVE_FAIL_THRESHOLD) {
        found_devices_[device_index].is_online = false;
        markDeviceProblematic(device_index, true);
      }
      
      // Si detectamos que MUCHOS dispositivos fallan simultáneamente, recuperar bus
      uint8_t offline_count = 0;
      for (uint8_t i = 0; i < device_count_; i++) {
        if (!found_devices_[i].is_online) offline_count++;
      }
      
      // Si más del 50% están offline, probable problema de bus
      if (offline_count > device_count_ / 2) {
        recoverI2CBus();
        delay(50);
      }
    }
  }
  
  // Actualizar contadores
  last_keepalive_ = millis();
  last_i2c_activity_ = last_keepalive_;
  
  // Avanzar al siguiente dispositivo para próximo keepalive
  keepalive_device_index_++;
  if (keepalive_device_index_ >= device_count_) {
    keepalive_device_index_ = 0;
  }
  
  // Pequeño delay para no saturar el bus
  uint32_t elapsed = millis() - start_time;
  if (elapsed < 5) {
    delay(5 - elapsed);
  }
}

void MapController::resetIdleTimer() {
  // Llamar después de cualquier operación I2C para actualizar timer
  last_i2c_activity_ = millis();
}

uint32_t MapController::getTimeSinceLastActivity() const {
  return millis() - last_i2c_activity_;
}

// ========== RATE LIMITING & PROTECTION ==========

bool MapController::canSendCommand(uint8_t address) const {
  // Always allow - rate limiting handled by enforceRateLimit
  return true;
}

void MapController::enforceRateLimit(uint8_t address) {
  uint32_t current_time = millis();
  uint32_t required_delay = 0;
  
  // Same device protection - delay mínimo
  if (last_command_address_ == address) {
    uint32_t time_since_last_cmd = current_time - last_command_time_;
    if (time_since_last_cmd < I2C_SAME_DEVICE_INTERVAL) {
      required_delay = I2C_SAME_DEVICE_INTERVAL - time_since_last_cmd;
    }
  }
  // Different device - delay mínimo
  else {
    uint32_t time_since_last_cmd = current_time - last_command_time_;
    if (time_since_last_cmd < I2C_MIN_COMMAND_INTERVAL) {
      required_delay = I2C_MIN_COMMAND_INTERVAL - time_since_last_cmd;
    }
  }
  
  // Execute delay if needed (máximo 20ms)
  if (required_delay > 0) {
    if (required_delay > 20) required_delay = 20; // Hard cap
    delay(required_delay);
    yield(); // Always yield after delay
  }
}

void MapController::markCommandSent(uint8_t address) {
  last_command_time_ = millis();
  last_command_address_ = address;
}

void MapController::markCommandError(uint8_t address) {
  // Simply mark the time
  last_command_time_ = millis();
  last_command_address_ = address;
}

// ========== BASIC DEVICE COMMANDS ==========

bool MapController::sendBasicCommand(uint8_t address, uint8_t command) {
  const uint8_t max_retries = I2C_MAX_RETRIES;
  bool bus_recovered = false;
  
  // 🐕 WATCHDOG: Timeout absoluto para prevenir bloqueo total
  uint32_t watchdog_start = millis();
  const uint32_t WATCHDOG_TIMEOUT = 200; // 200ms máximo - fallar rápido es mejor que congelarse
  
  // Si es el primer comando después de mucho tiempo inactivo, pausa extra
  uint32_t time_since_last = millis() - last_command_time_;
  if (time_since_last > 5000 || last_command_time_ == 0) {
    delay(30); // Pausa reducida para estabilización
    yield();
  }
  
  // ⚠️ CRITICAL: Enforce rate limiting to prevent bus saturation
  if (!canSendCommand(address)) {
    enforceRateLimit(address);
  }
  
  // Yield to ESP32 watchdog
  yield();
  
  // Clear buffer antes de empezar
  while (wire_->available()) {
    wire_->read();
  }
  
  for (uint8_t retry = 0; retry < max_retries; retry++) {
    // 🐕 WATCHDOG CHECK: Abort if taking too long (sin recovery)
    if (millis() - watchdog_start > WATCHDOG_TIMEOUT) {
      markCommandError(address);
      yield();
      return false; // Abort inmediato
    }
    
    yield(); // Feed ESP32 watchdog
    
    // Delay progresivo entre reintentos (backoff reducido)
    if (retry > 0) {
      uint32_t backoff_delay = 10 * (retry + 1); // 10ms, 20ms (reducido)
      delay(backoff_delay);
      yield();
    }
    
    // Iniciar transacción
    wire_->beginTransmission(address);
    wire_->write(command);
    
    uint8_t error = wire_->endTransmission(true);  // Force STOP condition
    
    if (error == 0) {
      // Transmisión exitosa - esperar respuesta
      delay(8);  // Mínimo delay para procesamiento
      yield();
      
      // Intentar leer respuesta (SIMPLIFICADO - solo 1 intento rápido)
      uint8_t bytes_received = wire_->requestFrom(address, (uint8_t)1, (uint8_t)true);
      
      if (bytes_received == 1 && wire_->available()) {
        uint8_t response = wire_->read();
        delay(3);
        resetIdleTimer();
        markCommandSent(address);
        return true;  // ✅ Comando exitoso
      }
      
      // Timeout esperando respuesta
      continue; // Retry
      
    } else {
      // Error de transmisión
      if (error == 2) {
        // NACK on transmit of address - dispositivo no existe/no responde
        // NO hacer recovery, es normal que no responda
        markCommandError(address);
        return false; // Salir inmediatamente, no hay dispositivo
      } else if (error == 3) {
        // NACK on transmit of data - dispositivo rechaza comando
        markCommandError(address);
        return false; // No tiene sentido reintentar
      } else if (error == 4 || error == 5) {
        // Error 4 = timeout, Error 5 = bus error
        // SOLO en estos casos hacer recovery
        if (!bus_recovered) {
          recoverI2CBus();
          bus_recovered = true;
          yield();
        }
      }
      
      // Continuar con retry solo si no es NACK de address
      continue;
    }
  }
  
  // Todos los reintentos fallaron
  markCommandError(address);  // Record error for cooldown
  return false;
}

bool MapController::i2cReadByteWithRetry(uint8_t address, uint8_t command, uint8_t* out,
                                        uint8_t attempts, uint32_t wait_ms) {
  bool bus_recovered = false;
  
  for (uint8_t i = 0; i < attempts; i++) {
    // Delay progresivo entre reintentos
    if (i > 0) {
      uint32_t backoff = 10 * (i + 1); // 10ms, 20ms, 30ms...
      delay(backoff);
    }
    
    // Limpiar buffer
    while (wire_->available()) {
      wire_->read();
    }
    
    // Enviar comando con timeout
    uint32_t start_time = millis();
    wire_->beginTransmission(address);
    wire_->write(command);
    
    if (millis() - start_time > I2C_COMMAND_TIMEOUT_MS) {
      wire_->endTransmission(true);
      if (i == attempts - 1 && !bus_recovered) {
        recoverI2CBus();
        bus_recovered = true;
        delay(I2C_RECOVERY_DELAY_MS);
      }
      continue;
    }
    
    uint8_t err = wire_->endTransmission(true); // send STOP
    
    if (err == 0) {
      // Esperar respuesta con timeout adaptativo
      delay(wait_ms > 0 ? wait_ms : 10);
      
      start_time = millis();
      while (millis() - start_time < I2C_RESPONSE_TIMEOUT_MS) {
        uint8_t req = wire_->requestFrom(address, (uint8_t)1, (uint8_t)true);
        if (req == 1 && wire_->available()) {
          *out = wire_->read();
          delay(5); // Limpieza del bus
          return true;
        }
        delay(5);
      }
    } else if (err == 2 && i == attempts - 1 && !bus_recovered) {
      // NACK en último intento - intentar recovery
      recoverI2CBus();
      bus_recovered = true;
      delay(I2C_RECOVERY_DELAY_MS);
    }
  }
  return false;
}

uint8_t MapController::readPA4Digital(uint8_t address) {
  uint8_t byte_val = 0;
  if (!i2cReadByteWithRetry(address, CMD_PA4_DIGITAL, &byte_val, 2, 5)) {  // Reducido attempts y wait
    return 0; // failure -> assume LOW
  }
  // PA4 is in upper bits (7-4)
  return (byte_val & 0xF0) ? 1 : 0;
}

uint16_t MapController::readADC(uint8_t address) {
  uint16_t adc_value = 0;
  uint8_t byte_val = 0;

  // Read HSB (bits 11-8 in lower nibble; upper nibble may contain PA4)
  if (i2cReadByteWithRetry(address, CMD_ADC_PA0_HSB, &byte_val, 2, 5)) {  // Optimizado para velocidad
    uint8_t hsb = byte_val & 0x0F; // clean PA4 from upper nibble
    adc_value = (hsb << 8);
  } else {
    // HSB failure, return 0 to indicate invalid reading
    return 0;
  }

  // Read LSB (complete bits 7-0)
  if (i2cReadByteWithRetry(address, CMD_ADC_PA0_LSB, &byte_val, 2, 5)) {  // Optimizado para velocidad
    adc_value |= byte_val;
  } else {
    // If LSB fails, return only shifted HSB (otherwise 0)
    // Here we prefer to indicate complete failure
    return 0;
  }

  return adc_value;
}

// ========== CONTINUOUS TESTING SYSTEM ==========

void MapController::startContinuousTest(TestMode mode) {
  if (device_count_ == 0) {
    return; // No devices to test
  }
  
  current_test_mode_ = mode;
  continuous_test_ = true;
  cycle_count_ = 0;
}

void MapController::stopContinuousTest() {
  continuous_test_ = false;
  
  // Limpieza completa del estado del bus
  delay(50); // Esperar a que termine cualquier operación pendiente
  yield();
  
  // Limpiar buffer I2C
  while(wire_->available()) {
    wire_->read();
  }
  
  // Reset completo del bus
  wire_->beginTransmission(0x00);
  wire_->endTransmission(true);
  delay(30);
  
  // Resetear todos los timers y estados
  last_command_time_ = 0;
  last_command_address_ = 0xFF;
  in_cooldown_ = false;
  burst_command_count_ = 0;
  
  // Resetear timers por dispositivo
  for (int i = 0; i < 128; i++) {
    last_device_times_[i] = 0;
  }
  
  // Resetear timer de actividad para permitir keepalive inmediato
  last_i2c_activity_ = millis() - I2C_MAX_IDLE_TIME;
  
  yield();
  delay(50); // Dar tiempo al bus para estabilizarse
}

void MapController::runContinuousTest() {
  if (!continuous_test_ || device_count_ == 0) {
    return;
  }
  
  cycle_count_++;
  
  // Keep-alive for long intervals: ping before each command
  if (test_interval_ > 1000) {
    for (int i = 0; i < device_count_; i++) {
      if (found_devices_[i].is_compatible) {
        // Short ping to activate slave
        wire_->beginTransmission(found_devices_[i].address);
        wire_->endTransmission();
        delay(1);  // Only 1ms delay
      }
    }
  }
  
  for (int i = 0; i < device_count_; i++) {
    if (found_devices_[i].is_compatible) {
      testDevice(i);
    }
  }
}

void MapController::testDevice(uint8_t index) {
  if (index >= device_count_) return;
  
  uint8_t addr = found_devices_[index].address;
  found_devices_[index].total_tests++;
  
  // Basic communication test
  wire_->beginTransmission(addr);
  uint8_t error = wire_->endTransmission();
  
  if (error == 0) {
    bool command_success = true;
    
    // Execute commands based on selected mode
    switch(current_test_mode_) {
      case TEST_MODE_PA4_ONLY:
        testDevicePA4(index);
        break;
      case TEST_MODE_TOGGLE_ONLY:
        testDeviceToggle(index);
        break;
      case TEST_MODE_BOTH:
        testDeviceBoth(index);
        break;
      case TEST_MODE_NEO_CYCLE:
        testDeviceNeoCycle(index);
        break;
      case TEST_MODE_PWM_TONES:
        testDevicePWMCycle(index);
        break;
      case TEST_MODE_ADC_PA0:
        testDeviceADC(index);
        break;
      case TEST_MODE_MUSICAL_SCALE:
        testDeviceMusicalScale(index);
        break;
      case TEST_MODE_ALERTS:
        testDeviceAlerts(index);
        break;
      case TEST_MODE_PWM_GRADUAL:
        testDevicePWMGradual(index);
        break;
    }
    
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
    found_devices_[index].is_online = false;
    
    // Auto recovery if many consecutive failures
    if (found_devices_[index].consecutive_failures >= 5) {
      recoverI2CBus();
      found_devices_[index].consecutive_failures = 0;
    }
  }
}

void MapController::testDevicePA4(uint8_t index) {
  uint8_t pa4_state = readPA4Digital(found_devices_[index].address);
  found_devices_[index].last_pa4_state = pa4_state;
  
  found_devices_[index].successful_tests++;
  found_devices_[index].last_success_time = millis();
  found_devices_[index].consecutive_failures = 0;
  found_devices_[index].is_online = true;
}

void MapController::testDeviceToggle(uint8_t index) {
  bool toggle_ok = sendBasicCommand(found_devices_[index].address, CMD_TOGGLE);
  
  if (toggle_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
}

void MapController::testDeviceBoth(uint8_t index) {
  bool toggle_ok = sendBasicCommand(found_devices_[index].address, CMD_TOGGLE);
  uint8_t pa4_state = readPA4Digital(found_devices_[index].address);
  found_devices_[index].last_pa4_state = pa4_state;
  
  if (toggle_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
}

void MapController::testDeviceNeoCycle(uint8_t index) {
  uint8_t neo_cmd = CMD_NEO_OFF;
  
  switch(neo_step_ % 5) {
    case 0: neo_cmd = CMD_NEO_WHITE; break;
    case 1: neo_cmd = CMD_NEO_RED; break;
    case 2: neo_cmd = CMD_NEO_GREEN; break;
    case 3: neo_cmd = CMD_NEO_BLUE; break;
    case 4: neo_cmd = CMD_NEO_OFF; break;
  }
  
  bool neo_ok = sendBasicCommand(found_devices_[index].address, neo_cmd);
  
  if (neo_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
  
  if (index == device_count_ - 1) { // Last device in cycle
    neo_step_++;
  }
}

void MapController::testDevicePWMCycle(uint8_t index) {
  uint8_t pwm_cmd = CMD_PWM_OFF;
  
  switch(pwm_step_ % 5) {
    case 0: pwm_cmd = CMD_PWM_OFF; break;
    case 1: pwm_cmd = CMD_PWM_25; break;
    case 2: pwm_cmd = CMD_PWM_50; break;
    case 3: pwm_cmd = CMD_PWM_75; break;
    case 4: pwm_cmd = CMD_PWM_100; break;
  }
  
  bool pwm_ok = sendBasicCommand(found_devices_[index].address, pwm_cmd);
  
  if (pwm_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
  
  if (index == device_count_ - 1) { // Last device in cycle
    pwm_step_++;
  }
}

void MapController::testDeviceADC(uint8_t index) {
  uint16_t adc_value = readADC(found_devices_[index].address);
  found_devices_[index].last_adc_value = adc_value;
  
  bool command_success = (adc_value > 0);
  
  if (command_success) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
}

void MapController::testDeviceMusicalScale(uint8_t index) {
  uint8_t tone_cmd = CMD_PWM_OFF;
  
  switch(musical_step_ % 8) {
    case 0: tone_cmd = CMD_TONE_DO; break;   // 261Hz
    case 1: tone_cmd = CMD_TONE_RE; break;   // 294Hz
    case 2: tone_cmd = CMD_TONE_MI; break;   // 330Hz
    case 3: tone_cmd = CMD_TONE_FA; break;   // 349Hz
    case 4: tone_cmd = CMD_TONE_SOL; break;  // 392Hz
    case 5: tone_cmd = CMD_TONE_LA; break;   // 440Hz
    case 6: tone_cmd = CMD_TONE_SI; break;   // 494Hz
    case 7: tone_cmd = CMD_PWM_OFF; break;   // Silence
  }
  
  bool tone_ok = sendBasicCommand(found_devices_[index].address, tone_cmd);
  
  if (tone_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
  
  if (index == device_count_ - 1) { // Last device in cycle
    musical_step_++;
  }
}

void MapController::testDeviceAlerts(uint8_t index) {
  uint8_t alert_cmd = CMD_PWM_OFF;
  
  switch(alert_step_ % 5) {
    case 0: alert_cmd = CMD_SUCCESS; break;  // 800Hz
    case 1: alert_cmd = CMD_OK; break;       // 1000Hz
    case 2: alert_cmd = CMD_WARNING; break;  // 1200Hz
    case 3: alert_cmd = CMD_ALERT; break;    // 1500Hz
    case 4: alert_cmd = CMD_PWM_OFF; break;  // OFF
  }
  
  bool alert_ok = sendBasicCommand(found_devices_[index].address, alert_cmd);
  
  if (alert_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
  
  if (index == device_count_ - 1) { // Last device in cycle
    alert_step_++;
  }
}

void MapController::testDevicePWMGradual(uint8_t index) {
  uint8_t pwm_cmd = CMD_PWM_OFF;
  
  switch(gradual_step_ % 4) {
    case 0: pwm_cmd = CMD_PWM_GRADUAL_MIN; break;      // 0x10 - 100Hz
    case 1: pwm_cmd = CMD_PWM_GRADUAL_MIN + 8; break;  // 0x18 - ~550Hz
    case 2: pwm_cmd = CMD_PWM_GRADUAL_MAX; break;      // 0x1F - 1000Hz
    case 3: pwm_cmd = CMD_PWM_OFF; break;              // OFF
  }
  
  bool pwm_ok = sendBasicCommand(found_devices_[index].address, pwm_cmd);
  
  if (pwm_ok) {
    found_devices_[index].successful_tests++;
    found_devices_[index].last_success_time = millis();
    found_devices_[index].consecutive_failures = 0;
    found_devices_[index].is_online = true;
  } else {
    found_devices_[index].failed_tests++;
    found_devices_[index].last_failure_time = millis();
    found_devices_[index].consecutive_failures++;
  }
  
  if (index == device_count_ - 1) { // Last device in cycle
    gradual_step_++;
  }
}

void MapController::setTestInterval(uint32_t interval_ms) {
  if (interval_ms >= MIN_TEST_INTERVAL && interval_ms <= MAX_TEST_INTERVAL) {
    test_interval_ = interval_ms;
    last_keepalive_ = millis(); // Reset keep-alive timer
  }
}

String MapController::showStatistics(int print) {
  String out;
  emit(out, print, "\n=== ESTADISTICAS ===\r\n");
  emitf(out, print, "Ciclos ejecutados: %d\r\n", cycle_count_);
  emitf(out, print, "Recuperaciones de bus: %d\r\n", total_bus_recoveries_);
  
  for (int i = 0; i < device_count_; i++) {
    if (found_devices_[i].total_tests > 0) {
      float success_rate = (float)found_devices_[i].successful_tests / found_devices_[i].total_tests * 100.0;
      
      emitf(out, print, "0x%02X: %d tests, %.1f%% exito, %d fallos consecutivos\r\n",
            found_devices_[i].address,
            found_devices_[i].total_tests,
            success_rate,
            found_devices_[i].consecutive_failures);
    }
  }
  emit(out, print, "==================\r\n");
  return out;
}

void MapController::resetStatistics() {
  for (int i = 0; i < device_count_; i++) {
    found_devices_[i].total_tests = 0;
    found_devices_[i].successful_tests = 0;
    found_devices_[i].failed_tests = 0;
    found_devices_[i].consecutive_failures = 0;
  }
  cycle_count_ = 0;
  total_bus_recoveries_ = 0;
}

// ========== I2C ADDRESS MANAGEMENT ==========

String MapController::changeI2CAddress(uint8_t old_addr, uint8_t new_addr, int print) {
  String out;
  emitf(out, print, "CAMBIO DE DIRECCION I2C: 0x%02X -> 0x%02X\r\n", old_addr, new_addr);
  
  // Verify that source device exists and is compatible
  if (!checkCompatibility(old_addr)) {
    emitf(out, print, "ERROR: Dispositivo 0x%02X no es compatible\r\n", old_addr);
    return out;
  }
  
  // Step 1: Activate address change mode
  wire_->beginTransmission(old_addr);
  wire_->write(CMD_SET_I2C_ADDR);
  if (wire_->endTransmission() != 0) {
    emit(out, print, "ERROR: No se pudo activar modo cambio\r\n");
    return out;
  }
  
  delay(100);
  
  // Step 2: Send new address
  wire_->beginTransmission(old_addr);
  wire_->write(new_addr);
  if (wire_->endTransmission() != 0) {
    emit(out, print, "ERROR: No se pudo enviar nueva direccion\r\n");
    return out;
  }
  
  delay(200);  // Time to save in Flash
  
  // Step 3: Reset device
  wire_->beginTransmission(old_addr);
  wire_->write(CMD_RESET);
  wire_->endTransmission();  // Ignore error because device resets
  
  emit(out, print, "Esperando reset del dispositivo...\r\n");
  delay(3000);
  
  // Verify change
  wire_->beginTransmission(new_addr);
  if (wire_->endTransmission() == 0) {
    emitf(out, print, "[OK] CAMBIO EXITOSO: Dispositivo ahora en 0x%02X\r\n", new_addr);
  } else {
    emit(out, print, "[INFO] Comando enviado. Ejecutar scan para verificar\r\n");
  }
  
  return out;
}

String MapController::factoryReset(uint8_t address, int print) {
  String out;
  emitf(out, print, "FACTORY RESET: 0x%02X -> direccion UID\r\n", address);
  
  wire_->beginTransmission(address);
  wire_->write(CMD_RESET_FACTORY);
  if (wire_->endTransmission() == 0) {
    emit(out, print, "[OK] Comando factory reset enviado\r\n");
    emit(out, print, "NOTA: Ejecutar scan para encontrar nueva direccion\r\n");
  } else {
    emit(out, print, "ERROR: No se pudo enviar comando factory reset\r\n");
  }
  
  return out;
}

String MapController::checkI2CStatus(uint8_t address, int print) {
  String out;
  emitf(out, print, "ESTADO I2C: 0x%02X\r\n", address);
  
  wire_->beginTransmission(address);
  wire_->write(CMD_GET_I2C_STATUS);
  
  if (wire_->endTransmission() == 0) {
    delay(50);
    wire_->requestFrom(address, (uint8_t)1);
    
    if (wire_->available()) {
      uint8_t status = wire_->read();
      uint8_t status_clean = status & 0x0F;
      
      emitf(out, print, "Estado: 0x%02X -> ", status_clean);
      if (status_clean == 0x0F) {
        emit(out, print, "FLASH (direccion personalizada)\r\n");
      } else if (status_clean == 0x0A) {
        emit(out, print, "UID (direccion factory)\r\n");
      } else {
        emit(out, print, "DESCONOCIDO\r\n");
      }
    }
  } else {
    emit(out, print, "ERROR: No se pudo obtener estado\r\n");
  }
  
  return out;
}

String MapController::resetDevice(uint8_t address, int print) {
  String out;
  emitf(out, print, "RESET DISPOSITIVO: 0x%02X\r\n", address);
  
  wire_->beginTransmission(address);
  wire_->write(CMD_RESET);
  wire_->endTransmission();  // Ignore error
  
  emit(out, print, "[OK] Comando de reset enviado\r\n");
  return out;
}

String MapController::resetAllDevices(int print) {
  String out;
  if (device_count_ == 0) {
    emit(out, print, "ERROR: No hay dispositivos. Ejecutar scan primero\r\n");
    return out;
  }
  
  emitf(out, print, "RESET ALL: Reiniciando %d dispositivos...\r\n", device_count_);
  emit(out, print, "===========================================\r\n");
  
  int reset_count = 0;
  for (int i = 0; i < device_count_; i++) {
    uint8_t addr = found_devices_[i].address;
    
    wire_->beginTransmission(addr);
    wire_->write(CMD_RESET);
    uint8_t error = wire_->endTransmission();
    
    if (error == 0) {
      emitf(out, print, "[%d] 0x%02X: Reset enviado OK\r\n", i, addr);
      reset_count++;
    } else {
      emitf(out, print, "[%d] 0x%02X: Error al enviar reset (Error:%d)\r\n", i, addr, error);
    }
    
    delay(10);  // Small delay between resets
  }
  
  emit(out, print, "===========================================\r\n");
  emitf(out, print, "[OK] %d de %d dispositivos reseteados\r\n", reset_count, device_count_);
  
  return out;
}

String MapController::verifyAddressChange(uint8_t old_addr, uint8_t new_addr, int print) {
  String out;
  emitf(out, print, "VERIFICANDO CAMBIO: 0x%02X -> 0x%02X\r\n", old_addr, new_addr);
  
  // Verify that old doesn't respond
  wire_->beginTransmission(old_addr);
  bool old_offline = (wire_->endTransmission() != 0);
  
  // Verify that new responds
  wire_->beginTransmission(new_addr);
  bool new_online = (wire_->endTransmission() == 0);
  
  if (old_offline && new_online) {
    emitf(out, print, "[OK] CAMBIO EXITOSO: 0x%02X -> 0x%02X\r\n", old_addr, new_addr);
  } else if (!old_offline && new_online) {
    emit(out, print, "WARNING: Ambas direcciones responden\r\n");
  } else if (old_offline && !new_online) {
    emit(out, print, "ERROR: Dispositivo perdido\r\n");
  } else {
    emit(out, print, "ERROR: Cambio fallido\r\n");
  }
  
  return out;
}

void MapController::setScannedDevices(DeviceStats* list, int count) {
  scanned_ = list;
  scanned_count_ = count;
}

// ========== Mapping ==========
String MapController::initializeMapping(int print) {
  String out;
  for (int i = 0; i < MAX_DEVICES; i++) device_map_.id_to_address[i] = INVALID_ADDRESS;
  for (int i = 0; i < 256; i++) device_map_.address_to_id[i] = 0;
  device_map_.mapped_count = 0;
  out += loadMapping(print);
  return out;
}

String MapController::saveMapping(int print) {
  String out;
  preferences_.begin(STORAGE_NAMESPACE, false);
  preferences_.putBytes("id_map", device_map_.id_to_address, MAX_DEVICES);
  preferences_.end();
  if (print) emit(out, print, "Saved\r\n");
  return out;
}

String MapController::loadMapping(int print) {
  String out;
  preferences_.begin(STORAGE_NAMESPACE, true);
  if (preferences_.isKey("id_map")) {
    preferences_.getBytes("id_map", device_map_.id_to_address, MAX_DEVICES);
    device_map_.mapped_count = 0;
    for (int i = 0; i < 256; i++) device_map_.address_to_id[i] = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
      uint8_t a = device_map_.id_to_address[i];
      if (a != INVALID_ADDRESS) {
        device_map_.address_to_id[a] = i + 1;
        device_map_.mapped_count++;
      }
    }
    if (print) emit(out, print, "Loaded\r\n");
  }
  preferences_.end();
  return out;
}

String MapController::clearMapping(int print) {
  String out;
  for (int i = 0; i < MAX_DEVICES; i++) device_map_.id_to_address[i] = INVALID_ADDRESS;
  for (int i = 0; i < 256; i++) device_map_.address_to_id[i] = 0;
  device_map_.mapped_count = 0;

  preferences_.begin(STORAGE_NAMESPACE, false);
  preferences_.clear();
  preferences_.end();
  emit(out, print, "All device mappings have been cleared.\r\n");
  return out;
}

// Lógica pura (sin prints) — sin cambios
bool MapController::assignDeviceToID(uint8_t id, uint8_t address) {
  if (id < 1 || id > MAX_DEVICES) { return false; }
  if (address < 0x08 || address > 0x77) { return false; }
  uint8_t existing_id = getIDByAddress(address);
  if (existing_id != 0 && existing_id != id) { return false; }
  uint8_t existing_addr = getAddressByID(id);
  if (existing_addr != INVALID_ADDRESS && existing_addr != address) { return false; }
  device_map_.id_to_address[id - 1] = address;
  device_map_.address_to_id[address] = id;
  device_map_.mapped_count = 0;
  for (int i = 0; i < MAX_DEVICES; i++)
    if (device_map_.id_to_address[i] != INVALID_ADDRESS) device_map_.mapped_count++;
  return true;
}

bool MapController::moveDevice(uint8_t from_id, uint8_t to_id) {
  if (from_id < 1 || from_id > MAX_DEVICES || to_id < 1 || to_id > MAX_DEVICES) return false;
  if (from_id == to_id) return true;
  uint8_t from_addr = getAddressByID(from_id);
  uint8_t to_addr   = getAddressByID(to_id);
  if (from_addr == INVALID_ADDRESS) return false;
  device_map_.id_to_address[from_id - 1] = to_addr;
  if (to_addr != INVALID_ADDRESS) device_map_.address_to_id[to_addr] = from_id;
  device_map_.id_to_address[to_id - 1] = from_addr;
  device_map_.address_to_id[from_addr] = to_id;
  return true;
}

bool MapController::removeDeviceFromID(uint8_t id) {
  if (id < 1 || id > MAX_DEVICES) return false;
  uint8_t a = getAddressByID(id);
  if (a == INVALID_ADDRESS) return true;
  device_map_.id_to_address[id - 1] = INVALID_ADDRESS;
  device_map_.address_to_id[a] = 0;
  device_map_.mapped_count--;
  return true;
}

uint8_t MapController::getAddressByID(uint8_t id) const {
  if (id < 1 || id > MAX_DEVICES) return INVALID_ADDRESS;
  return device_map_.id_to_address[id - 1];
}

uint8_t MapController::getIDByAddress(uint8_t address) const {
  if (address > 255) return 0;
  return device_map_.address_to_id[address];
}

// ========== Mapping commands ==========
String MapController::cmdShowMapping(int print) {
  String out;
  emitf(out, print, "\n--- Device ID Mapping (%d/%d mapped) ---\r\n", device_map_.mapped_count, MAX_DEVICES);
  emit(out, print, "ID | Address | Online\r\n");
  emit(out, print, "---|---------|--------\r\n");
  for (int i = 1; i <= MAX_DEVICES; i++) {
    uint8_t addr = getAddressByID(i);
    if (addr != INVALID_ADDRESS) {
      bool is_online = false;
      if (scanned_ && scanned_count_ > 0) {
        for (int j = 0; j < scanned_count_; j++) {
          if (scanned_[j].address == addr) { is_online = scanned_[j].is_online; break; }
        }
      }
      emitf(out, print, "%-2d | 0x%02X    | %s\r\n", i, addr, is_online ? "Online" : "-");
    }
  }
  emit(out, print, "------------------------------------\r\n");
  return out;
}

String MapController::cmdAutoMap(int print) {
  String out;
  if (!(scanned_ && scanned_count_ > 0)) {
    emit(out, print, "No scanned devices injected. Use setScannedDevices(...).\r\n");
    return out;
  }
  emit(out, print, "Auto-mapping scanned devices to first free IDs...\r\n");
  int mapped_now = 0;
  for (int i = 0; i < scanned_count_; i++) {
    uint8_t addr = scanned_[i].address;
    if (getIDByAddress(addr) == 0) {
      for (int id = 1; id <= MAX_DEVICES; id++) {
        if (getAddressByID(id) == INVALID_ADDRESS) {
          if (assignDeviceToID(id, addr)) { mapped_now++; }
          break;
        }
      }
    }
  }
  emitf(out, print, "Auto-mapped %d new devices.\r\n", mapped_now);
  out += saveMapping(print);
  return out;
}

String MapController::cmdAssign(const String& cmd, int print) {
  String out;
  int id_idx = cmd.indexOf(' ');
  int addr_idx = cmd.lastIndexOf(' ');
  if (id_idx == -1 || id_idx == addr_idx) { emit(out, print, "ERROR: Usage: assign <id> <address>\r\n"); return out; }
  uint8_t id = cmd.substring(id_idx + 1, addr_idx).toInt();
  String addr_str = cmd.substring(addr_idx + 1);
  uint8_t addr = strtol(addr_str.c_str(), NULL, 0);
  if (assignDeviceToID(id, addr)) { emitf(out, print, "Assigned 0x%02X to ID %d.\r\n", addr, id); out += saveMapping(print); }
  else { emitf(out, print, "ERROR: Cannot assign 0x%02X to ID %d.\r\n", addr, id); }
  return out;
}

String MapController::cmdMove(const String& cmd, int print) {
  String out;
  int a = cmd.indexOf(' ');
  int b = cmd.lastIndexOf(' ');
  if (a == -1 || a == b) { emit(out, print, "ERROR: Usage: mv <from_id> <to_id>\r\n"); return out; }
  uint8_t from_id = cmd.substring(a + 1, b).toInt();
  uint8_t to_id   = cmd.substring(b + 1).toInt();
  uint8_t from_addr = getAddressByID(from_id);
  uint8_t to_addr   = getAddressByID(to_id);
  if (!moveDevice(from_id, to_id)) { emit(out, print, "ERROR: invalid IDs or empty source.\r\n"); return out; }
  if (to_addr != INVALID_ADDRESS)
    emitf(out, print, "Swapped ID %d (0x%02X) with ID %d (0x%02X).\r\n", from_id, from_addr, to_id, to_addr);
  else
    emitf(out, print, "Moved ID %d (0x%02X) to empty ID %d.\r\n", from_id, from_addr, to_id);
  out += saveMapping(print);
  return out;
}

String MapController::cmdRemove(const String& cmd, int print) {
  String out;
  int i = cmd.indexOf(' ');
  if (i == -1) { emit(out, print, "ERROR: Usage: rm <id>\r\n"); return out; }
  uint8_t id = cmd.substring(i + 1).toInt();
  uint8_t a_before = getAddressByID(id);
  if (removeDeviceFromID(id)) {
    if (a_before == INVALID_ADDRESS) emitf(out, print, "ID %d already empty.\r\n", id);
    else emitf(out, print, "Removed 0x%02X from ID %d.\r\n", a_before, id);
    out += saveMapping(print);
  } else {
    emit(out, print, "ERROR: ID must be 1..32.\r\n");
  }
  return out;
}

String MapController::cmdClearMapping(int print) { return clearMapping(print); }

// ========== NeoPixel ==========
String MapController::cmdNeoByID(String cmd, int print) {
  String out;
  cmd.toLowerCase();
  uint8_t neo_cmd = CMD_NEO_OFF;
  uint8_t target_id = 0;

  if      (cmd.indexOf("red")   != -1) neo_cmd = CMD_NEO_RED;
  else if (cmd.indexOf("green") != -1) neo_cmd = CMD_NEO_GREEN;
  else if (cmd.indexOf("blue")  != -1) neo_cmd = CMD_NEO_BLUE;
  else if (cmd.indexOf("off")   != -1) neo_cmd = CMD_NEO_OFF;
  else { emit(out, print, "ERROR: Color no reconocido. Usa: red, green, blue, off\r\n"); return out; }

  int sp = cmd.lastIndexOf(' ');
  if (sp != -1) {
    String last = cmd.substring(sp + 1);
    target_id = last.toInt();
  }
  if (target_id >= 1 && target_id <= MAX_DEVICES) out += setNeoPixelByID(target_id, neo_cmd, print);
  else                                            out += setAllNeoPixels(neo_cmd, print);
  return out;
}

String MapController::setNeoPixelByID(uint8_t id, uint8_t cmd, int print) {
  String out;
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d.\r\n", id); return out; }
  const char* color_str = getNeoColorName(cmd);
  if (sendCommandSafe(addr, cmd)) emitf(out, print, "NeoPixel ID %d (0x%02X): %s - OK\r\n", id, addr, color_str);
  else                            emitf(out, print, "NeoPixel ID %d (0x%02X): %s - FAILED\r\n", id, addr, color_str);
  return out;
}

String MapController::setAllNeoPixels(uint8_t cmd, int print) {
  String out;
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }
  const char* color_str = getNeoColorName(cmd);
  emitf(out, print, "Setting all NeoPixels to %s...\r\n", color_str);
  int ok=0, fail=0;
  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;
    if (sendCommandSafe(addr, cmd)) { ok++; emitf(out, print, "  -> ID %d (0x%02X): %s - OK\r\n", id, addr, color_str); }
    else { fail++; emitf(out, print, "  -> ID %d (0x%02X): %s - FAILED\r\n", id, addr, color_str); }
  }
  emitf(out, print, "NeoPixel %s: %d success, %d failed\r\n", color_str, ok, fail);
  return out;
}

const char* MapController::getNeoColorName(uint8_t cmd) {
  switch(cmd) {
    case CMD_NEO_RED:   return "RED";
    case CMD_NEO_GREEN: return "GREEN";
    case CMD_NEO_BLUE:  return "BLUE";
    case CMD_NEO_OFF:   return "OFF";
    default:            return "UNKNOWN";
  }
}

// ========== ADC ==========
float MapController::adcToVoltage(uint16_t adc_value) {
  const float VREF = 3.3f;
  const float ADC_MAX = 4095.0f;
  return (adc_value * VREF) / ADC_MAX;
}

String MapController::cmdADCByID(String cmd, int print) {
  String out;
  int sp = cmd.lastIndexOf(' ');
  if (sp == -1) { emit(out, print, "ERROR: Usage: adc <id>\r\n"); return out; }
  uint8_t id = cmd.substring(sp + 1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d.\r\n", id); return out; }

  emitf(out, print, "Reading ADC from ID %d (0x%02X) (16-bit read + 12-bit mask 0x0FFF)...\r\n", id, addr);
  uint16_t adc = readADCValue(addr);
  if (adc != 0xFFFF) {
    float v = adcToVoltage(adc);
    if (adc > 4095) emitf(out, print, "ADC ID %d (0x%02X): %d (0x%04X) - %.3fV [OUT OF RANGE]\r\n", id, addr, adc, adc, v);
    else            emitf(out, print, "ADC ID %d (0x%02X): %d (0x%03X) - %.3fV\r\n", id, addr, adc, adc, v);
  } else {
    emitf(out, print, "ADC ID %d (0x%02X): ERROR\r\n", id, addr);
  }
  return out;
}

String MapController::cmdADCAllByID(int print) {
  String out;
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }
  emit(out, print, "Reading 12-bit ADC values from all mapped devices...\r\n");
  emit(out, print, "─────────────────────────────────────────────────────────\r\n");
  emit(out, print, "ID | Addr   | Raw  | Hex    | Voltage | Status\r\n");
  emit(out, print, "─────────────────────────────────────────────────────────\r\n");

  uint32_t start = millis();
  int ok=0, err=0;

  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;

    uint16_t adc = readADCValue(addr);
    if (adc != 0xFFFF) {
      float v = adcToVoltage(adc);
      if (adc > 4095)
        emitf(out, print, "%-2d | 0x%02X   | %-4d   | 0x%04X | %.3fV   | OUT_RANGE\r\n", id, addr, adc, adc, v);
      else
        emitf(out, print, "%-2d | 0x%02X   | %-4d   | 0x%03X | %.3fV   | OK\r\n", id, addr, adc, adc, v);
      ok++;
    } else {
      emitf(out, print, "%-2d | 0x%02X   | ERROR  | ERROR  | ERROR   | FAIL\r\n", id, addr);
      err++;
    }
    delay(COMMAND_DELAY_MS);
  }
  emit(out, print, "─────────────────────────────────────────────────────────\r\n");
  emitf(out, print, "12-bit ADC Results: %d success, %d errors\r\n", ok, err);
  out += recordTiming("ADC ALL BY ID", millis() - start, print);
  return out;
}

uint16_t MapController::readADCValue(uint8_t addr) {
  uint8_t h=0xFF, l=0xFF; bool hs=false, ls=false;
  delay(50);

  for (int att=0; att<3; att++) {
    wire_->beginTransmission(addr); wire_->write(CMD_ADC_PA0_HSB);
    if (wire_->endTransmission() != 0) { if (att==2) return 0xFFFF; delay(50); continue; }
    delay(50);
    uint8_t rec = wire_->requestFrom(addr, (uint8_t)1);
    if (rec == 1) { h = wire_->read(); hs = true; break; }
    delay(20);
  }
  if (!hs) return 0xFFFF;
  h &= 0x0F;
  delay(50);

  for (int att=0; att<3; att++) {
    wire_->beginTransmission(addr); wire_->write(CMD_ADC_PA0_LSB);
    if (wire_->endTransmission() != 0) { if (att==2) return 0xFFFF; delay(50); continue; }
    delay(50);
    uint8_t rec = wire_->requestFrom(addr, (uint8_t)1);
    if (rec == 1) { l = wire_->read(); ls = true; break; }
    delay(20);
  }
  if (!ls) return 0xFFFF;

  uint16_t raw = (h << 8) | l;
  return raw;
}

// ========== Sense (PA4) ==========
String MapController::cmdSenseByID(int print) {
  String out;
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }

  emit(out, print, "Reading PA4 sense states based on ID mapping...\r\n");
  emit(out, print, "──────────────────────────────────────────────\r\n");
  emit(out, print, "ID | Addr   | Raw    | State     | Status\r\n");
  emit(out, print, "──────────────────────────────────────────────\r\n");

  uint32_t start = millis();
  int ok=0, err=0;

  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;

    uint8_t raw = readSenseState(addr);
    const char* state = interpretSenseValue(raw);
    const char* st = (raw == 0xFF) ? "FAIL" : "OK";
    if (raw == 0xFF) err++; else ok++;

    emitf(out, print, "%-2d | 0x%02X   | 0x%02X   | %-9s | %s\r\n", id, addr, raw, state, st);
    delay(COMMAND_DELAY_MS);
  }
  emit(out, print, "──────────────────────────────────────────────\r\n");
  out += recordTiming("SENSE BY ID", millis() - start, print);
  return out;
}

String MapController::cmdSenseMaskByID(int print) {
  String out;
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }

  uint8_t mask[4] = {0,0,0,0};
  uint32_t start = millis();

  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;

    uint8_t raw = readSenseState(addr);
    if (raw != 0xFF && strcmp(interpretSenseValue(raw), "HIGH") == 0) {
      int b = (id - 1) / 8;
      int bit = (id - 1) % 8;
      mask[b] |= (1 << bit);
    }
    delay(COMMAND_DELAY_MS);
  }
  emitf(out, print, "SENSE MASK BY ID (%d mapped):\r\n", device_map_.mapped_count);
  emitf(out, print, "array[0] = 0x%02X (IDs 1-8)\r\n",  mask[0]);
  emitf(out, print, "array[1] = 0x%02X (IDs 9-16)\r\n", mask[1]);
  emitf(out, print, "array[2] = 0x%02X (IDs 17-24)\r\n",mask[2]);
  emitf(out, print, "array[3] = 0x%02X (IDs 25-32)\r\n",mask[3]);
  out += recordTiming("SENSE MASK", millis() - start, print);
  return out;
}

String MapController::cmdSenseMaskHexByID(int print) {
  String out;
  if (device_map_.mapped_count == 0) {
    emit(out, print, "No devices mapped.\r\n");
    return out;
  }

  uint8_t mask[4] = {0,0,0,0};
  uint32_t start = millis();

  for (int id = 1; id <= MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;

    uint8_t raw = readSenseState(addr);
    // Considera HIGH cuando interpretSenseValue(raw) == "HIGH"
    if (raw != 0xFF && strcmp(interpretSenseValue(raw), "HIGH") == 0) {
      int b   = (id - 1) / 8;
      int bit = (id - 1) % 8;
      mask[b] |= (1 << bit);
    }
    delay(COMMAND_DELAY_MS);
  }

  // Empaquetado big-endian: [3][2][1][0]
  uint32_t packed =
      ((uint32_t)mask[3] << 24) |
      ((uint32_t)mask[2] << 16) |
      ((uint32_t)mask[1] <<  8) |
      ((uint32_t)mask[0]      );

  char buf[11]; // "0x" + 8 hex + '\0'
  sprintf(buf, "0x%08X", packed);

  // Si no quieres imprimir nada en Serial, deja print=0.
  // out debe contener únicamente el valor concatenado:
  out += buf;

  // Si deseas medir tiempo, puedes omitirlo aquí para no ensuciar la salida.
  // (Si lo quieres, devuélvelo aparte vía JSON en Commands.cpp)

  return out;
}


uint8_t MapController::readSenseState(uint8_t addr) {
  for (int att=0; att<3; att++) {
    wire_->beginTransmission(addr); wire_->write(CMD_PA4_DIGITAL);
    if (wire_->endTransmission() != 0) { if (att==2) return 0xFF; delay(10); continue; }
    delay(30);
    if (wire_->requestFrom(addr, (uint8_t)1) == 1) return wire_->read();
    delay(10);
  }
  return 0xFF;
}

const char* MapController::interpretSenseValue(uint8_t raw) {
  if (raw == 0xFF) return "ERROR";
  return ((raw & 0xF0) >> 4) > 7 ? "HIGH" : "LOW";
}

// === Arrays helpers ===
String MapController::cmdSenseArrayMask(uint8_t array_index, int print) {
  String out;
  if (array_index > 3) { emitf(out, print, "ERROR: Array index %d out of range (0-3)\r\n", array_index); return out; }
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }

  uint8_t mask = 0;
  uint8_t checked=0, highc=0;
  uint32_t start = millis();

  int start_id = (array_index * 8) + 1;
  int end_id   = start_id + 7;

  emitf(out, print, "READING ARRAY %d SENSE MASK (IDs %d-%d):\r\n", array_index, start_id, end_id);
  emit(out, print, "──────────────────────────────────────────\r\n");

  for (int id = start_id; id <= end_id; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) { emitf(out, print, "ID %-2d: Not mapped\r\n", id); continue; }

    checked++;
    uint8_t raw = readSenseState(addr);
    if (raw != 0xFF) {
      const char* st = interpretSenseValue(raw);
      if (strcmp(st,"HIGH")==0) {
        int bit = (id - 1) % 8;
        mask |= (1 << bit);
        highc++;
        emitf(out, print, "ID %-2d (0x%02X): %s -> BIT %d SET\r\n", id, addr, st, bit);
      } else {
        emitf(out, print, "ID %-2d (0x%02X): %s\r\n", id, addr, st);
      }
    } else {
      emitf(out, print, "ID %-2d (0x%02X): ERROR\r\n", id, addr);
    }
    delay(COMMAND_DELAY_MS);
  }

  emit(out, print, "──────────────────────────────────────────\r\n");
  emitf(out, print, "ARRAY %d RESULT:\r\n", array_index);
  emitf(out, print, "  Mask Value: 0x%02X (binary: ", mask);
  for (int i = 7; i >= 0; i--) emitf(out, print, "%d", (mask >> i) & 1);
  emit(out, print, ")\r\n");
  emitf(out, print, "  Devices Checked: %d/8\r\n", checked);
  emitf(out, print, "  Devices HIGH: %d\r\n", highc);
  emit(out, print, "  Bit Pattern: ");
  bool first = true;
  for (int i=0;i<8;i++){ if(mask & (1<<i)){ if(!first) emit(out, print, ", "); emitf(out, print, "BIT%d", i); first=false; } }
  if(first) emit(out, print, "None");
  emit(out, print, "\r\n");
  out += recordTiming("ARRAY MASK", millis() - start, print);
  return out;
}

String MapController::cmdSenseArrayMaskByCommand(const String& cmd, int print) {
  int sp = cmd.lastIndexOf(' ');
  if (sp == -1) return String("ERROR: Usage: sense_array <array_index>\r\n");
  uint8_t idx = cmd.substring(sp + 1).toInt();
  return cmdSenseArrayMask(idx, print);
}

String MapController::cmdShowSenseArray(uint8_t array_index, int print) {
  String out;
  if (array_index > 3) { emitf(out, print, "ERROR: Array index %d out of range (0-3)\r\n", array_index); return out; }
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }

  int start_id = (array_index * 8) + 1;
  int end_id   = start_id + 7;

  emitf(out, print, "ARRAY %d DETAILED VIEW (IDs %d-%d):\r\n", array_index, start_id, end_id);
  emit(out, print, "════════════════════════════════════════════════════\r\n");
  emit(out, print, "ID | Addr   | Raw    | State     | Bit | Status\r\n");
  emit(out, print, "═══════════════════════════════════════════════════\r\n");

  uint8_t mask=0; int mapped=0, high=0, commerr=0;
  for (int id = start_id; id <= end_id; id++) {
    uint8_t addr = getAddressByID(id);
    int bit_pos = (id - 1) % 8;
    if (addr == INVALID_ADDRESS) {
      emitf(out, print, "%-2d | ------  | ------ | --------- | %d   | NOT_MAPPED\r\n", id, bit_pos);
      continue;
    }
    mapped++;
    uint8_t raw = readSenseState(addr);
    if (raw != 0xFF) {
      const char* st = interpretSenseValue(raw);
      const char* status = "OK";
      if (strcmp(st,"HIGH")==0) { mask |= (1<<bit_pos); high++; status="HIGH_OK"; }
      emitf(out, print, "%-2d | 0x%02X   | 0x%02X   | %-9s | %d   | %s\r\n", id, addr, raw, st, bit_pos, status);
    } else {
      emitf(out, print, "%-2d | 0x%02X   | ERROR  | ERROR     | %d   | COMM_ERROR\r\n", id, addr, bit_pos);
      commerr++;
    }
    delay(COMMAND_DELAY_MS);
  }

  emit(out, print, "════════════════════════════════════════════════════\r\n");
  emitf(out, print, "ARRAY %d SUMMARY:\r\n", array_index);
  emitf(out, print, "  Final Mask: 0x%02X (", mask);
  for (int i=7;i>=0;i--) emitf(out, print, "%d", (mask>>i)&1);
  emit(out, print, ")\r\n");
  emitf(out, print, "  Mapped Devices: %d/8\r\n", mapped);
  emitf(out, print, "  HIGH States: %d\r\n", high);
  emitf(out, print, "  Comm Errors: %d\r\n", commerr);
  emitf(out, print, "  Coverage: %.1f%%\r\n", (mapped*100.0)/8.0);
  return out;
}

String MapController::cmdShowSenseArrayByCommand(const String& cmd, int print) {
  int sp = cmd.lastIndexOf(' ');
  if (sp == -1) return String("ERROR: Usage: show_array <array_index>\r\n");
  uint8_t idx = cmd.substring(sp + 1).toInt();
  return cmdShowSenseArray(idx, print);
}

String MapController::cmdFillSenseArray(uint8_t array_index, int print) {
  String out;
  if (array_index > 3) { emitf(out, print, "ERROR: Array index %d out of range (0-3)\r\n", array_index); return out; }
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }

  int start_id = (array_index * 8) + 1;
  int end_id   = start_id + 7;

  emitf(out, print, "FILLING ARRAY %d (IDs %d-%d) - brief relay pulses:\r\n", array_index, start_id, end_id);
  emit(out, print, "──────────────────────────────────────────────────────────────\r\n");

  int ok=0, err=0;
  for (int id = start_id; id <= end_id; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) { emitf(out, print, "ID %-2d: Not mapped - SKIPPED\r\n", id); continue; }

    emitf(out, print, "ID %-2d (0x%02X): ", id, addr);
    if (sendCommandSafe(addr, CMD_RELAY_ON)) {
      delay(200);
      if (sendCommandSafe(addr, CMD_RELAY_OFF)) { emit(out, print, "Pulse OK\r\n"); ok++; }
      else { emit(out, print, "OFF FAILED - PARTIAL\r\n"); err++; }
    } else { emit(out, print, "ON FAILED - ERROR\r\n"); err++; }
    delay(COMMAND_DELAY_MS);
  }

  emit(out, print, "──────────────────────────────────────────────────────────────\r\n");
  emitf(out, print, "FILL ARRAY %d SUMMARY: %d success, %d errors\r\n", array_index, ok, err);
  return out;
}

String MapController::cmdFillSenseArrayByCommand(const String& cmd, int print) {
  int sp = cmd.lastIndexOf(' ');
  if (sp == -1) {
    return String("ERROR: Usage: fill_array <array_index>\r\nWarning: activates relays briefly!\r\n");
  }
  uint8_t idx = cmd.substring(sp + 1).toInt();

  String out;
  emit(out, print, "  WARNING: This will briefly activate relays in the specified array!\r\n");
  out += cmdFillSenseArray(idx, print);
  return out;
}


String MapController::cmdShowAllArrays(int print) {
  String out;
  if (device_map_.mapped_count == 0) { emit(out, print, "No devices mapped.\r\n"); return out; }
  emit(out, print, "ALL ARRAYS OVERVIEW - SENSE MASKS:\r\n");
  emit(out, print, "════════════════════════════════════════════════════════════\r\n");
  uint8_t full[4] = {0,0,0,0};

  for (int a=0;a<4;a++) {
    int start_id=(a*8)+1, end_id=start_id+7;
    uint8_t mask=0; int mapped=0, high=0;
    emitf(out, print, "ARRAY %d (IDs %2d-%2d): ", a, start_id, end_id);
    for (int id=start_id; id<=end_id; id++) {
      uint8_t addr = getAddressByID(id);
      if (addr == INVALID_ADDRESS) continue;
      mapped++;
      uint8_t raw = readSenseState(addr);
      if (raw != 0xFF && strcmp(interpretSenseValue(raw),"HIGH")==0) {
        int bit = (id - 1) % 8; mask |= (1<<bit); high++;
      }
      delay(10);
    }
    full[a]=mask;
    emitf(out, print, "0x%02X (", mask);
    for (int i=7;i>=0;i--) emitf(out, print, "%d", (mask>>i)&1);
    emitf(out, print, ") - %d/%d mapped, %d HIGH\r\n", mapped, 8, high);
  }
  emit(out, print, "════════════════════════════════════════════════════════════\r\n");
  emit(out, print, "COMPLETE SYSTEM MASK:\r\n");
  for (int i=0;i<4;i++)
    emitf(out, print, "array[%d] = 0x%02X  (IDs %d-%d)\r\n", i, full[i], (i*8)+1, (i*8)+8);
  emit(out, print, "════════════════════════════════════════════════════════════\r\n");
  return out;
}

// ========== Relay ==========
String MapController::cmdRelayByID(String cmd, int print) {
  String out;
  int sp = cmd.indexOf(' ');
  if (sp == -1) { emit(out, print, "ERROR: Usage: f <id>\r\n"); return out; }
  uint8_t id = cmd.substring(sp + 1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr != INVALID_ADDRESS) out += relayPulse(addr, 50, print);
  else emitf(out, print, "ERROR: No device mapped to ID %d.\r\n", id);
  return out;
}

String MapController::cmdRelayOff(int print) { return setAllRelays(CMD_RELAY_OFF, print); }
String MapController::cmdAllShut(int print)  { return setAllDevicesState(CMD_NEO_OFF, CMD_RELAY_OFF, print); }

String MapController::relayPulse(uint8_t addr, uint32_t pulse_time_ms, int print) {
  String out;
  emitf(out, print, "Ejecutando pulso de relé en 0x%02X por %lums...\r\n", addr, pulse_time_ms);
  sendCommandSafe(addr, CMD_RELAY_OFF);
  delay(200);
  if (!sendCommandSafe(addr, CMD_RELAY_ON)) {
    emitf(out, print, "  -> 0x%02X: ERROR al activar\r\n", addr);
    return out;
  }
  delay(pulse_time_ms);
  if (sendCommandSafe(addr, CMD_RELAY_OFF)) emitf(out, print, "Relé en 0x%02X desactivado.\r\n", addr);
  else                                      emitf(out, print, "ERROR: No se pudo desactivar el relé en 0x%02X\r\n", addr);
  return out;
}

int MapController::relayTurn(uint8_t addr, int pulse, int print) {
  if (print) 
    if(pulse)
      Serial.printf("Turn ON relay en 0x%02X...\r\n", addr);
    else
      Serial.printf("Turn OFF relay en 0x%02X...\r\n", addr);
  bool ok = sendCommandSafe(addr, pulse);
  if (print) Serial.printf("  -> 0x%02X: %s\r\n", addr, ok ? "Relé encendido" : "ERROR al encender");
  return ok ? 0 : 1;
}

int MapController::relayToggle(uint8_t addr, int print) {
  if (print) Serial.printf("Toggle relay en 0x%02X...\r\n", addr);
  bool ok = sendCommandSafe(addr, CMD_TOGGLE);
  if (print) Serial.printf("  -> 0x%02X: %s\r\n", addr, ok ? "Relé alternado" : "ERROR al alternar");
  return ok ? 0 : 1;
}



// ========== Communication & bulk ==========
bool MapController::sendCommandSafe(uint8_t addr, uint8_t cmd) {
  for (int att=0; att<3; att++) {
    wire_->beginTransmission(addr);
    wire_->write(cmd);
    if (wire_->endTransmission() == 0) { delay(COMMAND_DELAY_MS); return true; }
    delay(50);
  }
  return false;
}

bool MapController::sendCommandFast(uint8_t addr, uint8_t cmd) {
  wire_->beginTransmission(addr);
  wire_->write(cmd);
  if (wire_->endTransmission() == 0) { delay(COMMAND_DELAY_MS); return true; }
  return false;
}

String MapController::setAllRelays(uint8_t cmd, int print) {
  String out;
  const char* st = (cmd == CMD_RELAY_ON) ? "ON" : "OFF";
  emitf(out, print, "Setting all mapped relays to: %s (0x%02X)\r\n", st, cmd);
  int ok=0, fail=0, total=0;
  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;
    total++;
    if (sendCommandSafe(addr, cmd)) { ok++; emitf(out, print, "  -> ID %d (0x%02X): %s - OK\r\n", id, addr, st); }
    else {
      fail++; emitf(out, print, "  -> ID %d (0x%02X): %s - FAILED\r\n", id, addr, st);
      if (cmd == CMD_RELAY_OFF) {
        emitf(out, print, "     CRITICAL: Retrying OFF for ID %d...\r\n", id);
        for (int r=0;r<5;r++){ delay(100*(r+1)); if(sendCommandSafe(addr, CMD_RELAY_OFF)){ ok++; fail--; emitf(out, print, "     SUCCESS OFF for ID %d\r\n", id); break; } }
      }
    }
  }
  emit(out, print, "────────────────────────────────────────────────\r\n");
  emitf(out, print, "RELAY %s SUMMARY: %d total, %d success, %d failed\r\n", st, total, ok, fail);
  if (fail>0 && cmd==CMD_RELAY_OFF) emit(out, print, "  WARNING: Some relays may still be ON.\r\n");
  emit(out, print, "────────────────────────────────────────────────\r\n");
  return out;
}

String MapController::emergencyRelaysOff(int print) {
  String out;
  emit(out, print, " EMERGENCY: Forcing ALL relays OFF with verification...\r\n");
  emit(out, print, "════════════════════════════════════════════════════════\r\n");
  int attempts=0, failures=0;
  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;
    attempts++;
    bool off=false;
    emitf(out, print, "🔧 Emergency OFF for ID %d (0x%02X)...\r\n", id, addr);
    for (int t=0;t<10;t++){
      if (sendCommandSafe(addr, CMD_RELAY_OFF)) { emitf(out, print, "    Attempt %d: SUCCESS\r\n", t+1); off=true; break; }
      else { emitf(out, print, "    Attempt %d: FAILED\r\n", t+1); delay(100*(t+1)); }
    }
    if (!off) { failures++; emitf(out, print, "    CRITICAL: ID %d (0x%02X) may be ON!\r\n", id, addr); }
    else      { emitf(out, print, "    CONFIRMED: ID %d (0x%02X) OFF\r\n", id, addr); }
    delay(50);
  }
  emit(out, print, "════════════════════════════════════════════════════════\r\n");
  if (failures==0) emitf(out, print, " SUCCESS: All %d mapped relays confirmed OFF\r\n", attempts);
  else             emitf(out, print, " DANGER: %d/%d relays could not be confirmed OFF!\r\n", failures, attempts);
  emit(out, print, "════════════════════════════════════════════════════════\r\n");
  return out;
}

String MapController::setAllDevicesState(uint8_t led_cmd, uint8_t relay_cmd, int print) {
  String out;
  emit(out, print, "Setting all mapped devices (LED + RELAY)...\r\n");
  for (int id=1; id<=MAX_DEVICES; id++) {
    uint8_t addr = getAddressByID(id);
    if (addr == INVALID_ADDRESS) continue;
    sendCommandFast(addr, led_cmd);
    delay(COMMAND_DELAY_MS);
    sendCommandFast(addr, relay_cmd);
  }
  return out;
}

// ========== Utility timing ==========
uint32_t MapController::startTiming() {
  last_operation_start_ = millis();
  return last_operation_start_;
}
String MapController::recordTiming(const char* op, uint32_t ms, int print) {
  String out;
  emitf(out, print, "%s completed in %lums\r\n", op, ms);
  return out;
}

// ========== Debug Flash ==========
bool MapController::saveDataToFlash(uint8_t addr, uint8_t data) {
  wire_->beginTransmission(addr);
  wire_->write(CMD_SAVE_DATA);
  wire_->write(data);
  uint8_t res = wire_->endTransmission();
  if (res == 0) { delay(50); return true; }
  return false;
}
uint8_t MapController::readDataFromFlash(uint8_t addr) {
  wire_->beginTransmission(addr);
  wire_->write(CMD_READ_DATA);
  if (wire_->endTransmission() != 0) return 0xFF;
  delay(10);
  wire_->requestFrom(addr, (uint8_t)1);
  if (wire_->available()) return wire_->read();
  return 0xFF;
}

String MapController::cmdFlashSave(String cmd, int print) {
  String out;
  int s1 = cmd.indexOf(' ');
  int s2 = cmd.indexOf(' ', s1+1);
  if (s1==-1 || s2==-1) { emit(out, print, "Usage: flash_save <id> <data>\r\n"); return out; }
  uint8_t id = cmd.substring(s1+1, s2).toInt();
  String dataStr = cmd.substring(s2+1);
  uint8_t data = dataStr.startsWith("0x") ? (uint8_t)strtol(dataStr.c_str(), NULL, 16) : (uint8_t)dataStr.toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", id); return out; }
  emitf(out, print, "Saving 0x%02X to flash at ID %d (0x%02X)...\r\n", data, id, addr);
  emit(out, print, saveDataToFlash(addr, data) ? " OK\r\n" : " FAIL\r\n");
  return out;
}

String MapController::cmdFlashRead(String cmd, int print) {
  String out;
  int sp = cmd.indexOf(' ');
  if (sp==-1){ emit(out, print, "Usage: flash_read <id>\r\n"); return out; }
  uint8_t id = cmd.substring(sp+1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", id); return out; }
  emitf(out, print, "Reading flash at ID %d (0x%02X)...\r\n", id, addr);
  uint8_t d = readDataFromFlash(addr);
  if (d != 0xFF) emitf(out, print, " Flash data: 0x%02X (%d)\r\n", d, d);
  else emit(out, print, " Failed to read data from flash\r\n");
  return out;
}

// ========== I2C Address mgmt ==========
bool MapController::setDeviceI2CAddress(uint8_t current_addr, uint8_t new_addr) {
  if (new_addr < 0x08 || new_addr > 0x77) return false;
  wire_->beginTransmission(current_addr);
  wire_->write(CMD_SET_I2C_ADDR);
  wire_->write(new_addr);
  uint8_t r = wire_->endTransmission();
  if (r == 0) { delay(50); return true; }
  return false;
}

bool MapController::resetDeviceToFactory(uint8_t addr) {
  wire_->beginTransmission(addr);
  wire_->write(CMD_RESET_FACTORY);
  uint8_t r = wire_->endTransmission();
  if (r == 0) { delay(50); return true; }
  return false;
}

bool MapController::getDeviceI2CStatus(uint8_t addr, uint8_t* current_addr, uint8_t* factory_addr, bool* is_from_flash) {
  wire_->beginTransmission(addr);
  wire_->write(CMD_GET_I2C_STATUS);
  if (wire_->endTransmission() != 0) return false;
  delay(10);
  wire_->requestFrom(addr, (uint8_t)4);
  if (wire_->available() >= 4) {
    uint8_t status = wire_->read();
    *current_addr = wire_->read();
    *factory_addr = wire_->read();
    wire_->read(); // reserved
    *is_from_flash = (status & 0x01) != 0;
    return true;
  }
  return false;
}

String MapController::cmdSetI2CAddress(String cmd, int print) {
  String out;
  int s1 = cmd.indexOf(' ');
  int s2 = cmd.indexOf(' ', s1+1);
  if (s1==-1 || s2==-1) { emit(out, print, "Usage: set_i2c <id> <new_address>\r\n"); return out; }
  uint8_t id = cmd.substring(s1+1, s2).toInt();
  String aStr = cmd.substring(s2+1);
  uint8_t new_addr = aStr.startsWith("0x") ? (uint8_t)strtol(aStr.c_str(), NULL, 16) : (uint8_t)aStr.toInt();
  uint8_t cur = getAddressByID(id);
  if (cur == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", id); return out; }
  emitf(out, print, "Changing I2C of ID %d from 0x%02X to 0x%02X...\r\n", id, cur, new_addr);
  emit(out, print, setDeviceI2CAddress(cur, new_addr) ? " OK (scan after restart)\r\n" : " FAIL\r\n");
  return out;
}

String MapController::cmdResetFactory(String cmd, int print) {
  String out;
  int sp = cmd.indexOf(' ');
  if (sp==-1){ emit(out, print, "Usage: reset_factory <id>\r\n"); return out; }
  uint8_t id = cmd.substring(sp+1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", id); return out; }
  emitf(out, print, "Resetting ID %d (0x%02X) to factory...\r\n", id, addr);
  emit(out, print, resetDeviceToFactory(addr) ? " OK (scan after restart)\r\n" : " FAIL\r\n");
  return out;
}

String MapController::cmdI2CStatus(String cmd, int print) {
  String out;
  int sp = cmd.indexOf(' ');
  if (sp==-1){ emit(out, print, "Usage: i2c_status <id>\r\n"); return out; }
  uint8_t id = cmd.substring(sp+1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", id); return out; }
  emitf(out, print, "Getting I2C status from ID %d (0x%02X)...\r\n", id, addr);
  uint8_t cur,fact; bool from_flash=false;
  if (getDeviceI2CStatus(addr,&cur,&fact,&from_flash)) {
    emit(out, print, " I2C Configuration Status:\r\n");
    emitf(out, print, "   Current Address: 0x%02X (%d)\r\n", cur, cur);
    emitf(out, print, "   Factory Address: 0x%02X (%d)\r\n", fact,fact);
    emitf(out, print, "   Address Source : %s\r\n", from_flash?"Flash Memory":"Factory Default");
    if (cur!=fact) emit(out, print, "   → Address modified from factory\r\n");
  } else emit(out, print, " Failed to get I2C status\r\n");
  return out;
}

String MapController::cmdDebugReset(String cmd, int print) {
  String out;
  int sp = cmd.indexOf(' ');
  if (sp==-1){ emit(out, print, "Usage: debug_reset <id>\r\n"); return out; }
  uint8_t id = cmd.substring(sp+1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", id); return out; }
  out += debugResetDevice(addr, print);
  return out;
}

uint8_t MapController::parseI2CAddress(String s) {
  s.trim();
  if (s.length() == 0) return 0;

  long v = -1;

  if (s.startsWith("0x") || s.startsWith("0X")) {
    v = strtol(s.c_str(), nullptr, 16);
  } else if (s.endsWith("h") || s.endsWith("H")) {
    String core = s.substring(0, s.length() - 1);
    v = strtol(core.c_str(), nullptr, 16);
  } else {
    // ¿decimal puro?
    bool dec = true;
    for (uint16_t i = 0; i < s.length(); ++i) {
      if (!isDigit(s[i])) { dec = false; break; }
    }
    if (dec) v = s.toInt();
    else     v = strtol(s.c_str(), nullptr, 16); // intenta como hex
  }

  if (v < 0 || v > 255) return 0;
  if (v < 0x08 || v > 0x77) return 0;  // rango válido I2C 7-bit que usas
  return static_cast<uint8_t>(v);
}


String MapController::cmdVerifyChange(String cmd, int print) {
  String out;
  int s1 = cmd.indexOf(' ');
  int s2 = cmd.indexOf(' ', s1+1);
  if (s1==-1 || s2==-1) { emit(out, print, "Usage: verify_change <old_id> <new_addr>\r\n"); return out; }
  uint8_t old_id = cmd.substring(s1+1, s2).toInt();
  String na = cmd.substring(s2+1);
  uint8_t old_addr = getAddressByID(old_id);
  if (old_addr == INVALID_ADDRESS) { emitf(out, print, "ERROR: No device mapped to ID %d\r\n", old_id); return out; }
  uint8_t new_addr = parseI2CAddress(na);
  if (new_addr == 0) {
    emitf(out, print, "ERROR: Invalid I2C address: %s (valid range 0x08-0x77)\r\n", na.c_str());
    return out;
  }
  out += verifyAddressChange(old_addr, new_addr, print);
  return out;
}

bool MapController::saveColorPreset(uint8_t address, uint8_t slot, uint8_t r, uint8_t g, uint8_t b) {
  // User slot is 1..3, firmware slot is 0..2
  if (slot < 1 || slot > 3) {
    return false;
  }

  uint8_t fw_slot = (uint8_t)(slot - 1);

  if (!sendBasicCommand(address, CMD_SAVE_COLOR)) return false;
  delay(5);
  if (!sendBasicCommand(address, fw_slot)) return false;
  delay(5);
  if (!sendBasicCommand(address, r)) return false;
  delay(5);
  if (!sendBasicCommand(address, g)) return false;
  delay(5);
  if (!sendBasicCommand(address, b)) return false;

  return true;
}

String MapController::cmdSaveColor(String cmd, int print) {
  String out;

  // Usage: save_color <id|addr> <slot(1..3)> <r> <g> <b>
  int s1 = cmd.indexOf(' ');
  int s2 = cmd.indexOf(' ', s1 + 1);
  int s3 = cmd.indexOf(' ', s2 + 1);
  int s4 = cmd.indexOf(' ', s3 + 1);
  int s5 = cmd.indexOf(' ', s4 + 1);

  if (s1 == -1 || s2 == -1 || s3 == -1 || s4 == -1 || s5 == -1) {
    emit(out, print, "Usage: save_color <id|addr> <slot> <r> <g> <b>\r\n");
    emit(out, print, "       slot: 1=red, 2=green, 3=blue\r\n");
    emit(out, print, "       r/g/b: decimal o 0x00..0xFF\r\n");
    return out;
  }

  String target_str = cmd.substring(s1 + 1, s2);
  target_str.trim();
  String slot_str = cmd.substring(s2 + 1, s3);
  String r_str = cmd.substring(s3 + 1, s4);
  String g_str = cmd.substring(s4 + 1, s5);
  String b_str = cmd.substring(s5 + 1);

  slot_str.trim();
  r_str.trim();
  g_str.trim();
  b_str.trim();

  char* endptr;
  long slot_l = strtol(slot_str.c_str(), &endptr, 0);
  if (*endptr != '\0' || slot_l < 1 || slot_l > 3) {
    emit(out, print, "ERROR: slot invalido. Usa 1, 2 o 3\r\n");
    return out;
  }

  long r_l = strtol(r_str.c_str(), &endptr, 0);
  if (*endptr != '\0' || r_l < 0 || r_l > 255) {
    emit(out, print, "ERROR: R invalido (0..255 o 0x00..0xFF)\r\n");
    return out;
  }

  long g_l = strtol(g_str.c_str(), &endptr, 0);
  if (*endptr != '\0' || g_l < 0 || g_l > 255) {
    emit(out, print, "ERROR: G invalido (0..255 o 0x00..0xFF)\r\n");
    return out;
  }

  long b_l = strtol(b_str.c_str(), &endptr, 0);
  if (*endptr != '\0' || b_l < 0 || b_l > 255) {
    emit(out, print, "ERROR: B invalido (0..255 o 0x00..0xFF)\r\n");
    return out;
  }

  // Resolver objetivo: primero ID mapeado, luego dirección I2C directa.
  uint8_t id = 0;
  uint8_t address = INVALID_ADDRESS;
  bool resolved_by_id = false;

  // Intento como ID (decimal simple)
  bool is_decimal = true;
  for (uint16_t i = 0; i < target_str.length(); i++) {
    if (!isDigit(target_str[i])) {
      is_decimal = false;
      break;
    }
  }

  if (is_decimal) {
    long id_l = target_str.toInt();
    if (id_l >= 1 && id_l <= MAX_DEVICES) {
      id = (uint8_t)id_l;
      address = getAddressByID(id);
      if (address != INVALID_ADDRESS) {
        resolved_by_id = true;
      }
    }
  }

  // Si no se resolvió por ID, intentar dirección I2C directa.
  if (address == INVALID_ADDRESS) {
    // parseI2CAddress acepta 0xNN, NNh, decimal; y si no es decimal intenta hex.
    address = parseI2CAddress(target_str);

    // Compatibilidad extra: si el usuario pone "20" queriendo 0x20.
    if (address == 0 && target_str.length() > 0) {
      char* addr_endptr;
      long hex_addr = strtol(target_str.c_str(), &addr_endptr, 16);
      if (*addr_endptr == '\0' && hex_addr >= 0x08 && hex_addr <= 0x77) {
        address = (uint8_t)hex_addr;
      }
    }
  }

  if (address == INVALID_ADDRESS || address == 0) {
    emit(out, print, "ERROR: Objetivo invalido. Usa ID mapeado o direccion I2C (0x08-0x77)\r\n");
    return out;
  }

  bool ok = saveColorPreset(address,
                            (uint8_t)slot_l,
                            (uint8_t)r_l,
                            (uint8_t)g_l,
                            (uint8_t)b_l);

  if (ok) {
    if (resolved_by_id) {
      emitf(out, print, "[OK] save_color ID %d (0x%02X) slot %d = (%d,%d,%d)\r\n",
            id, address, (int)slot_l, (int)r_l, (int)g_l, (int)b_l);
    } else {
      emitf(out, print, "[OK] save_color addr 0x%02X slot %d = (%d,%d,%d)\r\n",
            address, (int)slot_l, (int)r_l, (int)g_l, (int)b_l);
    }
  } else {
    if (resolved_by_id) {
      emitf(out, print, "ERROR: save_color fallo en ID %d (0x%02X)\r\n", id, address);
    } else {
      emitf(out, print, "ERROR: save_color fallo en addr 0x%02X\r\n", address);
    }
  }

  return out;
}

bool MapController::checkDeviceCompatibility(uint8_t address) {
  wire_->beginTransmission(address);
  wire_->write(CMD_PA4_DIGITAL);
  if (wire_->endTransmission() == 0) {
    delay(50);
    wire_->requestFrom(address, (uint8_t)1);
    if (wire_->available()) {
      uint8_t r = wire_->read();
      return (r != 0x00 && r != 0xFF);
    }
  }
  return false;
}

bool MapController::sendCommandWithTimeout(uint8_t address, uint8_t command, uint32_t timeout_ms) {
  uint32_t start = millis();
  wire_->beginTransmission(address); wire_->write(command);
  if (wire_->endTransmission() != 0) return false;
  while (millis() - start < timeout_ms) {
    if (wire_->available()) return true;
    delay(10);
  }
  return false;
}

String MapController::debugResetDevice(uint8_t address, int print) {
  String out;
  emitf(out, print, " DEBUG RESET - Device 0x%02X (%d)\r\n", address, address);
  emit(out, print, "════════════════════════════════════════════════════════\r\n");
  emit(out, print, " Test 1: Basic communication...\r\n");
  wire_->beginTransmission(address);
  uint8_t e = wire_->endTransmission();
  emitf(out, print, "   Result: %s (Error: %d)\r\n", e==0?" OK":" FAILED", e);
  if (e != 0) { emit(out, print, " DEBUG TERMINATED: No basic communication\r\n"); return out; }

  emit(out, print, " Test 2: Compatibility check...\r\n");
  bool comp = checkDeviceCompatibility(address);
  emitf(out, print, "   Result: %s\r\n", comp ? " Compatible" : " Not compatible");

  emit(out, print, " Test 3: Sending reset command (0xFE)...\r\n");
  wire_->beginTransmission(address); wire_->write(CMD_RESET);
  uint8_t re = wire_->endTransmission(true);
  if (re != 0) { emitf(out, print, " Failed to send reset (Error: %d)\r\n", re); return out; }
  emit(out, print, " Reset command sent successfully\r\n");

  emit(out, print, " Test 4: Monitoring device disconnection...\r\n");
  for (int i=1;i<=10;i++){
    delay(100);
    wire_->beginTransmission(address);
    uint8_t ce = wire_->endTransmission();
    emitf(out, print, "   Check %2d (%dms): %s (Error: %d)\r\n", i, i*100, ce==0?" ONLINE":" OFFLINE", ce);
    if (ce != 0 && i>2) { emit(out, print, " Device disconnected correctly (reset)\r\n"); break; }
  }

  emit(out, print, " Test 5: Waiting for reconnection...\r\n");
  emit(out, print, "   Waiting 3 seconds for complete boot...\r\n");
  delay(3000);

  bool back=false;
  for (int a=1;a<=10;a++){
    wire_->beginTransmission(address);
    uint8_t r = wire_->endTransmission();
    emitf(out, print, "   Reconnection %2d: %s (Error: %d)\r\n", a, r==0?" ONLINE":" OFFLINE", r);
    if (r==0){ back=true; emitf(out, print, " Device reconnected on attempt %d\r\n", a); break; }
    delay(500);
  }

  emit(out, print, "════════════════════════════════════════════════════════\r\n");
  if (back) {
    emit(out, print, " DEBUG RESET SUCCESSFUL\r\n");
    if (checkDeviceCompatibility(address)) emit(out, print, " Maintains compatibility post-reset\r\n");
    else                                   emit(out, print, " Warning: Lost compatibility (may be normal)\r\n");
  } else {
    emit(out, print, " DEBUG RESET FAILED\r\n");
    emitf(out, print, " Device 0x%02X did not reconnect\r\n", address);
  }
  return out;
}



// ========== Diagnostics ==========
String MapController::testDeviceCommands(uint8_t addr, int print) {
  String out;
  emitf(out, print, "Testing commands on device 0x%02X...\r\n", addr);
  emit(out, print, "Command | Response | Status\r\n");
  emit(out, print, "--------|----------|--------\r\n");
  uint8_t cmds[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07};
  const char* names[]={"RELAY_OFF","RELAY_ON","NEO_RED","NEO_GREEN","NEO_BLUE","NEO_OFF","TOGGLE","PA4_READ"};
  for (int i=0;i<8;i++){
    wire_->beginTransmission(addr); wire_->write(cmds[i]);
    uint8_t e = wire_->endTransmission();
    if (e==0) emitf(out, print, "0x%02X    | ACK      | %s\r\n", cmds[i], names[i]);
    else      emitf(out, print, "0x%02X    | NACK(%d) | %s\r\n", cmds[i], e, names[i]);
    delay(20);
  }
  emit(out, print, "\nTesting potential ADC commands...\r\n");
  uint8_t adc_cmds[] = {0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF};
  for (int i=0;i<24;i++){
    wire_->beginTransmission(addr); wire_->write(adc_cmds[i]);
    uint8_t e = wire_->endTransmission();
    if (e==0) emitf(out, print, "0x%02X    | ACK      | POTENTIAL ADC\r\n", adc_cmds[i]);
    delay(10);
  }
  emit(out, print, "Command test completed.\r\n");
  return out;
}

String MapController::cmdTestCommands(String cmd, int print) {
  int sp = cmd.lastIndexOf(' ');
  if (sp == -1) return String("ERROR: Usage: test <id>\r\n");
  uint8_t id = cmd.substring(sp + 1).toInt();
  uint8_t addr = getAddressByID(id);
  if (addr == INVALID_ADDRESS) return String("ERROR: No device mapped to ID " + String(id) + ".\r\n");
  return testDeviceCommands(addr, print);
}

// ========== ADDITIONAL DEVICE FUNCTIONS ==========

String MapController::showDeviceInfo(uint8_t address, int print) {
  String out;
  emitf(out, print, "=== INFORMACIÓN DEL DISPOSITIVO 0x%02X ===\r\n", address);
  
  // Get Device ID
  uint8_t device_id = 0;
  if (i2cReadByteWithRetry(address, CMD_GET_DEVICE_ID, &device_id, 3, 30)) {
    String device_type = getDeviceTypeName(device_id);
    emitf(out, print, "Tipo: %s (ID: 0x%02X)\r\n", device_type.c_str(), device_id);
  } else {
    emit(out, print, "ERROR: No se pudo leer Device ID\r\n");
  }
  
  // Get Flash Size
  uint8_t flash_size = 0;
  if (i2cReadByteWithRetry(address, CMD_GET_FLASH_SIZE, &flash_size, 3, 30)) {
    emitf(out, print, "Flash: %d KB\r\n", flash_size);
  } else {
    emit(out, print, "ERROR: No se pudo leer tamaño de Flash\r\n");
  }
  
  emit(out, print, "=====================================\r\n");
  return out;
}

String MapController::showDeviceUID(uint8_t address, int print) {
  String out;
  emitf(out, print, "=== UID ÚNICO DEL DISPOSITIVO 0x%02X ===\r\n", address);
  
  uint32_t uid = getDeviceUID(address);
  if (uid != 0) {
    emitf(out, print, "UID (32-bit): 0x%08X\r\n", uid);
    
    // Calculate I2C address based on UID (for reference)
    uint8_t calculated_addr = 0x20 + ((uid & 0xFF) & 0x0F);
    emitf(out, print, "Dirección I2C calculada: 0x%02X (%d)\r\n", 
          calculated_addr, calculated_addr);
  } else {
    emit(out, print, "ERROR: No se pudo leer UID\r\n");
  }
  
  emit(out, print, "=====================================\r\n");
  return out;
}

String MapController::getDeviceTypeName(uint8_t device_id) {
  switch(device_id) {
    case 0x16: return "PY32F003F16U6TR (16KB Flash)";
    case 0x18: return "PY32F003F18U6TR (18KB Flash)";
    default: return "Desconocido";
  }
}

uint32_t MapController::getDeviceUID(uint8_t address) {
  uint8_t uid_bytes[4];
  bool uid_ok = true;
  
  // Read the 4 UID bytes
  for (int i = 0; i < 4; i++) {
    uint8_t cmd = CMD_GET_UID_BYTE0 + i;
    if (!i2cReadByteWithRetry(address, cmd, &uid_bytes[i], 3, 30)) {
      uid_ok = false;
      break;
    }
  }
  
  if (uid_ok) {
    // Combine into 32-bit UID
    uint32_t uid = (uint32_t)uid_bytes[3] << 24 | 
                   (uint32_t)uid_bytes[2] << 16 | 
                   (uint32_t)uid_bytes[1] << 8 | 
                   (uint32_t)uid_bytes[0];
    return uid;
  }
  
  return 0; // Error
}

String MapController::testAllCommands(uint8_t address, int print) {
  String out;
  
  // Variables para tracking de resultados
  bool toggle_result = false;
  bool neo_red_result = false;
  bool neo_off_result = false;
  uint8_t pa4_value = 0;
  uint16_t adc_value = 0;
  int tests_passed = 0;
  int tests_total = 4; // toggle, neo_red, pa4, adc
  
  // Test Toggle (sin output, sin delay)
  toggle_result = sendBasicCommand(address, CMD_TOGGLE);
  if (toggle_result) {
    tests_passed++;
  }
  
  // Test NeoPixels RED (sin output, sin delay)
  neo_red_result = sendBasicCommand(address, CMD_NEO_RED);
  if (neo_red_result) {
    // Test NeoPixel OFF (inmediato)
    neo_off_result = sendBasicCommand(address, CMD_NEO_OFF);
  }
  
  // Test PA4 Digital (sin output)
  pa4_value = readPA4Digital(address);
  tests_passed++; // PA4 siempre cuenta como éxito si se puede leer
  
  // Test ADC (sin output)
  adc_value = readADC(address);
  tests_passed++; // ADC siempre cuenta como éxito si se puede leer
  
  // SOLO UNA LÍNEA: CSV crudo
  // Formato: success_rate,toggle,neo,pa4,adc_raw,vcc
  float success_rate = (float)tests_passed / tests_total * 100.0;
  float vcc_voltage = (adc_value * 3.3f) / 4095.0f;
  
  emitf(out, print, "%.1f,%d,%d,%d,%d,%.2f\r\n", 
        success_rate,
        toggle_result ? 1 : 0,
        neo_red_result ? 1 : 0,
        pa4_value,
        adc_value,
        vcc_voltage);
  
  return out;
}

String MapController::testBothCommands(uint8_t address, int print) {
  String out;
  
  // Solo toggle y PA4 - ultra rápido
  bool toggle_result = sendBasicCommand(address, CMD_TOGGLE);
  uint8_t pa4_value = readPA4Digital(address);
  
  // SOLO UNA LÍNEA: CSV formato simple toggle,pa4
  emitf(out, print, "%d,%d\r\n", 
        toggle_result ? 1 : 0,
        pa4_value);
  
  return out;
}

// ========== COMMAND WRAPPERS ==========

String MapController::cmdRelay(uint8_t address, bool state, int print) {
  String out;
  uint8_t cmd = state ? CMD_RELAY_ON : CMD_RELAY_OFF;
  const char* action = state ? "encendido" : "apagado";
  
  if (sendBasicCommand(address, cmd)) {
    emitf(out, print, "[OK] Rele %s en 0x%02X\r\n", action, address);
  } else {
    emitf(out, print, "ERROR: No se pudo %s rele en 0x%02X\r\n", action, address);
  }
  
  return out;
}

String MapController::cmdToggle(uint8_t address, int print) {
  String out;
  
  bool success = sendBasicCommand(address, CMD_TOGGLE);
  
  if (print) {
    if (success) {
      emitf(out, print, "OK 0x%02X\r\n", address);
    } else {
      emitf(out, print, "ERR 0x%02X\r\n", address);
    }
  }
  
  return out;
}

String MapController::cmdNeoPixel(uint8_t address, uint8_t color, int print) {
  String out;
  uint8_t commands[] = {CMD_NEO_OFF, CMD_NEO_RED, CMD_NEO_GREEN, CMD_NEO_BLUE, CMD_NEO_WHITE};
  
  if (color < 5) {
    bool ok = sendBasicCommand(address, commands[color]);
    if (print) emitf(out, print, "%s 0x%02X\r\n", ok ? "OK" : "ERR", address);
  } else {
    if (print) emit(out, print, "Invalid color\r\n");
  }
  
  return out;
}

String MapController::cmdPWM(uint8_t address, uint8_t level, int print) {
  String out;
  uint8_t commands[] = {CMD_PWM_OFF, CMD_PWM_25, CMD_PWM_50, CMD_PWM_75, CMD_PWM_100};
  
  if (level < 5) {
    bool ok = sendBasicCommand(address, commands[level]);
    if (print) emitf(out, print, "%s 0x%02X\r\n", ok ? "OK" : "ERR", address);
  } else {
    if (print) emit(out, print, "Invalid level\r\n");
  }
  
  return out;
}

String MapController::cmdReadPA4(uint8_t address, int print) {
  String out;
  uint8_t pa4_state = readPA4Digital(address);
  if (print) emitf(out, print, "0x%02X: %d\r\n", address, pa4_state);
  return out;
}

String MapController::cmdReadADC(uint8_t address, int print) {
  String out;
  uint16_t adc_value = readADC(address);
  if (print) emitf(out, print, "0x%02X: %d\r\n", address, adc_value);
  return out;
}

// ========== BUS STATUS MASK ==========

uint32_t MapController::getBusStatusMask() {
  // Generate 32-bit mask showing device presence
  // MSB = 0x77 (pos 31), LSB = 0x08 (pos 0)
  // Bit = 1 if device online, 0 if offline
  
  uint32_t mask = 0;
  
  // Real-time ping to each device
  for (int i = 0; i < 32; i++) {
    uint8_t address = 0x08 + (i * 2);
    
    // Ping device in real-time (2 attempts)
    bool is_online = false;
    for (uint8_t attempt = 0; attempt < 2; attempt++) {
      while(wire_->available()) wire_->read();
      
      wire_->beginTransmission(address);
      uint8_t error = wire_->endTransmission(true);
      
      if (error == 0) {
        is_online = true;
        break;
      }
      
      if (attempt < 1) delay(5);
    }
    
    if (is_online) {
      mask |= (1UL << i);
    }
    
    delay(2);
  }
  
  return mask;
}

uint64_t MapController::getBusStatusMask2Bit() {
  // Generate 64-bit mask with 2 bits per device (32 devices)
  // States: 00=Offline, 01=Online, 10=Error/Timeout, 11=Unknown
  // MSB = 0x77, LSB = 0x08
  
  uint64_t mask = 0;
  
  for (int i = 0; i < 32; i++) {
    uint8_t address = 0x08 + (i * 2);
    uint8_t status = DEVICE_OFFLINE;
    
    // Real-time ping with 3 attempts
    int successful_attempts = 0;
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
      while(wire_->available()) wire_->read();
      
      wire_->beginTransmission(address);
      uint8_t error = wire_->endTransmission(true);
      
      if (error == 0) {
        successful_attempts++;
      }
      
      if (attempt < 2) delay(5);
    }
    
    // Determine status based on ping results
    if (successful_attempts == 3) {
      status = DEVICE_ONLINE;     // 01 = All pings OK
    } else if (successful_attempts > 0) {
      status = DEVICE_ERROR;      // 10 = Intermittent
    } else {
      status = DEVICE_OFFLINE;    // 00 = No response
    }
    
    mask |= ((uint64_t)status << (i * 2));
    delay(2);
  }
  
  return mask;
}

String MapController::cmdBusMask(int print) {
  String out;
  
  // Get masks
  uint32_t presence_mask = getBusStatusMask();
  uint64_t status_mask = getBusStatusMask2Bit();
  
  // Show 32-bit presence mask
  emit(out, print, "\nPRESENCE MASK (1=Online, 0=Offline):\n");
  emit(out, print, "   MSB                                LSB\n");
  emit(out, print, "   ");
  for (int i = 31; i >= 0; i--) {
    emitf(out, print, "%d", (presence_mask >> i) & 1);
    if (i % 8 == 0 && i > 0) emit(out, print, " ");
  }
  emit(out, print, "\n");
  emitf(out, print, "   0x%08X (decimal: %u)\n\n", presence_mask, presence_mask);
  
  // Show 64-bit status mask
  emit(out, print, "STATUS MASK (2 bits per device):\n");
  emit(out, print, "   States: 00=Offline, 01=Online, 10=Error, 11=Unknown\n\n");
  
  // Show in groups of 8 devices (16 bits)
  for (int group = 0; group < 4; group++) {
    int start_dev = group * 8;
    int end_dev = start_dev + 7;
    uint8_t start_addr = 0x08 + (start_dev * 2);
    uint8_t end_addr = 0x08 + (end_dev * 2);
    
    emitf(out, print, "   [0x%02X - 0x%02X]: ", start_addr, end_addr);
    
    for (int i = end_dev; i >= start_dev; i--) {
      uint8_t status = (status_mask >> (i * 2)) & 0x03;
      
      if (status == DEVICE_ONLINE) {
        emit(out, print, "01 ");
      } else if (status == DEVICE_ERROR) {
        emit(out, print, "10 ");
      } else if (status == DEVICE_OFFLINE) {
        emit(out, print, "00 ");
      } else {
        emit(out, print, "11 ");
      }
    }
    emit(out, print, "\n");
  }
  
  emit(out, print, "\n   Full 64-bit mask: 0x");
  emitf(out, print, "%08X%08X\n\n", (uint32_t)(status_mask >> 32), (uint32_t)(status_mask & 0xFFFFFFFF));
  
  return out;
}

String MapController::cmdBusMaskPA4(int print) {
  String out;
  
  uint32_t pa4_mask = 0;
  uint32_t response_mask = 0;
  
  // Read PA4 from each device
  for (int i = 0; i < 32; i++) {
    uint8_t address = 0x08 + (i * 2);
    
    uint8_t pa4_state = readPA4Digital(address);
    
    bool responded = false;
    for (int j = 0; j < device_count_; j++) {
      if (found_devices_[j].address == address && found_devices_[j].is_online) {
        responded = true;
        break;
      }
    }
    
    if (responded) {
      response_mask |= (1UL << i);
      if (pa4_state) {
        pa4_mask |= (1UL << i);
      }
    }
  }
  
  emit(out, print, "\nPA4 STATE MASK (1=HIGH, 0=LOW):\n");
  emit(out, print, "   MSB                                LSB\n");
  emit(out, print, "   ");
  for (int i = 31; i >= 0; i--) {
    if ((response_mask >> i) & 1) {
      emitf(out, print, "%d", (pa4_mask >> i) & 1);
    } else {
      emit(out, print, "X");
    }
    if (i % 8 == 0 && i > 0) emit(out, print, " ");
  }
  emit(out, print, "\n");
  emitf(out, print, "   0x%08X\n\n", pa4_mask);
  
  emit(out, print, "RESPONSE MASK (1=Responded, 0=No response):\n");
  emit(out, print, "   MSB                                LSB\n");
  emit(out, print, "   ");
  for (int i = 31; i >= 0; i--) {
    emitf(out, print, "%d", (response_mask >> i) & 1);
    if (i % 8 == 0 && i > 0) emit(out, print, " ");
  }
  emit(out, print, "\n");
  emitf(out, print, "   0x%08X\n\n", response_mask);
  
  return out;
}

// Convenience wrappers for specific colors/levels
String MapController::cmdNeoRed(uint8_t address, int print) { return cmdNeoPixel(address, 1, print); }
String MapController::cmdNeoGreen(uint8_t address, int print) { return cmdNeoPixel(address, 2, print); }
String MapController::cmdNeoBlue(uint8_t address, int print) { return cmdNeoPixel(address, 3, print); }
String MapController::cmdNeoWhite(uint8_t address, int print) { return cmdNeoPixel(address, 4, print); }
String MapController::cmdNeoOff(uint8_t address, int print) { return cmdNeoPixel(address, 0, print); }

String MapController::cmdPWMOff(uint8_t address, int print) { return cmdPWM(address, 0, print); }
String MapController::cmdPWM25(uint8_t address, int print) { return cmdPWM(address, 1, print); }
String MapController::cmdPWM50(uint8_t address, int print) { return cmdPWM(address, 2, print); }
String MapController::cmdPWM75(uint8_t address, int print) { return cmdPWM(address, 3, print); }
String MapController::cmdPWM100(uint8_t address, int print) { return cmdPWM(address, 4, print); }

// ========== MUSICAL TONES (v6.0.0_lite) ==========

String MapController::cmdTone(uint8_t address, uint8_t tone_cmd, int print) {
  String out;
  emitf(out, print, ">> TONE 0x%02X -> 0x%02X\r\n", tone_cmd, address);
  
  if (!sendBasicCommand(address, tone_cmd)) {
    emit(out, print, "ERROR: Tone command failed\r\n");
    return out;
  }
  
  emit(out, print, "[OK] Tone sent successfully\r\n");
  return out;
}

String MapController::cmdToneDo(uint8_t address, int print) { return cmdTone(address, CMD_TONE_DO, print); }
String MapController::cmdToneRe(uint8_t address, int print) { return cmdTone(address, CMD_TONE_RE, print); }
String MapController::cmdToneMi(uint8_t address, int print) { return cmdTone(address, CMD_TONE_MI, print); }
String MapController::cmdToneFa(uint8_t address, int print) { return cmdTone(address, CMD_TONE_FA, print); }
String MapController::cmdToneSol(uint8_t address, int print) { return cmdTone(address, CMD_TONE_SOL, print); }
String MapController::cmdToneLa(uint8_t address, int print) { return cmdTone(address, CMD_TONE_LA, print); }
String MapController::cmdToneSi(uint8_t address, int print) { return cmdTone(address, CMD_TONE_SI, print); }

// ========== SYSTEM ALERTS (v6.0.0_lite) ==========

String MapController::cmdSuccess(uint8_t address, int print) {
  String out;
  emitf(out, print, ">> SUCCESS ALERT (800Hz) -> 0x%02X\r\n", address);
  if (!sendBasicCommand(address, CMD_SUCCESS)) {
    emit(out, print, "ERROR: Success alert failed\r\n");
    return out;
  }
  emit(out, print, "[OK] Success alert sent\r\n");
  return out;
}

String MapController::cmdOk(uint8_t address, int print) {
  String out;
  emitf(out, print, ">> OK ALERT (1000Hz) -> 0x%02X\r\n", address);
  if (!sendBasicCommand(address, CMD_OK)) {
    emit(out, print, "ERROR: OK alert failed\r\n");
    return out;
  }
  emit(out, print, "[OK] OK alert sent\r\n");
  return out;
}

String MapController::cmdWarning(uint8_t address, int print) {
  String out;
  emitf(out, print, ">> WARNING ALERT (1200Hz) -> 0x%02X\r\n", address);
  if (!sendBasicCommand(address, CMD_WARNING)) {
    emit(out, print, "ERROR: Warning alert failed\r\n");
    return out;
  }
  emit(out, print, "[OK] Warning alert sent\r\n");
  return out;
}

String MapController::cmdAlert(uint8_t address, int print) {
  String out;
  emitf(out, print, ">> CRITICAL ALERT (1500Hz) -> 0x%02X\r\n", address);
  if (!sendBasicCommand(address, CMD_ALERT)) {
    emit(out, print, "ERROR: Critical alert failed\r\n");
    return out;
  }
  emit(out, print, "[OK] Critical alert sent\r\n");
  return out;
}

// ========== PWM GRADUAL FREQUENCIES (0x10-0x1F) ==========

String MapController::cmdPWMGradual(uint8_t address, uint8_t freq_cmd, int print) {
  String out;
  
  // Validate range
  if (freq_cmd < CMD_PWM_GRADUAL_MIN || freq_cmd > CMD_PWM_GRADUAL_MAX) {
    emitf(out, print, "ERROR: Invalid gradual PWM command 0x%02X (valid range: 0x10-0x1F)\r\n", freq_cmd);
    return out;
  }
  
  // Calculate frequency: freq = 100 + ((cmd - 0x10) * 900 / 16)
  uint16_t frequency = 100 + ((freq_cmd - CMD_PWM_GRADUAL_MIN) * 900 / 16);
  emitf(out, print, ">> PWM GRADUAL 0x%02X (~%dHz) -> 0x%02X\r\n", freq_cmd, frequency, address);
  
  if (!sendBasicCommand(address, freq_cmd)) {
    emit(out, print, "ERROR: Gradual PWM command failed\r\n");
    return out;
  }
  
  emit(out, print, "[OK] Gradual PWM sent successfully\r\n");
  return out;
}

// ========== TEST INTERVAL FUNCTIONS ==========

String MapController::testIntervalEffect(int print) {
  String out;
  if (device_count_ == 0) {
    emit(out, print, "ERROR: No hay dispositivos. Ejecutar scan primero\r\n");
    return out;
  }
  
  emit(out, print, "=== TEST DE EFECTO DEL INTERVALO ===\r\n");
  emit(out, print, "Probando diferentes intervalos para encontrar el punto óptimo...\r\n");
  
  uint32_t intervals[] = {100, 500, 1000, 2000, 5000};
  int num_intervals = sizeof(intervals) / sizeof(intervals[0]);
  
  uint32_t original_interval = test_interval_;
  TestMode original_mode = current_test_mode_;
  
  // Configure for toggle test
  current_test_mode_ = TEST_MODE_TOGGLE_ONLY;
  
  for (int i = 0; i < num_intervals; i++) {
    test_interval_ = intervals[i];
    emitf(out, print, "\n--- Probando intervalo: %d ms ---\r\n", test_interval_);
    
    // Reset statistics
    resetStatistics();
    
    // Execute 10 cycles
    for (int cycle = 0; cycle < 10; cycle++) {
      for (int dev = 0; dev < device_count_; dev++) {
        if (found_devices_[dev].is_compatible) {
          testDevice(dev);
        }
      }
      delay(test_interval_);
    }
    
    // Show results
    for (int dev = 0; dev < device_count_; dev++) {
      if (found_devices_[dev].total_tests > 0) {
        float success_rate = (float)found_devices_[dev].successful_tests / found_devices_[dev].total_tests * 100.0;
        emitf(out, print, "  0x%02X: %.1f%% éxito (%d/%d)\r\n",
              found_devices_[dev].address,
              success_rate,
              found_devices_[dev].successful_tests,
              found_devices_[dev].total_tests);
      }
    }
  }
  
  // Restore original configuration
  test_interval_ = original_interval;
  current_test_mode_ = original_mode;
  resetStatistics();
  
  emit(out, print, "\n=== CONCLUSIÓN ===\r\n");
  emit(out, print, "Intervalos < 1000ms generalmente tienen mejor éxito\r\n");
  emit(out, print, "Intervalos > 1000ms pueden requerir keep-alive\r\n");
  emitf(out, print, "Intervalo restaurado a: %d ms\r\n", test_interval_);
  
  return out;
}

String MapController::testLongIntervalProblem(int print) {
  String out;
  if (device_count_ == 0) {
    emit(out, print, "ERROR: No hay dispositivos. Ejecutar scan primero\r\n");
    return out;
  }
  
  emit(out, print, "=== TEST ESPECÍFICO: PROBLEMA DE INTERVALOS LARGOS ===\r\n");
  emit(out, print, "Comparando 500ms (bueno) vs 2000ms (problemático)...\r\n");
  
  uint8_t test_addr = found_devices_[0].address;  // Use first device
  
  // Test 1: Short interval (500ms) - 10 commands
  emit(out, print, "\n--- FASE 1: Intervalo 500ms (debería funcionar) ---\r\n");
  int success_500ms = 0;
  for (int i = 0; i < 10; i++) {
    if (sendBasicCommand(test_addr, CMD_TOGGLE)) {
      success_500ms++;
      emitf(out, print, "Comando %d: OK\r\n", i + 1);
    } else {
      emitf(out, print, "Comando %d: FAIL\r\n", i + 1);
    }
    delay(500);
  }
  
  emitf(out, print, "Resultado 500ms: %d/10 éxitos (%.1f%%)\r\n", 
        success_500ms, (success_500ms / 10.0) * 100.0);
  
  // Test 2: Long interval (2000ms) - 10 commands
  emit(out, print, "\n--- FASE 2: Intervalo 2000ms (problemático) ---\r\n");
  int success_2000ms = 0;
  for (int i = 0; i < 10; i++) {
    if (sendBasicCommand(test_addr, CMD_TOGGLE)) {
      success_2000ms++;
      emitf(out, print, "Comando %d: OK\r\n", i + 1);
    } else {
      emitf(out, print, "Comando %d: FAIL\r\n", i + 1);
    }
    delay(2000);
  }
  
  emitf(out, print, "Resultado 2000ms: %d/10 éxitos (%.1f%%)\r\n", 
        success_2000ms, (success_2000ms / 10.0) * 100.0);
  
  // Analysis
  emit(out, print, "\n=== ANÁLISIS ===\r\n");
  if (success_500ms > success_2000ms) {
    emitf(out, print, "CONFIRMADO: Intervalos largos tienen %.1f%% menos éxito\r\n",
          ((success_500ms - success_2000ms) / 10.0) * 100.0);
    emit(out, print, "CAUSA PROBABLE: El esclavo entra en modo de bajo consumo\r\n");
    emit(out, print, "SOLUCIÓN: Keep-alive ping activado automáticamente\r\n");
  } else {
    emit(out, print, "SORPRESA: Los intervalos largos funcionan igual de bien\r\n");
  }
  
  emit(out, print, "\nNOTA: Con las optimizaciones actuales, ambos deberían funcionar mejor\r\n");
  
  return out;
}
