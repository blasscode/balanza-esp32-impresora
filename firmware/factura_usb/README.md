# factura_usb

Firmware principal: el ESP32-S3 imprime una factura simulada en una impresora térmica USB (probado con Xprinter XP-58IIH).

- **Requiere:** paquete esp32 de Espressif 3.2.0+ y librería EspUsbHost 2.10.0+.
- **Archivos:** `factura_usb.ino` es el programa. `logo_casanico.h` es el logo en formato raster. Los tres `.hpp` vienen del ejemplo de la librería (ver `THIRD_PARTY_NOTICES.md`).
- **Uso:** programar por el puerto UART, impresora en el puerto USB nativo con un OTG, y escribir `p` en el Monitor Serie (115200) para encolar un ticket.

Qué hace en cada ticket:
1. Consulta el papel y los errores con `DLE EOT` antes de imprimir.
2. Arma el ticket completo en memoria: logo, ítems por peso y por unidad, total, QR y EAN-13 de balanza.
3. Lo envía en partes de 4 KB, uno por vez, con 5 s entre tickets para no desbordar los 32 KB de la impresora.
4. Si un envío falla, vacía la memoria de la impresora con `SOFT_RESET`.
