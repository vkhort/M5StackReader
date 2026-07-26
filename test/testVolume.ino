#include "Arduino.h"
#include <M5Unified.h>  // Используем только родную библиотеку

int currentVolume = 10; // Стартовая громкость
uint32_t lastToneTime = 0;

void setup() {
    // 1. Инициализируем плату (автоматика сама включит нужные цепи)
    auto cfg = M5.config();
    M5.begin(cfg);

    delay(1000); 

    // 2. Инициализируем встроенный спикер M5Unified
    M5.Speaker.begin();

    // Выводим информацию на экран
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.println("--- AUDIO TEST ---");
    M5.Display.println("CheckVolume Sketch");
}

void loop() {
    M5.update(); // Обновляем состояние системы

    // Каждую секунду меняем громкость и издаем писк
    if (millis() - lastToneTime > 1000) {
        lastToneTime = millis();

        // Устанавливаем громкость встроенного спикера (шкала от 0 до 255)
        M5.Speaker.setVolume(currentVolume);

        // Стираем старый текст и пишем текущую громкость в процентах
        M5.Display.clearDisplay();
        M5.Display.setCursor(0, 0);
        M5.Display.setTextColor(GREEN);
        M5.Display.printf("Volume Level: %d / 255\n", currentVolume);
        M5.Display.printf("Percent: %d%%\n", (currentVolume * 100) / 255);

        // Издаем звук: tone(частота_Гц, длительность_мс)
        // Меняем частоту в зависимости от громкости, чтобы слышать разницу
        uint32_t frequency = 1000 + (currentVolume * 4);
        M5.Speaker.tone(frequency, 400); 

        // Выводим лог в монитор порта
        Serial.printf("[TEST] Volume set to: %d, Freq: %d Hz\n", currentVolume, frequency);

        // Шаг увеличения громкости
        currentVolume += 35;

        // Если дошли до максимума, сбрасываем обратно на 10
        if (currentVolume > 255) {
            currentVolume = 10;
            M5.Display.setTextColor(YELLOW);
            M5.Display.println("\nLoop restarted!");
            delay(1000);
        }
    }
}
