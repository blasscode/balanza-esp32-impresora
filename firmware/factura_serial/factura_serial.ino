// Factura simulada: ESP32 + impresora térmica ESC/POS por puerto serie TTL o RS232 (camino A).
// Alternativa a firmware/factura_usb para impresoras de panel (CSN-A2, QR204, etc.).
// Arma el ticket completo en memoria y lo manda en una sola escritura.
#include <Arduino.h>
#include <initializer_list>

// ---------- Configuración ----------
const int  PIN_RX   = 18;     // RX del ESP32 <- TX de la impresora
const int  PIN_TX   = 17;     // TX del ESP32 -> RX de la impresora
const long BAUDIOS  = 9600;   // el valor que mostró el autotest
const int  COLUMNAS = 32;     // 58 mm = 32 columnas, 80 mm = 48
const bool CORTAR   = false;  // true solo si la impresora tiene cortador

HardwareSerial Impresora(1);

// Va arriba de todo para que el Arduino IDE la conozca antes de las funciones.
struct Item { const char *nombre; long gramos; int unidades; long precio; };  // precio en centavos, por kg o por unidad

// ---------- Buffer y comandos básicos ----------
uint8_t buf[20480];                // alcanza para el logo (14 KB) y el ticket
size_t  len = 0;

void add(uint8_t b) { if (len < sizeof(buf)) buf[len++] = b; }
void cmd(std::initializer_list<uint8_t> bytes) { for (uint8_t b : bytes) add(b); }

#include "logo_casanico.h"      // debe estar en la misma carpeta que este archivo

void iniciar()          { cmd({0x1B, '@'}); cmd({0x1C, '.'}); cmd({0x1B, 't', 2}); }  // reinicio + sin modo chino + página PC850
void alinear(uint8_t n) { cmd({0x1B, 'a', n}); }                    // 0 izquierda, 1 centro, 2 derecha
void negrita(bool on)   { cmd({0x1B, 'E', (uint8_t)(on ? 1 : 0)}); }
void tamano(uint8_t ancho, uint8_t alto) { cmd({0x1D, '!', (uint8_t)(((ancho - 1) << 4) | (alto - 1))}); }
void avanzar(uint8_t n) { cmd({0x1B, 'd', n}); }

// ---------- Texto con acentos (UTF-8 a PC850) ----------
void texto(const char *s) {
  while (*s) {
    uint8_t c = *s++;
    if (c < 0x80) { add(c); continue; }
    if ((c == 0xC2 || c == 0xC3) && *s) {
      uint16_t u = ((c & 0x1F) << 6) | ((uint8_t)*s++ & 0x3F);
      switch (u) {
        case 0xE1: add(0xA0); break;  // á
        case 0xE9: add(0x82); break;  // é
        case 0xED: add(0xA1); break;  // í
        case 0xF3: add(0xA2); break;  // ó
        case 0xFA: add(0xA3); break;  // ú
        case 0xF1: add(0xA4); break;  // ñ
        case 0xD1: add(0xA5); break;  // Ñ
        case 0xFC: add(0x81); break;  // ü
        case 0xC1: add(0xB5); break;  // Á
        case 0xC9: add(0x90); break;  // É
        case 0xCD: add(0xD6); break;  // Í
        case 0xD3: add(0xE0); break;  // Ó
        case 0xDA: add(0xE9); break;  // Ú
        case 0xBA: add(0xA7); break;  // º
        case 0xBF: add(0xA8); break;  // ¿
        case 0xA1: add(0xAD); break;  // ¡
        default:   add('?');
      }
    } else {
      add('?');                                    // carácter fuera de la tabla
      while (((uint8_t)*s & 0xC0) == 0x80) s++;    // salta el resto de sus bytes
    }
  }
}

int largo(const char *s) {                         // caracteres visibles de un texto UTF-8
  int n = 0;
  for (; *s; s++) if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}
void linea(const char *s) { texto(s); add('\n'); }
void separador() { for (int i = 0; i < COLUMNAS; i++) add('-'); add('\n'); }
void renglon(const char *izq, const char *der) {   // texto a la izquierda, monto a la derecha
  int espacios = COLUMNAS - largo(izq) - largo(der);
  texto(izq);
  for (int i = 0; i < (espacios < 1 ? 1 : espacios); i++) add(' ');
  texto(der);
  add('\n');
}

// ---------- Formato argentino ----------
void pesos(long centavos, char *out) {             // 420000 -> "$ 4.200,00"
  char cifras[16], miles[24];
  snprintf(cifras, sizeof(cifras), "%ld", centavos / 100);
  int n = strlen(cifras), j = 0;
  for (int i = 0; i < n; i++) {
    miles[j++] = cifras[i];
    int resto = n - i - 1;
    if (resto > 0 && resto % 3 == 0) miles[j++] = '.';
  }
  miles[j] = '\0';
  snprintf(out, 24, "$ %s,%02ld", miles, centavos % 100);
}
void kilos(long gramos, char *out) {               // 350 -> "0,350 kg"
  snprintf(out, 24, "%ld,%03ld kg", gramos / 1000, gramos % 1000);
}

// ---------- Códigos ----------
void ean13(const char *doceDigitos) {              // la impresora calcula el dígito verificador
  cmd({0x1D, 'h', 80});                            // alto en puntos
  cmd({0x1D, 'w', 3});                             // ancho del módulo (bajalo a 2 si no entra)
  cmd({0x1D, 'H', 2});                             // números debajo de las barras
  cmd({0x1D, 'k', 67, 12});                        // EAN-13 con 12 dígitos
  for (int i = 0; i < 12; i++) add(doceDigitos[i]);
}
void qr(const char *datos) {
  size_t k = strlen(datos) + 3;
  cmd({0x1D, '(', 'k', 4, 0, 49, 65, 50, 0});      // modelo 2
  cmd({0x1D, '(', 'k', 3, 0, 49, 67, 6});          // tamaño del módulo
  cmd({0x1D, '(', 'k', 3, 0, 49, 69, 49});         // corrección de errores nivel M
  cmd({0x1D, '(', 'k', (uint8_t)(k & 0xFF), (uint8_t)(k >> 8), 49, 80, 48});  // guarda los datos
  for (const char *p = datos; *p; p++) add(*p);
  cmd({0x1D, '(', 'k', 3, 0, 49, 81, 48});         // imprime el QR
}

// ---------- Datos de prueba ----------
Item items[] = {
  {"Jamón cocido",  350, 0, 1200000},              // $ 12.000,00 el kg
  {"Queso cremoso", 520, 0,  980000},              // $ 9.800,00 el kg
  {"Pan lactal",      0, 1,  250000},              // $ 2.500,00 la unidad
};
long subtotal(const Item &it) {
  if (it.gramos > 0) return (long)((int64_t)it.precio * it.gramos / 1000);
  return it.precio * it.unidades;
}

// ---------- La factura ----------
void armarFactura() {
  char a[24], b[24], l[64];
  len = 0;
  iniciar();

  alinear(1);
  imprimirLogo();
  linea("CUIT 00-00000000-0 (ficticio)");
  negrita(true); linea("COMPROBANTE DE PRUEBA"); linea("SIN VALIDEZ FISCAL"); negrita(false);

  alinear(0);
  separador();
  linea("Ticket Nº 0001-00000123");
  linea("Fecha 07/10/2026 17:00");
  separador();

  long total = 0;
  for (const Item &it : items) {
    long st = subtotal(it);
    total += st;
    pesos(st, a);
    renglon(it.nombre, a);
    pesos(it.precio, b);
    if (it.gramos > 0) { kilos(it.gramos, a); snprintf(l, sizeof(l), "  %s x %s/kg", a, b); }
    else               { snprintf(l, sizeof(l), "  %d u x %s", it.unidades, b); }
    linea(l);
  }

  separador();
  pesos(total, a);
  tamano(1, 2); negrita(true); renglon("TOTAL", a); negrita(false); tamano(1, 1);
  separador();

  alinear(1);
  snprintf(l, sizeof(l), "PRUEBA|0001-00000123|%ld.%02ld", total / 100, total % 100);
  qr(l);
  linea("");
  linea("Gracias por su compra");
  avanzar(3);

  linea("Prueba de código de balanza");                // 20 + PLU 00123 + 00350 gramos
  ean13("200012300350");
  if (CORTAR) cmd({0x1D, 'V', 66, 3}); else avanzar(4);
}

void setup() {
  Serial.begin(115200);
  Impresora.begin(BAUDIOS, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(500);
  armarFactura();
  Impresora.write(buf, len);
  Serial.printf("Factura enviada: %u bytes. Escribí p para reimprimir.\n", (unsigned)len);
}

void loop() {
  if (Serial.available() && Serial.read() == 'p') {
    armarFactura();
    Impresora.write(buf, len);
  }
}
