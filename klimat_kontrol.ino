#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include <Adafruit_AHTX0.h>

#define BUTTON_PIN 2
#define LED_HIGH_PIN 5   // синий низкая температура / влажность / свет
#define LED_LOW_PIN 4  // красный высокая температура / влажность
#define LIGHT_SENSOR_PIN A0
LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_PCF8563 rtc;
Adafruit_AHTX0 aht;

// -------------------- Константы --------------------
// Температура
const float TEMP_DAY_MIN = 20.0;
const float TEMP_DAY_MAX = 28.0;
const float TEMP_NIGHT_MIN = 15.0;
const float TEMP_NIGHT_MAX = 18.0;
const float TEMP_CRIT_LOW = 10.0;
const float TEMP_CRIT_HIGH = 35.0;

// Влажность
const float HUM_DAY_MIN = 50.0;
const float HUM_DAY_MAX = 60.0;
const float HUM_NIGHT_MIN = 60.0;
const float HUM_NIGHT_MAX = 85.0;

// Освещенность
const int LIGHT_DAY_MIN = 700;
const int LIGHT_NIGHT_MAX = 300;

// -------------------- Переменные --------------------
unsigned long lastPressTime = 0;
bool lcdOn = true;

unsigned long lastAHTRead = 0;
float temperature = 0;
float humidityValue = 0;

unsigned long lastLightRead = 0;
int lightValue = 0;

unsigned long lastLedToggleLow = 0;
unsigned long lastLedToggleHigh = 0;
bool ledLowState = false;
bool ledHighState = false;

// -------------------- Настройка --------------------
void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_LOW_PIN, OUTPUT);
  pinMode(LED_HIGH_PIN, OUTPUT);

  // RTC
  if (!rtc.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("RTC not found!");
    while (1);
  }

  // AHT20
  if (!aht.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("AHT20 error!");
    while (1);
  }

  // Настройка времени RTC (пример)
  // rtc.adjust(DateTime(2026, 4, 8, 15, 30, 0));
}

// -------------------- Функция управления светодиодами --------------------
void updateLEDs(float temp, float hum, int light, DateTime now) {
  unsigned long currentMillis = millis();
  //*********************ВРЕМЯ ДНЯ******************
  bool isDay = (now.hour() >= 8 && now.hour() < 20);

  float tempMin = isDay ? TEMP_DAY_MIN : TEMP_NIGHT_MIN;
  float tempMax = isDay ? TEMP_DAY_MAX : TEMP_NIGHT_MAX;
  float humMin  = isDay ? HUM_DAY_MIN : HUM_NIGHT_MIN;
  float humMax  = isDay ? HUM_DAY_MAX : HUM_NIGHT_MAX;

  // -------------------- Экстремальные температуры --------------------
  if (temp < TEMP_CRIT_LOW) {
    if (currentMillis - lastLedToggleLow > 5000) { // 5 сек ON / 1 сек OFF
      ledLowState = !ledLowState;
      digitalWrite(LED_LOW_PIN, ledLowState ? HIGH : LOW);
      lastLedToggleLow = currentMillis;
    }
  } else if (temp > TEMP_CRIT_HIGH) {
    if (currentMillis - lastLedToggleHigh > 5000) {
      ledHighState = !ledHighState;
      digitalWrite(LED_HIGH_PIN, ledHighState ? HIGH : LOW);
      lastLedToggleHigh = currentMillis;
    }
  } else {
    // -------------------- Нормальный диапазон --------------------
    // LED LOW
    if (temp < tempMin || hum < humMin || (isDay && light < LIGHT_DAY_MIN)) {
      if (currentMillis - lastLedToggleLow > 1000) { // мягкое мигание 1 сек
        ledLowState = !ledLowState;
        digitalWrite(LED_LOW_PIN, ledLowState ? HIGH : LOW);
        lastLedToggleLow = currentMillis;
      }
    } else {
      digitalWrite(LED_LOW_PIN, LOW);
      ledLowState = false;
    }

    // LED HIGH
    if (temp > tempMax || hum > humMax || (!isDay && light > LIGHT_NIGHT_MAX) ) {
      if (currentMillis - lastLedToggleHigh > 1000) {
        ledHighState = !ledHighState;
        digitalWrite(LED_HIGH_PIN, ledHighState ? HIGH : LOW);
        lastLedToggleHigh = currentMillis;
      }
    } else {
      digitalWrite(LED_HIGH_PIN, LOW);
      ledHighState = false;
    }
  }
}

// -------------------- Основной цикл --------------------
void loop() {
  unsigned long currentMillis = millis();

  //  AHT20 раз в 10 секунд
  if (currentMillis - lastAHTRead > 10000) {
    sensors_event_t humidityEvent, tempEvent;
    aht.getEvent(&humidityEvent, &tempEvent);
    temperature = tempEvent.temperature;
    humidityValue = humidityEvent.relative_humidity;
    lastAHTRead = currentMillis;
  }

  //  Фоторезистор раз в 3 секунды
  if (currentMillis - lastLightRead > 3000) {
    lightValue = analogRead(LIGHT_SENSOR_PIN);
    lastLightRead = currentMillis;
  }

  //  Кнопка включения подсветки
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      lcd.backlight();
      lcdOn = true;
      lastPressTime = currentMillis;
      while (digitalRead(BUTTON_PIN) == LOW);
    }
  }

  // Авто-выключение подсветки через 30 секунд
  if (lcdOn && (currentMillis - lastPressTime > 30000)) {
    lcd.noBacklight();
    lcdOn = false;
  }

  //  Время
  DateTime now = rtc.now();

  lcd.setCursor(0, 0);
  lcd.print("Time: ");
  if (now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());
  lcd.print("   ");

  //  Температура
  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temperature, 1);
  lcd.print(" C   ");

  //  Влажность
  lcd.setCursor(0, 2);
  lcd.print("Hum: ");
  lcd.print(humidityValue, 1);
  lcd.print(" %   ");

  //  Фоторезистор
  lcd.setCursor(0, 3);
  lcd.print("Light: ");
  lcd.print(lightValue);
  lcd.print("   ");

  //  Управление светодиодами
  updateLEDs(temperature, humidityValue, lightValue, now);

  delay(200);
}