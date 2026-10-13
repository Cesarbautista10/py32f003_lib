#include <Wire.h>
#include <stdlib.h>
#include <string.h>

constexpr uint8_t SLAVE_ADDRESS = 0x40;
constexpr uint8_t CMD_RED = 0x02;
constexpr uint8_t CMD_GREEN = 0x03;
constexpr uint8_t CMD_BLUE = 0x04;
constexpr uint8_t CMD_OFF = 0x05;
constexpr uint8_t CMD_WHITE = 0x08;
constexpr uint8_t CMD_FILL_RGB = 0x50;
constexpr uint8_t CMD_SET_PIXEL = 0x51;
constexpr uint8_t CMD_SHOW = 0x52;
constexpr uint8_t RESPONSE_WAIT_MS = 10;  // Covers a frame of up to 200 LEDs.

char inputLine[48];
uint8_t inputLength = 0;
bool discardLine = false;

void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(6, 7);  // SDA, SCL: same wiring as the working ADC and ON/OFF masters.
#else
  Wire.begin();
#endif
  Wire.setClock(100000);
}

bool sendCommand(const uint8_t *bytes, uint8_t length) {
  Wire.beginTransmission(SLAVE_ADDRESS);
  Wire.write(bytes, length);
  uint8_t status = Wire.endTransmission();  // STOP lets rgb_pa0 process the write.
  if (status != 0) {
    Serial.print(F("I2C write error: "));
    Serial.println(status);
    return false;
  }

  delay(RESPONSE_WAIT_MS);
  if (Wire.requestFrom(SLAVE_ADDRESS, (uint8_t)1) != 1) {
    Serial.println(F("I2C read error"));
    return false;
  }

  uint8_t response = Wire.read();
  if (response != bytes[0]) {
    Serial.print(F("RGB command rejected: 0x"));
    Serial.println(response, HEX);
    return false;
  }

  Serial.println(F("OK"));
  return true;
}

bool parseByte(const char *text, uint8_t &value) {
  char *end;
  unsigned long parsed = strtoul(text, &end, 10);
  if (text[0] == '\0' || end == text || *end != '\0' || parsed > 255UL) {
    return false;
  }
  value = (uint8_t)parsed;
  return true;
}

void printHelp() {
  Serial.println(F("Commands: red, green, blue, white, off"));
  Serial.println(F("rgb R G B          (0-255; fills and shows)"));
  Serial.println(F("pixel INDEX R G B  (zero-based; buffers only)"));
  Serial.println(F("show               (displays buffered pixels)"));
  Serial.println(F("demo, help"));
}

void processLine() {
  char *words[5];
  uint8_t count = 0;
  char *word = strtok(inputLine, " \t");

  while (word != nullptr) {
    if (count == 5) {
      Serial.println(F("Too many arguments"));
      return;
    }
    words[count++] = word;
    word = strtok(nullptr, " \t");
  }
  if (count == 0) {
    return;
  }

  if (count == 1) {
    uint8_t command;
    if (strcmp(words[0], "red") == 0) command = CMD_RED;
    else if (strcmp(words[0], "green") == 0) command = CMD_GREEN;
    else if (strcmp(words[0], "blue") == 0) command = CMD_BLUE;
    else if (strcmp(words[0], "white") == 0) command = CMD_WHITE;
    else if (strcmp(words[0], "off") == 0) command = CMD_OFF;
    else if (strcmp(words[0], "show") == 0) command = CMD_SHOW;
    else if (strcmp(words[0], "help") == 0) {
      printHelp();
      return;
    } else if (strcmp(words[0], "demo") == 0) {
      const uint8_t colors[] = {CMD_RED, CMD_GREEN, CMD_BLUE, CMD_WHITE, CMD_OFF};
      for (uint8_t i = 0; i < sizeof(colors); ++i) {
        if (!sendCommand(&colors[i], 1)) return;
        delay(500);
      }
      return;
    } else {
      printHelp();
      return;
    }
    (void)sendCommand(&command, 1);
    return;
  }

  if (count == 4 && strcmp(words[0], "rgb") == 0) {
    uint8_t command[4] = {CMD_FILL_RGB};
    if (parseByte(words[1], command[1]) &&
        parseByte(words[2], command[2]) &&
        parseByte(words[3], command[3])) {
      (void)sendCommand(command, sizeof(command));
    } else {
      Serial.println(F("Use: rgb R G B (0-255)"));
    }
    return;
  }

  if (count == 5 && strcmp(words[0], "pixel") == 0) {
    uint8_t command[5] = {CMD_SET_PIXEL};
    if (parseByte(words[1], command[1]) &&
        parseByte(words[2], command[2]) &&
        parseByte(words[3], command[3]) &&
        parseByte(words[4], command[4])) {
      (void)sendCommand(command, sizeof(command));
    } else {
      Serial.println(F("Use: pixel INDEX R G B (0-255)"));
    }
    return;
  }

  printHelp();
}

void setup() {
  Serial.begin(115200);
  beginI2C();
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    char character = (char)Serial.read();
    if (character == '\r') continue;
    if (character == '\n') {
      if (!discardLine) {
        inputLine[inputLength] = '\0';
        processLine();
      }
      inputLength = 0;
      discardLine = false;
    } else if (!discardLine) {
      if (inputLength < sizeof(inputLine) - 1) {
        inputLine[inputLength++] = character;
      } else {
        Serial.println(F("Line too long"));
        discardLine = true;
      }
    }
  }
}
