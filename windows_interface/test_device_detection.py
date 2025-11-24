"""
Script de prueba para verificar la detección de dispositivos CMSIS-DAP
Ejecutar con: python test_device_detection.py
"""

import sys

def test_pyusb():
    """Prueba detección con pyusb"""
    print("=" * 60)
    print("Probando detección con pyusb...")
    print("=" * 60)
    
    try:
        import usb.core
        import usb.util
        
        print("✓ pyusb instalado correctamente")
        
        # Buscar dispositivos USB
        devices = list(usb.core.find(find_all=True))
        print(f"✓ Se encontraron {len(devices)} dispositivos USB totales")
        
        # Buscar CMSIS-DAP específicamente
        keywords = ['cmsis', 'dap', 'daplink', 'jlink', 'stlink', 'debugger', 'probe']
        cmsis_devices = []
        
        for device in devices:
            try:
                product_name = usb.util.get_string(device, device.iProduct) if device.iProduct else "Unknown"
                manufacturer = usb.util.get_string(device, device.iManufacturer) if device.iManufacturer else "Unknown"
                
                device_info = f"{product_name} {manufacturer}".lower()
                
                if any(keyword in device_info for keyword in keywords):
                    cmsis_devices.append({
                        'vendor_id': f"0x{device.idVendor:04x}",
                        'product_id': f"0x{device.idProduct:04x}",
                        'manufacturer': manufacturer,
                        'product': product_name,
                        'bus': device.bus,
                        'address': device.address
                    })
            except (ValueError, usb.core.USBError):
                continue
        
        if cmsis_devices:
            print(f"✓ Se encontraron {len(cmsis_devices)} dispositivo(s) CMSIS-DAP:")
            for i, dev in enumerate(cmsis_devices, 1):
                print(f"\n  Dispositivo {i}:")
                print(f"    Producto: {dev['product']}")
                print(f"    Fabricante: {dev['manufacturer']}")
                print(f"    VID: {dev['vendor_id']}, PID: {dev['product_id']}")
                print(f"    Bus: {dev['bus']}, Address: {dev['address']}")
        else:
            print("⚠ No se encontraron dispositivos CMSIS-DAP con pyusb")
        
        return True, cmsis_devices
        
    except ImportError as e:
        print(f"✗ pyusb NO está instalado: {e}")
        print("  Instalar con: pip install pyusb")
        return False, []
    except Exception as e:
        print(f"✗ Error al usar pyusb: {e}")
        return False, []

def test_pyocd():
    """Prueba detección con pyocd"""
    print("\n" + "=" * 60)
    print("Probando detección con pyocd...")
    print("=" * 60)
    
    try:
        from pyocd.probe import aggregator
        
        print("✓ pyocd instalado correctamente")
        
        # Buscar sondas
        probes = aggregator.DebugProbeAggregator.get_all_connected_probes()
        
        if probes:
            print(f"✓ Se encontraron {len(probes)} sonda(s) CMSIS-DAP:")
            for i, probe in enumerate(probes, 1):
                print(f"\n  Sonda {i}:")
                if hasattr(probe, 'product_name'):
                    print(f"    Producto: {probe.product_name}")
                if hasattr(probe, 'vendor_name'):
                    print(f"    Fabricante: {probe.vendor_name}")
                if hasattr(probe, 'vendor_id'):
                    print(f"    VID: 0x{probe.vendor_id:04x}", end="")
                if hasattr(probe, 'product_id'):
                    print(f", PID: 0x{probe.product_id:04x}")
                else:
                    print()
                if hasattr(probe, 'unique_id'):
                    print(f"    ID único: {probe.unique_id}")
        else:
            print("⚠ No se encontraron sondas CMSIS-DAP con pyocd")
        
        return True, probes
        
    except ImportError as e:
        print(f"✗ pyocd NO está instalado: {e}")
        print("  Instalar con: pip install pyocd")
        return False, []
    except Exception as e:
        print(f"✗ Error al usar pyocd: {e}")
        return False, []

def main():
    print("\n")
    print("╔" + "═" * 58 + "╗")
    print("║" + " " * 10 + "TEST DE DETECCIÓN CMSIS-DAP" + " " * 21 + "║")
    print("╚" + "═" * 58 + "╝")
    print()
    
    # Probar ambos métodos
    pyusb_ok, pyusb_devices = test_pyusb()
    pyocd_ok, pyocd_probes = test_pyocd()
    
    # Resumen
    print("\n" + "=" * 60)
    print("RESUMEN")
    print("=" * 60)
    
    if pyusb_ok:
        print(f"✓ pyusb: {len(pyusb_devices)} dispositivo(s) encontrado(s)")
    else:
        print("✗ pyusb: No disponible o error")
    
    if pyocd_ok:
        print(f"✓ pyocd: {len(pyocd_probes)} sonda(s) encontrada(s)")
    else:
        print("✗ pyocd: No disponible o error")
    
    print()
    
    if not pyusb_ok and not pyocd_ok:
        print("⚠ ADVERTENCIA: Ninguna librería de detección está disponible")
        print("  Instalar con: pip install pyusb pyocd")
        sys.exit(1)
    
    if pyusb_ok and len(pyusb_devices) > 0:
        print("✓ ÉXITO: Sistema listo para detectar dispositivos CMSIS-DAP")
        sys.exit(0)
    
    if pyocd_ok and len(pyocd_probes) > 0:
        print("✓ ÉXITO: Sistema listo para detectar dispositivos CMSIS-DAP")
        sys.exit(0)
    
    print("⚠ ADVERTENCIA: Librerías instaladas pero no se encontraron dispositivos")
    print("  Verifica que:")
    print("  1. El dispositivo CMSIS-DAP esté conectado")
    print("  2. Los drivers estén instalados (Windows: usar Zadig)")
    print("  3. Tengas permisos USB (Linux: reglas udev)")
    sys.exit(0)

if __name__ == "__main__":
    main()
