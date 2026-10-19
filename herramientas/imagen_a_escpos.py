#!/usr/bin/env python3
"""Convierte una imagen (logo) al formato raster ESC/POS de una impresora térmica.

Genera dos archivos a partir de cualquier PNG o JPG:
  - <nombre>.bin : listo para mandar a la impresora desde la PC
                   (Linux: cat logo.bin | sudo tee /dev/usb/lp0 > /dev/null)
  - <nombre>.h   : arreglo de bytes para incluir en el firmware del ESP32

Uso:
  python3 imagen_a_escpos.py logo.png --nombre LOGO_COMERCIO --ancho 384 --umbral 175

El ancho por defecto (384 puntos) es el ancho de impresión de una impresora de 58 mm
a 203 dpi. Para 80 mm usá 576. Bajá el umbral si el logo sale muy oscuro, subilo si
partes claras (dorados, pasteles) desaparecen. --vista genera un PNG para revisar el
resultado antes de imprimir.

Requiere Pillow: pip install pillow
"""
import argparse
from pathlib import Path

from PIL import Image, ImageOps


def convertir(origen: Path, ancho: int, umbral: int) -> Image.Image:
    img = Image.open(origen).convert("RGB")
    gris = img.convert("L")
    # Recorta los márgenes blancos para aprovechar todo el ancho del papel.
    caja = ImageOps.invert(gris).point(lambda v: 255 if v > 30 else 0).getbbox()
    if caja:
        img = img.crop(caja)
    alto = round(img.height * ancho / img.width)
    gris = img.resize((ancho, alto), Image.LANCZOS).convert("L")
    return gris.point(lambda v: 0 if v < umbral else 255, "1")


def a_raster(bw: Image.Image) -> bytes:
    ancho_bytes = bw.width // 8
    datos = bytearray()
    for y in range(bw.height):
        fila = bytearray(ancho_bytes)
        for x in range(bw.width):
            if bw.getpixel((x, y)) == 0:          # punto negro
                fila[x // 8] |= 0x80 >> (x % 8)
        datos += fila
    return bytes(datos)


def comandos(raster: bytes, ancho_bytes: int, alto: int) -> bytes:
    """GS v 0 en bloques de hasta 255 filas, que aceptan todas las impresoras."""
    out = bytearray(b"\x1b@\x1ba\x01")          # reinicio + centrado
    y = 0
    while y < alto:
        n = min(255, alto - y)
        out += b"\x1dv0\x00" + bytes([ancho_bytes, 0, n & 0xFF, n >> 8])
        out += raster[y * ancho_bytes:(y + n) * ancho_bytes]
        y += n
    out += b"\n\x1bd\x04"                       # avanza 4 líneas
    return bytes(out)


def cabecera(raster: bytes, nombre: str, ancho: int, alto: int) -> str:
    filas = []
    for i in range(0, len(raster), 16):
        filas.append("  " + ", ".join(f"0x{b:02X}" for b in raster[i:i + 16]) + ",")
    return f"""// {nombre}: {ancho} x {alto} puntos, 1 bit por punto, {len(raster)} bytes.
// Generado con herramientas/imagen_a_escpos.py. Se imprime con GS v 0.
#pragma once
#include <Arduino.h>

const uint16_t {nombre}_ANCHO = {ancho};
const uint16_t {nombre}_ALTO = {alto};
const uint8_t {nombre}_BYTES_FILA = {ancho // 8};

const uint8_t {nombre}[] PROGMEM = {{
""" + "\n".join(filas) + "\n};\n"


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("imagen", type=Path)
    p.add_argument("--nombre", default="LOGO", help="nombre del arreglo en el .h")
    p.add_argument("--ancho", type=int, default=384, help="puntos de ancho (múltiplo de 8)")
    p.add_argument("--umbral", type=int, default=175, help="0-255: más alto = más negro")
    p.add_argument("--vista", action="store_true", help="guardar también una vista previa PNG")
    a = p.parse_args()

    if a.ancho % 8:
        p.error("el ancho tiene que ser múltiplo de 8")
    bw = convertir(a.imagen, a.ancho, a.umbral)
    raster = a_raster(bw)
    base = a.imagen.with_suffix("")
    Path(f"{base}.bin").write_bytes(comandos(raster, a.ancho // 8, bw.height))
    Path(f"{base}.h").write_text(cabecera(raster, a.nombre, a.ancho, bw.height), encoding="utf-8")
    if a.vista:
        bw.save(f"{base}_vista.png")
    print(f"{a.ancho} x {bw.height} puntos, {len(raster)} bytes -> {base}.bin y {base}.h")


if __name__ == "__main__":
    main()
