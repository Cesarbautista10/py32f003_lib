# Guía de Uso con UV

Este proyecto utiliza [uv](https://github.com/astral- sh/uv) para gestionar dependencias y entornos de Python, garantizando que siempre uses **Python 3.11**.

# #  Instalación de UV

# ## En Linux/macOS:
```bash
curl - LsSf https://astral.sh/uv/install.sh | sh
```

# ## En Windows (PowerShell):
```powershell
powershell - c "irm https://astral.sh/uv/install.ps1 | iex"
```

# ## Verificar instalación:
```bash
uv - - version
```

# #  Uso del Proyecto con UV

# ## 1. Primera vez - Sincronizar dependencias
```bash
cd windows_interface

# Esto instalará Python 3.11 automáticamente si no lo tienes
# y creará un entorno virtual con todas las dependencias
uv sync
```

# ## 2. Ejecutar el agente LockNode
```bash
# Ejecutar con uv run (usa el entorno automáticamente)
uv run python locknode_agent_https.py

# O activar el entorno y ejecutar normalmente
source .venv/bin/activate  # Linux/macOS
# o
.venv\Scripts\activate     # Windows

python locknode_agent_https.py
```

# ## 3. Ejecutar scripts de prueba
```bash
# Test de detección de dispositivos
uv run python test_device_detection.py

# Test de programación de firmware
uv run python test_firmware_programming.py
```

# ## 4. Agregar nuevas dependencias
```bash
# Agregar una dependencia
uv add nombre- del- paquete

# Agregar una dependencia de desarrollo
uv add - - dev nombre- del- paquete

# Agregar una dependencia opcional (ej: pyserial)
uv add - - optional serial pyserial==3.5
```

# ## 5. Actualizar dependencias
```bash
# Actualizar todas las dependencias
uv sync - - upgrade

# Actualizar una dependencia específica
uv add nombre- del- paquete - - upgrade
```

# ## 6. Ver dependencias instaladas
```bash
uv pip list
```

# #  Bloqueo de Versión de Python

El archivo `.python- version` especifica que este proyecto **SOLO** funciona con Python 3.11.

El archivo `pyproject.toml` tiene:
```toml
requires- python = ">=3.11,<3.12"
```

Esto significa:
- Python 3.11.x funcionará
- Python 3.12+ será rechazado
- Python 3.10 y anteriores serán rechazados

# # 📝 Archivos Importantes

- **`pyproject.toml`**: Configuración del proyecto y dependencias
- **`.python- version`**: Especifica Python 3.11 para uv
- **`uv.lock`**: Archivo de bloqueo generado por uv (similar a package- lock.json)
- **`.gitignore`**: Archivos que no se deben versionar

# #  Migración desde pip/venv

Si tenías un `venv` anterior:

```bash
# 1. Desactivar y eliminar el venv viejo
deactivate
rm - rf venv

# 2. Sincronizar con uv
uv sync

# ¡Listo! Ahora usa uv run para ejecutar scripts
```

# #  Ventajas de UV sobre pip/venv

1. **Más rápido**: 10- 100x más rápido que pip
2. **Resolución de dependencias mejorada**: Evita conflictos
3. **Gestión automática de Python**: Instala Python 3.11 si no lo tienes
4. **Lock file robusto**: Reproducibilidad garantizada
5. **Todo en uno**: No necesitas pip, venv, virtualenv por separado

# #  Comandos Útiles

```bash
# Ver información del proyecto
uv show

# Limpiar caché de uv
uv clean

# Reinstalar todo desde cero
rm - rf .venv uv.lock
uv sync

# Ejecutar cualquier comando en el entorno
uv run <comando>
```

# #  Solución de Problemas

# ## Error: "Python 3.11 not found"
```bash
# uv lo instalará automáticamente
uv sync
```

# ## Error: "lock file is out of date"
```bash
# Regenerar el lock file
uv lock
uv sync
```

# ## Quiero usar Python 3.12 o superior
Esto no está permitido en este proyecto debido a requisitos de compatibilidad. Si realmente necesitas otra versión, edita:
- `.python- version`
- `pyproject.toml` (la línea `requires- python`)

# #  Más Información

- [Documentación oficial de UV](https://docs.astral.sh/uv/)
- [Guía de pyproject.toml](https://packaging.python.org/en/latest/guides/writing- pyproject- toml/)
