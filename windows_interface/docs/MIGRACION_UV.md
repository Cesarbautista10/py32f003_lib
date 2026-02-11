# Guía de Migración a UV

Esta guía te ayudará a migrar desde el sistema anterior (pip + venv) al nuevo sistema con UV.

# # ¿Por qué migrar a UV?

1. ** Bloqueo de Python 3.11**: Garantiza que nadie use versiones incompatibles
2. ** Velocidad**: 10- 100x más rápido que pip
3. ** Resolución de dependencias**: Evita conflictos de versiones
4. ** Gestión automática**: UV instala Python 3.11 si no lo tienes
5. ** Todo en uno**: No necesitas pip, venv, virtualenv por separado

# # Pasos de Migración

# ## Paso 1: Instalar UV

**Linux/macOS:**
```bash
curl - LsSf https://astral.sh/uv/install.sh | sh
```

**Windows (PowerShell como Administrador):**
```powershell
powershell - c "irm https://astral.sh/uv/install.ps1 | iex"
```

**Verificar instalación:**
```bash
uv - - version
```

# ## Paso 2: Limpiar entorno anterior (opcional)

Si tenías un `venv` anterior con pip:

```bash
cd windows_interface

# Desactivar el entorno si está activo
deactivate  # Si estás dentro de un venv

# Eliminar el venv viejo
rm - rf venv           # Linux/macOS
# o
rmdir /s /q venv      # Windows CMD
# o
Remove- Item - Recurse - Force venv  # Windows PowerShell
```

# ## Paso 3: Sincronizar proyecto con UV

```bash
cd windows_interface

# Esto creará un nuevo entorno .venv con Python 3.11
uv sync
```

**¿Qué hace `uv sync`?**
- Detecta que necesitas Python 3.11 (desde `.python- version`)
- Lo instala automáticamente si no lo tienes
- Crea un entorno virtual en `.venv/`
- Instala todas las dependencias de `pyproject.toml`
- Genera `uv.lock` para reproducibilidad exacta

# ## Paso 4: Verificar instalación

```bash
# Ver versión de Python (debe ser 3.11.x)
uv run python - - version

# Ver dependencias instaladas
uv pip list

# Probar el agente
uv run python locknode_agent_https.py
```

# # Comparación de Comandos

| Tarea | Comando ANTERIOR (pip + venv) | Comando NUEVO (uv) |
|- - - - - - - |- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - |- - - - - - - - - - - - - - - - - - - |
| Crear entorno | `python - m venv venv` | *Automático con `uv sync`* |
| Activar entorno | `source venv/bin/activate` | *No necesario con `uv run`* |
| Instalar dependencias | `pip install - r requirements.txt` | `uv sync` |
| Agregar paquete | `pip install paquete` | `uv add paquete` |
| Ejecutar script | `python script.py` | `uv run python script.py` |
| Listar paquetes | `pip list` | `uv pip list` |
| Actualizar paquete | `pip install - - upgrade paquete` | `uv add paquete - - upgrade` |
| Desinstalar paquete | `pip uninstall paquete` | `uv remove paquete` |

# # Flujo de Trabajo Diario

# ## Con el sistema ANTERIOR:
```bash
cd windows_interface
source venv/bin/activate        # Activar entorno
pip install - r requirements.txt # Instalar dependencias
python locknode_agent_https.py  # Ejecutar agente
deactivate                      # Desactivar al terminar
```

# ## Con UV (NUEVO):
```bash
cd windows_interface
uv run python locknode_agent_https.py  # ¡Solo esto!
```

O usando el script de inicio:
```bash
# Windows
start_agent_uv.bat

# Linux/macOS
./start_agent_uv.sh
```

# # Gestión de Dependencias

# ## Agregar nueva dependencia

**ANTES:**
```bash
pip install nuevo- paquete
pip freeze > requirements.txt
```

**AHORA:**
```bash
uv add nuevo- paquete
# Se actualiza pyproject.toml y uv.lock automáticamente
```

# ## Actualizar dependencias

**ANTES:**
```bash
pip install - - upgrade paquete
pip freeze > requirements.txt
```

**AHORA:**
```bash
uv add paquete - - upgrade
# o actualizar todas:
uv sync - - upgrade
```

# ## Dependencias opcionales

Si quieres agregar pyserial (opcional):

```bash
uv add - - optional serial pyserial==3.5
```

Para instalarla:
```bash
uv sync - - extra serial
```

# # Archivos Importantes

Después de la migración tendrás:

```
windows_interface/
├── pyproject.toml      # ← Nuevo: Define proyecto y dependencias
├── .python- version     # ← Nuevo: Especifica Python 3.11
├── uv.lock            # ← Nuevo: Lock file (como package- lock.json)
├── .gitignore         # ← Nuevo: Ignora archivos temporales
├── requirements.txt   # ← Mantenido por compatibilidad
├── .venv/             # ← Nuevo entorno de uv (antes era venv/)
└── ...
```

# # ¿Mantener requirements.txt?

**Sí**, por compatibilidad con sistemas que no usan UV. UV puede sincronizarse con requirements.txt:

```bash
# Generar requirements.txt desde pyproject.toml
uv pip compile pyproject.toml - o requirements.txt

# O instalar desde requirements.txt en UV
uv pip install - r requirements.txt
```

# # Solución de Problemas

# ## "uv: command not found"

UV no está en el PATH. Cierra y abre la terminal, o ejecuta:

**Linux/macOS:**
```bash
source $HOME/.cargo/env
```

**Windows:**
Reinicia PowerShell o agrega `%USERPROFILE%\.cargo\bin` al PATH.

# ## "Python 3.11 not found"

UV lo instalará automáticamente:
```bash
uv python install 3.11
uv sync
```

# ## "Lock file is out of date"

Regenerar el lock file:
```bash
uv lock
uv sync
```

# ## Quiero volver al sistema anterior

1. Elimina `.venv/`, `uv.lock`, `pyproject.toml`, `.python- version`
2. Usa el `requirements.txt` anterior:
   ```bash
   python - m venv venv
   source venv/bin/activate
   pip install - r requirements.txt
   ```

# # Ventajas Inmediatas

- **No más "funciona en mi máquina"**: Todos usan Python 3.11
- **Instalación 10x más rápida**: UV es extremadamente rápido
- **Lock file robusto**: `uv.lock` garantiza las mismas versiones siempre
- **Gestión automática de Python**: No necesitas instalar Python 3.11 manualmente
- **Menos comandos**: `uv run` en lugar de activar/desactivar venv

# # Recursos

- [Documentación oficial de UV](https://docs.astral.sh/uv/)
- [Guía de pyproject.toml](https://packaging.python.org/en/latest/guides/writing- pyproject- toml/)
- [README_UV.md](./README_UV.md) - Guía completa de uso
