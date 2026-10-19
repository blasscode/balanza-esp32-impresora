# Puesta en marcha

## 1. Probar la impresora sola

**Autotest:** con la impresora apagada y con papel, mantené apretado FEED y encendela. Sale una hoja con su configuración. La de la XP-58IIH usada en este proyecto dice:

| Dato | Valor |
| --- | --- |
| Firmware | 3.002CR5I |
| Interfaz | USB |
| Página de códigos por defecto | Page0 (PC437) |
| Códigos 2D | QR |
| Densidad | 5 de 8 |

Si mantenés FEED más tiempo entra en modo *Hexadecimal Dump*: imprime en números todo lo que recibe. Sirve para depurar; para salir, apagala y prendela.

**Desde Linux, sin drivers:**

```bash
ls /dev/usb/      # tiene que aparecer lp0
printf '\x1b@Hola\nPrueba: \xa0 \x82 \xa1 \xa2 \xa3 \xa4\n\x1bd\x04' | sudo tee /dev/usb/lp0 > /dev/null
cat logo.bin | sudo tee /dev/usb/lp0 > /dev/null   # un logo generado con herramientas/imagen_a_escpos.py
```

## 2. Preparar el Arduino IDE

1. Instalá el Arduino IDE 2.
   - **Ubuntu 24.04:** la AppImage necesita `libfuse2t64` y abrirse con `--no-sandbox`.
   - **Ubuntu en general:** `sudo usermod -aG dialout $USER` y `sudo apt remove brltty`, que suele quedarse con el puerto de la placa.
   - **Windows:** si la placa no aparece como puerto COM, instalá el driver del chip USB de la placa (CH340 o CP210x).
2. En Preferencias, agregá `https://espressif.github.io/arduino-esp32/package_esp32_index.json` e instalá **esp32 by Espressif Systems** (3.2.0 o posterior).
3. En el gestor de bibliotecas instalá **EspUsbHost** (2.10.0 o posterior).

**Configuración de la placa:**

| Opción | Valor |
| --- | --- |
| Placa | ESP32S3 Dev Module |
| Flash Size | según el módulo (N16R8 = 16 MB, N8R8 = 8 MB) |
| PSRAM | OPI PSRAM |
| USB CDC On Boot | Disabled |
| USB Mode | Hardware CDC and JTAG |

## 3. Conexión

- **Puerto UART/COM del ESP32 → PC:** programa y Monitor Serie.
- **Puerto USB nativo del ESP32 → OTG → cable USB → impresora**, con la impresora enchufada a su fuente.

No hace falta soldar los pines del ESP32 para esta etapa.

## 4. Verificar la detección

Con el ejemplo `EspUsbHostPrinterEscPos` de la librería y `PRINT_SAMPLE = false`, el Monitor Serie tiene que mostrar el estado cada 5 segundos:

```
DLE EOT 1 printer 0x16 offline=0
DLE EOT 4 paper 0x12 near_end=0 out=0
GET_PORT_STATUS failed
```

- `0x16`: impresora en línea. `0x12`: hay papel.
- `GET_PORT_STATUS failed` es normal en este modelo: no responde esa consulta opcional y se usa el estado de ESC/POS.

Si solo aparece el mensaje de inicio y nunca el estado: revisá que el OTG esté en el puerto nativo, que *USB Mode* sea *Hardware CDC and JTAG*, y reconectá la impresora encendida. Si sigue, medí que el puerto nativo entregue 5 V.

## 5. Imprimir la factura

Abrí `firmware/factura_usb/factura_usb.ino`, subilo y escribí `p` en el Monitor Serie. Cada `p` agrega un ticket a la cola.

| Constante | Para qué |
| --- | --- |
| `CON_LOGO` | `false` imprime el nombre en texto en lugar del logo |
| `TIEMPO_TICKET_MS` | Tiempo entre tickets (5000 ms). Bajalo si la impresora termina antes |
| `CORTAR` | `true` solo en impresoras con cortador |

## Seguridad

- Ajustá el regulador LM2596 a 5 V con el tester **antes** de conectarle nada.
- Los pines del ESP32 son de 3,3 V.
- No alimentes la placa a la vez por la PC y por el pin de 5 V.
- El conector tipo teléfono (RJ11) de la impresora es para el cajón de dinero y saca 12 V.
