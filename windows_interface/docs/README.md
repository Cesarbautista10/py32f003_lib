# Documentación - LockNode Agent

Índice de documentación del proyecto LockNode Agent.

## Guías de Inicio

- **[INICIO_RAPIDO.md](INICIO_RAPIDO.md)** - Guía de inicio rápido (3 comandos para empezar)
- **[README_UV.md](README_UV.md)** - Guía completa de uso con UV

## Configuración y Gestión

- **[MIGRACION_UV.md](MIGRACION_UV.md)** - Migración desde pip/venv a UV
- **[RESUMEN_UV.md](RESUMEN_UV.md)** - Resumen del setup con UV
- **[CAMBIAR_VERSION_PYTHON.md](CAMBIAR_VERSION_PYTHON.md)** - Cómo cambiar la versión de Python

## Estructura de la Documentación

```
docs/
├── README.md                    # Este archivo (índice)
├── INICIO_RAPIDO.md             # Para empezar rápidamente
├── README_UV.md                 # Guía completa de UV
├── MIGRACION_UV.md              # Migrar desde pip/venv
├── RESUMEN_UV.md                # Resumen del proyecto
└── CAMBIAR_VERSION_PYTHON.md   # Cambiar versiones de Python
```

## Orden de Lectura Recomendado

### Si eres nuevo en el proyecto:
1. [INICIO_RAPIDO.md](INICIO_RAPIDO.md) - Para instalar y ejecutar rápidamente
2. [README_UV.md](README_UV.md) - Para entender UV a fondo
3. [CAMBIAR_VERSION_PYTHON.md](CAMBIAR_VERSION_PYTHON.md) - Si necesitas otra versión de Python

### Si vienes de pip/venv:
1. [MIGRACION_UV.md](MIGRACION_UV.md) - Entender las diferencias
2. [RESUMEN_UV.md](RESUMEN_UV.md) - Ver qué cambió en el proyecto
3. [README_UV.md](README_UV.md) - Comandos y uso diario

## Documentación del README Principal

El archivo [../README.md](../README.md) contiene:
- Estructura del proyecto
- Guía de instalación tradicional
- Configuración de HTTPS/SSL
- Solución de problemas
- Referencias adicionales

## Temas Cubiertos

### Gestión de Dependencias
- Instalación y configuración de UV
- Sincronización de paquetes
- Agregar/actualizar dependencias
- Lock files y reproducibilidad

### Entornos Virtuales
- Creación automática con UV
- Comparación con venv tradicional
- Gestión de múltiples entornos

### Versiones de Python
- Bloqueo de versiones
- Cambio entre versiones
- Compatibilidad de dependencias
- Instalación automática de Python

### Migración y Compatibilidad
- Desde pip + venv a UV
- Mantener requirements.txt
- Comparación de comandos
- Resolución de problemas

## Contribuir a la Documentación

Si encuentras errores o quieres mejorar la documentación:

1. Edita el archivo correspondiente en `docs/`
2. Mantén el formato Markdown consistente
3. Actualiza este índice si agregas nuevos documentos
4. Verifica los enlaces internos

## Recursos Externos

- [Documentación oficial de UV](https://docs.astral.sh/uv/)
- [PEP 621 - pyproject.toml](https://peps.python.org/pep-0621/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [pyOCD Documentation](https://pyocd.io/)

## Contacto

Para preguntas o problemas no cubiertos en la documentación, contacta al equipo de desarrollo.
