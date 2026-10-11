#include <Wire.h>

constexpr uint8_t SLAVE_ADDRESS = 0x20;
constexpr uint8_t REG_PWM_PERCENT = 0x03;

void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(6, 7);  // SDA, SCL
#else
  Wire.begin();
#endif
  Wire.setClock(100000);
}

bool writePwm(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  // Dos bytes: registro y porcentaje de PWM.
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(REG_PWM_PERCENT);
  Wire.write(percent);
  return Wire.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  beginI2C();
}

void loop() {
  for (uint8_t percent = 0; percent <= 100; percent += 10) {
    if (!writePwm(percent)) {
      Serial.println("I2C error");
    } else {
      Serial.println(percent);
    }
    delay(250);
  }
}
