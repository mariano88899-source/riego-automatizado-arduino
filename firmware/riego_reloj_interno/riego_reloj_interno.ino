/*
 * ============================================================================
 *  RIEGO AUTOMATICO OFFLINE (reloj interno)  -  Iglesia
 *  Arduino UNO R4 WiFi  -  SIN internet, SIN modulos externos de reloj
 * ============================================================================
 *
 *  Que hace:
 *    - El horario (dias, hora y duracion) esta escrito aqui en el codigo.
 *    - Usa el RELOJ INTERNO del propio UNO R4 (no necesita DS3231).
 *    - Enciende la electrovalvula (rele) el dia y la hora programados y la
 *      apaga cuando se cumple la duracion.
 *    - Muestra el Padre Nuestro y el Ave Maria corriendo en la LCD.
 *
 *  IMPORTANTE sobre la hora:
 *    El reloj interno se pierde si se corta la CORRIENTE, a menos que le
 *    conectes una pila. Para que aguante los cortes de luz:
 *      - Conecta una pila de boton CR2032 (3V):  + al pin VRTC,  - a GND.
 *      - El pin VRTC esta en el header que esta junto al conector de barril.
 *    Con la pila puesta, pones la hora UNA sola vez y ya nunca se pierde.
 *    (Si la corriente NUNCA se corta, puede funcionar sin pila.)
 *
 *  CONEXIONES:
 *    Rele:  pin 8 -> IN1 | 5V -> VCC | GND -> GND   (active-LOW)
 *           +12V -> COM  | NO -> valvula | valvula -> GND fuente
 *           Diodo 1N4007 en paralelo con la valvula (raya hacia +12V)
 *    Pila reloj: CR2032 + -> VRTC | CR2032 - -> GND
 *    LCD (paralelo 4 bits):
 *           GND->GND VDD->5V VO->pote10k(centro) RS->12 RW->GND
 *           E->11 D4->5 D5->4 D6->3 D7->2 BLA->5V(220ohm) BLK->GND
 *
 *  LIBRERIAS:  ninguna extra (RTC y LiquidCrystal ya vienen con el IDE).
 * ============================================================================
 */

#include "RTC.h"
#include <LiquidCrystal.h>

// ####################  PROGRAMA DE RIEGO  ####################
// #########  Edita SOLO esta seccion a tu gusto  #############
const bool RIEGA_LUNES     = true;
const bool RIEGA_MARTES    = true;
const bool RIEGA_MIERCOLES = true;
const bool RIEGA_JUEVES    = true;
const bool RIEGA_VIERNES   = true;
const bool RIEGA_SABADO    = true;
const bool RIEGA_DOMINGO   = true;

const int HORA_RIEGO   = 7;    // hora de inicio  (0 a 23)
const int MINUTO_RIEGO = 0;    // minuto de inicio (0 a 59)
const int DURACION_MIN = 45;   // cuanto dura el riego, en minutos
// ############################################################

// ############  PONER EL RELOJ EN HORA  ######################
// La primera vez (o al cambiar la pila), ajusta estos numeros a la fecha y
// hora ACTUAL, pon FORZAR_PONER_HORA en true, sube el codigo, y en cuanto
// arranque vuelvelo a poner en false y sube otra vez.
const bool FORZAR_PONER_HORA = false;
const int  SET_ANIO   = 2026;
const int  SET_MES    = 7;     // 1-12
const int  SET_DIA    = 20;    // 1-31
const int  SET_HORA   = 14;    // 0-23
const int  SET_MINUTO = 30;    // 0-59
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

// ---- Estado ----
bool          releEncendido     = false;
unsigned long instanteEncendido = 0;
int           ultimoMinDisparo  = -1;

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
  releEncendido = encender;
  uint8_t nivel;
  if (RELE_ACTIVO_BAJO) nivel = encender ? LOW : HIGH;
  else                  nivel = encender ? HIGH : LOW;
  digitalWrite(PIN_RELE, nivel);
}

// Dia de la semana calculado desde la fecha (0=Lunes ... 6=Domingo),
// para no depender del getDayOfWeek() del RTC (que numera distinto).
int diaSemana() {
  RTCTime now;
  RTC.getTime(now);
  int d = now.getDayOfMonth();
  int m = static_cast<int>(now.getMonth()) + 1;       // 1-12
  int y = now.getYear();
  static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
  if (m < 3) y -= 1;
  int w = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7; // 0=Domingo ... 6=Sabado
  return (w + 6) % 7;                                 // 0=Lunes ... 6=Domingo
}

// ¿Hoy toca regar?  dow: 0=Lunes ... 6=Domingo
bool riegaHoy(int dow) {
  switch (dow) {
    case 0: return RIEGA_LUNES;
    case 1: return RIEGA_MARTES;
    case 2: return RIEGA_MIERCOLES;
    case 3: return RIEGA_JUEVES;
    case 4: return RIEGA_VIERNES;
    case 5: return RIEGA_SABADO;
    case 6: return RIEGA_DOMINGO;
  }
  return false;
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

// ============================ DIAGNOSTICO ==================================
void imprimirHoraRTC() {
  const char* dias[7] = {"Lunes","Martes","Miercoles","Jueves",
                         "Viernes","Sabado","Domingo"};
  RTCTime now;
  RTC.getTime(now);
  char b[40];
  snprintf(b, sizeof(b), "%02d/%02d/%04d  %02d:%02d:%02d",
           now.getDayOfMonth(), static_cast<int>(now.getMonth()) + 1, now.getYear(),
           now.getHour(), now.getMinutes(), now.getSeconds());
  Serial.print(F(">> El Arduino cree que es: "));
  Serial.print(dias[diaSemana()]);
  Serial.print(F("  "));
  Serial.println(b);
}

// ============================ LOGICA DE RIEGO =============================
void logicaRiego() {
  RTCTime now;
  RTC.getTime(now);
  int hh = now.getHour();
  int mm = now.getMinutes();
  int dow = diaSemana();

  if (!releEncendido) {
    if (riegaHoy(dow) && hh == HORA_RIEGO && mm == MINUTO_RIEGO
        && mm != ultimoMinDisparo) {
      ultimoMinDisparo  = mm;
      instanteEncendido = millis();
      aplicarRele(true);
      Serial.println(F(">> RIEGO INICIADO"));
    }
  }

  if (releEncendido &&
      millis() - instanteEncendido >= (unsigned long)DURACION_MIN * 60000UL) {
    aplicarRele(false);
    Serial.println(F(">> Riego terminado"));
  }
}

// ============================ SETUP / LOOP ================================
void setup() {
  digitalWrite(PIN_RELE, RELE_ACTIVO_BAJO ? HIGH : LOW);
  pinMode(PIN_RELE, OUTPUT);
  aplicarRele(false);

  Serial.begin(9600);
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcdMensaje("Riego iglesia", "Iniciando...");

  RTC.begin();
  // Pone la hora solo si el reloj no esta corriendo (primera vez / pila nueva),
  // o si tu lo fuerzas con FORZAR_PONER_HORA.
  if (FORZAR_PONER_HORA || !RTC.isRunning()) {
    RTCTime inicio(SET_DIA, static_cast<Month>(SET_MES - 1), SET_ANIO,
                   SET_HORA, SET_MINUTO, 0,
                   DayOfWeek::MONDAY, SaveLight::SAVING_TIME_INACTIVE);
    RTC.setTime(inicio);
    Serial.println(F(">> Reloj puesto en la hora inicial del codigo."));
  }
  imprimirHoraRTC();
  delay(2000);

  construirMarquesina();
  lcd.clear();
  ultimoScroll = millis();
}

void loop() {
  logicaRiego();
  actualizarLCD();

  static unsigned long ultimoAviso = 0;
  if (millis() - ultimoAviso >= 10000) {
    ultimoAviso = millis();
    imprimirHoraRTC();
  }

  delay(50);
}
