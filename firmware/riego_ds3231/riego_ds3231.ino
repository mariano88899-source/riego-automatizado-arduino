/*
 * ============================================================================
 *  RIEGO AUTOMATICO OFFLINE  -  Iglesia
 *  Arduino UNO R4 WiFi (funciona SIN internet ni WiFi)
 * ============================================================================
 *
 *  Que hace:
 *    - El horario (dias, hora y duracion) esta escrito aqui en el codigo.
 *    - Un modulo RTC DS3231 mantiene la fecha y la hora corriendo solo,
 *      con su propia pila, aunque no haya internet ni corriente.
 *    - Enciende la electrovalvula (rele) el dia y la hora programados, y la
 *      apaga cuando se cumple la duracion.
 *    - Muestra el Padre Nuestro y el Ave Maria corriendo en la LCD.
 *
 *  NECESITAS un modulo RTC DS3231 (el "temporizador", con pila).
 *
 *  CONEXIONES:
 *    Rele:    pin 8 -> IN1 | 5V -> VCC | GND -> GND   (active-LOW)
 *             +12V -> COM  | NO -> valvula | valvula -> GND fuente
 *             Diodo 1N4007 en paralelo con la valvula (raya hacia +12V)
 *    DS3231:  VCC -> 5V | GND -> GND | SDA -> A4 | SCL -> A5
 *    LCD (paralelo 4 bits):
 *             GND->GND VDD->5V VO->pote10k(centro) RS->12 RW->GND
 *             E->11 D4->5 D5->4 D6->3 D7->2 BLA->5V(220ohm) BLK->GND
 *
 *  LIBRERIAS A INSTALAR (Gestor de librerias del IDE):
 *    - RTClib  (por Adafruit)
 *    (LiquidCrystal ya viene incluida con el IDE)
 * ============================================================================
 */

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal.h>

// ####################  PROGRAMA DE RIEGO  ####################
// #########  Edita SOLO esta seccion a tu gusto  #############
//
//  Pon 'true' en los dias que quieras regar y 'false' en los que no.
const bool RIEGA_LUNES     = true;
const bool RIEGA_MARTES    = false;
const bool RIEGA_MIERCOLES = true;
const bool RIEGA_JUEVES    = false;
const bool RIEGA_VIERNES   = true;
const bool RIEGA_SABADO    = false;
const bool RIEGA_DOMINGO   = true;

const int HORA_RIEGO   = 6;    // hora de inicio  (0 a 23)
const int MINUTO_RIEGO = 0;    // minuto de inicio (0 a 59)
const int DURACION_MIN = 45;   // cuanto dura el riego, en minutos
// ############################################################

// ============================ CONFIGURACION HARDWARE =======================
const int  PIN_RELE         = 8;
const bool RELE_ACTIVO_BAJO = true;

const uint8_t LCD_COLS = 20;
const uint8_t LCD_ROWS = 4;
const int     FILA_TEXTO = 0;                    // renglon donde corre el texto (0-3)
const unsigned long SCROLL_MS = 300;             // velocidad de la marquesina (ms/caracter)
// ==========================================================================

// LCD en modo paralelo 4 bits.  Orden: (RS, E, D4, D5, D6, D7)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
RTC_DS3231    rtc;

// ---- Estado ----
bool          releEncendido     = false;
unsigned long instanteEncendido = 0;
int           ultimoMinDisparo  = -1;
bool          rtcOk             = false;

// ---- Marquesina ----
int           posMarq      = 0;
unsigned long ultimoScroll  = 0;
String        marquesina    = "";
int           lenMarq       = 0;

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

// ¿Hoy toca regar?  dow: 0=Domingo, 1=Lunes ... 6=Sabado (formato de RTClib)
bool riegaHoy(int dow) {
  switch (dow) {
    case 0: return RIEGA_DOMINGO;
    case 1: return RIEGA_LUNES;
    case 2: return RIEGA_MARTES;
    case 3: return RIEGA_MIERCOLES;
    case 4: return RIEGA_JUEVES;
    case 5: return RIEGA_VIERNES;
    case 6: return RIEGA_SABADO;
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
  for (int i = 0; i < N_LINEAS; i++) {
    marquesina += LINEAS[i];
    marquesina += "  ";
  }
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
  if (!rtcOk) { Serial.println(F(">> RTC no disponible.")); return; }
  const char* dias[7] = {"Domingo","Lunes","Martes","Miercoles",
                         "Jueves","Viernes","Sabado"};
  DateTime now = rtc.now();
  char b[40];
  snprintf(b, sizeof(b), "%02d/%02d/%04d  %02d:%02d:%02d",
           now.day(), now.month(), now.year(),
           now.hour(), now.minute(), now.second());
  Serial.print(F(">> El Arduino cree que es: "));
  Serial.print(dias[now.dayOfTheWeek()]);
  Serial.print(F("  "));
  Serial.println(b);
}

// ============================ LOGICA DE RIEGO =============================
void logicaRiego() {
  if (!rtcOk) return;
  DateTime now = rtc.now();
  int hh = now.hour();
  int mm = now.minute();

  // Disparo por horario (una sola vez por minuto coincidente)
  if (!releEncendido) {
    if (riegaHoy(now.dayOfTheWeek()) && hh == HORA_RIEGO && mm == MINUTO_RIEGO
        && mm != ultimoMinDisparo) {
      ultimoMinDisparo  = mm;
      instanteEncendido = millis();
      aplicarRele(true);
      Serial.println(F(">> RIEGO INICIADO"));
    }
  }

  // Apagado por duracion cumplida
  if (releEncendido &&
      millis() - instanteEncendido >= (unsigned long)DURACION_MIN * 60000UL) {
    aplicarRele(false);
    Serial.println(F(">> Riego terminado"));
  }
}

// ============================ SETUP / LOOP ================================
void setup() {
  // Fail-safe: rele APAGADO antes de configurar el pin como salida
  digitalWrite(PIN_RELE, RELE_ACTIVO_BAJO ? HIGH : LOW);
  pinMode(PIN_RELE, OUTPUT);
  aplicarRele(false);

  Serial.begin(9600);
  Wire.begin();
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcdMensaje("Riego iglesia", "Iniciando...");

  // ---- Arranque del reloj DS3231 ----
  if (!rtc.begin()) {
    rtcOk = false;
    lcdMensaje("Error: sin RTC", "Revisa DS3231");
    Serial.println(F("*** No se encontro el modulo DS3231. Revisa el cableado I2C."));
  } else {
    rtcOk = true;
    // Si la pila se agoto o es la primera vez, pone la hora de compilacion.
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      Serial.println(F(">> RTC puesto en hora (hora de compilacion)."));
    }
    // ---- Para poner la hora MANUALMENTE, descomenta la linea de abajo,
    //      ajusta los numeros (anio, mes, dia, hora, min, seg), sube el
    //      codigo UNA vez, y luego vuelvela a comentar y sube otra vez: ----
    // rtc.adjust(DateTime(2026, 7, 20, 14, 30, 0));
    imprimirHoraRTC();
  }
  delay(2000);

  construirMarquesina();
  lcd.clear();
  ultimoScroll = millis();
}

void loop() {
  logicaRiego();     // enciende/apaga el rele segun el horario programado
  actualizarLCD();   // oraciones corriendo en la pantalla

  // Muestra la hora del reloj en la terminal cada 10 s (diagnostico)
  static unsigned long ultimoAviso = 0;
  if (millis() - ultimoAviso >= 10000) {
    ultimoAviso = millis();
    imprimirHoraRTC();
  }

  delay(50);
}
