# Configuraciones predefinidas para diferentes boards y proyectos

# ESP32-C6 CDC-JTAG Configuration
ESP32C6_FQBN="esp32:esp32:esp32c6:CDCOnBoot=cdc,JTAGAdapter=builtin"
ESP32C6_FLAGS="-DESP32C6=1 -DARDUINO_ESP32C6_DEV=1 -DCONFIG_IDF_TARGET_ESP32C6=1 -DESP32C6_JTAG_ENABLED=1"
ESP32C6_BAUDRATE="115200"

# ESP32 Standard Configuration  
ESP32_FQBN="esp32:esp32:esp32"
ESP32_FLAGS="-DESP32=1 -DARDUINO_ESP32_DEV=1"
ESP32_BAUDRATE="115200"

# ESP32-S3 Configuration
ESP32S3_FQBN="esp32:esp32:esp32s3"
ESP32S3_FLAGS="-DESP32S3=1 -DARDUINO_ESP32S3_DEV=1"
ESP32S3_BAUDRATE="115200"

# Puertos serie comunes
COMMON_PORTS=("/dev/ttyACM0" "/dev/ttyUSB0" "/dev/cu.usbserial*" "/dev/cu.usbmodem*")

# Rutas de Arduino por sistema operativo
LINUX_ARDUINO_HOME="$HOME/.arduino15"
MACOS_ARDUINO_HOME="$HOME/Library/Arduino15"  
WINDOWS_ARDUINO_HOME="$USERPROFILE/AppData/Local/Arduino15"

# Versiones soportadas de ESP32 Core
ESP32_CORE_VERSIONS=("3.2.0" "3.1.0" "3.0.0" "2.0.17" "2.0.16")
