# Decisiones técnicas

Cada decisión con su alternativa descartada y el motivo.

## Requisitos de partida

- Balanza comercial china genérica: cálculo de precio, 3 displays LCD de segmentos, teclado de membrana, celda de carga.
- Debe verse como una balanza normal, sin cables ni pantallas agregadas.
- Unos 30 productos. Ticket en papel térmico común, no etiqueta.
- Uso interno de un comercio, con los permisos correspondientes.
- Fabricar 3 o 4 unidades si el prototipo funciona.
- Funcionar en red local hoy, y migrar a un SaaS más adelante.

## Microcontrolador: ESP32-S3

| Opción | Por qué sí / por qué no |
| --- | --- |
| **ESP32-S3** ✅ | WiFi para cargar productos, memoria de sobra y **USB OTG**: puede ser *host* de una impresora USB |
| ESP32 clásico | Sirve con impresoras por puerto serie, pero no tiene USB host |
| Raspberry Pi | Maneja USB fácil, pero tarda en arrancar y puede corromper la tarjeta SD con un corte de luz |
| STM32 | Sin WiFi |

Con 30 productos la memoria no es un problema: la lista ocupa unos 3 KB. La PSRAM no hace falta.

## Impresora: externa por USB

| Opción | Por qué sí / por qué no |
| --- | --- |
| **Impresora de escritorio USB, desmontable** ✅ | Robusta, con fuente propia y rollos estándar de 57 x 30 m. Si la balanza falla, la impresora se usa en la caja |
| Impresora de panel TTL (CSN-A2) dentro de la balanza | Más vistosa y simple de programar, pero es de uso liviano y lleva rollos chicos. Quedó como plan B en `firmware/factura_serial` |
| Impresora por red | Depende de que la red local funcione |

El riesgo era que el modelo comprado (Xprinter XP-58IIH) no estaba verificado con la librería USB. Se compró una sola unidad para probar antes de comprar más. **Funcionó.**

## Código de barras: EAN-13 de peso variable propio

Formato: `20` (pesable) o `21` (por unidad) + PLU de 5 dígitos + peso en gramos o cantidad de 5 dígitos + dígito verificador.

Se guarda **el peso, no el precio**, y la caja busca el precio en el servidor. Así:
- se evita el tope de $ 99.999 que tiene meter un precio de 5 dígitos;
- un cambio de precio no obliga a reimprimir nada.

## Arquitectura: primero sin red

La balanza guarda una copia local de los productos y sincroniza por HTTP/JSON con un servidor en la red local. Si se corta el WiFi, sigue pesando e imprimiendo. Para pasar a un SaaS solo cambia la dirección del servidor.

## Displays: conservar los originales

La placa de displays se une a la principal con solo 8 cables para 3 LCD de unas 16 líneas cada uno, así que el chip que los maneja está en la propia placa de displays. El ESP32 reemplaza a la placa principal y le habla a ese chip. El análisis de riesgos y alternativas está en [`plan-displays.md`](plan-displays.md).

## Alimentación sin batería

Las balanzas funcionan enchufadas. Se elimina la batería de plomo, que suele ser lo primero que falla. Al cortarse la luz se pierde el ticket en curso, pero no los productos, que quedan en la memoria flash del ESP32.
