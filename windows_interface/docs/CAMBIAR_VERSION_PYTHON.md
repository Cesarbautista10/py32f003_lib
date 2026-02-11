# Cambiar Versión de Python

Esta guía explica cómo cambiar la versión de Python requerida por el proyecto.

## Versión Actual

El proyecto está configurado para usar Python con bloqueo de versión específica definido en:
- `.python-version` - Versión exacta para UV
- `pyproject.toml` - Rango de versiones permitidas

## Pasos para Cambiar la Versión

### 1. Modificar `.python-version`

Edita el archivo `.python-version` en la raíz de `windows_interface/`:

```bash
# Para Python 3.11
3.11

# Para Python 3.12
3.12

# Para Python 3.13
3.13
```

### 2. Modificar `pyproject.toml`

Edita la línea `requires-python` en `pyproject.toml`:

**Para Python 3.11 (bloquear solo 3.11.x):**
```toml
requires-python = ">=3.11,<3.12"
```

**Para Python 3.12 (bloquear solo 3.12.x):**
```toml
requires-python = ">=3.12,<3.13"
```

**Para Python 3.13 (bloquear solo 3.13.x):**
```toml
requires-python = ">=3.13,<3.14"
```

**Para permitir un rango amplio (ej: 3.11 o superior):**
```toml
requires-python = ">=3.11"
```

### 3. Regenerar el Entorno Virtual

Después de cambiar la versión, elimina el entorno viejo y crea uno nuevo:

```bash
cd windows_interface

# Eliminar entorno viejo
rm -rf .venv

# Sincronizar con la nueva versión
uv sync
```

UV automáticamente:
- Detectará la nueva versión requerida
- La instalará si no está disponible
- Creará un nuevo entorno virtual
- Reinstalará todas las dependencias

### 4. Verificar la Versión

Confirma que estás usando la versión correcta:

```bash
uv run python --version
```

## Ejemplos Completos

### Cambiar de Python 3.12 a 3.11

```bash
# 1. Editar .python-version
echo "3.11" > .python-version

# 2. Editar pyproject.toml (cambiar requires-python)
# requires-python = ">=3.11,<3.12"

# 3. Regenerar entorno
rm -rf .venv
uv sync

# 4. Verificar
uv run python --version
# Debería mostrar: Python 3.11.x
```

### Cambiar de Python 3.11 a 3.12

```bash
# 1. Editar .python-version
echo "3.12" > .python-version

# 2. Editar pyproject.toml (cambiar requires-python)
# requires-python = ">=3.12,<3.13"

# 3. Regenerar entorno
rm -rf .venv
uv sync

# 4. Verificar
uv run python --version
# Debería mostrar: Python 3.12.x
```

## Bloqueo de Versiones - Explicación

### Bloqueo Estricto (Recomendado)

```toml
requires-python = ">=3.12,<3.13"
```

- Permite: 3.12.0, 3.12.1, 3.12.2, etc.
- Rechaza: 3.11.x, 3.13.x
- **Ventaja**: Garantiza compatibilidad exacta
- **Uso**: Proyectos de producción

### Bloqueo Flexible

```toml
requires-python = ">=3.11"
```

- Permite: 3.11.x, 3.12.x, 3.13.x, etc.
- Rechaza: 3.10.x y anteriores
- **Ventaja**: Permite actualizaciones automáticas
- **Riesgo**: Pueden surgir incompatibilidades

### Bloqueo Múltiple

```toml
requires-python = ">=3.11,<3.14"
```

- Permite: 3.11.x, 3.12.x, 3.13.x
- Rechaza: 3.10.x, 3.14.x
- **Ventaja**: Balance entre flexibilidad y control
- **Uso**: Desarrollo activo

## Problemas Comunes

### Error: "No interpreter found for Python 3.XX"

UV intentará instalar la versión automáticamente. Si falla:

```bash
# Instalar manualmente con UV
uv python install 3.12

# Listar versiones disponibles
uv python list

# Sincronizar proyecto
uv sync
```

### Error: "Package X requires python_version < 3.13"

Algunas dependencias pueden no ser compatibles con versiones más nuevas. Opciones:

1. Mantener la versión anterior de Python
2. Actualizar la dependencia: `uv add paquete --upgrade`
3. Buscar alternativas compatibles

### Dependencias no se reinstalan correctamente

```bash
# Limpiar todo y empezar de cero
rm -rf .venv uv.lock
uv sync
```

## Compatibilidad de Dependencias

Antes de cambiar la versión de Python, verifica la compatibilidad:

```bash
# Ver dependencias actuales
uv pip list

# Verificar si una versión específica funciona
uv run --python 3.12 python --version
```

### Dependencias Críticas del Proyecto

- **Flask 3.0.0**: Compatible con Python 3.8+
- **pyocd 0.36.0+**: Compatible con Python 3.7+
- **pyusb 1.2.1**: Compatible con Python 3.6+

Todas las dependencias actuales son compatibles con Python 3.11, 3.12 y 3.13.

## Recomendaciones

1. **Producción**: Usa bloqueo estricto (ej: `>=3.12,<3.13`)
2. **Desarrollo**: Puedes usar bloqueo flexible si prefieres
3. **Pruebas**: Verifica que todas las funciones trabajen después del cambio
4. **Documentación**: Actualiza el README si cambias la versión oficial

## Actualizar Documentación

Después de cambiar la versión, actualiza:

1. `README.md` - Mencionar la nueva versión requerida
2. `docs/README_UV.md` - Actualizar referencias
3. Scripts de instalación (`install_uv_setup.sh`, etc.)

## Scripts Automatizados

Puedes crear un script para facilitar el cambio:

```bash
#!/bin/bash
# cambiar_python.sh
VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Uso: ./cambiar_python.sh 3.12"
    exit 1
fi

echo "Cambiando a Python $VERSION..."

# Actualizar .python-version
echo "$VERSION" > .python-version

# Actualizar pyproject.toml
NEXT_VERSION=$(echo "$VERSION + 0.01" | bc | cut -d. -f1,2)
sed -i "s/requires-python = \">=.*\"/requires-python = \">=$VERSION,<$NEXT_VERSION\"/" pyproject.toml

# Regenerar entorno
rm -rf .venv
uv sync

echo "Versión actualizada a:"
uv run python --version
```

Uso:
```bash
chmod +x cambiar_python.sh
./cambiar_python.sh 3.12
```

## Referencias

- [Documentación de UV](https://docs.astral.sh/uv/)
- [PEP 621 - pyproject.toml](https://peps.python.org/pep-0621/)
- [Versiones de Python](https://www.python.org/downloads/)
