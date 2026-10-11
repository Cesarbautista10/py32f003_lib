#include <Wire.h>

constexpr uint8_t SLAVE_ADDRESS = 0x20;
constexpr uint8_t REG_OUTPUT = 0x02;

void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(6, 7);  // SDA, SCL
#else
  Wire.begin();
#endif
  Wire.setClock(100000);
}

bool writeOutput(bool enabled) {
  // Dos bytes: registro y nuevo estado.
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(REG_OUTPUT);
  Wire.write(enabled ? 1 : 0);
  return Wire.endTransmission() == 0;
}

bool readOutput(uint8_t &state) {
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(REG_OUTPUT);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(1);
  if (Wire.requestFrom(SLAVE_ADDRESS, (uint8_t)1) != 1) {
    return false;
  }
  state = Wire.read();
  return true;
}

void setup() {
  Serial.begin(115200);
  beginI2C();
}

void loop() {
  static bool enabled = false;
  uint8_t state;

  enabled = !enabled;
  if (writeOutput(enabled)) {
    delay(1);
    if (readOutput(state)) {
      Serial.println(state ? "ON" : "OFF");
    }
  } else {
    Serial.println("I2C error");
  }
  delay(1000);
}
