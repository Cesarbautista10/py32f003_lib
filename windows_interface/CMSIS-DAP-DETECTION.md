# Detección de Dispositivos CMSIS-DAP

## Descripción

Los agentes LockNode ahora incluyen funcionalidad para detectar dispositivos CMSIS-DAP conectados al sistema. Esta característica permite verificar la presencia de programadores/depuradores antes de intentar operaciones de firmware.

## Funcionamiento

### Backend (Python Flask)

#### Endpoint
```
GET /api/locknode/detect-cmsis-dap
```

#### Respuesta Exitosa
```json
{
  "success": true,
  "message": "Se encontraron 1 dispositivo(s) CMSIS-DAP",
  "devices": [
    {
      "vendor_id": "0x0d28",
      "product_id": "0x0204",
      "manufacturer": "ARM",
      "product": "DAPLink CMSIS-DAP",
      "bus": 1,
      "address": 10
    }
  ],
  "count": 1,
  "timestamp": "2024-01-15T10:30:00.000000"
}
```

#### Sin Dispositivos
```json
{
  "success": true,
  "message": "No se encontraron dispositivos CMSIS-DAP conectados",
  "devices": [],
  "count": 0,
  "timestamp": "2024-01-15T10:30:00.000000"
}
```

### Librerías Utilizadas

El sistema usa dos métodos de detección en orden de prioridad:

1. **pyusb** (Principal)
   - Escanea todos los dispositivos USB del sistema
   - Busca palabras clave en nombres de producto/fabricante
   - Más flexible y ligero
   
2. **pyocd** (Fallback)
   - Usa la API específica de pyOCD para encontrar sondas
   - Más específico para CMSIS-DAP
   - Se activa si pyusb no está disponible

### Palabras Clave de Detección

El sistema busca dispositivos que contengan alguna de estas palabras:
- `cmsis`
- `dap`
- `daplink`
- `jlink`
- `stlink`
- `debugger`
- `probe`

### Frontend (React/TypeScript)

#### Estado
```typescript
const [detectedDevices, setDetectedDevices] = useState<Array<{
  vendor_id: string;
  product_id: string;
  manufacturer: string;
  product: string;
  bus?: number;
  address?: number;
  unique_id?: string;
}>>([]);
const [detectingDevices, setDetectingDevices] = useState<boolean>(false);
```

#### Función de Detección
```typescript
const detectarDispositivos = async () => {
  setDetectingDevices(true);
  // ... llamada al API ...
  const response = await fetch(`${agentBaseUrl}/api/locknode/detect-cmsis-dap`);
  // ... procesamiento de respuesta ...
}
```

#### UI
- Botón "Detectar Dispositivos CMSIS-DAP" en la sección de Operaciones de Firmware (Paso 2)
- Muestra estado "Detectando dispositivos..." durante la búsqueda
- Lista visual de dispositivos encontrados con:
  - Nombre del producto
  - Fabricante
  - IDs (VID/PID)
  - Información de bus/dirección

## Instalación

### Dependencias Python

Actualiza las dependencias:
```bash
pip install -r requirements.txt
```

O instala manualmente:
```bash
pip install pyusb==1.2.1
pip install pyocd==0.36.0
```

### Drivers USB (Windows)

Para que pyusb funcione en Windows, puede ser necesario instalar drivers:

1. **libusb** o **WinUSB** driver
   - Usar Zadig (https://zadig.akeo.ie/)
   - Seleccionar el dispositivo CMSIS-DAP
   - Instalar driver WinUSB

2. **Alternativamente**, pyocd maneja sus propios drivers

## Uso

1. **Iniciar el agente local** (HTTP o HTTPS)
   ```bash
   python locknode_agent.py
   # o
   python locknode_agent_https.py
   ```

2. **En la interfaz web**:
   - Navegar al Panel de LockNode
   - Establecer conexión con el agente
   - Ir a "Operaciones de Firmware" (Paso 2)
   - Hacer clic en "Detectar Dispositivos CMSIS-DAP"

3. **Ver resultados**:
   - La lista de dispositivos aparece debajo del botón
   - Los mensajes de log muestran detalles adicionales

## Dispositivos Soportados

La detección funciona con cualquier dispositivo que se identifique como:
- CMSIS-DAP (ARM)
- DAPLink
- J-Link (Segger)
- ST-Link (STMicroelectronics)
- Otros depuradores USB genéricos

## Troubleshooting

### No se detectan dispositivos

1. **Verificar conexión física**: Cable USB conectado correctamente
2. **Drivers**: Instalar drivers WinUSB con Zadig (Windows)
3. **Permisos**: En Linux, agregar reglas udev para acceso USB
4. **Librerías**: Verificar que pyusb o pyocd estén instalados
5. **Logs**: Revisar la consola del agente Python para errores

### Error de librerías

Si aparece:
```
No hay librerías de detección instaladas. Instala: pip install pyusb pyocd
```

Ejecutar:
```bash
pip install pyusb pyocd
```

### Linux: Permission Denied

Crear regla udev en `/etc/udev/rules.d/50-cmsis-dap.rules`:
```
SUBSYSTEM=="usb", ATTR{idVendor}=="0d28", MODE="0666"
```

Recargar reglas:
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Ejemplos de Dispositivos Comunes

| Dispositivo | VID | PID | Fabricante |
|-------------|-----|-----|------------|
| DAPLink CMSIS-DAP | 0x0d28 | 0x0204 | ARM |
| J-Link | 0x1366 | Varios | Segger |
| ST-Link/V2 | 0x0483 | 0x3748 | STMicroelectronics |
| ST-Link/V3 | 0x0483 | 0x374e | STMicroelectronics |

## Notas Técnicas

- La detección es no invasiva (solo lectura)
- No abre ni reserva el dispositivo
- Compatible con múltiples dispositivos simultáneos
- Timeout de 5 segundos para operaciones USB
- Thread-safe para uso concurrente

## Próximas Mejoras

- [ ] Selección de dispositivo específico para programación
- [ ] Verificación de firmware del programador
- [ ] Auto-detección al conectar dispositivo
- [ ] Caché de dispositivos conocidos
- [ ] Indicador visual de estado del dispositivo (ocupado/libre)
