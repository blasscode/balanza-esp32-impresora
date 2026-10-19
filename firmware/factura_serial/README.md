# factura_serial

Variante para impresoras de panel con puerto serie (TTL o RS232 con un MAX3232), como la CSN-A2 o la QR204. Es el "camino A" que quedó como plan B.

- **Conexión TTL:** GPIO17 (TX) al RX de la impresora, GPIO18 (RX) al TX con un divisor si sale a 5 V, y GND común. La impresora usa su propia fuente.
- **Ajustes:** `BAUDIOS` según el autotest de la impresora (9600 o 19200), `COLUMNAS` 32 para 58 mm.

No está probada con hardware real: el proyecto avanzó por USB.
