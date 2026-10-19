# Problemas resueltos

Bitácora de lo que falló durante el desarrollo, por qué y cómo se arregló.

## Varios tickets seguidos: "Falló el envío"

**Síntoma:** al pedir varias facturas seguidas, las primeras salían y después aparecía `Falló el envío`, seguido de `No se pudo leer el estado del papel`.

**Causa:** la impresora tiene 32 KB de memoria y cada factura con logo ocupa 14,5 KB. El ESP32 envía mucho más rápido de lo que la impresora imprime: con varios tickets en camino la memoria se llenó, la impresora dejó de aceptar datos y la librería abandonó el envío al segundo de espera. Quedó un ticket a medias, con la impresora esperando el resto de un comando de imagen.

**Solución:**
- Cola de impresión: cada pedido entra en una cola y se manda un ticket a la vez, con 5 segundos entre uno y otro.
- Envío en partes de 4 KB, para que ninguna escritura llegue al tiempo límite.
- Si un envío falla igual, se vacía la memoria de la impresora (`SOFT_RESET` de la clase USB) para que el ticket a medias no se mezcle con el siguiente.

**Mejora pendiente:** guardar el logo en la memoria no volátil de la impresora. Cada ticket pasaría de 14,5 KB a menos de 1 KB.

## Error de compilación: `'Item' does not name a type`

**Causa:** antes de compilar un `.ino`, el Arduino IDE genera las declaraciones de todas las funciones y las inserta al principio del archivo. La declaración de `subtotal(const Item &)` quedaba antes de que existiera `struct Item`.

**Solución:** definir los tipos propios al principio del sketch, antes de cualquier función.

## Acentos y caracteres raros

**Causa:** las impresoras Xprinter suelen venir con el modo de caracteres chinos activado. En ese modo, los bytes de las letras con acento se interpretan como la mitad de un ideograma. Además, la página de códigos de este modelo es PC437.

**Solución:** al iniciar cada ticket se manda `FS .` (apaga el modo chino) y `ESC t 0` (PC437), y el firmware convierte el texto UTF-8 a esa tabla. PC437 no tiene Á Í Ó Ú, así que esas mayúsculas salen sin tilde.

## La impresora del proyecto no estaba verificada

La librería EspUsbHost solo estaba probada con una Xprinter XP-C58K. Antes de comprar varias unidades se probó una sola con el ejemplo en modo "sin imprimir", que solo consulta el estado. La XP-58IIH respondió con los mismos valores que el modelo verificado (`0x16` y `0x12`).

## Arduino IDE en Ubuntu 24.04

- **La AppImage aborta con un error de "SUID sandbox".** Se debe a las restricciones de seguridad de Ubuntu 24.04; se abre con `--no-sandbox`.
- **La placa no aparece como puerto.** El paquete `brltty` (lectores de braille) toma los chips USB-serie: `sudo apt remove brltty`.
