# Windows Interface - LockNode Agent

Este directorio contiene los agentes Python que corren en Windows para comunicarse con el panel web LockNode.

## Estructura de Archivos

```
windows_interface/
├── locknode_agent.py           # Agente HTTP (desarrollo)
├── locknode_agent_https.py     # Agente HTTPS (producción)
├── generate_ssl_certs.sh       # Script para generar certificados SSL
├── test_device_detection.py    # Script de prueba para CMSIS-DAP
├── test_firmware_programming.py # Script de prueba para programación
├── install_uv_setup.sh         # Script de instalación de UV
├── start_agent_uv.sh           # Script de inicio con UV (Linux/macOS)
├── start_agent_uv.bat          # Script de inicio con UV (Windows)
├── pyocd_uv.sh                 # Wrapper de pyOCD con UV (Linux/macOS)
├── pyocd_uv.bat                # Wrapper de pyOCD con UV (Windows)
├── requirements.txt            # Dependencias Python
├── pyproject.toml              # Configuración del proyecto con UV
├── .python-version             # Versión de Python bloqueada (3.12)
├── README.md                   # Este archivo
├── docs/                       # Documentación del proyecto
│   ├── INICIO_RAPIDO.md        # Guía de inicio rápido
│   ├── README_UV.md            # Guía completa de UV
│   ├── MIGRACION_UV.md         # Guía de migración desde pip
│   ├── CAMBIAR_VERSION_PYTHON.md # Guía para cambiar versión de Python
│   └── RESUMEN_UV.md           # Resumen del setup con UV
├── certs/                      # Carpeta para certificados SSL
│   ├── cert.pem
│   └── key.pem
└── firmware/                   # Archivos de firmware
    ├── *.bin                   # Archivos binarios de firmware
    └── Misc/                   # Configuración pyOCD
        ├── pyocd.yaml
        └── *.pack              # Packs de soporte
```

## Propósito

Los agentes Python actúan como puente entre:
- **Panel Web** (Frontend React/TypeScript)
- **Dispositivo LockNode** (Hardware físico)

```
Panel Web ←→ Agente Python ←→ Dispositivo LockNode
(Frontend)   (Windows)         (Hardware)
```

## Inicio Rápido

> RECOMENDADO: Usa [UV](docs/README_UV.md) para gestión automática de Python 3.11 y dependencias.

### Opción A: Usando UV (Recomendado)

```bash
# Navegar al directorio
cd windows_interface

# Instalar uv (primera vez)
curl -LsSf https://astral.sh/uv/install.sh | sh  # Linux/macOS
# o en Windows: powershell -c "irm https://astral.sh/uv/install.ps1 | iex"

# Sincronizar proyecto (instala Python 3.11 y dependencias automáticamente)
uv sync

# Ejecutar el agente
uv run python locknode_agent_https.py
```

**[Ver guía completa de UV](docs/README_UV.md)**

### Opción B: Usando pip tradicional

```bash
# Navegar al directorio
cd windows_interface

# Instalar paquetes Python (requiere Python 3.11 instalado)
pip install -r requirements.txt
```

## 2. Ejecutar Agente

**Para Desarrollo (HTTP):**
```bash
python locknode_agent.py
```

**Para Producción (HTTPS):**
```bash
# Primero generar certificados
bash generate_ssl_certs.sh

# Luego ejecutar agente HTTPS
python locknode_agent_https.py
```

# ## 3. Probar Detección de Dispositivos

```bash
# Ejecutar script de prueba
python test_device_detection.py
```

Este script verifica:
- Instalación de librerías (pyusb, pyocd)
- Detección de dispositivos CMSIS- DAP conectados
- Información detallada de cada dispositivo

##  Detección de Dispositivos CMSIS- DAP

Los agentes incluyen capacidad para detectar automáticamente dispositivos CMSIS- DAP (programadores/depuradores).

### Características
- Detecta múltiples dispositivos simultáneos
- Soporta DAPLink, J- Link, ST- Link, etc.
- Muestra información detallada (VID/PID, fabricante)
- No invasivo (solo lectura, no bloquea dispositivos)

### Uso desde el Panel Web
1. Establecer conexión con el agente
2. Ir a "Operaciones de Firmware" (Paso 2)
3. Hacer clic en "Detectar Dispositivos CMSIS- DAP"
4. Ver lista de dispositivos encontrados

### Uso desde API
```bash
curl http://localhost:8081/api/locknode/detect- cmsis- dap
```

**Ver documentación completa:** `CMSIS- DAP- DETECTION.md`

##  Programación de Firmware

Los agentes permiten programar firmware en dispositivos usando pyOCD directamente desde la interfaz web.

### Características
- Borrado automático del chip (erase)
- Carga de firmware (.bin)
- Selector de archivos en la interfaz
- Soporte para PY32F003x4 y otros targets
- Monitoreo de progreso en tiempo real

### Uso desde el Panel Web
1. Conectar dispositivo CMSIS- DAP
2. Establecer conexión con el agente
3. Ir a "Operaciones de Firmware" (Paso 2)
4. En "Programar Firmware (pyOCD)":
   - Seleccionar archivo .bin
   - Clic en " Programar Firmware"
5. Esperar confirmación

### Comandos Equivalentes
```bash
# Lo que hace el agente automáticamente:
python - m pyocd erase - t py32f003x4 - - chip - - config Misc/pyocd.yaml
python - m pyocd load firmware.bin - t py32f003x4 - - config Misc/pyocd.yaml
```

### Agregar Nuevos Firmwares
```bash
# Copiar archivo .bin al directorio firmware
cp mi_firmware.bin windows_interface/firmware/

# Actualizar lista en la interfaz web
# (clic en " Actualizar Lista")
```

**Ver documentación completa:** `FIRMWARE- PROGRAMMING.md`

##  Requisitos

### Sistema
- **Windows 10/11**
- **Python 3.11 o 3.12** (IMPORTANTE: pyOCD no funciona en Python 3.13 debido a incompatibilidades con pyreadline)
- OpenSSL (para HTTPS)

### Python Packages
- `flask` - Servidor web
- `flask-cors` - Manejo de CORS
- `pyusb` - Detección de dispositivos USB
- `pyocd` - Detección específica de CMSIS-DAP
- `pyserial` - Comunicación serial con dispositivo (opcional)

### Nota Importante para Windows
Si tienes Python 3.13 instalado en Windows, pyOCD fallará con error de `collections.Callable`. 
**Solución**: Usa el entorno UV configurado (Python 3.12) para ejecutar pyocd:

```bash
# En lugar de:
# pyocd erase --chip ...

# Usa:
uv run pyocd erase --chip --config ../Misc/pyocd.yaml

# O más fácil, usa el script wrapper:
# Windows:
pyocd_uv.bat erase --chip --config ../Misc/pyocd.yaml

# Linux/macOS:
./pyocd_uv.sh erase --chip --config ../Misc/pyocd.yaml
```

Los scripts `pyocd_uv.bat` y `pyocd_uv.sh` son wrappers que automáticamente usan UV con Python 3.12, evitando problemas de compatibilidad.

##  Configuración

### Cambiar Puerto

Por defecto usa el puerto **8081**. Para cambiarlo:

**En `locknode_agent.py` o `locknode_agent_https.py`:**
```python
app.run(
    host='0.0.0.0',
    port=8081,  # ← Cambiar aquí
    debug=True
)
```

**En el panel web (`Panel_LockNode_Programacion.tsx`):**
```typescript
const AGENT_BASE_URL = 'http://localhost:8081';  // ← Actualizar puerto
```

### Configurar Dispositivo Serial

Si necesitas comunicación serial con el dispositivo LockNode:

```python
import serial

# Agregar en locknode_agent.py
device = serial.Serial(
    port='COM3',        # Puerto serial del dispositivo
    baudrate=115200,
    timeout=1
)
```

##  API Endpoints

El agente expone los siguientes endpoints:
| Método | Endpoint | Descripción |
|--------|----------|-------------|
| POST | `/api/locknode/connect` | Establecer conexión |
| POST | `/api/locknode/disconnect` | Desconectar agente |
| POST | `/api/locknode/command` | Enviar comando al dispositivo |
| POST | `/api/locknode/upload-firmware` | Cargar firmware |
| POST | `/api/locknode/download-firmware` | Descargar firmware |
| GET | `/api/locknode/detect-cmsis-dap` | Detectar dispositivos CMSIS-DAP |
| GET | `/api/locknode/firmware/list` | **NUEVO:** Listar firmwares disponibles |
| POST | `/api/locknode/firmware/program` | **NUEVO:** Programar firmware con pyOCD |
| GET | `/api/locknode/heartbeat` | Verificar estado (automático) |
| GET | `/api/locknode/status` | Obtener estado completo |
| POST | `/api/locknode/device/connect` | Conectar dispositivo físico |

# # 🔐 Seguridad

# ## Desarrollo (HTTP)
- Rápido y simple
- Sin cifrado (solo para local)
- No requiere certificados

# ## Producción (HTTPS)
- Conexión cifrada SSL/TLS
- Compatible con panel HTTPS
- Requiere certificados

Ver `HTTP_vs_HTTPS_GUIDE.md` para más detalles.

# #  Comandos Útiles

# ## Ver Logs del Agente
```bash
python locknode_agent.py 2>&1 | tee agent.log
```

# ## Verificar Puerto Disponible
```bash
# Windows
netstat - ano | findstr :8081

# Si está ocupado, matar proceso
taskkill /PID <PID> /F
```

# ## Probar Conexión
```bash
# Desde otra terminal
curl http://localhost:8081/api/locknode/heartbeat
```

# # 📝 Ejemplos de Uso

# ## Ejemplo 1: Comando Básico
```python
# Cliente Python
import requests

response = requests.post(
    'http://localhost:8081/api/locknode/command',
    json={'command': 'status', 'timestamp': '2025- 11- 24T10:00:00'}
)
print(response.json())
```

# ## Ejemplo 2: Cargar Firmware
```python
response = requests.post(
    'http://localhost:8081/api/locknode/upload- firmware',
    json={'action': 'upload', 'timestamp': '2025- 11- 24T10:00:00'}
)
print(response.json())
```

# #  Troubleshooting

# ## Error: Puerto 8081 en uso
```bash
# Ver qué proceso usa el puerto
netstat - ano | findstr :8081

# Cambiar a otro puerto en locknode_agent.py
```

# ## Error: Flask no instalado
```bash
pip install flask flask- cors
```

# ## Error: Certificados no encontrados (HTTPS)
```bash
bash generate_ssl_certs.sh
```

# ## Error: No se conecta con el panel web
1. Verificar que el agente esté corriendo
2. Verificar que el puerto sea el correcto (8081)
3. Revisar la consola del navegador (F12)
4. Verificar que CORS esté habilitado

## Archivos Relacionados

### Documentación del Proyecto

El directorio `docs/` contiene documentación detallada:

- **[INICIO_RAPIDO.md](docs/INICIO_RAPIDO.md)** - Guía de inicio rápido con UV
- **[README_UV.md](docs/README_UV.md)** - Guía completa de uso con UV
- **[MIGRACION_UV.md](docs/MIGRACION_UV.md)** - Migración desde pip/venv a UV
- **[RESUMEN_UV.md](docs/RESUMEN_UV.md)** - Resumen del setup con UV
- **[CAMBIAR_VERSION_PYTHON.md](docs/CAMBIAR_VERSION_PYTHON.md)** - Cómo cambiar la versión de Python

##En el Proyecto
- `testbench_platform/src/pages/Panel_LockNode_Programacion.tsx` - Panel web
- `testbench_platform/src/Routes.tsx` - Configuración de rutas
- `docs/locknode-panel-setup.md` - Documentación completa

### En este Directorio
- `locknode_agent.py` - Agente principal HTTP
- `locknode_agent_https.py` - Agente con SSL/TLS
- `pyproject.toml` - Configuración del proyecto y dependencias
- `.python-version` - Versión de Python bloqueada

# #  Actualizar Agente

Para agregar nuevos endpoints o funcionalidades:

1. Editar `locknode_agent.py` o `locknode_agent_https.py`
2. Agregar el nuevo endpoint:
```python
@app.route('/api/locknode/nueva-funcion', methods=['POST'])
def nueva_funcion():
    data = request.json
    # Tu lógica aquí
    return jsonify({'success': True, 'message': 'OK'})
```
3. Actualizar el panel web para llamar al nuevo endpoint
4. Actualizar esta documentación

## Tips

1. **Modo Debug:** El agente corre con `debug=True` por defecto
2. **Auto-reload:** Flask recarga automáticamente al guardar cambios
3. **Logs:** Los logs aparecen en la consola donde ejecutas el agente
4. **CORS:** Está habilitado para todas las origenes en desarrollo
5. **Estado:** El agente guarda su estado en la variable `agent_state`

## Recursos

- [Flask Documentation](https://flask.palletsprojects.com/)
- [Flask-CORS](https://flask-cors.readthedocs.io/)
- [pySerial](https://pyserial.readthedocs.io/)
- [OpenSSL](https://www.openssl.org/)
- [UV Documentation](https://docs.astral.sh/uv/)

## Soporte

Para problemas o preguntas:
1. Revisar documentación en `docs/`
2. Revisar `HTTP_vs_HTTPS_GUIDE.md`
3. Contactar al equipo de desarrollo

---

**Versión:** 1.0.0
**Última actualización:** Febrero 2026
**Plataforma:** Windows 10/11, Linux, macOS
**Python:** 3.12+ (configurable en `.python-version`)
