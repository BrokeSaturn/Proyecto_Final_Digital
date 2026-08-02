#include <LiquidCrystal.h>
#include <Servo.h>

// LCD1: velocidad + inclinacion  | RS=8  E=9   D4-D7 = 10,11,12,13 (bus compartido)
// LCD2: tiempo + distancia       | RS=36 E=38  D4-D7 = 10,11,12,13
// LCD3: kcal + faltan            | RS=46 E=48  D4-D7 = 10,11,12,13
LiquidCrystal lcd1(8, 9, 10, 11, 12, 13);
LiquidCrystal lcd2(36, 38, 10, 11, 12, 13);
LiquidCrystal lcd3(46, 48, 10, 11, 12, 13);

Servo servoInc;

const uint8_t BTN_START  = 22;
const uint8_t BTN_VEL_UP = 24;
const uint8_t BTN_VEL_DN = 26;
const uint8_t BTN_INC_UP = 28;
const uint8_t BTN_INC_DN = 30;
const uint8_t BTN_PAGE   = 32; // ya no cambia pantalla, libre para uso futuro
const uint8_t SW_PARO    = 34;
const uint8_t SW_GENERAL = 3; // Switch principal de encendido/apagado
const uint8_t SENS_PASO  = 2;

const uint8_t MOTOR_PWM  = 5;
const uint8_t SERVO_INC  = 6;
const uint8_t LED_MARCHA = 40;
const uint8_t LED_PARO   = 42;
const uint8_t BUZZER     = 44;

const uint8_t POT_PESO   = A0;

const float VEL_MAX = 16.0;
const float VEL_PASO = 0.5;
const uint8_t INC_MAX = 15;
const unsigned long RESET_HOLD_MS = 2000;

float velocidad = 0.0;
uint8_t inclinacion = 0;
float distancia = 0.0;
float kcal = 0.0;
float metaKcal = 300.0;
float peso = 70.0;
unsigned long tiempo = 0;
volatile unsigned long pasos = 0;

bool enMarcha = false;
bool sistemaEncendido = false; // Control de encendido general

unsigned long tFisica = 0, tLcd1 = 0, tLcd2 = 0, tLcd3 = 0, tBtn = 0;
unsigned long tParoPress = 0;
bool paroPresionado = false;
bool resetHecho = false;

void contarPaso() {
  static unsigned long ult = 0;
  unsigned long ahora = millis();
  if (ahora - ult > 120) { pasos++; ult = ahora; }
}

void setup() {
  pinMode(BTN_START,  INPUT_PULLUP);
  pinMode(BTN_VEL_UP, INPUT_PULLUP);
  pinMode(BTN_VEL_DN, INPUT_PULLUP);
  pinMode(BTN_INC_UP, INPUT_PULLUP);
  pinMode(BTN_INC_DN, INPUT_PULLUP);
  pinMode(BTN_PAGE,   INPUT_PULLUP);
  pinMode(SW_PARO,    INPUT_PULLUP);
  pinMode(SENS_PASO,  INPUT_PULLUP);
  pinMode(SW_GENERAL, INPUT);

  pinMode(MOTOR_PWM,  OUTPUT);
  pinMode(LED_MARCHA, OUTPUT);
  pinMode(LED_PARO,   OUTPUT);
  pinMode(BUZZER,     OUTPUT);

  servoInc.attach(SERVO_INC);
  servoInc.write(0);

  attachInterrupt(digitalPinToInterrupt(SENS_PASO), contarPaso, FALLING);

  lcd1.begin(16, 2);
  lcd2.begin(16, 2);
  lcd3.begin(16, 2);

}

void iniciarSistema() {
  lcd1.clear(); lcd2.clear(); lcd3.clear();
  lcd1.print("CAMINADORA ESPE");
  lcd1.setCursor(0, 1);
  lcd1.print("Iniciando...");
  digitalWrite(LED_PARO, HIGH);
  delay(1500);
  reiniciarCaminadora();
}

void apagarSistema() {
  detener();
  digitalWrite(LED_PARO, LOW);
  digitalWrite(LED_MARCHA, LOW);
  servoInc.write(0);
  lcd1.clear();
  lcd2.clear();
  lcd3.clear();
}

float kcalPorMinuto() {
  if (velocidad <= 0.0) return 0.0;
  float S = velocidad * 16.6667;
  float G = inclinacion / 100.0;
  float vo2;
  if (velocidad < 7.0) vo2 = 3.5 + 0.1 * S + 1.8 * S * G;
  else                 vo2 = 3.5 + 0.2 * S + 0.9 * S * G;
  return vo2 * peso * 5.0 / 1000.0;
}

void beep(uint16_t f, uint16_t ms) {
  tone(BUZZER, f, ms);
}

void arrancar() {
  enMarcha = true;
  if (velocidad < 1.0) velocidad = 1.0;
  beep(1500, 120);
}

void detener() {
  enMarcha = false;
  velocidad = 0.0;
  analogWrite(MOTOR_PWM, 0);
  beep(600, 250);
}

void reiniciarCaminadora() {
  detener();
  distancia = 0.0;
  kcal = 0.0;
  metaKcal = 300.0;
  tiempo = 0;
  pasos = 0;
  inclinacion = 0;
  servoInc.write(0);
  beep(2200, 150);
  delay(180);
  beep(2200, 150);
  lcd1.clear();
  lcd2.clear();
  lcd3.clear();
}

void leerBotones() {
  // --- Manejo de SW_PARO: paro inmediato + reset por presion larga (2 s) ---
  if (digitalRead(SW_PARO) == LOW) {
    if (!paroPresionado) {
      paroPresionado = true;
      tParoPress = millis();
      resetHecho = false;
      if (enMarcha) detener();
    } else if (!resetHecho && millis() - tParoPress >= RESET_HOLD_MS) {
      reiniciarCaminadora();
      resetHecho = true;
    }
    return; // mientras este activado, no se procesan otros botones
  } else {
    paroPresionado = false;
    resetHecho = false;
  }

  if (millis() - tBtn < 180) return;

  if (digitalRead(BTN_START) == LOW) {
    if (enMarcha) detener(); else arrancar();
    tBtn = millis();
  }
  else if (digitalRead(BTN_VEL_UP) == LOW && enMarcha) {
    if (velocidad + VEL_PASO <= VEL_MAX) velocidad += VEL_PASO;
    beep(2000, 60);
    tBtn = millis();
  }
  else if (digitalRead(BTN_VEL_DN) == LOW && enMarcha) {
    if (velocidad - VEL_PASO >= 0.5) velocidad -= VEL_PASO;
    beep(1200, 60);
    tBtn = millis();
  }
  else if (digitalRead(BTN_INC_UP) == LOW) {
    if (inclinacion < INC_MAX) inclinacion++;
    beep(2000, 60);
    tBtn = millis();
  }
  else if (digitalRead(BTN_INC_DN) == LOW) {
    if (inclinacion > 0) inclinacion--;
    beep(1200, 60);
    tBtn = millis();
  }
}

void actualizarFisica() {
  unsigned long ahora = millis();
  float dt = (ahora - tFisica) / 1000.0;
  if (dt < 0.1) return;
  tFisica = ahora;

  peso = 40.0 + (analogRead(POT_PESO) / 1023.0) * 80.0;

  servoInc.write(map(inclinacion, 0, INC_MAX, 0, 90));
  digitalWrite(LED_MARCHA, enMarcha);
  digitalWrite(LED_PARO, !enMarcha);

  if (!enMarcha) { analogWrite(MOTOR_PWM, 0); return; }

  analogWrite(MOTOR_PWM, (uint8_t)(velocidad / VEL_MAX * 255.0));

  tiempo += (unsigned long)(dt * 1000);
  distancia += velocidad * dt / 3600.0;
  kcal += kcalPorMinuto() * dt / 60.0;

  if (kcal >= metaKcal) {
    beep(2500, 400);
    metaKcal += 300.0;
  }
}

void mostrarLcd1() {
  if (millis() - tLcd1 < 300) return;
  tLcd1 = millis();

  char l1[17], l2[17];
  dtostrf(velocidad, 4, 1, l1);
  lcd1.setCursor(0, 0);
  lcd1.print("VEL "); lcd1.print(l1); lcd1.print(" km/h ");
  lcd1.setCursor(0, 1);
  sprintf(l2, "INC %2u%%  %s", inclinacion, enMarcha ? "MARCHA" : "PARADO");
  lcd1.print(l2); lcd1.print("  ");
}

void mostrarLcd2() {
  if (millis() - tLcd2 < 300) return;
  tLcd2 = millis();

  char l1[17];
  unsigned long seg = tiempo / 1000;
  sprintf(l1, "TIEMPO %02lu:%02lu:%02lu", seg / 3600, (seg % 3600) / 60, seg % 60);
  lcd2.setCursor(0, 0); lcd2.print(l1);

  char d[8]; dtostrf(distancia, 5, 2, d);
  lcd2.setCursor(0, 1);
  lcd2.print("DIST "); lcd2.print(d); lcd2.print(" km ");
}

void mostrarLcd3() {
  if (millis() - tLcd3 < 300) return;
  tLcd3 = millis();

  char k[8]; dtostrf(kcal, 5, 1, k);
  lcd3.setCursor(0, 0);
  lcd3.print("KCAL "); lcd3.print(k); lcd3.print("     ");

  float falta = metaKcal - kcal;
  if (falta < 0) falta = 0;
  char f[8]; dtostrf(falta, 5, 1, f);
  lcd3.setCursor(0, 1);
  lcd3.print("FALTAN "); lcd3.print(f); lcd3.print("  ");
}

void loop() {
  bool estadoSwitch = (digitalRead(SW_GENERAL) == HIGH);

  if (estadoSwitch && !sistemaEncendido) {
    sistemaEncendido = true;
    iniciarSistema();
  } 
  else if (!estadoSwitch && sistemaEncendido) {
    sistemaEncendido = false;
    apagarSistema();
  }

  if (sistemaEncendido) {
    leerBotones();
    actualizarFisica();
    mostrarLcd1();
    mostrarLcd2();
    mostrarLcd3();
  }
}
