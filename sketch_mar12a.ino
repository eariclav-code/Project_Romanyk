#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // адрес 0x27, экран 16x2

// DS18B20
#define ONE_WIRE_BUS 15
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  delay(2000); // ждём USB
  lcd.init();  // инициализация LCD
  lcd.backlight();

  sensors.begin(); // инициализация DS18B20

  lcd.setCursor(0,0);
  lcd.print("Temp Sensor"); // первая строка
}

void loop() {
  sensors.requestTemperatures();            // запрашиваем температуру
  float tempC = sensors.getTempCByIndex(0); // читаем первую температуру

  lcd.setCursor(0,1);                        // вторая строка
  lcd.print("Temp: ");
  lcd.print(tempC);                           // выводим число
  lcd.print((char)223);                        // символ °
  lcd.print("C   ");                          // чтобы очищать старые значения

  delay(1000); // обновляем каждую секунду
}