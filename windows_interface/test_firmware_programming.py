"""
Script de prueba para verificar la programación de firmware
Ejecutar con: python test_firmware_programming.py
"""

import requests
import sys
import json

BASE_URL = "http://localhost:8081"

def test_connection():
    """Prueba la conexión con el agente"""
    print("=" * 60)
    print("Probando conexión con el agente...")
    print("=" * 60)
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/locknode/connect",
            json={"action": "establish_link"},
            timeout=5
        )
        
        if response.ok:
            data = response.json()
            print(f"✓ Conexión exitosa: {data.get('message')}")
            return True
        else:
            print(f"✗ Error al conectar: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Error de conexión: {e}")
        print("  Asegúrate de que el agente esté ejecutándose:")
        print("  python locknode_agent.py")
        return False

def test_list_firmware():
    """Prueba el listado de firmwares"""
    print("\n" + "=" * 60)
    print("Listando archivos de firmware disponibles...")
    print("=" * 60)
    
    try:
        response = requests.get(
            f"{BASE_URL}/api/locknode/firmware/list",
            timeout=5
        )
        
        if response.ok:
            data = response.json()
            
            if data['success']:
                files = data['files']
                print(f"✓ Se encontraron {len(files)} archivo(s) de firmware:")
                
                for i, file in enumerate(files, 1):
                    print(f"\n  Archivo {i}:")
                    print(f"    Nombre: {file['name']}")
                    print(f"    Tamaño: {file['size_kb']} KB")
                    print(f"    Modificado: {file['modified']}")
                
                return files
            else:
                print(f"✗ Error: {data.get('error')}")
                return []
        else:
            print(f"✗ Error HTTP: {response.status_code}")
            return []
    except Exception as e:
        print(f"✗ Error al listar firmwares: {e}")
        return []

def test_detect_cmsis_dap():
    """Prueba la detección de dispositivos CMSIS-DAP"""
    print("\n" + "=" * 60)
    print("Detectando dispositivos CMSIS-DAP...")
    print("=" * 60)
    
    try:
        response = requests.get(
            f"{BASE_URL}/api/locknode/detect-cmsis-dap",
            timeout=5
        )
        
        if response.ok:
            data = response.json()
            
            if data['success'] and data['count'] > 0:
                devices = data['devices']
                print(f"✓ Se encontraron {len(devices)} dispositivo(s):")
                
                for i, device in enumerate(devices, 1):
                    print(f"\n  Dispositivo {i}:")
                    print(f"    Producto: {device['product']}")
                    print(f"    Fabricante: {device['manufacturer']}")
                    print(f"    VID: {device['vendor_id']}, PID: {device['product_id']}")
                
                return True
            else:
                print(f"⚠ {data.get('message', 'No se encontraron dispositivos')}")
                print("  Conecta un dispositivo CMSIS-DAP antes de programar")
                return False
        else:
            print(f"✗ Error HTTP: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Error al detectar dispositivos: {e}")
        return False

def test_program_firmware(firmware_file, dry_run=True):
    """Prueba la programación de firmware"""
    print("\n" + "=" * 60)
    if dry_run:
        print("SIMULACIÓN: Programación de firmware (dry-run)")
    else:
        print("PROGRAMANDO FIRMWARE (REAL)")
    print("=" * 60)
    
    print(f"\nArchivo: {firmware_file}")
    print(f"Target: py32f003x4")
    
    if dry_run:
        print("\n⚠️  DRY RUN: No se programará realmente")
        print("   Para programar de verdad, ejecuta:")
        print(f"   python test_firmware_programming.py --program {firmware_file}")
        return True
    
    try:
        print("\n⏳ Enviando comando de programación...")
        
        response = requests.post(
            f"{BASE_URL}/api/locknode/firmware/program",
            json={
                "firmware_file": firmware_file,
                "target": "py32f003x4"
            },
            timeout=120  # 2 minutos para programación completa
        )
        
        if response.ok:
            data = response.json()
            
            if data['success']:
                print("\n✓ FIRMWARE PROGRAMADO EXITOSAMENTE")
                print(f"  Archivo: {data['firmware_file']}")
                print(f"  Target: {data['target']}")
                print(f"  Timestamp: {data['timestamp']}")
                
                if 'erase_output' in data and data['erase_output']:
                    print("\n  Salida de borrado:")
                    for line in data['erase_output'].split('\n')[:5]:
                        if line.strip():
                            print(f"    {line}")
                
                if 'load_output' in data and data['load_output']:
                    print("\n  Salida de carga:")
                    for line in data['load_output'].split('\n')[:5]:
                        if line.strip():
                            print(f"    {line}")
                
                return True
            else:
                print(f"\n✗ ERROR AL PROGRAMAR: {data.get('error')}")
                return False
        else:
            print(f"\n✗ Error HTTP: {response.status_code}")
            try:
                error_data = response.json()
                print(f"   Error: {error_data.get('error')}")
            except:
                pass
            return False
    except requests.exceptions.Timeout:
        print("\n✗ TIMEOUT: La programación tardó demasiado")
        print("   Verifica la conexión con el dispositivo")
        return False
    except Exception as e:
        print(f"\n✗ Error al programar firmware: {e}")
        return False

def main():
    print("\n")
    print("╔" + "═" * 58 + "╗")
    print("║" + " " * 8 + "TEST DE PROGRAMACIÓN DE FIRMWARE" + " " * 18 + "║")
    print("╚" + "═" * 58 + "╝")
    print()
    
    # Verificar argumentos
    program_real = False
    firmware_to_program = None
    
    if len(sys.argv) > 1:
        if sys.argv[1] == "--program" and len(sys.argv) > 2:
            program_real = True
            firmware_to_program = sys.argv[2]
    
    # Paso 1: Probar conexión
    if not test_connection():
        sys.exit(1)
    
    # Paso 2: Listar firmwares
    firmware_files = test_list_firmware()
    
    if not firmware_files:
        print("\n⚠️  ADVERTENCIA: No hay archivos de firmware disponibles")
        print("   Copia archivos .bin a: windows_interface/firmware/")
        sys.exit(0)
    
    # Paso 3: Detectar dispositivos CMSIS-DAP
    has_device = test_detect_cmsis_dap()
    
    if not has_device:
        print("\n⚠️  ADVERTENCIA: No se detectó dispositivo CMSIS-DAP")
        print("   Conecta un dispositivo antes de programar")
    
    # Paso 4: Programar firmware (o simulación)
    if firmware_to_program:
        # Usuario especificó archivo
        if firmware_to_program not in [f['name'] for f in firmware_files]:
            print(f"\n✗ ERROR: Archivo '{firmware_to_program}' no encontrado")
            print(f"   Archivos disponibles: {[f['name'] for f in firmware_files]}")
            sys.exit(1)
    else:
        # Usar el primer archivo disponible
        firmware_to_program = firmware_files[0]['name']
    
    success = test_program_firmware(firmware_to_program, dry_run=not program_real)
    
    # Resumen
    print("\n" + "=" * 60)
    print("RESUMEN")
    print("=" * 60)
    print(f"✓ Conexión: OK")
    print(f"✓ Firmwares disponibles: {len(firmware_files)}")
    print(f"{'✓' if has_device else '⚠'} Dispositivo CMSIS-DAP: {'Detectado' if has_device else 'No detectado'}")
    
    if program_real:
        print(f"{'✓' if success else '✗'} Programación: {'EXITOSA' if success else 'FALLIDA'}")
    else:
        print(f"⚠ Programación: SIMULADA (usar --program para programar realmente)")
    
    print()
    
    if not program_real:
        print("💡 Para programar firmware realmente:")
        print(f"   python test_firmware_programming.py --program {firmware_to_program}")
    
    sys.exit(0 if success or not program_real else 1)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⚠️  Prueba cancelada por el usuario")
        sys.exit(1)
