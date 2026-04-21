# Changelog - MapController Library

## [2.1.1] - 2025-11-14

### ⚠️ BREAKING CHANGES - Security Update v6.0.0_lite

#### Relay Commands Moved to Security Range (0xA0-0xAF)
**CRITICAL**: Relay commands relocated to prevent accidental activation from I2C bus corruption.

**Old Commands (DEPRECATED)**:
- `CMD_RELAY_OFF = 0x00` ❌
- `CMD_RELAY_ON = 0x01` ❌  
- `CMD_TOGGLE = 0x06` ❌

**New Commands (REQUIRED)**:
- `CMD_RELAY_OFF = 0xA0` ✅ (Lock/Close)
- `CMD_RELAY_ON = 0xA1` ✅ (Unlock/Open)
- `CMD_TOGGLE = 0xA6` ✅ (10ms pulse)

**Response Codes**:
- `RESP_RELAY_OFF = 0xB0`
- `RESP_RELAY_ON = 0xB1`
- `RESP_TOGGLE = 0xB6`

**Reason**: With 160 code separation between relay (0xA0) and read (0x07) commands, 1-2 corrupted bits in I2C cannot cause accidental door openings in multi-device (20+) bus configurations.

### 🎵 Added - Musical Tones (0x25-0x2B)

Complete musical scale implementation:
- `CMD_TONE_DO = 0x25` (261Hz)
- `CMD_TONE_RE = 0x26` (294Hz)
- `CMD_TONE_MI = 0x27` (330Hz)
- `CMD_TONE_FA = 0x28` (349Hz)
- `CMD_TONE_SOL = 0x29` (392Hz)
- `CMD_TONE_LA = 0x2A` (440Hz)
- `CMD_TONE_SI = 0x2B` (494Hz)
- Response: `RESP_TONE_SET = 0x0C`

**Library Methods**:
```cpp
cmdToneDo(address, print)
cmdToneRe(address, print)
cmdToneMi(address, print)
cmdToneFa(address, print)
cmdToneSol(address, print)
cmdToneLa(address, print)
cmdToneSi(address, print)
```

### 🔔 Added - System Alerts (0x2C-0x2F)

Predefined alert tones for system feedback:
- `CMD_SUCCESS = 0x2C` (800Hz) - Operation success
- `CMD_OK = 0x2D` (1000Hz) - Command acknowledged
- `CMD_WARNING = 0x2E` (1200Hz) - Attention required
- `CMD_ALERT = 0x2F` (1500Hz) - Critical error

**Response Codes**:
- `RESP_SUCCESS = 0x0D`
- `RESP_OK = 0x0E`
- `RESP_WARNING = 0x0F`
- `RESP_ALERT = 0x0A`

**Library Methods**:
```cpp
cmdSuccess(address, print)
cmdOk(address, print)
cmdWarning(address, print)
cmdAlert(address, print)
```

### 📈 Added - PWM Gradual Frequencies (0x10-0x1F)

Programmable frequency range for precise tone control:
- Range: `CMD_PWM_GRADUAL_MIN (0x10)` to `CMD_PWM_GRADUAL_MAX (0x1F)`
- Frequency formula: `freq = 100 + ((cmd - 0x10) * 900 / 16)`
- Examples:
  - `0x10` = 100Hz (low tone)
  - `0x18` = ~550Hz (mid tone)
  - `0x1F` = 1000Hz (high tone)

**Library Method**:
```cpp
cmdPWMGradual(address, freq_cmd, print)
```

### 🧪 Added - New Test Modes

Three additional continuous test modes:
1. `TEST_MODE_MUSICAL_SCALE` - Cycles through DO-RE-MI-FA-SOL-LA-SI scale
2. `TEST_MODE_ALERTS` - Cycles through SUCCESS→OK→WARNING→ALERT alerts
3. `TEST_MODE_PWM_GRADUAL` - Tests gradual frequency sweep (100Hz→550Hz→1000Hz)

**Usage**:
```cpp
deviceManager.startContinuousTest(MapController::TEST_MODE_MUSICAL_SCALE);
deviceManager.startContinuousTest(MapController::TEST_MODE_ALERTS);
deviceManager.startContinuousTest(MapController::TEST_MODE_PWM_GRADUAL);
```

### 📝 Updated - Documentation

- README.md updated with v6.0.0_lite command reference
- Quick reference table for all I2C commands
- Security notes explaining relay command separation
- Example code updated with new features

### 📦 Updated - Example Sketch

Standalone.ino now includes:
- Musical tone commands: `tone 0x20 do`, `tone 0x20 la`
- Alert commands: `alert 0x20 ok`, `alert 0x20 warn`
- Test commands: `test-start music`, `test-start alerts`, `test-start gradual`
- Warning banner about v6.0.0_lite changes

### 🔧 Internal Changes

- Added static variables for new test cycle tracking
- Implemented `testDeviceMusicalScale()`, `testDeviceAlerts()`, `testDevicePWMGradual()`
- Extended command switch cases in `testDevice()`
- Added parse functions: `parseToneCommand()`, `parseAlertCommand()`

---

## [2.1.0] - 2024-10-XX

### Added
- Continuous testing system with multiple modes
- Real-time statistics tracking
- CSV output format for data logging
- I2C bus auto-recovery
- Device information retrieval (UID, Flash size, Device ID)
- NeoPixel control commands
- PWM/Buzzer control (5 levels)
- ADC reading (12-bit PA0)
- Digital input reading (PA4)

### Changed
- Refactored from MapCRUD v1.x to MapController
- Improved I2C timing and reliability
- Enhanced error handling and recovery

---

## Migration Guide v2.1.0 → v2.1.1

### For Firmware Developers (PY32F003)
**CRITICAL**: Update firmware to v6.0.0_lite or newer to support new command ranges.

### For Library Users (ESP32/Arduino)

#### Step 1: Update Relay Command Calls
```cpp
// OLD (will not work with v6.0.0_lite firmware)
sendBasicCommand(addr, 0x00); // RELAY_OFF
sendBasicCommand(addr, 0x01); // RELAY_ON
sendBasicCommand(addr, 0x06); // TOGGLE

// NEW (required for v6.0.0_lite firmware)
sendBasicCommand(addr, MapController::CMD_RELAY_OFF);  // 0xA0
sendBasicCommand(addr, MapController::CMD_RELAY_ON);   // 0xA1
sendBasicCommand(addr, MapController::CMD_TOGGLE);     // 0xA6
```

#### Step 2: Update Test Modes (Optional)
```cpp
// If using toggle testing:
startContinuousTest(TEST_MODE_TOGGLE_ONLY); // Already updated internally
```

#### Step 3: Explore New Features (Optional)
```cpp
// Musical tones
deviceManager.cmdToneDo(0x20, 1);
deviceManager.cmdToneLa(0x20, 1);

// System alerts
deviceManager.cmdSuccess(0x20, 1);
deviceManager.cmdWarning(0x20, 1);

// PWM gradual
deviceManager.cmdPWMGradual(0x20, 0x10, 1); // 100Hz
deviceManager.cmdPWMGradual(0x20, 0x1F, 1); // 1000Hz
```

### Backward Compatibility

- ✅ All non-relay commands remain unchanged
- ✅ NeoPixel, PWM, ADC, PA4 commands fully compatible
- ✅ I2C address management unchanged
- ⚠️ Firmware v5.x devices will not respond to new 0xA0-0xAF relay commands
- ⚠️ Library v2.1.1 sending 0xA0-0xAF to firmware v5.x will result in no response

### Recommended Update Path

1. **Test Environment**: Update 1-2 devices to firmware v6.0.0_lite
2. **Verify**: Test relay commands work correctly (0xA0, 0xA1, 0xA6)
3. **Update Library**: Deploy MapController v2.1.1 to all ESP32 controllers
4. **Roll Out**: Update remaining devices to v6.0.0_lite firmware
5. **Validate**: Run continuous tests to verify operation

---

## Support & Issues

- **Library**: [GitHub Issues](https://github.com/unit-electronics/MapController/issues)
- **Firmware**: Contact firmware maintainer for v6.0.0_lite compatibility
- **Documentation**: See [README.md](README.md) for complete command reference

---

**Last Updated**: November 14, 2025  
**Compatible Firmware**: PY32F003 Locknode v6.0.0_lite or newer
