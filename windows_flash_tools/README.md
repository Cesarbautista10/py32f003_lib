# Flash PY32F003X4 - Windows

## Archivos
- `install_simple.bat` - Instalar PyOCD
- `flash_simple.bat` - Programar firmware  
- `firmware.hex` - Tu archivo de firmware

## Uso

1. **Instalar PyOCD** (solo una vez):
   ```cmd
   install_simple.bat
   ```

2. **Copiar firmware** al mismo directorio

3. **Programar**:
   ```cmd
   flash_simple.bat firmware.hex
   ```

## Ejemplo
```
windows_flash_tools/
├── install_simple.bat
├── flash_simple.bat  
├── firmware_v5_4_1.hex  ← Tu firmware aquí
└── Misc/
    └── pyocd.yaml       ← Configuración PyOCD
```

```cmd
flash_simple.bat firmware_v5_4_1.hex
```

## Hardware
- ST-Link V2: SWDIO→PA13, SWCLK→PA14, GND→GND
