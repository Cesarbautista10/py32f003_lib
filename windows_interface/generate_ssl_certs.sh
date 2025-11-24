#!/bin/bash

# Script para generar certificados SSL auto-firmados para desarrollo
# Estos certificados permiten usar HTTPS en el agente local

echo "=================================================="
echo "🔐 Generador de Certificados SSL para LockNode"
echo "=================================================="
echo ""

# Crear directorio para certificados
mkdir -p certs

# Verificar si OpenSSL está instalado
if ! command -v openssl &> /dev/null; then
    echo "❌ Error: OpenSSL no está instalado"
    echo "   Instala con: sudo apt-get install openssl"
    exit 1
fi

# Verificar si ya existen certificados
if [ -f "certs/cert.pem" ] && [ -f "certs/key.pem" ]; then
    echo "⚠️  Los certificados ya existen:"
    echo "   - certs/cert.pem"
    echo "   - certs/key.pem"
    echo ""
    read -p "¿Deseas regenerarlos? (s/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Ss]$ ]]; then
        echo "Operación cancelada"
        exit 0
    fi
    echo ""
fi

# Generar certificado SSL auto-firmado
echo "🔨 Generando certificado SSL..."
echo ""

openssl req -x509 -newkey rsa:4096 -nodes \
    -out certs/cert.pem \
    -keyout certs/key.pem \
    -days 365 \
    -subj "/C=MX/ST=Estado/L=Ciudad/O=UNIT Electronics/OU=Development/CN=localhost"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Certificados generados exitosamente:"
    echo "   📄 certs/cert.pem (Certificado público)"
    echo "   🔑 certs/key.pem  (Clave privada)"
    echo ""
    echo "⏰ Validez: 365 días"
    echo ""
    echo "=================================================="
    echo "🚀 Ahora puedes ejecutar el agente con HTTPS:"
    echo "   python locknode_agent_https.py"
    echo "=================================================="
else
    echo ""
    echo "❌ Error al generar certificados"
    exit 1
fi
