# Guía de Configuración HTTP vs HTTPS para LockNode Agent

## 📋 Resumen

El agente LockNode puede funcionar tanto en **HTTP** como en **HTTPS**. Esta guía te ayudará a elegir y configurar la opción correcta.

---

## 🎯 ¿Cuál Debo Usar?

### ✅ **HTTP** (Recomendado para Desarrollo)

**Usa HTTP si:**
- Estás en desarrollo local
- El panel web corre en `http://localhost:5173`
- No necesitas cifrado (todo es local)

**Ventajas:**
- ✅ Configuración simple
- ✅ Sin certificados
- ✅ Funciona de inmediato

**Archivo:** `locknode_agent.py`

---

### 🔒 **HTTPS** (Recomendado para Producción)

**Usa HTTPS si:**
- El panel web está en producción con HTTPS
- Necesitas comunicación cifrada
- Quieres evitar advertencias de "Mixed Content"

**Ventajas:**
- ✅ Conexión cifrada
- ✅ Compatible con sitios HTTPS
- ✅ Más seguro

**Archivo:** `locknode_agent_https.py`

---

## 🚀 Configuración HTTP (Simple)

### 1. Usar el Agente HTTP

```bash
# Ejecutar directamente
python locknode_agent.py
```

### 2. Configurar el Panel

En `Panel_LockNode_Programacion.tsx`:
```typescript
const AGENT_BASE_URL = 'http://localhost:8081';  // ← HTTP
```

### 3. Listo ✅

El agente estará disponible en `http://localhost:8081`

---

## 🔐 Configuración HTTPS (Avanzada)

### 1. Generar Certificados SSL

**Opción A: Script Automático (Recomendado)**
```bash
chmod +x generate_ssl_certs.sh
./generate_ssl_certs.sh
```

**Opción B: Comando Manual**
```bash
openssl req -x509 -newkey rsa:4096 -nodes \
  -out cert.pem \
  -keyout key.pem \
  -days 365 \
  -subj "/C=MX/ST=Estado/L=Ciudad/O=UNIT Electronics/OU=Development/CN=localhost"
```

Esto genera:
- `cert.pem` - Certificado público
- `key.pem` - Clave privada

### 2. Ejecutar el Agente HTTPS

```bash
python locknode_agent_https.py
```

### 3. Configurar el Panel

En `Panel_LockNode_Programacion.tsx`:
```typescript
const AGENT_BASE_URL = 'https://localhost:8081';  // ← HTTPS
```

### 4. Aceptar Certificado en el Navegador

Al acceder por primera vez, el navegador mostrará una advertencia porque el certificado es auto-firmado:

**Chrome/Edge:**
1. Aparecerá "Tu conexión no es privada"
2. Haz clic en "Avanzado"
3. Haz clic en "Continuar a localhost (no seguro)"

**Firefox:**
1. Aparecerá "Advertencia: Riesgo potencial de seguridad"
2. Haz clic en "Avanzado"
3. Haz clic en "Aceptar el riesgo y continuar"

### 5. Listo ✅

El agente estará disponible en `https://localhost:8081`

---

## 📊 Comparación de Escenarios

| Escenario | Panel Web | Agente | ¿Funciona? | Recomendación |
|-----------|-----------|--------|------------|---------------|
| **Desarrollo Local** | `http://localhost:5173` | `http://localhost:8081` | ✅ Sí | **Usar HTTP** |
| **Desarrollo Local** | `http://localhost:5173` | `https://localhost:8081` | ✅ Sí | Opcional |
| **Producción** | `https://dominio.com` | `http://localhost:8081` | ❌ No (Mixed Content) | **Usar HTTPS** |
| **Producción** | `https://dominio.com` | `https://localhost:8081` | ✅ Sí | **Correcto** |

---

## 🔍 Cómo Saber Si Estás Usando HTTP o HTTPS

En el panel web, verás un indicador en la sección "Estado de Conexión":

- 🔓 **HTTP** - Badge naranja
- 🔒 **HTTPS** - Badge verde

---

## 🛠️ Cambiar Entre HTTP y HTTPS

### Desde HTTP → HTTPS

**1. Panel Web:**
```typescript
// Panel_LockNode_Programacion.tsx
const AGENT_BASE_URL = 'https://localhost:8081';  // ← Cambiar a HTTPS
```

**2. Agente Python:**
```bash
# Detener agente HTTP (Ctrl+C)
# Generar certificados si no existen
./generate_ssl_certs.sh
# Ejecutar agente HTTPS
python locknode_agent_https.py
```

### Desde HTTPS → HTTP

**1. Panel Web:**
```typescript
// Panel_LockNode_Programacion.tsx
const AGENT_BASE_URL = 'http://localhost:8081';  // ← Cambiar a HTTP
```

**2. Agente Python:**
```bash
# Detener agente HTTPS (Ctrl+C)
# Ejecutar agente HTTP
python locknode_agent.py
```

---

## ⚠️ Solución de Problemas

### Error: "Mixed Content"

**Problema:** El panel HTTPS no puede conectarse al agente HTTP

**Solución:**
1. Usa el agente HTTPS: `python locknode_agent_https.py`
2. O sirve el panel en HTTP en desarrollo

---

### Error: "Certificados no encontrados"

**Problema:** `locknode_agent_https.py` no encuentra `cert.pem` y `key.pem`

**Solución:**
```bash
./generate_ssl_certs.sh
```

---

### Error: "Connection refused"

**Problema:** No se puede conectar al agente

**Verificar:**
1. ¿El agente está corriendo?
   ```bash
   # Debería mostrar: "Escuchando en: http://localhost:8081"
   ```

2. ¿El puerto es correcto?
   - Panel: Revisa `AGENT_BASE_URL`
   - Agente: Revisa `app.run(port=8081)`

3. ¿HTTP o HTTPS coincide en ambos lados?

---

### Advertencia: "Certificado no válido"

**Problema:** El navegador advierte sobre el certificado

**Esto es normal** con certificados auto-firmados. Es seguro en desarrollo local.

**Para producción:** Usa certificados de Let's Encrypt o una CA válida.

---

## 🎓 Para Producción Real

Si despliegas en producción, considera:

1. **Certificados válidos:**
   - Usar Let's Encrypt (gratis)
   - O certificados de una CA comercial

2. **Proxy inverso:**
   - Nginx o Apache delante del agente
   - Manejar SSL/TLS en el proxy

3. **Firewall:**
   - Limitar acceso solo desde IPs conocidas
   - Usar autenticación adicional

4. **Tokens de API:**
   - Implementar autenticación por token
   - Validar cada petición

---

## 📚 Archivos Relacionados

| Archivo | Propósito |
|---------|-----------|
| `locknode_agent.py` | Agente HTTP (desarrollo) |
| `locknode_agent_https.py` | Agente HTTPS (producción) |
| `generate_ssl_certs.sh` | Generador de certificados SSL |
| `Panel_LockNode_Programacion.tsx` | Panel web frontend |

---

## 💡 Consejos

1. **Desarrollo:** Usa HTTP por simplicidad
2. **Producción:** Siempre usa HTTPS
3. **Testing:** Prueba ambos modos antes de desplegar
4. **Certificados:** Renueva certificados antes de que expiren (365 días)

---

**¿Necesitas ayuda?** Consulta la documentación completa en `docs/locknode-panel-setup.md`
