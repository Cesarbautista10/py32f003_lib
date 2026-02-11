# Inicio Rápido - UV Setup

## Para Empezar YA (3 comandos)

```bash
# 1. Instalar UV (solo primera vez)
curl -LsSf https://astral.sh/uv/install.sh | sh
source $HOME/.cargo/env

# 2. Sincronizar proyecto (instala Python 3.11 + dependencias)
cd windows_interface
uv sync

# 3. Ejecutar el agente
uv run python locknode_agent_https.py
```

## En Windows

```powershell
# 1. Instalar UV (PowerShell como Administrador)
powershell -c "irm https://astral.sh/uv/install.ps1 | iex"

# 2. Reiniciar PowerShell, luego:
cd windows_interface
uv sync

# 3. Ejecutar
uv run python locknode_agent_https.py

# O usar el script:
start_agent_uv.bat
```

## En Linux (más fácil aún)

```bash
cd windows_interface

# Script todo-en-uno (instala UV y configura proyecto)
./install_uv_setup.sh

# Después ejecutar:
./start_agent_uv.sh
```

## ¿Qué hace esto?

- Instala Python 3.11 automáticamente
- Bloquea la versión para evitar incompatibilidades
- Instala todas las dependencias (Flask, pyocd, etc.)
- Crea entorno aislado en `.venv/`
- Listo para ejecutar en segundos

## Más Información

- **Guía completa**: [README_UV.md](./README_UV.md)
- **Migración desde pip**: [MIGRACION_UV.md](./MIGRACION_UV.md)
- **Resumen del setup**: [RESUMEN_UV.md](./RESUMEN_UV.md)

## Comandos Diarios

```bash
# Ejecutar agente
uv run python locknode_agent_https.py

# Ver versión Python (debe ser 3.11.x)
uv run python --version

# Agregar dependencia
uv add nombre-paquete

# Ver dependencias
uv pip list
```

**¡Eso es todo!**

