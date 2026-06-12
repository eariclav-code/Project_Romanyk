#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"
#include <Adafruit_AHTX0.h>

#define BUTTON_PIN 2 //Определение пинов
#define LED_HIGH_PIN 5   // синий низкая температура / влажность / свет
#define LED_LOW_PIN 4  // красный высокая температура / влажность
#define LIGHT_SENSOR_PIN A0 //Красный светодиод — пин 4. Загорается при высокой температуре/влажности.
LiquidCrystal_I2C lcd(0x27, 20, 4); //LCD-дисплей: I2C-адрес 0x27, размер 20 символов × 4 строки.
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
unsigned long lastPressTime = 0; //Время последнего нажатия кнопки (для авто-выключения подсветки).
bool lcdOn = true; //Флаг — подсветка включена или нет.

unsigned long lastAHTRead = 0;
float temperature = 0;         //Время последнего чтения датчика + текущие показания температуры и влажности.
float humidityValue = 0;

unsigned long lastLightRead = 0;
int lightValue = 0;               //Время последнего чтения фоторезистора + текущая освещённость.

unsigned long lastLedToggleLow = 0;
unsigned long lastLedToggleHigh = 0;
bool ledLowState = false;              //Таймеры и состояния для мигания каждого светодиода независимо.
bool ledHighState = false;

// -------------------- Настройка --------------------
void setup() {
  Wire.begin();
  lcd.init();      //Инициализация дисплея и включение подсветки.
  lcd.backlight(); //Инициализация дисплея и включение подсветки.

  pinMode(BUTTON_PIN, INPUT_PULLUP); //Кнопка с подтяжкой к питанию — при нажатии даёт LOW.
  pinMode(LED_LOW_PIN, OUTPUT); //Пины светодиодов — на выход.
  pinMode(LED_HIGH_PIN, OUTPUT); //Пины светодиодов — на выход.

  // RTC
  if (!rtc.begin()) {      //Если RTC не найден — вывести ошибку и зависнуть навсегда.
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
      ledLowState = !ledLowState;                                               //Если температура ниже 10 °C — синий LED мигает каждые 5 секунд (тревожный сигнал). Используется millis() — без блокировки delay().
      digitalWrite(LED_LOW_PIN, ledLowState ? HIGH : LOW);
      lastLedToggleLow = currentMillis;
    }
  } else if (temp > TEMP_CRIT_HIGH) {                   //Если выше 35 °C — то же самое, но мигает красный LED.
    if (currentMillis - lastLedToggleHigh > 5000) {
      ledHighState = !ledHighState;
      digitalWrite(LED_HIGH_PIN, ledHighState ? HIGH : LOW);
      lastLedToggleHigh = currentMillis;
    }
  } else {
    // -------------------- Нормальный диапазон --------------------
    // LED LOW
    if (temp < tempMin || hum < humMin || (isDay && light < LIGHT_DAY_MIN)) {  //Ошибка в коде — пропущены операторы ||. Должно быть именно так: мигаем, если хотя бы одно условие нарушено в меньшую сторону.
      if (currentMillis - lastLedToggleLow > 1000) { // мягкое мигание 1 сек
        ledLowState = !ledLowState;
        digitalWrite(LED_LOW_PIN, ledLowState ? HIGH : LOW);
        lastLedToggleLow = currentMillis;
      }
    } else {
      digitalWrite(LED_LOW_PIN, LOW);  //Всё в норме — светодиод выключен.
      ledLowState = false;
    }

    // LED HIGH
    if (temp > tempMax || hum > humMax || (!isDay && light > LIGHT_NIGHT_MAX) ) { //Мигает красный, если что-то превышает норму или ночью слишком светло.
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
  if (currentMillis - lastAHTRead > 10000) { //Считываем температуру и влажность раз в 10 секунд (датчик медленный).
    sensors_event_t humidityEvent, tempEvent;
    aht.getEvent(&humidityEvent, &tempEvent);
    temperature = tempEvent.temperature;
    humidityValue = humidityEvent.relative_humidity;
    lastAHTRead = currentMillis;
  }

  //  Фоторезистор раз в 3 секунды
  if (currentMillis - lastLightRead > 3000) { //Считываем фоторезистор раз в 3 секунды.
    lightValue = analogRead(LIGHT_SENSOR_PIN);
    lastLightRead = currentMillis;
  }

  //  Кнопка включения подсветки
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      lcd.backlight();                       //Нажатие кнопки включает подсветку и сбрасывает таймер. delay(50) — защита от дребезга контактов.
      lcdOn = true;
      lastPressTime = currentMillis;
      while (digitalRead(BUTTON_PIN) == LOW);
    }
  }

  // Авто-выключение подсветки через 30 секунд
  if (lcdOn && (currentMillis - lastPressTime > 30000)) {
    lcd.noBacklight();                                     //Если 30 секунд не было нажатий — подсветка гаснет автоматически.
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
