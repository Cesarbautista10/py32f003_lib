# Programación de Firmware con pyOCD

## Descripción

Los agentes LockNode ahora incluyen funcionalidad completa para programar firmware en dispositivos mediante pyOCD. Esta característica permite borrar y cargar firmware directamente desde la interfaz web.

## Estructura de Archivos

```
windows_interface/
└── firmware/
    ├── devlab_v6_0_0_lite.bin    # Archivo de firmware
    └── Misc/
        ├── pyocd.yaml             # Configuración de pyOCD
        ├── Puya.PY32.pack         # Pack de soporte para PY32
        └── [otros archivos de configuración]
```

## Funcionamiento

### Backend (Python Flask)

#### Endpoints

##### 1. Listar Firmwares Disponibles
```
GET /api/locknode/firmware/list
```

**Respuesta Exitosa:**
```json
{
  "success": true,
  "files": [
    {
      "name": "devlab_v6_0_0_lite.bin",
      "path": "devlab_v6_0_0_lite.bin",
      "size": 245760,
      "size_kb": 240.0,
      "modified": "2025-11-24T10:30:00.000000"
    }
  ],
  "count": 1,
  "firmware_dir": "/path/to/firmware"
}
```

##### 2. Programar Firmware
```
POST /api/locknode/firmware/program
Content-Type: application/json

{
  "firmware_file": "devlab_v6_0_0_lite.bin",
  "target": "py32f003x4"
}
```

**Respuesta Exitosa:**
```json
{
  "success": true,
  "message": "Firmware programado exitosamente",
  "firmware_file": "devlab_v6_0_0_lite.bin",
  "target": "py32f003x4",
  "erase_output": "...",
  "load_output": "...",
  "timestamp": "2025-11-24T10:35:00.000000"
}
```

### Proceso de Programación

El agente ejecuta el siguiente flujo (equivalente al script batch):

```bash
# 1. Borrar el chip
python -m pyocd erase -t py32f003x4 --chip --config Misc/pyocd.yaml

# 2. Cargar el firmware
python -m pyocd load "firmware.bin" -t py32f003x4 --config Misc/pyocd.yaml
```

### Frontend (React/TypeScript)

#### Estados
```typescript
const [firmwareFiles, setFirmwareFiles] = useState<Array<{
  name: string;
  path: string;
  size: number;
  size_kb: number;
  modified: string;
}>>([]);
const [selectedFirmware, setSelectedFirmware] = useState<string>("");
const [loadingFirmware, setLoadingFirmware] = useState<boolean>(false);
const [programming, setProgramming] = useState<boolean>(false);
```

#### Funciones

##### Listar Firmwares
```typescript
const listarFirmwares = async () => {
  const response = await fetch(`${agentBaseUrl}/api/locknode/firmware/list`);
  // ... procesamiento ...
}
```

##### Programar Firmware
```typescript
const programarFirmware = async () => {
  const response = await fetch(`${agentBaseUrl}/api/locknode/firmware/program`, {
    method: 'POST',
    body: JSON.stringify({
      firmware_file: selectedFirmware,
      target: 'py32f003x4'
    })
  });
  // ... procesamiento ...
}
```

#### UI

La interfaz incluye:

1. **Selector de Firmware**
   - Dropdown con lista de archivos .bin disponibles
   - Muestra tamaño del archivo
   - Carga automática al conectar

2. **Información del Firmware**
   - Nombre del archivo
   - Tamaño en KB
   - Fecha de modificación
   - Target (py32f003x4)

3. **Botones de Acción**
   - 🔄 Actualizar Lista: Refresca la lista de firmwares
   - ⚡ Programar Firmware: Inicia el proceso de programación

4. **Advertencia**
   - Recordatorio para conectar dispositivo CMSIS-DAP

## Instalación

### Dependencias Python

Actualiza las dependencias:
```bash
pip install -r requirements.txt
```

El archivo `requirements.txt` ya incluye:
```
pyocd==0.36.0
pyusb==1.2.1
```

### Configuración pyOCD

El archivo `firmware/Misc/pyocd.yaml` debe contener:
```yaml
pack:
  - Misc/Puya.PY32.pack
```

Este archivo permite que pyOCD encuentre el soporte para chips PY32.

### Archivos de Firmware

1. Coloca tus archivos `.bin` en el directorio `windows_interface/firmware/`
2. Los archivos aparecerán automáticamente en la lista del panel web

## Uso

### Desde la Interfaz Web

1. **Conectar al agente**
   ```
   Panel LockNode → Configuración → Establecer Enlace
   ```

2. **Detectar dispositivo CMSIS-DAP**
   ```
   Operaciones de Firmware → Detectar Dispositivos CMSIS-DAP
   ```

3. **Seleccionar firmware**
   ```
   Programar Firmware (pyOCD) → Selector desplegable
   ```

4. **Programar**
   ```
   Clic en "⚡ Programar Firmware"
   ```

5. **Monitorear progreso**
   - Ver mensajes en el panel de historial
   - Esperar confirmación "Firmware programado exitosamente"

### Desde API (cURL)

```bash
# Listar firmwares
curl http://localhost:8081/api/locknode/firmware/list

# Programar firmware
curl -X POST http://localhost:8081/api/locknode/firmware/program \
  -H "Content-Type: application/json" \
  -d '{
    "firmware_file": "devlab_v6_0_0_lite.bin",
    "target": "py32f003x4"
  }'
```

### Desde Python

```python
import requests

# Listar firmwares
response = requests.get('http://localhost:8081/api/locknode/firmware/list')
files = response.json()['files']
print(f"Firmwares disponibles: {[f['name'] for f in files]}")

# Programar firmware
response = requests.post(
    'http://localhost:8081/api/locknode/firmware/program',
    json={
        'firmware_file': 'devlab_v6_0_0_lite.bin',
        'target': 'py32f003x4'
    }
)
result = response.json()
if result['success']:
    print('✓ Firmware programado exitosamente')
else:
    print(f'✗ Error: {result["error"]}')
```

## Targets Soportados

El sistema está configurado para el target `py32f003x4`, pero puedes especificar otros targets soportados por pyOCD:

| Target | Chip | Pack Requerido |
|--------|------|----------------|
| py32f003x4 | PY32F003x4 | Puya.PY32.pack |
| py32f030x6 | PY32F030x6 | Puya.PY32.pack |
| py32f030x8 | PY32F030x8 | Puya.PY32.pack |

Para agregar más targets, instala los packs correspondientes en `firmware/Misc/`.

## Timeouts

- **Erase (Borrado)**: 30 segundos
- **Load (Carga)**: 60 segundos

Si el proceso excede estos tiempos, la operación se cancela automáticamente.

## Troubleshooting

### Error: "Archivo de firmware no encontrado"

**Causa:** El archivo .bin no existe en `windows_interface/firmware/`

**Solución:**
```bash
# Verificar archivos
ls windows_interface/firmware/*.bin

# Copiar firmware si es necesario
cp /ruta/a/tu/firmware.bin windows_interface/firmware/
```

### Error: "Archivo de configuración pyocd.yaml no encontrado"

**Causa:** Falta el archivo de configuración

**Solución:**
```bash
# Verificar estructura
ls windows_interface/firmware/Misc/pyocd.yaml

# Si falta, crear con:
echo "pack:" > windows_interface/firmware/Misc/pyocd.yaml
echo "  - Misc/Puya.PY32.pack" >> windows_interface/firmware/Misc/pyocd.yaml
```

### Error: "No CMSIS-DAP device found"

**Causa:** No hay dispositivo conectado o no es detectado

**Solución:**
1. Conectar dispositivo CMSIS-DAP
2. Verificar detección: Clic en "Detectar Dispositivos CMSIS-DAP"
3. Instalar drivers (Windows: usar Zadig)
4. Verificar con: `python -m pyocd list`

### Error: "Error al borrar" o "Error al cargar"

**Causa:** Problemas de comunicación con el chip

**Solución:**
1. Verificar conexiones físicas
2. Comprobar alimentación del chip target
3. Probar manualmente:
   ```bash
   cd windows_interface/firmware
   python -m pyocd list
   python -m pyocd erase -t py32f003x4 --chip --config Misc/pyocd.yaml
   ```

### Timeout al programar

**Causa:** El chip tarda mucho en borrarse o programarse

**Solución:**
- Verificar que el chip no esté protegido
- Usar pyOCD manualmente con `--verbose` para ver detalles
- Aumentar timeouts en el código si es necesario

### Error: "pyocd: command not found"

**Causa:** pyOCD no está instalado

**Solución:**
```bash
pip install pyocd
python -m pyocd --version
```

## Logs y Debug

### Ver logs del agente

Los logs muestran cada paso del proceso:
```
INFO - Programando firmware: devlab_v6_0_0_lite.bin
INFO - Target: py32f003x4
INFO - Borrando chip...
INFO - Chip borrado exitosamente
INFO - Cargando firmware...
INFO - Firmware cargado exitosamente
```

### Ejecutar con debug

```bash
# En la terminal del agente
python locknode_agent.py
# Los logs aparecen automáticamente
```

### pyOCD verbose

Para más detalles, ejecuta manualmente:
```bash
cd windows_interface/firmware
python -m pyocd load firmware.bin -t py32f003x4 --config Misc/pyocd.yaml --verbose
```

## Características Adicionales

### Validación de Archivos

- Solo archivos `.bin` son listados
- Verificación de existencia antes de programar
- Validación de configuración pyOCD

### Manejo de Errores

- Captura de errores de pyOCD
- Mensajes descriptivos al usuario
- Timeouts automáticos

### Seguridad

- Validación de rutas de archivos
- Ejecución en directorio correcto
- Restauración de directorio original

### Performance

- Listado rápido de archivos
- Ejecución asíncrona en frontend
- Indicadores de progreso visuales

## Próximas Mejoras

- [ ] Soporte para múltiples targets (selector en UI)
- [ ] Verificación de firmware después de programar
- [ ] Comparación de firmware (chip vs archivo)
- [ ] Historial de programaciones
- [ ] Upload de archivos .bin desde el navegador
- [ ] Progress bar durante programación
- [ ] Soporte para .hex además de .bin
- [ ] Programación de configuración/fuses

## Referencias

- [pyOCD Documentation](https://pyocd.io/)
- [Puya PY32 Series](https://www.puyasemi.com/)
- [CMSIS-DAP Specification](https://arm-software.github.io/CMSIS_5/DAP/html/index.html)
