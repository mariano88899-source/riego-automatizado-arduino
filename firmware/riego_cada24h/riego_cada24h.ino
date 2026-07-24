/*
 * ============================================================================
 *  RIEGO AUTOMATICO CADA 24 HORAS  -  Iglesia
 *  Arduino UNO R4 WiFi  -  SIN reloj, SIN internet, SIN modulos extra
 * ============================================================================
 *
 *  Que hace:
 *    - Al encender, riega de inmediato durante 45 minutos.
 *    - Despues repite el riego cada 24 horas, en bucle, para siempre.
 *    - No necesita saber la hora: solo cuenta el tiempo transcurrido.
 *    - Muestra el Padre Nuestro y el Ave Maria corriendo en la LCD.
 *
 *  OJO: el conteo se mide desde que enciende la placa. Si se corta la luz,
 *  al volver la corriente vuelve a regar de inmediato y reinicia el conteo
 *  de 24 h desde ese momento.
 *
 *  CONEXIONES:
 *    Rele:  pin 8 -> IN1 | 5V -> VCC | GND -> GND   (active-LOW)
 *           +12V -> COM  | NO -> valvula | valvula -> GND fuente
 *           Diodo 1N4007 en paralelo con la valvula (raya hacia +12V)
 *    LCD (paralelo 4 bits):
 *           GND->GND VDD->5V VO->pote10k(centro) RS->12 RW->GND
 *           E->11 D4->5 D5->4 D6->3 D7->2 BLA->5V(220ohm) BLK->GND
 *
 *  LIBRERIAS:  ninguna extra (LiquidCrystal ya viene con el IDE).
 * ============================================================================
 */

#include <LiquidCrystal.h>

// ####################  AJUSTES DE RIEGO  ####################
const unsigned long INTERVALO_HORAS = 24;   // cada cuantas horas riega
const unsigned long DURACION_MIN    = 45;   // cuanto dura cada riego (minutos)
// ############################################################

// ============================ CONFIGURACION HARDWARE =======================
const int  PIN_RELE         = 8;
const bool RELE_ACTIVO_BAJO = true;

const uint8_t LCD_COLS = 20;
const uint8_t LCD_ROWS = 4;
const int     FILA_TEXTO = 0;                // renglon donde corre el texto (0-3)
const unsigned long SCROLL_MS = 300;         // velocidad de la marquesina (ms/caracter)
// ==========================================================================

// LCD en modo paralelo 4 bits.  Orden: (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Tiempos en milisegundos
const unsigned long INTERVALO = INTERVALO_HORAS * 60UL * 60UL * 1000UL;
const unsigned long DURACION  = DURACION_MIN    * 60UL * 1000UL;

// ---- Estado ----
bool          regando       = false;
unsigned long inicioRiego   = 0;   // cuando empezo el riego actual
unsigned long ultimoInicio  = 0;   // cuando empezo el ultimo riego (para contar 24 h)

// ---- Marquesina ----
int           posMarq     = 0;
unsigned long ultimoScroll = 0;
String        marquesina   = "";
int           lenMarq      = 0;

// ---- Oraciones (sin acentos: la LCD estandar no los muestra bien) ----
const char* LINEAS[] = {
  "-- PADRE NUESTRO --",
  "Padre nuestro, que", "estas en el cielo,", "santificado sea tu",
  "Nombre; venga a", "nosotros tu reino;", "hagase tu voluntad",
  "en la tierra como", "en el cielo. Danos", "hoy nuestro pan de",
  "cada dia; perdona", "nuestras ofensas,", "como nosotros",
  "perdonamos a los", "que nos ofenden;", "no nos dejes caer",
  "en la tentacion, y", "libranos del mal.", "Amen.", "",
  "--- AVE MARIA ---",
  "Dios te salve,", "Maria, llena eres", "de gracia, el Senor",
  "es contigo; bendita", "tu eres entre todas", "las mujeres, y",
  "bendito es el fruto", "de tu vientre, Jesus", "Santa Maria, Madre",
  "de Dios, ruega por", "nosotros, pecadores,", "ahora y en la hora",
  "de nuestra muerte.", "Amen.", ""
};
const int N_LINEAS = sizeof(LINEAS) / sizeof(LINEAS[0]);

// ============================ RELE =========================================
void aplicarRele(bool encender) {
  uint8_t nivel;
  if (RELE_ACTIVO_BAJO) nivel = encender ? LOW : HIGH;
  else                  nivel = encender ? HIGH : LOW;
  digitalWrite(PIN_RELE, nivel);
}

void iniciarRiego() {
  regando      = true;
  inicioRiego  = millis();
  ultimoInicio = millis();
  aplicarRele(true);
  Serial.println(F(">> RIEGO INICIADO (45 min)"));
}

void detenerRiego() {
  regando = false;
  aplicarRele(false);
  Serial.println(F(">> Riego terminado. Siguiente en 24 h."));
}

// ============================ LCD ==========================================
void lcdMensaje(const char* l1, const char* l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1);
  lcd.setCursor(0, 1); lcd.print(l2);
}

void construirMarquesina() {
  marquesina = "";
  for (int i = 0; i < N_LINEAS; i++) { marquesina += LINEAS[i]; marquesina += "  "; }
  for (int i = 0; i < LCD_COLS; i++) marquesina += ' ';
  lenMarq = marquesina.length();
}

void actualizarLCD() {
  if (millis() - ultimoScroll < SCROLL_MS) return;
  ultimoScroll = millis();
  char buf[LCD_COLS + 1];
  for (int i = 0; i < LCD_COLS; i++) buf[i] = marquesina[(posMarq + i) % lenMarq];
  buf[LCD_COLS] = '\0';
  lcd.setCursor(0, FILA_TEXTO);
  lcd.print(buf);
  posMarq = (posMarq + 1) % lenMarq;
}

// ============================ SETUP / LOOP ================================
void setup() {
  // Fail-safe: rele APAGADO antes de configurar el pin como salida
  digitalWrite(PIN_RELE, RELE_ACTIVO_BAJO ? HIGH : LOW);
  pinMode(PIN_RELE, OUTPUT);
  aplicarRele(false);

  Serial.begin(9600);
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcdMensaje("Riego iglesia", "Iniciando...");
  delay(1500);

  construirMarquesina();
  lcd.clear();
  ultimoScroll = millis();

  // Primer riego apenas enciende
  iniciarRiego();
}

void loop() {
  // Apaga al cumplir los 45 minutos
  if (regando && millis() - inicioRiego >= DURACION) {
    detenerRiego();
  }

  // Vuelve a regar 24 h despues del ultimo inicio
  if (!regando && millis() - ultimoInicio >= INTERVALO) {
    iniciarRiego();
  }

  actualizarLCD();   // oraciones corriendo en la pantalla

  // Aviso en la terminal cada 30 s: cuanto falta para el proximo riego
  static unsigned long ultimoAviso = 0;
  if (millis() - ultimoAviso >= 30000) {
    ultimoAviso = millis();
    if (regando) {
      unsigned long restante = (DURACION - (millis() - inicioRiego)) / 60000UL;
      Serial.print(F("Regando... quedan aprox. "));
      Serial.print(restante);
      Serial.println(F(" min"));
    } else {
      unsigned long faltan = (INTERVALO - (millis() - ultimoInicio)) / 60000UL;
      Serial.print(F("Proximo riego en aprox. "));
      Serial.print(faltan);
      Serial.println(F(" min"));
    }
  }

  delay(50);
}
