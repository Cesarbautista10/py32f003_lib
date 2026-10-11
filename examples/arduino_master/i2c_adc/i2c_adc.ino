#include <Wire.h>

constexpr uint8_t SLAVE_ADDRESS = 0x20;
constexpr uint8_t REG_ADC_RAW = 0x01;

void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(6, 7);  // SDA, SCL
#else
  Wire.begin();
#endif
  Wire.setClock(100000);
}

bool readAdc(uint16_t &value) {
  // Primero se envía un byte: el registro que se quiere leer.
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(REG_ADC_RAW);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(1);  // El esclavo prepara los dos bytes de respuesta.
  if (Wire.requestFrom(SLAVE_ADDRESS, (uint8_t)2) != 2) {
    return false;
  }

  uint8_t lowByte = Wire.read();
  uint8_t highByte = Wire.read();
  value = (uint16_t)lowByte | ((uint16_t)highByte << 8);
  return true;
}

void setup() {
  Serial.begin(115200);
  beginI2C();
}

void loop() {
  uint16_t adc;
  if (readAdc(adc)) {
    Serial.println(adc);
  } else {
    Serial.println("I2C error");
  }
  delay(500);
}
