# Examples

## Overview

Two example programs are included:

1. **Blink** - Simple LED blinking
2. **UART** - Character echo with startup stabilization

## Blink Example

### Purpose

Demonstrates basic GPIO control and timing on the PY32F003.

### Source

`examples/blink.c`

### Hardware

- **LED**: PB5 (active high)
- **Clock**: HSI 8MHz

### Behavior

- Toggles PB5 every ~50ms (400,000 clock cycles at 8MHz)
- Creates visible LED blink pattern

### Build

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=blink ..
cmake --build . -j
```

### Test

1. Flash: `cmake --build . --target flash_reset_blink -j`
2. Observe LED on PB5 blinking steadily

### Code Snippet

```c
while (1) {
    gpio_toggle(GPIOB, GPIO5);
    for (uint32_t i = 0; i < 400000; i++);
}
```

## UART Example

### Purpose

Demonstrates serial communication with echo functionality and startup stabilization.

### Source

`examples/uart.c`

### Hardware

- **UART**: USART2
- **TX**: PA0 (AF9)
- **RX**: PA1 (AF9)
- **Baud Rate**: 115200
- **Format**: 8N1 (8 bits, no parity, 1 stop bit)

### Behavior

1. Prints banner on startup (with 2.4M cycle stabilization delay)
2. Echoes received characters back to sender
3. Converts `\r` to `\r\n` for terminal compatibility
4. Filters RX errors (frame errors, noise, overrun)

### Build

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-arm-none-eabi.cmake -DAPP=uart ..
cmake --build . -j
```

### Test

1. Flash: `cmake --build . --target flash_reset_uart -j`
2. Open serial terminal:
   ```bash
   screen /dev/ttyUSB0 115200
   # or
   minicom -b 115200 -D /dev/ttyUSB0
   ```
3. You should see:
   ```
   PY32 libopencm3 UART echo
   ```
4. Type characters and see them echoed back

### Expected Output

```
PY32 libopencm3 UART echo
Hello
Hello
World
World
```

### Key Functions

#### `uart_link_stabilize()`

Waits for oscillator/USB adapter to stabilize after reset:
- 2,400,000 cycle delay (~300ms at 8MHz)
- Flushes RX error flags (FE, NE, ORE)
- Clears RXNE (receive ready) flag

```c
static void uart_link_stabilize(void) {
    for (uint32_t i = 0; i < 2400000; i++);
    
    // Clear error flags
    USART2_ICR = USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF;
    USART2_ISR &= ~USART_ISR_RXNE;
}
```

#### `uart_send_blocking()`

Sends a single character:
```c
static void uart_send_blocking(char c) {
    while (!(USART2_ISR & USART_ISR_TXE));
    USART2_TDR = c;
}
```

#### `uart_recv_blocking()`

Receives a single character:
```c
static uint8_t uart_recv_blocking(void) {
    while (!(USART2_ISR & USART_ISR_RXNE));
    uint8_t data = USART2_RDR;
    // Filter frame/noise errors
    if (USART2_ISR & (USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        return 0;
    }
    return data;
}
```

### Startup Sequence

1. Initialize USART2 at 115200 8N1
2. Configure PA0 (TX, AF9) and PA1 (RX, AF9)
3. Call `uart_link_stabilize()` for 300ms
4. Send banner message
5. Enter echo loop

### Common Issues

**Garbled text on startup**: Already handled by `uart_link_stabilize()`. If still occurring:
- Verify UART adapter is connected before powering device
- Check USB cable quality
- Try higher delay in `uart_link_stabilize()` (increase loop count)

**No characters echoed**: 
- Verify TX/RX pins (PA0 and PA1)
- Verify baud rate (115200)
- Check serial cable connections
- Verify common GND between device and adapter

**Only partial characters**: Usually caused by baud rate mismatch or noise on lines

## Next Steps

- [Flash examples to device](FLASH.md)
- [View hardware connections](HARDWARE.md)
- [Troubleshoot issues](TROUBLESHOOTING.md)
