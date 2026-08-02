#include <LiquidCrystal.h>
#include <Servo.h>

LiquidCrystal lcd(8, 9, 10, 11, 12, 13);
Servo servoInc;

const uint8_t BTN_START  = 22;
const uint8_t BTN_VEL_UP = 24;
const uint8_t BTN_VEL_DN = 26;
const uint8_t BTN_INC_UP = 28;
const uint8_t BTN_INC_DN = 30;
const uint8_t BTN_PAGE   = 32;
const uint8_t SW_PARO    = 34;
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

float velocidad = 0.0;
uint8_t inclinacion = 0;
float distancia = 0.0;
float kcal = 0.0;
float metaKcal = 300.0;
float peso = 70.0;
unsigned long tiempo = 0;
volatile unsigned long pasos = 0;

bool enMarcha = false;
uint8_t pagina = 0;

unsigned long tFisica = 0, tLcd = 0, tBtn = 0;

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

  pinMode(MOTOR_PWM,  OUTPUT);
  pinMode(LED_MARCHA, OUTPUT);
  pinMode(LED_PARO,   OUTPUT);
  pinMode(BUZZER,     OUTPUT);

  servoInc.attach(SERVO_INC);
  servoInc.write(0);

  attachInterrupt(digitalPinToInterrupt(SENS_PASO), contarPaso, FALLING);

  lcd.begin(16, 2);
  lcd.print("CAMINADORA ESPE");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  digitalWrite(LED_PARO, HIGH);
  delay(1500);
  lcd.clear();
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

void leerBotones() {
  if (millis() - tBtn < 180) return;

  if (digitalRead(SW_PARO) == LOW) {
    if (enMarcha) { detener(); tBtn = millis(); }
    return;
  }

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
  else if (digitalRead(BTN_PAGE) == LOW) {
    pagina = (pagina + 1) % 3;
    tBtn = millis();
    lcd.clear();
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

void mostrarLcd() {
  if (millis() - tLcd < 300) return;
  tLcd = millis();

  char l1[17], l2[17];
  unsigned long seg = tiempo / 1000;

  if (pagina == 0) {
    dtostrf(velocidad, 4, 1, l1);
    lcd.setCursor(0, 0);
    lcd.print("VEL "); lcd.print(l1); lcd.print(" km/h ");
    lcd.setCursor(0, 1);
    sprintf(l2, "INC %2u%%  %s", inclinacion, enMarcha ? "MARCHA" : "PARADO");
    lcd.print(l2); lcd.print("  ");
  }
  else if (pagina == 1) {
    sprintf(l1, "TIEMPO %02lu:%02lu:%02lu", seg / 3600, (seg % 3600) / 60, seg % 60);
    lcd.setCursor(0, 0); lcd.print(l1);
    char d[8]; dtostrf(distancia, 5, 2, d);
    lcd.setCursor(0, 1);
    lcd.print("DIST "); lcd.print(d); lcd.print(" km ");
  }
  else {
    char k[8]; dtostrf(kcal, 5, 1, k);
    lcd.setCursor(0, 0);
    lcd.print("KCAL "); lcd.print(k); lcd.print("     ");
    float falta = metaKcal - kcal;
    if (falta < 0) falta = 0;
    char f[8]; dtostrf(falta, 5, 1, f);
    lcd.setCursor(0, 1);
    lcd.print("FALTAN "); lcd.print(f); lcd.print("  ");
  }
}

void loop() {
  leerBotones();
  actualizarFisica();
  mostrarLcd();
}
