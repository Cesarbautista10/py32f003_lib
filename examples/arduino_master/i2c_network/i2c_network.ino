#include <Wire.h>
#include <stdlib.h>
#include <string.h>

constexpr uint8_t CMD_IDENTIFY = 0x00;
constexpr uint8_t CMD_ADC_RAW = 0x01;
constexpr uint8_t CMD_ON_OFF = 0x02;
constexpr uint8_t CMD_PWM = 0x03;

constexpr uint8_t TYPE_ADC = 0x01;
constexpr uint8_t TYPE_ON_OFF = 0x02;
constexpr uint8_t TYPE_PWM = 0x03;

constexpr uint8_t MODEL_NONE = 0x00;
constexpr uint8_t MODEL_TEMT6000 = 0x01;

constexpr uint8_t FIRST_I2C_ADDRESS = 0x08;
constexpr uint8_t LAST_I2C_ADDRESS = 0x77;
constexpr uint8_t MAX_NODES = 32;
constexpr uint8_t MAX_LINE_LENGTH = 63;

struct Node {
  uint8_t address;
  uint8_t type;
  uint8_t model;
};

Node nodeMap[MAX_NODES];
uint8_t nodeCount = 0;
char inputLine[MAX_LINE_LENGTH + 1];
uint8_t inputLength = 0;

void printAddress(uint8_t address) {
  Serial.print("0x");
  if (address < 0x10) {
    Serial.print('0');
  }
  Serial.print(address, HEX);
}

const char *typeName(uint8_t type) {
  switch (type) {
    case TYPE_ADC: return "ADC";
    case TYPE_ON_OFF: return "ON_OFF";
    case TYPE_PWM: return "PWM";
    default: return "UNKNOWN";
  }
}

const char *modelName(uint8_t type, uint8_t model) {
  if (type == TYPE_ADC && model == MODEL_TEMT6000) {
    return "TEMT6000";
  }
  if (model == MODEL_NONE) {
    return "generic";
  }
  return "unknown";
}

void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(6, 7);  // SDA, SCL
#else
  Wire.begin();
#endif
  Wire.setClock(100000);
}

bool pingAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
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

  if (!readRegister(address, CMD_IDENTIFY, identity, sizeof(identity))) {
    return false;
  }
  if (identity[0] < TYPE_ADC || identity[0] > TYPE_PWM) {
    return false;
  }

  node.address = address;
  node.type = identity[0];
  node.model = identity[1];
  return true;
}

void clearMap() {
  nodeCount = 0;
  Serial.println("Map cleared");
}

void printMap() {
  if (nodeCount == 0) {
    Serial.println("Map empty. Run: automap");
    return;
  }

  for (uint8_t i = 0; i < nodeCount; ++i) {
    Serial.print(i + 1);
    Serial.print(": ");
    printAddress(nodeMap[i].address);
    Serial.print("  ");
    Serial.print(typeName(nodeMap[i].type));
    Serial.print("  ");
    Serial.println(modelName(nodeMap[i].type, nodeMap[i].model));
  }
}

void scanBus() {
  uint8_t found = 0;

  Serial.println("Scanning I2C...");
  for (uint8_t address = FIRST_I2C_ADDRESS;
       address <= LAST_I2C_ADDRESS; ++address) {
    if (pingAddress(address)) {
      printAddress(address);
      Serial.println();
      ++found;
    }
  }
  Serial.print("Found: ");
  Serial.println(found);
}

void autoMap() {
  nodeCount = 0;
  Serial.println("Mapping workshop nodes...");

  for (uint8_t address = FIRST_I2C_ADDRESS;
       address <= LAST_I2C_ADDRESS; ++address) {
    if (!pingAddress(address)) {
      continue;
    }

    Node node;
    if (identifyNode(address, node)) {
      if (nodeCount < MAX_NODES) {
        nodeMap[nodeCount++] = node;
      }
    } else {
      Serial.print("Unknown I2C device at ");
      printAddress(address);
      Serial.println();
    }
  }

  printMap();
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
      Serial.println("Invalid I2C address");
      return false;
    }
    address = value;
    return true;
  }

  if (!parseByte(text, 1, nodeCount, value)) {
    Serial.println("Invalid map index. Use map or an address such as 0x20");
    return false;
  }

  const Node &node = nodeMap[value - 1];
  if (node.type != expectedType) {
    Serial.print("Node is ");
    Serial.print(typeName(node.type));
    Serial.print(", not ");
    Serial.println(typeName(expectedType));
    return false;
  }
  address = node.address;
  return true;
}

bool firstAddressOfType(uint8_t type, uint8_t &address) {
  int8_t index = firstNodeOfType(type);
  if (index < 0) {
    Serial.print("No ");
    Serial.print(typeName(type));
    Serial.println(" node in map. Run: automap");
    return false;
  }
  address = nodeMap[index].address;
  return true;
}

void readAdcAt(uint8_t address) {
  uint8_t data[2];

  if (!readRegister(address, CMD_ADC_RAW, data, sizeof(data))) {
    Serial.println("ADC I2C error");
    return;
  }

  uint16_t raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
  printAddress(address);
  Serial.print(" ADC=");
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
    Serial.println("No ADC nodes in map. Run: automap");
  }
}

void handleOn(char *first, char *second) {
  uint8_t address;
  uint8_t state;

  if (first == nullptr) {
    Serial.println("Usage: on <0|1> OR on <node|0xADDR> <0|1>");
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
      Serial.println("State must be 0 or 1");
      return;
    }
  }

  if (!writeRegister(address, CMD_ON_OFF, state)) {
    Serial.println("ON/OFF I2C error");
    return;
  }
  printAddress(address);
  Serial.println(state ? " ON" : " OFF");
}

void handlePwm(char *first, char *second) {
  uint8_t address;
  uint8_t percent;

  if (first == nullptr) {
    Serial.println("Usage: pwm <0-100> OR pwm <node|0xADDR> <0-100>");
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
      Serial.println("PWM must be between 0 and 100");
      return;
    }
  }

  if (!writeRegister(address, CMD_PWM, percent)) {
    Serial.println("PWM I2C error");
    return;
  }
  printAddress(address);
  Serial.print(" PWM=");
  Serial.print(percent);
  Serial.println('%');
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  scan");
  Serial.println("  automap");
  Serial.println("  map");
  Serial.println("  clear map");
  Serial.println("  adc                 (all mapped ADC nodes)");
  Serial.println("  adc <node|0xADDR>");
  Serial.println("  on <0|1>            (first mapped ON_OFF node)");
  Serial.println("  on <node|0xADDR> <0|1>");
  Serial.println("  pwm <0-100>         (first mapped PWM node)");
  Serial.println("  pwm <node|0xADDR> <0-100>");
}

void processLine(char *line) {
  char *command = strtok(line, " \t");
  char *first = strtok(nullptr, " \t");
  char *second = strtok(nullptr, " \t");

  if (command == nullptr) {
    return;
  }

  if (strcmp(command, "scan") == 0) {
    scanBus();
  } else if (strcmp(command, "automap") == 0) {
    autoMap();
  } else if (strcmp(command, "map") == 0) {
    printMap();
  } else if (strcmp(command, "clear") == 0 &&
             first != nullptr && strcmp(first, "map") == 0) {
    clearMap();
  } else if (strcmp(command, "adc") == 0) {
    handleAdc(first);
  } else if (strcmp(command, "on") == 0) {
    handleOn(first, second);
  } else if (strcmp(command, "pwm") == 0) {
    handlePwm(first, second);
  } else if (strcmp(command, "help") == 0) {
    printHelp();
  } else {
    Serial.println("Unknown command. Use: help");
  }
}

void setup() {
  Serial.begin(115200);
  beginI2C();
  delay(500);
  Serial.println("PY32 workshop I2C master");
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
      Serial.print("> ");
    } else if (inputLength < MAX_LINE_LENGTH) {
      inputLine[inputLength++] = character;
    }
  }
}
