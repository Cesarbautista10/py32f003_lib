"""
Agente Local para LockNode - Servidor Python
Este servidor se comunica con el panel web frontend para realizar
operaciones de carga y descarga de firmware.

Requisitos:
    pip install flask flask-cors

Uso:
    python locknode_agent.py
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
from datetime import datetime
import logging

# Configuración del servidor
app = Flask(__name__)
CORS(app)  # Permitir peticiones desde el frontend

# Configuración de logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Estado del agente
agent_state = {
    'connected': False,
    'last_command': None,
    'firmware_status': 'idle',
    'device_connected': False
}

@app.route('/api/locknode/connect', methods=['POST'])
def establish_connection():
    """Establece la conexión con el panel web"""
    try:
        data = request.json
        logger.info(f"Estableciendo conexión. Timestamp: {data.get('timestamp')}")
        
        agent_state['connected'] = True
        agent_state['firmware_status'] = 'ready'
        
        return jsonify({
            'success': True,
            'message': 'Agente local conectado correctamente',
            'timestamp': datetime.now().isoformat(),
            'agent_version': '1.0.0'
        }), 200
    except Exception as e:
        logger.error(f"Error al conectar: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/locknode/disconnect', methods=['POST'])
def disconnect():
    """Desconecta el agente"""
    try:
        logger.info("Desconectando agente local")
        
        agent_state['connected'] = False
        agent_state['firmware_status'] = 'idle'
        
        return jsonify({
            'success': True,
            'message': 'Agente desconectado',
            'timestamp': datetime.now().isoformat()
        }), 200
    except Exception as e:
        logger.error(f"Error al desconectar: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/locknode/command', methods=['POST'])
def execute_command():
    """Ejecuta un comando enviado desde el panel"""
    try:
        data = request.json
        command = data.get('command', '')
        timestamp = data.get('timestamp')
        
        logger.info(f"Comando recibido: {command} at {timestamp}")
        
        agent_state['last_command'] = command
        
        # Aquí puedes procesar el comando según tu lógica
        # Por ejemplo, enviar comandos al dispositivo LockNode
        
        response_message = f"Comando '{command}' ejecutado correctamente"
        
        # Ejemplo de respuesta según el comando
        if command.lower() == 'status':
            response_message = f"Estado del dispositivo: {'Conectado' if agent_state['device_connected'] else 'Desconectado'}"
        elif command.lower() == 'ping':
            response_message = "Pong! Agente respondiendo"
        elif command.lower().startswith('write'):
            response_message = "Datos escritos en el dispositivo"
        
        return jsonify({
            'success': True,
            'response': response_message,
            'timestamp': datetime.now().isoformat()
        }), 200
    except Exception as e:
        logger.error(f"Error al ejecutar comando: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/locknode/upload-firmware', methods=['POST'])
def upload_firmware():
    """Carga firmware al dispositivo LockNode"""
    try:
        data = request.json
        logger.info(f"Iniciando carga de firmware. Timestamp: {data.get('timestamp')}")
        
        agent_state['firmware_status'] = 'uploading'
        
        # Aquí implementarías la lógica real para cargar firmware
        # Por ejemplo:
        # 1. Leer archivo de firmware
        # 2. Conectar con el dispositivo LockNode
        # 3. Enviar firmware en bloques
        # 4. Verificar integridad
        
        # Simulación de carga exitosa
        agent_state['firmware_status'] = 'uploaded'
        
        return jsonify({
            'success': True,
            'message': 'Firmware cargado exitosamente al dispositivo',
            'timestamp': datetime.now().isoformat(),
            'firmware_version': '1.2.3',
            'bytes_uploaded': 245760
        }), 200
    except Exception as e:
        logger.error(f"Error al cargar firmware: {str(e)}")
        agent_state['firmware_status'] = 'error'
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/locknode/download-firmware', methods=['POST'])
def download_firmware():
    """Descarga firmware del dispositivo LockNode"""
    try:
        data = request.json
        logger.info(f"Iniciando descarga de firmware. Timestamp: {data.get('timestamp')}")
        
        agent_state['firmware_status'] = 'downloading'
        
        # Aquí implementarías la lógica real para descargar firmware
        # Por ejemplo:
        # 1. Conectar con el dispositivo LockNode
        # 2. Solicitar lectura de firmware
        # 3. Recibir datos en bloques
        # 4. Guardar archivo localmente
        
        # Simulación de descarga exitosa
        agent_state['firmware_status'] = 'downloaded'
        
        return jsonify({
            'success': True,
            'message': 'Firmware descargado exitosamente del dispositivo',
            'timestamp': datetime.now().isoformat(),
            'firmware_version': '1.2.3',
            'bytes_downloaded': 245760,
            'saved_path': './firmware_backup.bin'
        }), 200
    except Exception as e:
        logger.error(f"Error al descargar firmware: {str(e)}")
        agent_state['firmware_status'] = 'error'
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/locknode/heartbeat', methods=['GET'])
def heartbeat():
    """Endpoint para verificar que el agente está activo"""
    return jsonify({
        'alive': True,
        'timestamp': datetime.now().isoformat(),
        'state': agent_state
    }), 200

@app.route('/api/locknode/status', methods=['GET'])
def get_status():
    """Obtiene el estado completo del agente"""
    return jsonify({
        'success': True,
        'state': agent_state,
        'timestamp': datetime.now().isoformat()
    }), 200

# Endpoint adicional para configurar el dispositivo
@app.route('/api/locknode/device/connect', methods=['POST'])
def connect_device():
    """Conecta con el dispositivo físico LockNode"""
    try:
        data = request.json
        port = data.get('port', 'COM3')  # Puerto serial del dispositivo
        
        logger.info(f"Conectando con dispositivo en puerto {port}")
        
        # Aquí implementarías la conexión serial real
        # import serial
        # device = serial.Serial(port, baudrate=115200, timeout=1)
        
        agent_state['device_connected'] = True
        
        return jsonify({
            'success': True,
            'message': f'Dispositivo conectado en puerto {port}',
            'timestamp': datetime.now().isoformat()
        }), 200
    except Exception as e:
        logger.error(f"Error al conectar dispositivo: {str(e)}")
        return jsonify({'success': False, 'error': str(e)}), 500

@app.route('/api/locknode/detect-cmsis-dap', methods=['GET'])
def detect_cmsis_dap():
    """Detecta dispositivos CMSIS-DAP conectados"""
    try:
        logger.info("Buscando dispositivos CMSIS-DAP...")
        
        devices_found = []
        
        # Intentar primero con pyocd (más confiable en Windows)
        try:
            from pyocd.probe import aggregator
            
            logger.info("Usando pyocd para detectar dispositivos...")
            probes = aggregator.DebugProbeAggregator.get_all_connected_probes()
            for probe in probes:
                devices_found.append({
                    'vendor_id': f"0x{probe.vendor_id:04x}" if hasattr(probe, 'vendor_id') else 'N/A',
                    'product_id': f"0x{probe.product_id:04x}" if hasattr(probe, 'product_id') else 'N/A',
                    'manufacturer': probe.vendor_name if hasattr(probe, 'vendor_name') else 'Unknown',
                    'product': probe.product_name if hasattr(probe, 'product_name') else 'CMSIS-DAP',
                    'unique_id': probe.unique_id if hasattr(probe, 'unique_id') else 'N/A'
                })
            
            logger.info(f"pyocd encontró {len(devices_found)} dispositivo(s)")
            
        except ImportError:
            logger.warning("pyocd no está instalado, intentando con pyusb...")
            # Si pyocd no está, intentar con pyusb (puede requerir drivers en Windows)
            try:
                import usb.core
                import usb.util
                
                # Buscar dispositivos USB
                # CMSIS-DAP típicamente usa vendor ID 0x0d28 (ARM) pero puede variar
                # También buscar por interfaz HID o vendor class
                all_devices = usb.core.find(find_all=True)
                
                for device in all_devices:
                    try:
                        # Intentar leer el descriptor del producto
                        product_name = usb.util.get_string(device, device.iProduct) if device.iProduct else "Unknown"
                        manufacturer = usb.util.get_string(device, device.iManufacturer) if device.iManufacturer else "Unknown"
                        
                        # Buscar dispositivos que contengan palabras clave relacionadas con CMSIS-DAP
                        keywords = ['cmsis', 'dap', 'daplink', 'jlink', 'stlink', 'debugger', 'probe']
                        device_info = f"{product_name} {manufacturer}".lower()
                        
                        if any(keyword in device_info for keyword in keywords):
                            devices_found.append({
                                'vendor_id': f"0x{device.idVendor:04x}",
                                'product_id': f"0x{device.idProduct:04x}",
                                'manufacturer': manufacturer,
                                'product': product_name,
                                'bus': device.bus,
                                'address': device.address
                            })
                    except (ValueError, usb.core.USBError) as e:
                        # Algunos dispositivos pueden no tener strings descriptivos
                        continue
                        
            except (ImportError, Exception) as e:
                logger.error(f"No se pudo usar pyusb: {e}")
                return jsonify({
                    'success': False,
                    'error': 'No hay librerías de detección instaladas o falta configuración. Instala: pip install pyocd',
                    'devices': [],
                    'count': 0
                }), 200
        except Exception as e:
            logger.error(f"Error al usar pyocd: {e}")
            return jsonify({
                'success': False,
                'error': f'Error al detectar dispositivos con pyocd: {str(e)}',
                'devices': [],
                'count': 0
            }), 200
        
        if devices_found:
            logger.info(f"Se encontraron {len(devices_found)} dispositivo(s) CMSIS-DAP")
            return jsonify({
                'success': True,
                'message': f'Se encontraron {len(devices_found)} dispositivo(s) CMSIS-DAP',
                'devices': devices_found,
                'count': len(devices_found),
                'timestamp': datetime.now().isoformat()
            }), 200
        else:
            logger.info("No se encontraron dispositivos CMSIS-DAP")
            return jsonify({
                'success': True,
                'message': 'No se encontraron dispositivos CMSIS-DAP conectados',
                'devices': [],
                'count': 0,
                'timestamp': datetime.now().isoformat()
            }), 200
            
    except Exception as e:
        logger.error(f"Error al detectar dispositivos CMSIS-DAP: {str(e)}")
        return jsonify({
            'success': False,
            'error': str(e),
            'devices': [],
            'count': 0
        }), 500

@app.route('/api/locknode/firmware/list', methods=['GET'])
def list_firmware_files():
    """Lista los archivos de firmware disponibles en el directorio firmware/"""
    try:
        import os
        
        firmware_dir = os.path.join(os.path.dirname(__file__), 'firmware')
        
        if not os.path.exists(firmware_dir):
            return jsonify({
                'success': False,
                'error': 'Directorio firmware/ no encontrado',
                'files': []
            }), 404
        
        # Buscar archivos .bin en el directorio firmware
        firmware_files = []
        for file in os.listdir(firmware_dir):
            if file.endswith('.bin'):
                file_path = os.path.join(firmware_dir, file)
                file_size = os.path.getsize(file_path)
                file_mtime = os.path.getmtime(file_path)
                
                firmware_files.append({
                    'name': file,
                    'path': file,
                    'size': file_size,
                    'size_kb': round(file_size / 1024, 2),
                    'modified': datetime.fromtimestamp(file_mtime).isoformat()
                })
        
        logger.info(f"Se encontraron {len(firmware_files)} archivo(s) de firmware")
        
        return jsonify({
            'success': True,
            'files': firmware_files,
            'count': len(firmware_files),
            'firmware_dir': firmware_dir
        }), 200
        
    except Exception as e:
        logger.error(f"Error al listar archivos de firmware: {str(e)}")
        return jsonify({
            'success': False,
            'error': str(e),
            'files': []
        }), 500

@app.route('/api/locknode/firmware/program', methods=['POST'])
def program_firmware():
    """Programa el firmware usando pyocd"""
    try:
        import os
        import subprocess
        
        data = request.json
        firmware_file = data.get('firmware_file')
        target = data.get('target', 'py32f003x4')
        
        if not firmware_file:
            return jsonify({
                'success': False,
                'error': 'No se especificó archivo de firmware'
            }), 400
        
        # Construir ruta completa al firmware
        firmware_dir = os.path.join(os.path.dirname(__file__), 'firmware')
        firmware_path = os.path.join(firmware_dir, firmware_file)
        config_path = os.path.join(firmware_dir, 'Misc', 'pyocd.yaml')
        
        if not os.path.exists(firmware_path):
            return jsonify({
                'success': False,
                'error': f'Archivo de firmware no encontrado: {firmware_file}'
            }), 404
        
        if not os.path.exists(config_path):
            return jsonify({
                'success': False,
                'error': 'Archivo de configuración pyocd.yaml no encontrado'
            }), 404
        
        logger.info(f"Programando firmware: {firmware_file}")
        logger.info(f"Target: {target}")
        
        # Cambiar al directorio firmware para que pyocd.yaml encuentre los packs
        original_dir = os.getcwd()
        os.chdir(firmware_dir)
        
        try:
            # Paso 1: Borrar el chip
            logger.info("Borrando chip...")
            erase_result = subprocess.run(
                ['python', '-m', 'pyocd', 'erase', '-t', target, '--chip', '--config', 'Misc/pyocd.yaml'],
                capture_output=True,
                text=True,
                timeout=30
            )
            
            # Analizar errores específicos en el borrado
            if erase_result.returncode != 0:
                error_output = erase_result.stderr.lower()
                if 'no device' in error_output or 'failed to find' in error_output:
                    raise Exception("DEVICE_NOT_FOUND: No se detectó el programador CMSIS-DAP. Verifique la conexión USB.")
                elif 'failed to connect' in error_output or 'connection failed' in error_output:
                    raise Exception("CONNECTION_FAILED: No se pudo conectar al dispositivo. El programador puede haberse desconectado.")
                elif 'timeout' in error_output:
                    raise Exception("TIMEOUT_ERASE: Timeout durante el borrado del chip.")
                else:
                    raise Exception(f"ERASE_ERROR: Error al borrar el chip - {erase_result.stderr}")
            
            logger.info("Chip borrado exitosamente")
            
            # Paso 2: Cargar el firmware
            logger.info("Cargando firmware...")
            load_result = subprocess.run(
                ['python', '-m', 'pyocd', 'load', firmware_file, '-t', target, '--config', 'Misc/pyocd.yaml'],
                capture_output=True,
                text=True,
                timeout=60
            )
            
            # Analizar errores específicos en la carga
            if load_result.returncode != 0:
                error_output = load_result.stderr.lower()
                if 'no device' in error_output or 'failed to find' in error_output:
                    raise Exception("DEVICE_DISCONNECTED: El programador se desconectó durante la carga del firmware.")
                elif 'failed to connect' in error_output or 'connection failed' in error_output:
                    raise Exception("CONNECTION_LOST: Se perdió la conexión con el dispositivo durante la programación.")
                elif 'flash' in error_output and 'failed' in error_output:
                    raise Exception("FLASH_FAILED: Error al escribir en la memoria flash. Verifique el dispositivo y la conexión.")
                elif 'verify' in error_output and 'failed' in error_output:
                    raise Exception("VERIFY_FAILED: La verificación del firmware falló. El firmware no se cargó correctamente.")
                elif 'timeout' in error_output:
                    raise Exception("TIMEOUT_LOAD: Timeout durante la carga del firmware.")
                elif 'file not found' in error_output or 'cannot open' in error_output:
                    raise Exception("FILE_ERROR: No se pudo leer el archivo de firmware.")
                else:
                    raise Exception(f"LOAD_ERROR: Error al cargar el firmware - {load_result.stderr}")
            
            logger.info("Firmware cargado exitosamente")
            
            return jsonify({
                'success': True,
                'message': 'Firmware programado exitosamente',
                'firmware_file': firmware_file,
                'target': target,
                'erase_output': erase_result.stdout,
                'load_output': load_result.stdout,
                'timestamp': datetime.now().isoformat()
            }), 200
            
        finally:
            # Volver al directorio original
            os.chdir(original_dir)
        
    except subprocess.TimeoutExpired as e:
        logger.error("Timeout al programar firmware")
        # Determinar en qué fase ocurrió el timeout
        if 'erase' in str(e.cmd):
            error_msg = "TIMEOUT_ERASE: El borrado del chip tardó demasiado. Verifique la conexión."
        else:
            error_msg = "TIMEOUT_LOAD: La carga del firmware tardó demasiado. Verifique la conexión."
        return jsonify({
            'success': False,
            'error': error_msg
        }), 500
    except Exception as e:
        error_str = str(e)
        logger.error(f"Error al programar firmware: {error_str}")
        
        # Si el error ya tiene un código, pasarlo tal cual
        # Si no, agregarlo como error genérico
        if not any(code in error_str for code in ['DEVICE_', 'CONNECTION_', 'TIMEOUT_', 'ERASE_', 'LOAD_', 'FLASH_', 'VERIFY_', 'FILE_']):
            error_str = f"UNKNOWN_ERROR: {error_str}"
        
        return jsonify({
            'success': False,
            'error': error_str
        }), 500

if __name__ == '__main__':
    print("=" * 60)
    print("🚀 Agente Local LockNode iniciado (HTTP)")
    print("=" * 60)
    print(f"📡 Acceso local:  http://localhost:8081")
    print(f"📡 Acceso red:    http://<tu-ip>:8081")
    print(f"🔗 Esperando conexión del panel web...")
    print("=" * 60)
    print("💡 Configura el Host/IP en el panel web:")
    print("   - Usa 'localhost' para acceso local")
    print("   - Usa tu IP de red (ej: 192.168.15.91) para acceso desde otros dispositivos")
    print("=" * 60)
    
    # Iniciar el servidor Flask
    app.run(
        host='0.0.0.0',  # Permitir conexiones desde cualquier IP
        port=8081,
        debug=True
    )
