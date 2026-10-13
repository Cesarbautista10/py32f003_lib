#include <Wire.h>
#include <stdlib.h>
#include <string.h>

constexpr uint8_t CMD_IDENTIFY = 0x00;
constexpr uint8_t CMD_ADC_RAW = 0x01;
constexpr uint8_t CMD_ON_OFF = 0x02;
constexpr uint8_t CMD_PWM = 0x03;
constexpr uint8_t CMD_RGB_RED = 0x02;
constexpr uint8_t CMD_RGB_GREEN = 0x03;
constexpr uint8_t CMD_RGB_BLUE = 0x04;
constexpr uint8_t CMD_RGB_OFF = 0x05;
constexpr uint8_t CMD_RGB_WHITE = 0x08;
constexpr uint8_t CMD_RGB_FILL = 0x50;
constexpr uint8_t CMD_RGB_PIXEL = 0x51;
constexpr uint8_t CMD_RGB_SHOW = 0x52;

constexpr uint8_t TYPE_ADC = 0x01;
constexpr uint8_t TYPE_ON_OFF = 0x02;
constexpr uint8_t TYPE_PWM = 0x03;
constexpr uint8_t TYPE_NEOPIXEL = 0x04;

constexpr uint8_t MODEL_NONE = 0x00;
constexpr uint8_t MODEL_NEOPIXEL_PA0 = 0x01;

constexpr uint8_t FIRST_I2C_ADDRESS = 0x08;
constexpr uint8_t LAST_I2C_ADDRESS = 0x77;
constexpr uint8_t MAX_NODES = 32;
constexpr uint8_t MAX_LINE_LENGTH = 63;
constexpr uint8_t I2C_TIMEOUT_MS = 25;
constexpr uint8_t RGB_RESPONSE_WAIT_MS = 10;
constexpr uint8_t MAX_SCAN_BUS_ERRORS = 3;
constexpr uint8_t MAX_STARTUP_PASSES = 4;
constexpr uint16_t STARTUP_RETRY_DELAY_MS = 500;

enum ProbeResult { PROBE_PRESENT, PROBE_ABSENT, PROBE_BUS_ERROR };

struct Node {
  uint8_t address;
  uint8_t type;
  uint8_t model;
};

Node nodeMap[MAX_NODES];
uint8_t nodeCount = 0;
char inputLine[MAX_LINE_LENGTH + 1];
uint8_t inputLength = 0;
uint8_t lastProbeStatus = 0;

void printAddress(uint8_t address) {
  Serial.print(F("0x"));
  if (address < 0x10) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
}

void printHexByte(uint8_t value) {
  Serial.print(F("0x"));
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

const char *typeName(uint8_t type) {
  switch (type) {
    case TYPE_ADC: return "ADC";
    case TYPE_ON_OFF: return "ON_OFF";
    case TYPE_PWM: return "PWM";
    case TYPE_NEOPIXEL: return "NEOPIXEL";
    default: return "UNKNOWN";
  }
}

const char *modelName(uint8_t type, uint8_t model) {
  if (type == TYPE_ADC) {
    return "analog input";
  }
  if (type == TYPE_NEOPIXEL && model == MODEL_NEOPIXEL_PA0) {
    return "PA0";
  }
  if (type == TYPE_NEOPIXEL && model == MODEL_NONE) {
    return "PA0 (legacy)";
  }
  if (model == MODEL_NONE) {
    return "generic";
  }
  return "unknown";
}

bool beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  if (!Wire.begin(6, 7)) {  // SDA, SCL
    return false;
  }
  Wire.setTimeOut(I2C_TIMEOUT_MS);
#else
  Wire.begin();
#endif
  Wire.setClock(100000);
  return true;
}

bool restartI2C() {
  Wire.end();
  delay(5);
  return beginI2C();
}

ProbeResult pingAddress(uint8_t address) {
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    Wire.beginTransmission(address);
    uint8_t status = Wire.endTransmission();
    lastProbeStatus = status;
    if (status == 0) {
      return PROBE_PRESENT;
    }
    if (status == 2) {  // Address NACK is expected for an unused address.
      return PROBE_ABSENT;
    }
    if (!restartI2C()) {
      return PROBE_BUS_ERROR;
    }
  }
  return PROBE_BUS_ERROR;
}

void probeAddress(uint8_t address) {
  ProbeResult result = pingAddress(address);
  printAddress(address);
  if (result == PROBE_PRESENT) {
    Serial.println(F(": ACK"));
  } else if (result == PROBE_ABSENT) {
    Serial.println(F(": NACK (no device responded)"));
  } else {
    Serial.print(F(": bus error (Wire status "));
    Serial.print(lastProbeStatus);
    Serial.println(')');
  }
}

bool readRegister(uint8_t address, uint8_t command, uint8_t *data,
                  uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(command);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(2);
  uint8_t received = Wire.requestFrom(address, length);
  if (received != length) {
    while (Wire.available()) {
      (void)Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < length; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

bool writeRegister(uint8_t address, uint8_t command, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(command);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool identifyNode(uint8_t address, Node &node) {
  uint8_t identity[2];
  bool readOk = readRegister(address, CMD_IDENTIFY, identity, sizeof(identity));

  if (readOk && identity[0] >= TYPE_ADC && identity[0] <= TYPE_NEOPIXEL) {
    node.address = address;
    node.type = identity[0];
    node.model = identity[1];
    return true;
  }

  if (readOk) {
    Serial.print(F("Unknown identity at "));
    printAddress(address);
    Serial.print(F(": "));
    printHexByte(identity[0]);
    Serial.print(' ');
    printHexByte(identity[1]);
    Serial.println();
  } else {
    Serial.print(F("Identity read failed at "));
    printAddress(address);
    Serial.println();
  }
  return false;
}

void clearMap() {
  nodeCount = 0;
  Serial.println(F("Map cleared"));
}

void printMap() {
  if (nodeCount == 0) {
    Serial.println(F("Map empty. Run: automap"));
    return;
  }

  for (uint8_t i = 0; i < nodeCount; ++i) {
    Serial.print(i + 1);
    Serial.print(F(": "));
    printAddress(nodeMap[i].address);
    Serial.print(F("  "));
    Serial.print(typeName(nodeMap[i].type));
    Serial.print(F("  "));
    Serial.println(modelName(nodeMap[i].type, nodeMap[i].model));
  }
}

bool rememberNode(const Node &node) {
  for (uint8_t i = 0; i < nodeCount; ++i) {
    if (nodeMap[i].address == node.address) {
      nodeMap[i] = node;
      return true;
    }
  }
  if (nodeCount == MAX_NODES) {
    return false;
  }
  nodeMap[nodeCount++] = node;
  return true;
}

bool scanBusOnce(bool *present, uint8_t &found) {
  uint8_t busErrors = 0;

  for (uint8_t address = FIRST_I2C_ADDRESS;
       address <= LAST_I2C_ADDRESS; ++address) {
    ProbeResult result = pingAddress(address);
    if (result == PROBE_PRESENT) {
      if (!present[address]) {
        present[address] = true;
        ++found;
      }
      busErrors = 0;
    } else if (result == PROBE_BUS_ERROR) {
      if (++busErrors >= MAX_SCAN_BUS_ERRORS) {
        Serial.print(F("I2C bus error at "));
        printAddress(address);
        Serial.print(F(" (Wire status "));
        Serial.print(lastProbeStatus);
        Serial.println(')');
        return false;
      }
    } else {
      busErrors = 0;
    }
    delay(1);
  }
  return true;
}

void scanBus() {
  bool present[128] = {};
  uint8_t found = 0;
  bool completed = false;
  bool hadBusError = false;
  Serial.println(F("Scanning I2C..."));
  for (uint8_t pass = 0; pass < MAX_STARTUP_PASSES; ++pass) {
    bool complete = scanBusOnce(present, found);
    completed |= complete;
    hadBusError |= !complete;
    if (complete && found != 0 && pass >= 1) {
      break;
    }
    if (pass + 1 < MAX_STARTUP_PASSES) {
      if (!complete || found == 0) {
        Serial.println(complete ? F("No devices yet; restarting I2C...")
                                : F("I2C bus error; restarting I2C..."));
        (void)restartI2C();
      }
      delay(found == 0 ? STARTUP_RETRY_DELAY_MS : 300);
    }
  }

  for (uint8_t address = FIRST_I2C_ADDRESS;
       address <= LAST_I2C_ADDRESS; ++address) {
    if (present[address]) {
      printAddress(address);
      Serial.println();
    }
  }
  Serial.print(F("Found: "));
  Serial.println(found);
  if (!completed) {
    Serial.println(F("I2C bus error: both scan passes stopped early"));
  } else if (hadBusError) {
    Serial.println(F("I2C bus error: results may be incomplete"));
  }
}

bool autoMapOnce(Node *nodes, uint8_t &count) {
  uint8_t busErrors = 0;
  for (uint8_t address = FIRST_I2C_ADDRESS;
       address <= LAST_I2C_ADDRESS; ++address) {
    ProbeResult result = pingAddress(address);
    if (result == PROBE_BUS_ERROR) {
      if (++busErrors >= MAX_SCAN_BUS_ERRORS) {
        Serial.print(F("I2C bus error at "));
        printAddress(address);
        Serial.print(F(" (Wire status "));
        Serial.print(lastProbeStatus);
        Serial.println(')');
        return false;
      }
    } else {
      busErrors = 0;
      if (result == PROBE_PRESENT) {
        Node node;
        if (identifyNode(address, node)) {
          bool alreadyMapped = false;
          for (uint8_t i = 0; i < count; ++i) {
            if (nodes[i].address == address) {
              alreadyMapped = true;
              break;
            }
          }
          if (!alreadyMapped && count < MAX_NODES) {
            nodes[count++] = node;
          }
        }
      }
    }
    delay(1);
  }
  return true;
}

void autoMap() {
  Node discovered[MAX_NODES];
  uint8_t count = 0;
  bool completed = false;
  bool hadBusError = false;
  Serial.println(F("Mapping workshop nodes..."));
  for (uint8_t pass = 0; pass < MAX_STARTUP_PASSES; ++pass) {
    bool complete = autoMapOnce(discovered, count);
    completed |= complete;
    hadBusError |= !complete;
    if (complete && count != 0 && pass >= 1) {
      break;
    }
    if (pass + 1 < MAX_STARTUP_PASSES) {
      if (!complete || count == 0) {
        Serial.println(complete ? F("No workshop nodes yet; restarting I2C...")
                                : F("I2C bus error; restarting I2C..."));
        (void)restartI2C();
      }
      delay(count == 0 ? STARTUP_RETRY_DELAY_MS : 300);
    }
  }

  if (completed && count != 0) {
    for (uint8_t i = 0; i < count; ++i) {
      (void)rememberNode(discovered[i]);
    }
    printMap();
    if (hadBusError) {
      Serial.println(F("I2C bus error: map may be incomplete"));
    }
  } else if (completed) {
    Serial.println(F("No workshop nodes found; previous map kept"));
  } else {
    Serial.println(F("I2C bus error: map kept after repeated failures"));
  }
}

bool parseByte(const char *text, uint8_t minimum, uint8_t maximum,
               uint8_t &value) {
  char *end = nullptr;
  unsigned long parsed = strtoul(text, &end, 0);

  if (text[0] == '\0' || end == text || *end != '\0' ||
      parsed < minimum || parsed > maximum) {
    return false;
  }
  value = (uint8_t)parsed;
  return true;
}

void addManualNeopixel(const char *addressText) {
  uint8_t address;
  if (!parseByte(addressText, FIRST_I2C_ADDRESS, LAST_I2C_ADDRESS, address)) {
    Serial.println(F("Invalid I2C address. Use: map add 0x40 neopixel"));
    return;
  }
  Node node = {address, TYPE_NEOPIXEL, MODEL_NONE};
  if (!rememberNode(node)) {
    Serial.println(F("Map full"));
    return;
  }
  printMap();
}

int8_t firstNodeOfType(uint8_t type) {
  for (uint8_t i = 0; i < nodeCount; ++i) {
    if (nodeMap[i].type == type) {
      return (int8_t)i;
    }
  }
  return -1;
}

bool resolveNode(const char *text, uint8_t expectedType, uint8_t &address) {
  uint8_t value;

  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    if (!parseByte(text, FIRST_I2C_ADDRESS, LAST_I2C_ADDRESS, value)) {
      Serial.println(F("Invalid I2C address"));
      return false;
    }
    address = value;
    return true;
  }

  if (!parseByte(text, 1, nodeCount, value)) {
    Serial.println(F("Invalid map index. Use map or an address such as 0x20"));
    return false;
  }

  const Node &node = nodeMap[value - 1];
  if (node.type != expectedType) {
    Serial.print(F("Node is "));
    Serial.print(typeName(node.type));
    Serial.print(F(", not "));
    Serial.println(typeName(expectedType));
    return false;
  }
  address = node.address;
  return true;
}

bool firstAddressOfType(uint8_t type, uint8_t &address) {
  int8_t index = firstNodeOfType(type);
  if (index < 0) {
    Serial.print(F("No "));
    Serial.print(typeName(type));
    Serial.println(F(" node in map. Run: automap"));
    return false;
  }
  address = nodeMap[index].address;
  return true;
}

void readAdcAt(uint8_t address) {
  uint8_t data[2];

  if (!readRegister(address, CMD_ADC_RAW, data, sizeof(data))) {
    Serial.println(F("ADC I2C error"));
    return;
  }

  uint16_t raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  printAddress(address);
  Serial.print(F(" ADC="));
  Serial.println(raw);
}

void handleAdc(char *argument) {
  if (argument != nullptr) {
    uint8_t address;
    if (resolveNode(argument, TYPE_ADC, address)) {
      readAdcAt(address);
    }
    return;
  }

  bool found = false;
  for (uint8_t i = 0; i < nodeCount; ++i) {
    if (nodeMap[i].type == TYPE_ADC) {
      readAdcAt(nodeMap[i].address);
      found = true;
    }
  }
  if (!found) {
    Serial.println(F("No ADC nodes in map. Run: automap"));
  }
}

void handleOn(char *first, char *second) {
  uint8_t address;
  uint8_t state;

  if (first == nullptr) {
    Serial.println(F("Usage: on <0|1> OR on <node|0xADDR> <0|1>"));
    return;
  }

  if (second == nullptr) {
    if (!parseByte(first, 0, 1, state) ||
        !firstAddressOfType(TYPE_ON_OFF, address)) {
      return;
    }
  } else {
    if (!resolveNode(first, TYPE_ON_OFF, address) ||
        !parseByte(second, 0, 1, state)) {
      Serial.println(F("State must be 0 or 1"));
      return;
    }
  }

  if (!writeRegister(address, CMD_ON_OFF, state)) {
    Serial.println(F("ON/OFF I2C error"));
    return;
  }
  printAddress(address);
  Serial.println(state ? F(" ON") : F(" OFF"));
}

void handlePwm(char *first, char *second) {
  uint8_t address;
  uint8_t percent;

  if (first == nullptr) {
    Serial.println(F("Usage: pwm <0-100> OR pwm <node|0xADDR> <0-100>"));
    return;
  }

  if (second == nullptr) {
    if (!parseByte(first, 0, 100, percent) ||
        !firstAddressOfType(TYPE_PWM, address)) {
      return;
    }
  } else {
    if (!resolveNode(first, TYPE_PWM, address) ||
        !parseByte(second, 0, 100, percent)) {
      Serial.println(F("PWM must be between 0 and 100"));
      return;
    }
  }

  if (!writeRegister(address, CMD_PWM, percent)) {
    Serial.println(F("PWM I2C error"));
    return;
  }
  printAddress(address);
  Serial.print(F(" PWM="));
  Serial.print(percent);
  Serial.println('%');
}

bool sendRgbCommand(uint8_t address, const uint8_t *bytes, uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(bytes, length);
  uint8_t status = Wire.endTransmission();
  if (status != 0) {
    printAddress(address);
    Serial.print(F(" RGB I2C write error (Wire status "));
    Serial.print(status);
    Serial.println(')');
    return false;
  }

  delay(RGB_RESPONSE_WAIT_MS);
  if (Wire.requestFrom(address, (uint8_t)1) != 1) {
    Serial.println(F("RGB I2C read error"));
    return false;
  }

  uint8_t response = Wire.read();
  if (response != bytes[0]) {
    Serial.print(F("RGB rejected command: 0x"));
    Serial.println(response, HEX);
    return false;
  }
  printAddress(address);
  Serial.println(F(" RGB OK"));
  return true;
}

void handleRgb(const char *command, char **args, uint8_t count) {
  uint8_t bytes[5];
  uint8_t length = 1;
  uint8_t valueCount = 0;

  if (strcmp(command, "red") == 0) bytes[0] = CMD_RGB_RED;
  else if (strcmp(command, "green") == 0) bytes[0] = CMD_RGB_GREEN;
  else if (strcmp(command, "blue") == 0) bytes[0] = CMD_RGB_BLUE;
  else if (strcmp(command, "white") == 0) bytes[0] = CMD_RGB_WHITE;
  else if (strcmp(command, "off") == 0) bytes[0] = CMD_RGB_OFF;
  else if (strcmp(command, "show") == 0) bytes[0] = CMD_RGB_SHOW;
  else if (strcmp(command, "demo") == 0) bytes[0] = CMD_RGB_RED;
  else if (strcmp(command, "rgb") == 0) {
    bytes[0] = CMD_RGB_FILL;
    valueCount = 3;
    length = 4;
  } else {
    bytes[0] = CMD_RGB_PIXEL;
    valueCount = 4;
    length = 5;
  }

  if (count != valueCount && count != valueCount + 1) {
    Serial.println(F("RGB usage: red|green|blue|white|off|show|demo [node|0xADDR], rgb [node|0xADDR] R G B, pixel [node|0xADDR] index R G B"));
    return;
  }

  uint8_t address;
  uint8_t valueOffset = count - valueCount;
  if (valueOffset == 1) {
    if (!resolveNode(args[0], TYPE_NEOPIXEL, address)) {
      return;
    }
  } else if (!firstAddressOfType(TYPE_NEOPIXEL, address)) {
    return;
  }

  for (uint8_t i = 0; i < valueCount; ++i) {
    if (!parseByte(args[valueOffset + i], 0, 255, bytes[i + 1])) {
      Serial.println(F("RGB values must be bytes from 0 to 255"));
      return;
    }
  }

  if (strcmp(command, "demo") == 0) {
    const uint8_t colors[] = {CMD_RGB_RED, CMD_RGB_GREEN, CMD_RGB_BLUE,
                              CMD_RGB_WHITE, CMD_RGB_OFF};
    for (uint8_t i = 0; i < sizeof(colors); ++i) {
      if (!sendRgbCommand(address, &colors[i], 1)) {
        return;
      }
      delay(500);
    }
    return;
  }

  (void)sendRgbCommand(address, bytes, length);
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  scan"));
  Serial.println(F("  probe <0xADDR>"));
  Serial.println(F("  automap"));
  Serial.println(F("  map"));
  Serial.println(F("  map add <0xADDR> neopixel"));
  Serial.println(F("  clear map"));
  Serial.println(F("  adc                 (all mapped ADC nodes)"));
  Serial.println(F("  adc <node|0xADDR>"));
  Serial.println(F("  on <0|1>            (first mapped ON_OFF node)"));
  Serial.println(F("  on <node|0xADDR> <0|1>"));
  Serial.println(F("  pwm <0-100>         (first mapped PWM node)"));
  Serial.println(F("  pwm <node|0xADDR> <0-100>"));
  Serial.println(F("  red|green|blue|white|off|show|demo [node|0xADDR]"));
  Serial.println(F("  rgb [node|0xADDR] R G B"));
  Serial.println(F("  pixel [node|0xADDR] index R G B; then show"));
}

void processLine(char *line) {
  char *words[6];
  uint8_t wordCount = 0;
  char *word = strtok(line, " \t");
  while (word != nullptr) {
    if (wordCount == sizeof(words) / sizeof(words[0])) {
      Serial.println(F("Too many arguments"));
      return;
    }
    words[wordCount++] = word;
    word = strtok(nullptr, " \t");
  }
  if (wordCount == 0) {
    return;
  }

  char *command = words[0];
  char *first = wordCount > 1 ? words[1] : nullptr;
  char *second = wordCount > 2 ? words[2] : nullptr;

  if (strcmp(command, "scan") == 0 && wordCount == 1) {
    scanBus();
  } else if (strcmp(command, "probe") == 0 && wordCount == 2) {
    uint8_t address;
    if (parseByte(first, FIRST_I2C_ADDRESS, LAST_I2C_ADDRESS, address)) {
      probeAddress(address);
    } else {
      Serial.println(F("Invalid I2C address. Use: probe 0x40"));
    }
  } else if (strcmp(command, "automap") == 0 && wordCount == 1) {
    autoMap();
  } else if (strcmp(command, "map") == 0 && wordCount == 1) {
    printMap();
  } else if (strcmp(command, "map") == 0 && wordCount == 4 &&
             strcmp(first, "add") == 0 &&
             strcmp(words[3], "neopixel") == 0) {
    addManualNeopixel(second);
  } else if (strcmp(command, "clear") == 0 &&
             wordCount == 2 && strcmp(first, "map") == 0) {
    clearMap();
  } else if (strcmp(command, "adc") == 0 && wordCount <= 2) {
    handleAdc(first);
  } else if (strcmp(command, "on") == 0 && wordCount <= 3) {
    handleOn(first, second);
  } else if (strcmp(command, "pwm") == 0 && wordCount <= 3) {
    handlePwm(first, second);
  } else if (strcmp(command, "red") == 0 || strcmp(command, "green") == 0 ||
             strcmp(command, "blue") == 0 || strcmp(command, "white") == 0 ||
             strcmp(command, "off") == 0 || strcmp(command, "show") == 0 ||
             strcmp(command, "demo") == 0 || strcmp(command, "rgb") == 0 ||
             strcmp(command, "pixel") == 0) {
    handleRgb(command, words + 1, wordCount - 1);
  } else if (strcmp(command, "help") == 0 && wordCount == 1) {
    printHelp();
  } else {
    Serial.println(F("Unknown command. Use: help"));
  }
}

void setup() {
  Serial.begin(115200);
  if (!beginI2C()) {
    Serial.println(F("I2C initialization failed; retry with scan"));
  }
  delay(500);
  Serial.println(F("PY32 workshop I2C master"));
  printHelp();
}

void loop() {
  while (Serial.available()) {
    char character = (char)Serial.read();

    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      inputLine[inputLength] = '\0';
      processLine(inputLine);
      inputLength = 0;
      Serial.print(F("> "));
    } else if (inputLength < MAX_LINE_LENGTH) {
      inputLine[inputLength++] = character;
    }
  }
}
