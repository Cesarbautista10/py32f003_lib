// ═══════════════════════════════════════════════════════════
//                  MAP CONTROLLER (Library/Class)
//      I2C Device Manager with CRUD Operations & Testing
//           Compatible with PY32F003 firmware devices
// ═══════════════════════════════════════════════════════════
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>

class MapController {
public:
  // I2C CONFIG
  static constexpr int   DEFAULT_SDA   = 21;
  static constexpr int   DEFAULT_SCL   = 22;
  static constexpr uint32_t DEFAULT_I2C_FREQ = 100000; // 100kHz

  // ════════════════════════════════════════════════════════════
  // CRITICAL SECURITY COMMANDS (0xA0-0xAF)
  // ════════════════════════════════════════════════════════════
  // Separated from read commands (0x00-0x0F) by 160 codes
  // to prevent accidental relay activation from I2C corruption
  static constexpr uint8_t CMD_RELAY_OFF       = 0xA0;  // Turn relay OFF (lock)
  static constexpr uint8_t CMD_RELAY_ON        = 0xA1;  // Turn relay ON (unlock)
  static constexpr uint8_t CMD_TOGGLE          = 0xA6;  // Toggle relay (10ms pulse)
  
  // Response codes for relay commands
  static constexpr uint8_t RESP_RELAY_OFF      = 0xB0;
  static constexpr uint8_t RESP_RELAY_ON       = 0xB1;
  static constexpr uint8_t RESP_TOGGLE         = 0xB6;

  // ════════════════════════════════════════════════════════════
  // READ/STATUS COMMANDS (0x00-0x0F)
  // ════════════════════════════════════════════════════════════
  static constexpr uint8_t CMD_PA4_DIGITAL     = 0x07;  // Read PA4 digital state
  
  // ════════════════════════════════════════════════════════════
  // VISUAL EFFECTS COMMANDS (0x02-0x08)
  // ════════════════════════════════════════════════════════════
  static constexpr uint8_t CMD_NEO_RED         = 0x02;
  static constexpr uint8_t CMD_NEO_GREEN       = 0x03;
  static constexpr uint8_t CMD_NEO_BLUE        = 0x04;
  static constexpr uint8_t CMD_NEO_OFF         = 0x05;
  static constexpr uint8_t CMD_NEO_WHITE       = 0x08;

  // ════════════════════════════════════════════════════════════
  // PWM/AUDIO COMMANDS (0x10-0x2F)
  // ════════════════════════════════════════════════════════════
  // Gradual PWM range: 0x10-0x1F (100Hz-1000Hz)
  // Formula: freq = 100 + ((cmd - 0x10) * 900 / 16)
  static constexpr uint8_t CMD_PWM_GRADUAL_MIN = 0x10;  // 100Hz
  static constexpr uint8_t CMD_PWM_GRADUAL_MAX = 0x1F;  // 1000Hz
  
  // PWM Control
  static constexpr uint8_t CMD_PWM_OFF         = 0x20;  // Buzzer OFF
  static constexpr uint8_t CMD_PWM_25          = 0x21;  // 200Hz
  static constexpr uint8_t CMD_PWM_50          = 0x22;  // 500Hz
  static constexpr uint8_t CMD_PWM_75          = 0x23;  // 1000Hz
  static constexpr uint8_t CMD_PWM_100         = 0x24;  // 2000Hz
  
  // Musical Tones
  static constexpr uint8_t CMD_TONE_DO         = 0x25;  // DO - 261Hz
  static constexpr uint8_t CMD_TONE_RE         = 0x26;  // RE - 294Hz
  static constexpr uint8_t CMD_TONE_MI         = 0x27;  // MI - 330Hz
  static constexpr uint8_t CMD_TONE_FA         = 0x28;  // FA - 349Hz
  static constexpr uint8_t CMD_TONE_SOL        = 0x29;  // SOL - 392Hz
  static constexpr uint8_t CMD_TONE_LA         = 0x2A;  // LA - 440Hz
  static constexpr uint8_t CMD_TONE_SI         = 0x2B;  // SI - 494Hz
  
  // System Alerts
  static constexpr uint8_t CMD_SUCCESS         = 0x2C;  // Success - 800Hz
  static constexpr uint8_t CMD_OK              = 0x2D;  // OK - 1000Hz
  static constexpr uint8_t CMD_WARNING         = 0x2E;  // Warning - 1200Hz
  static constexpr uint8_t CMD_ALERT           = 0x2F;  // Alert - 1500Hz
  
  // PWM Response Codes
  static constexpr uint8_t RESP_PWM_SET        = 0x06;
  static constexpr uint8_t RESP_PWM_OFF        = 0x07;
  static constexpr uint8_t RESP_TONE_SET       = 0x0C;
  static constexpr uint8_t RESP_SUCCESS        = 0x0D;
  static constexpr uint8_t RESP_OK             = 0x0E;
  static constexpr uint8_t RESP_WARNING        = 0x0F;
  static constexpr uint8_t RESP_ALERT          = 0x0A;

  // I2C ADDRESS MANAGEMENT
  static constexpr uint8_t CMD_SET_I2C_ADDR    = 0x3D;
  static constexpr uint8_t CMD_RESET_FACTORY   = 0x3E;
  static constexpr uint8_t CMD_GET_I2C_STATUS  = 0x3F;
  static constexpr uint8_t CMD_SAVE_COLOR       = 0x46;

  // DEVICE INFO COMMANDS
  static constexpr uint8_t CMD_GET_DEVICE_ID   = 0x40;
  static constexpr uint8_t CMD_GET_FLASH_SIZE  = 0x41;
  static constexpr uint8_t CMD_GET_UID_BYTE0   = 0x42;
  static constexpr uint8_t CMD_GET_UID_BYTE1   = 0x43;
  static constexpr uint8_t CMD_GET_UID_BYTE2   = 0x44;
  static constexpr uint8_t CMD_GET_UID_BYTE3   = 0x45;

  // ADC COMMANDS
  static constexpr uint8_t CMD_ADC_PA0_HSB     = 0xD8;
  static constexpr uint8_t CMD_ADC_PA0_LSB     = 0xD9;
  static constexpr uint8_t CMD_ADC_PA0_I2C     = 0xDA;

  // SYSTEM COMMANDS
  static constexpr uint8_t CMD_RESET           = 0xFE;
  static constexpr uint8_t CMD_WATCHDOG_RESET  = 0xFF;

  // Debug Flash (legacy support)
  static constexpr uint8_t CMD_SAVE_DATA       = 0x3A;
  static constexpr uint8_t CMD_READ_DATA       = 0x3B;
  static constexpr uint8_t RESP_DATA           = 0x3C;
  static constexpr uint8_t RESP_COLOR_STAGE_READY = 0x15;
  static constexpr uint8_t RESP_COLOR_SAVED    = 0x16;

  // Responses/Status
  static constexpr uint8_t RESP_I2C_ADDR_SET   = 0x3D;
  static constexpr uint8_t RESP_FACTORY_RESET  = 0x3E;
  static constexpr uint8_t I2C_STATUS_FLASH    = 0x0F;
  static constexpr uint8_t I2C_STATUS_UID      = 0x0A;

  // Legacy aliases for compatibility
  static constexpr uint8_t CMD_RED        = CMD_NEO_RED;
  static constexpr uint8_t CMD_GREEN      = CMD_NEO_GREEN;
  static constexpr uint8_t CMD_BLUE       = CMD_NEO_BLUE;
  static constexpr uint8_t CMD_OFF        = CMD_NEO_OFF;
  static constexpr uint8_t CMD_RELAY_TOGGLE = CMD_TOGGLE;

  // MAPPING CONSTANTS
  static constexpr uint8_t MAX_DEVICES       = 32;
  static constexpr uint8_t INVALID_ADDRESS   = 0x00;
  static constexpr char    STORAGE_NAMESPACE[] = "devicemap";
  
  // TIMING CONSTANTS
  static constexpr uint8_t COMMAND_DELAY_MS = 50;
  static constexpr uint32_t DEFAULT_TEST_INTERVAL = 2000;  // 2 seconds between test cycles
  static constexpr uint32_t MIN_TEST_INTERVAL = 5;
  static constexpr uint32_t MAX_TEST_INTERVAL = 60000;
  
  // I2C TIMEOUT & RECOVERY CONSTANTS
  static constexpr uint32_t I2C_COMMAND_TIMEOUT_MS = 50;      // Timeout for individual I2C commands (reducido)
  static constexpr uint32_t I2C_RESPONSE_TIMEOUT_MS = 30;     // Timeout waiting for device response (reducido)
  static constexpr uint8_t  I2C_MAX_RETRIES = 2;              // Max retries (reducido de 3 a 2)
  static constexpr uint8_t  I2C_CONSECUTIVE_FAIL_THRESHOLD = 5; // Trigger recovery after N failures
  static constexpr uint32_t I2C_RECOVERY_DELAY_MS = 50;       // Delay after bus recovery (reducido)
  static constexpr uint32_t I2C_HEALTH_CHECK_INTERVAL = 10000; // Health check every 10s
  
  // ASYNC KEEPALIVE CONSTANTS
  static constexpr uint32_t I2C_KEEPALIVE_INTERVAL = 250;     // Keepalive ping every 250ms (reducido para 30 dispositivos)
  static constexpr uint32_t I2C_MIN_ACTIVITY_INTERVAL = 50;   // Minimum time between I2C operations
  static constexpr uint32_t I2C_MAX_IDLE_TIME = 1000;         // Max idle time before forced keepalive
  static constexpr uint32_t I2C_PREVENTIVE_RECOVERY = 30000;  // Preventive bus recovery every 30s
  
  // RATE LIMITING CONSTANTS
  static constexpr uint32_t I2C_MIN_COMMAND_INTERVAL = 5;     // Minimum 5ms between different devices
  static constexpr uint32_t I2C_SAME_DEVICE_INTERVAL = 20;    // Minimum 20ms for same device
  // COOLDOWN ELIMINADO - causa freezes en ESP32
  static constexpr uint32_t I2C_BURST_RECOVERY_DELAY = 3;     // 3ms between burst commands (reducido)

  // TEST MODES
  enum TestMode {
    TEST_MODE_PA4_ONLY,      // Solo lectura PA4
    TEST_MODE_TOGGLE_ONLY,   // Solo toggle relay (0xA6)
    TEST_MODE_BOTH,          // Toggle + PA4
    TEST_MODE_NEO_CYCLE,     // Ciclo NeoPixels (W->R->G->B->OFF)
    TEST_MODE_PWM_TONES,     // Ciclo tonos PWM (OFF->25->50->75->100->OFF)
    TEST_MODE_ADC_PA0,       // Solo lectura ADC PA0
    TEST_MODE_MUSICAL_SCALE, // Escala musical (DO->RE->MI->FA->SOL->LA->SI->OFF)
    TEST_MODE_ALERTS,        // Ciclo alertas (SUCCESS->OK->WARNING->ALERT->OFF)
    TEST_MODE_PWM_GRADUAL    // Frecuencias graduales (0x10->0x18->0x1F)
  };
  
  // DEVICE STATUS STATES (for 2-bit mask)
  enum DeviceStatus {
    DEVICE_OFFLINE = 0b00,      // Not present or powered off
    DEVICE_ONLINE = 0b01,       // Responding normally
    DEVICE_ERROR = 0b10,        // Timeout or communication error
    DEVICE_UNKNOWN = 0b11       // Not yet scanned
  };

  struct DeviceMap {
    uint8_t id_to_address[MAX_DEVICES];   // ID 1-32 -> I2C addr
    uint8_t address_to_id[256];           // I2C addr -> ID (rev)
    int     mapped_count;
  };

  struct DeviceInfo {
    uint8_t   address;
    uint8_t   assigned_id;         // 1-32 (0 = unassigned)
    bool      is_compatible;       // PY32F003 firmware compatible
    bool      is_online;
    String    status;
    
    // Test statistics
    uint32_t  total_tests;
    uint32_t  successful_tests;
    uint32_t  failed_tests;
    uint32_t  last_success_time;
    uint32_t  last_failure_time;
    uint8_t   consecutive_failures;
    
    // Last sensor readings
    uint16_t  last_adc_value;
    uint8_t   last_pa4_state;
    
    // Device identification
    uint8_t   device_id;           // F16/F18
    uint8_t   flash_size;          // KB
    uint32_t  uid;                 // Unique ID
    
    // Health tracking
    uint32_t  last_health_check;   // Timestamp of last health check
    bool      is_problematic;      // Device causing bus issues
    uint8_t   timeout_count;       // Number of timeout events
    uint32_t  avg_response_time;   // Average response time (ms)
  };

  // Legacy alias for compatibility
  typedef DeviceInfo DeviceStats;

public:
  // ========= Lifecycle =========
  MapController();
  void begin(TwoWire &wire = Wire,
             int sda = DEFAULT_SDA,
             int scl = DEFAULT_SCL,
             uint32_t i2c_freq = DEFAULT_I2C_FREQ);

  // ======= I2C Management =======
  String initializeI2C(int print = 0);
  String scanDevices(int print = 0);
  String fastScanDevices(int print = 0);  // Optimized fast scan
  String listDevices(int print = 0);
  String pingDevice(uint8_t address, int print = 0);
  void   setScannedDevices(DeviceInfo* list, int count);
  bool   recoverI2CBus();
  
  // ======= Health Monitoring =======
  void   performHealthCheck();
  bool   isDeviceHealthy(uint8_t index);
  String showHealthReport(int print = 0);
  void   markDeviceProblematic(uint8_t index, bool problematic);
  
  // ======= Async I2C Management =======
  void   updateAsyncI2C();                    // Call in loop() for async operations
  void   performKeepalive();                  // Automatic bus keepalive
  bool   shouldPerformKeepalive();            // Check if keepalive is needed
  void   resetIdleTimer();                    // Reset idle timer on I2C activity
  uint32_t getTimeSinceLastActivity() const;  // Get time since last I2C operation
  
  // ======= Rate Limiting & Protection =======
  bool   canSendCommand(uint8_t address) const; // Check if command can be sent now
  void   enforceRateLimit(uint8_t address);   // Enforce delays between commands
  void   markCommandSent(uint8_t address);    // Record command timestamp
  void   markCommandError(uint8_t address);   // Record error for cooldown
  
  // ========= Device Management =========
  bool   checkCompatibility(uint8_t address);
  String showDeviceInfo(uint8_t address, int print = 0);
  String showDeviceUID(uint8_t address, int print = 0);
  uint8_t parseAddress(String addr_str);

  // ========= Mapping (CRUD) =========
  String initializeMapping(int print = 0);
  String saveMapping(int print = 0);
  String loadMapping(int print = 0);
  String clearMapping(int print = 0);

  bool    assignDeviceToID(uint8_t id, uint8_t address); // lógica pura (sin imprimir)
  bool    moveDevice(uint8_t from_id, uint8_t to_id);    // idem
  bool    removeDeviceFromID(uint8_t id);                // idem
  uint8_t getAddressByID(uint8_t id) const;
  uint8_t getIDByAddress(uint8_t address) const;

  // ========= Mapping commands (console-style helpers) =========
  String cmdShowMapping(int print = 0);
  String cmdAutoMap(int print = 0);
  String cmdAssign(const String& cmd, int print = 0);
  String cmdMove(const String& cmd, int print = 0);
  String cmdRemove(const String& cmd, int print = 0);
  String cmdClearMapping(int print = 0);

  // ========= CONTINUOUS TESTING =========
  void    startContinuousTest(TestMode mode = TEST_MODE_PA4_ONLY);
  void    stopContinuousTest();
  void    runContinuousTest();
  void    testDevice(uint8_t index);
  String  showStatistics(int print = 0);
  void    resetStatistics();
  void    setTestInterval(uint32_t interval_ms);
  uint32_t getTestInterval() const { return test_interval_; }
  TestMode getTestMode() const { return current_test_mode_; }
  bool    isContinuousTestActive() const { return continuous_test_; }
  uint32_t getCycleCount() const { return cycle_count_; }
  
  // Test interval effects
  String  testIntervalEffect(int print = 0);
  String  testLongIntervalProblem(int print = 0);

  // ========= DEVICE COMMANDS =========
  // Basic relay control
  bool    sendBasicCommand(uint8_t address, uint8_t command);
  String  cmdRelay(uint8_t address, bool state, int print = 0);
  String  cmdToggle(uint8_t address, int print = 0);
  
  // NeoPixel control
  String  cmdNeoPixel(uint8_t address, uint8_t color, int print = 0);
  String  cmdNeoRed(uint8_t address, int print = 0);
  String  cmdNeoGreen(uint8_t address, int print = 0);
  String  cmdNeoBlue(uint8_t address, int print = 0);
  String  cmdNeoWhite(uint8_t address, int print = 0);
  String  cmdNeoOff(uint8_t address, int print = 0);
  
  // PWM/Buzzer control
  String  cmdPWM(uint8_t address, uint8_t level, int print = 0);
  String  cmdPWMOff(uint8_t address, int print = 0);
  String  cmdPWM25(uint8_t address, int print = 0);
  String  cmdPWM50(uint8_t address, int print = 0);
  String  cmdPWM75(uint8_t address, int print = 0);
  String  cmdPWM100(uint8_t address, int print = 0);
  
  // Musical tones control (v6.0.0_lite)
  String  cmdTone(uint8_t address, uint8_t tone_cmd, int print = 0);
  String  cmdToneDo(uint8_t address, int print = 0);
  String  cmdToneRe(uint8_t address, int print = 0);
  String  cmdToneMi(uint8_t address, int print = 0);
  String  cmdToneFa(uint8_t address, int print = 0);
  String  cmdToneSol(uint8_t address, int print = 0);
  String  cmdToneLa(uint8_t address, int print = 0);
  String  cmdToneSi(uint8_t address, int print = 0);
  
  // System alerts control (v6.0.0_lite)
  String  cmdSuccess(uint8_t address, int print = 0);
  String  cmdOk(uint8_t address, int print = 0);
  String  cmdWarning(uint8_t address, int print = 0);
  String  cmdAlert(uint8_t address, int print = 0);
  
  // PWM gradual frequencies (0x10-0x1F)
  String  cmdPWMGradual(uint8_t address, uint8_t freq_cmd, int print = 0);
  
  // Sensor reading
  uint8_t  readPA4Digital(uint8_t address);
  uint16_t readADC(uint8_t address);
  
  // Bus status mask
  String  cmdBusMask(int print = 0);              // Show 32-bit mask of bus status
  String  cmdBusMaskPA4(int print = 0);           // Show PA4 states as mask
  uint32_t getBusStatusMask();                     // Get 32-bit presence mask
  uint64_t getBusStatusMask2Bit();                 // Get 64-bit status (2 bits per device)
  String   cmdReadPA4(uint8_t address, int print = 0);
  String   cmdReadADC(uint8_t address, int print = 0);
  
  // Device testing
  String  testAllCommands(uint8_t address, int print = 0);
  String  testBothCommands(uint8_t address, int print = 0);

  // ========= I2C ADDRESS MANAGEMENT =========
  String  changeI2CAddress(uint8_t old_addr, uint8_t new_addr, int print = 0);
  String  factoryReset(uint8_t address, int print = 0);
  String  checkI2CStatus(uint8_t address, int print = 0);
  String  resetDevice(uint8_t address, int print = 0);
  String  resetAllDevices(int print = 0);
  String  verifyAddressChange(uint8_t old_addr, uint8_t new_addr, int print = 0);
  
  // Low-level I2C management
  bool    setDeviceI2CAddress(uint8_t current_addr, uint8_t new_addr);
  bool    resetDeviceToFactory(uint8_t addr);
  bool    getDeviceI2CStatus(uint8_t addr, uint8_t* current_addr, uint8_t* factory_addr, bool* is_from_flash);
  uint8_t parseI2CAddress(String s);
  bool    checkDeviceCompatibility(uint8_t address);
  String  debugResetDevice(uint8_t address, int print = 0);
  String  testDeviceCommands(uint8_t addr, int print = 0);
  
  // Command processing helpers
  String  cmdSetI2CAddress(String cmd, int print = 0);
  String  cmdResetFactory(String cmd, int print = 0);
  String  cmdI2CStatus(String cmd, int print = 0);
  String  cmdDebugReset(String cmd, int print = 0);
  String  cmdVerifyChange(String cmd, int print = 0);
  String  cmdTestCommands(String cmd, int print = 0);

  // Save default NeoPixel colors (slots 1..3 user-facing)
  bool    saveColorPreset(uint8_t address, uint8_t slot, uint8_t r, uint8_t g, uint8_t b);
  String  cmdSaveColor(String cmd, int print = 0);

  // ========= MAPPING BY ID =========
  String   cmdSenseByID(int print = 0);
  String   cmdSenseMaskByID(int print = 0);
  String   cmdSenseMaskHexByID(int print = 0);
  uint8_t  readSenseState(uint8_t addr);
  const char* interpretSenseValue(uint8_t raw_value);
  
  // Sense array functions
  String   cmdSenseArrayMask(uint8_t array_index, int print = 0);
  String   cmdSenseArrayMaskByCommand(const String& cmd, int print = 0);
  String   cmdShowSenseArray(uint8_t array_index, int print = 0);
  String   cmdShowSenseArrayByCommand(const String& cmd, int print = 0);
  String   cmdFillSenseArray(uint8_t array_index, int print = 0);
  String   cmdFillSenseArrayByCommand(const String& cmd, int print = 0);
  String   cmdShowAllArrays(int print = 0);

  // NeoPixel by mapping
  String     cmdNeoByID(String cmd, int print = 0);
  String     setNeoPixelByID(uint8_t id, uint8_t cmd, int print = 0);
  String     setAllNeoPixels(uint8_t cmd, int print = 0);
  const char* getNeoColorName(uint8_t cmd);

  // Relay by mapping
  String cmdRelayByID(String cmd, int print = 0);
  String cmdRelayOff(int print = 0);
  String cmdAllShut(int print = 0);
  String relayPulse(uint8_t addr, uint32_t pulse_time_ms = 50, int print = 0);
  int    relayTurn(uint8_t addr, int pulse, int print = 0);
  int    relayToggle(uint8_t addr, int print = 0);

  // ADC by mapping
  float    adcToVoltage(uint16_t adc_value);
  String   cmdADCByID(String cmd, int print = 0);
  String   cmdADCAllByID(int print = 0);
  uint16_t readADCValue(uint8_t addr);

  // ========= COMMUNICATION HELPERS =========
  bool    i2cReadByteWithRetry(uint8_t address, uint8_t command, uint8_t* out, 
                               uint8_t attempts = 3, uint32_t wait_ms = 30);
  bool    sendCommandWithTimeout(uint8_t address, uint8_t command, uint32_t timeout_ms = 1000);
  bool    sendCommandSafe(uint8_t addr, uint8_t cmd);
  bool    sendCommandFast(uint8_t addr, uint8_t cmd);
  String  setAllRelays(uint8_t cmd, int print = 0);
  String  emergencyRelaysOff(int print = 0);
  String  setAllDevicesState(uint8_t led_cmd, uint8_t relay_cmd, int print = 0);

  // ========= UTILITY & TIMING =========
  uint32_t startTiming();
  String   recordTiming(const char* operation, uint32_t elapsed_ms, int print = 0);
  
  // ========= DEBUG & FLASH =========
  bool    saveDataToFlash(uint8_t addr, uint8_t data);
  uint8_t readDataFromFlash(uint8_t addr);
  String  cmdFlashSave(String cmd, int print = 0);
  String  cmdFlashRead(String cmd, int print = 0);

  // ========= DEVICE INFO =========
  String  getDeviceTypeName(uint8_t device_id);
  uint32_t getDeviceUID(uint8_t address);
  String  cmdGetDeviceInfo(uint8_t address, int print = 0);
  String  cmdGetDeviceUID(uint8_t address, int print = 0);

  // ========= Accessors =========
  const DeviceMap& map() const { return device_map_; }
  const DeviceInfo* getDevices() const { return found_devices_; }
  int getDeviceCount() const { return device_count_; }
  uint32_t getTotalBusRecoveries() const { return total_bus_recoveries_; }
  
  // Legacy compatibility
  const DeviceStats* getScannedDevices() const { return (DeviceStats*)found_devices_; }

private:
  // helpers para capturar/imprimir
  static void emit(String& out, int print, const String& s);
  static void emitf(String& out, int print, const char* fmt, ...);
  
  // Internal test helpers
  void testDevicePA4(uint8_t index);
  void testDeviceToggle(uint8_t index);
  void testDeviceBoth(uint8_t index);
  void testDeviceNeoCycle(uint8_t index);
  void testDevicePWMCycle(uint8_t index);
  void testDeviceMusicalScale(uint8_t index);
  void testDeviceAlerts(uint8_t index);
  void testDevicePWMGradual(uint8_t index);
  void testDeviceADC(uint8_t index);
  
  // Command parsing helpers (for console-style interface)
  void parseChangeCommand(String cmd, int print = 0);
  void parseVerifyCommand(String cmd, int print = 0);
  void parseRelayCommand(String cmd, int print = 0);
  void parseNeoCommand(String cmd, int print = 0);
  void parsePWMCommand(String cmd, int print = 0);

private:
  TwoWire*     wire_ = nullptr;
  Preferences  preferences_;
  DeviceMap    device_map_;
  uint32_t     last_operation_start_ = 0;

  // Device management
  DeviceInfo   found_devices_[MAX_DEVICES];
  int          device_count_ = 0;
  
  // Test system
  bool         continuous_test_ = false;
  TestMode     current_test_mode_ = TEST_MODE_PA4_ONLY;
  uint32_t     cycle_count_ = 0;
  uint32_t     test_interval_ = DEFAULT_TEST_INTERVAL;
  uint32_t     total_bus_recoveries_ = 0;
  
  // Async I2C management
  uint32_t     last_keepalive_ = 0;
  uint32_t     last_i2c_activity_ = 0;
  uint32_t     last_health_check_time_ = 0;
  bool         keepalive_enabled_ = true;
  uint8_t      keepalive_device_index_ = 0;  // Rotate through devices for keepalive
  
  // Rate limiting
  uint32_t     last_command_time_ = 0;          // Last command sent to any device
  uint8_t      last_command_address_ = 0;       // Last device addressed
  uint32_t     last_device_times_[128];         // Last command time per device
  uint32_t     last_error_time_ = 0;            // Last error occurrence
  bool         in_cooldown_ = false;            // In error cooldown period
  uint8_t      burst_command_count_ = 0;        // Commands in current burst

  // Config I2C recordada por begin()
  int       sda_ = DEFAULT_SDA;
  int       scl_ = DEFAULT_SCL;
  uint32_t  i2c_freq_ = DEFAULT_I2C_FREQ;

  // Test cycle state variables
  static uint8_t neo_step_;
  static uint8_t pwm_step_;
  static uint8_t musical_step_;
  static uint8_t alert_step_;
  static uint8_t gradual_step_;
  
  // Legacy support - external device list injection
  DeviceInfo* scanned_ = nullptr;
  int         scanned_count_ = 0;
};

// Legacy alias for backward compatibility
typedef MapController MapCRUD;
