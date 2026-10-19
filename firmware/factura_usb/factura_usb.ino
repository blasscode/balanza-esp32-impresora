// Factura simulada de Casa Nico: ESP32-S3 + impresora Xprinter XP-58IIH por USB.
//
// Usa la librería EspUsbHost (2.10.0 o posterior) y los archivos del ejemplo
// EspUsbHostPrinterEscPos. En esta misma carpeta tienen que estar:
//   factura_usb.ino (este archivo)
//   logo_casanico.h
//   PrinterDevice.hpp, EscPos.hpp, PrinterProtocol.hpp  (copiados del ejemplo)
//
// Conexión: programar por el puerto UART/COM; la impresora va al puerto USB
// nativo con el adaptador OTG. Abrí el Monitor Serie a 115200 y escribí p para imprimir.

#include <initializer_list>
#include "EspUsbHost.h"
#include "PrinterDevice.hpp"

// ---------- Configuración ----------
const int  COLUMNAS = 32;     // 58 mm = 32 columnas
const bool CORTAR   = false;  // la XP-58IIH no tiene cortador
const bool CON_LOGO = true;   // false para imprimir sin el logo
// Tiempo que se le da a la impresora para terminar un ticket antes de mandar otro.
// La memoria de la impresora es de 32 KB y cada ticket con logo ocupa 14,5 KB:
// si se mandan varios seguidos, se llena y el envío falla.
const uint32_t TIEMPO_TICKET_MS = 5000;
const uint8_t  MAX_EN_COLA = 5;

EspUsbHost usb;
printer::PrinterDevice impresora(usb);
bool conectada = false;
uint8_t  enCola = 0;          // tickets pedidos que esperan su turno
uint32_t libreDesde = 0;      // momento a partir del cual la impresora está libre

// Va arriba de todo para que el Arduino IDE la conozca antes de las funciones.
struct Item { const char *nombre; long gramos; int unidades; long precio; };  // precio en centavos

// ---------- Buffer y comandos básicos ----------
uint8_t buf[20480];           // alcanza para el logo (14 KB) y el ticket
size_t  len = 0;
bool    desborde = false;

void add(uint8_t b) { if (len < sizeof(buf)) buf[len++] = b; else desborde = true; }
void cmd(std::initializer_list<uint8_t> bytes) { for (uint8_t b : bytes) add(b); }

#include "logo_casanico.h"

void iniciar()          { cmd({0x1B, '@'}); cmd({0x1C, '.'}); cmd({0x1B, 't', 0}); }  // reinicio + sin modo chino + PC437
void alinear(uint8_t n) { cmd({0x1B, 'a', n}); }                    // 0 izquierda, 1 centro, 2 derecha
void negrita(bool on)   { cmd({0x1B, 'E', (uint8_t)(on ? 1 : 0)}); }
void tamano(uint8_t ancho, uint8_t alto) { cmd({0x1D, '!', (uint8_t)(((ancho - 1) << 4) | (alto - 1))}); }
void avanzar(uint8_t n) { cmd({0x1B, 'd', n}); }

// ---------- Texto con acentos (UTF-8 a PC437, la página de la impresora) ----------
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
        case 0xC9: add(0x90); break;  // É
        case 0xC1: add('A');  break;  // Á no existe en PC437
        case 0xCD: add('I');  break;  // Í
        case 0xD3: add('O');  break;  // Ó
        case 0xDA: add('U');  break;  // Ú
        case 0xBA: add(0xA7); break;  // º
        case 0xBF: add(0xA8); break;  // ¿
        case 0xA1: add(0xAD); break;  // ¡
        default:   add('?');
      }
    } else {
      add('?');
      while (((uint8_t)*s & 0xC0) == 0x80) s++;
    }
  }
}

int largo(const char *s) {
  int n = 0;
  for (; *s; s++) if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}
void linea(const char *s) { texto(s); add('\n'); }
void separador() { for (int i = 0; i < COLUMNAS; i++) add('-'); add('\n'); }
void renglon(const char *izq, const char *der) {
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
void ean13(const char *doceDigitos) {
  cmd({0x1D, 'h', 80});
  cmd({0x1D, 'w', 2});
  cmd({0x1D, 'H', 2});
  cmd({0x1D, 'k', 67, 12});
  for (int i = 0; i < 12; i++) add(doceDigitos[i]);
}
void qr(const char *datos) {
  size_t k = strlen(datos) + 3;
  cmd({0x1D, '(', 'k', 4, 0, 49, 65, 50, 0});
  cmd({0x1D, '(', 'k', 3, 0, 49, 67, 6});
  cmd({0x1D, '(', 'k', 3, 0, 49, 69, 49});
  cmd({0x1D, '(', 'k', (uint8_t)(k & 0xFF), (uint8_t)(k >> 8), 49, 80, 48});
  for (const char *p = datos; *p; p++) add(*p);
  cmd({0x1D, '(', 'k', 3, 0, 49, 81, 48});
}

// ---------- Datos de prueba ----------
Item items[] = {
  {"Pan francés",      1250, 0,  350000},          // $ 3.500,00 el kg
  {"Medialunas",          0, 6,   60000},          // $ 600,00 la unidad
  {"Bizcochos de grasa", 400, 0,  480000},         // $ 4.800,00 el kg
};
long subtotal(const Item &it) {
  if (it.gramos > 0) return (long)((int64_t)it.precio * it.gramos / 1000);
  return it.precio * it.unidades;
}

// ---------- La factura ----------
void armarFactura() {
  char a[24], b[24], l[64];
  len = 0;
  desborde = false;
  iniciar();

  alinear(1);
  if (CON_LOGO) imprimirLogo();
  else { tamano(2, 2); negrita(true); linea("CASA NICO"); negrita(false); tamano(1, 1); }
  negrita(true); linea("COMPROBANTE DE PRUEBA"); linea("SIN VALIDEZ FISCAL"); negrita(false);

  alinear(0);
  separador();
  linea("Ticket Nº 0001-00000123");
  linea("Fecha 18/10/2026 23:50");
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
  linea("¡Gracias por su compra!");
  avanzar(2);

  linea("Código de balanza (prueba)");               // 20 + PLU 00123 + 01250 gramos
  ean13("200012301250");
  if (CORTAR) cmd({0x1D, 'V', 66, 3}); else avanzar(4);
}

// ---------- Impresión ----------
bool enviar() {
  // La librería espera hasta 1 segundo por envío: se manda en partes de 4 KB.
  const size_t PARTE = 4096;
  for (size_t i = 0; i < len; i += PARTE) {
    size_t n = (len - i > PARTE) ? PARTE : (len - i);
    if (!impresora.write(buf + i, n)) return false;
  }
  return true;
}

// Devuelve true si el ticket salió; false si hubo un problema.
bool imprimirFactura() {
  bool sinPapel = false, pocoPapel = false, error = false;
  if (!impresora.checkPaper(sinPapel, pocoPapel, error)) {
    Serial.println("No se pudo leer el estado del papel; no imprimo.");
    return false;
  }
  if (sinPapel) { Serial.println("Sin papel: no imprimo."); return false; }
  if (error)    { Serial.println("La impresora informa un error (¿tapa abierta?): no imprimo."); return false; }
  if (pocoPapel) Serial.println("Aviso: queda poco papel.");

  armarFactura();
  if (desborde) { Serial.println("El ticket no entra en el buffer: no imprimo."); return false; }

  Serial.printf("Enviando %u bytes...\n", (unsigned)len);
  if (!enviar()) {
    // Quedó un ticket a medias en la impresora: se vacía su memoria para que
    // no se mezcle con el siguiente.
    Serial.println("Falló el envío. Vacío la memoria de la impresora.");
    impresora.softReset();
    Serial.println("Si no vuelve a responder, desenchufá y enchufá el cable USB.");
    return false;
  }
  Serial.println("Factura enviada.");
  return true;
}

void pedirTicket() {
  if (!conectada) { Serial.println("No hay impresora conectada."); return; }
  if (enCola >= MAX_EN_COLA) { Serial.println("Ya hay demasiados tickets esperando."); return; }
  enCola++;
  Serial.printf("Ticket en cola (%u esperando).\n", enCola);
}

void atenderCola() {
  if (!conectada || enCola == 0) return;
  if ((int32_t)(millis() - libreDesde) < 0) return;   // la impresora todavía está imprimiendo
  if (imprimirFactura()) {
    enCola--;
    libreDesde = millis() + TIEMPO_TICKET_MS;
  } else {
    enCola = 0;                                       // ante un error se cancela la cola
    libreDesde = millis() + 2000;
  }
}

// ---------- Conexión ----------
bool conectar() {
  if (!impresora.begin()) return false;
  Serial.printf("Impresora lista (dirección %u).\n", impresora.address());
  Serial.println("Escribí p y Enter en el Monitor Serie para imprimir la factura.");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  usb.onDeviceConnected([](const EspUsbHostDeviceInfo &d) {
    Serial.printf("USB conectado: vid=%04x pid=%04x\n", d.vid, d.pid);
  });
  usb.onDeviceDisconnected([](const EspUsbHostDeviceInfo &d) {
    if (conectada && d.address == impresora.address()) {
      impresora.end();
      conectada = false;
      enCola = 0;
      Serial.println("Impresora desconectada.");
    }
  });

  usb.begin();
  Serial.println("Factura USB lista. Conectá la impresora encendida.");
}

void loop() {
  static uint32_t ultimoIntento = 0;
  if (!conectada && millis() - ultimoIntento >= 1000) {
    ultimoIntento = millis();
    conectada = conectar();
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'p' || c == 'P') pedirTicket();
  }

  atenderCola();
}
