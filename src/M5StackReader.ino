#include <Arduino.h>
#include <M5Unified.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[FACTORY TEST] Старт эталонного теста M5Unified...");

    // 1. Инициализация железа M5Stack Core2 (Микшер AW9523 и кодек включаются сами внутри библиотеки!)
    auto cfg = M5.config();
    M5.begin(cfg);

    // 2. Включаем силовое внешнее питание периферии через AXP192
    M5.Power.setExtOutput(true);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // 3. Инициализация SD-карты на стандартном для Core2 слоте (GPIO 4)
    Serial.println("[FACTORY TEST] Монтируем SD-карту...");
    if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
        Serial.println("[FACTORY TEST КРИТ] Ошибка: Не удалось открыть SD-карту!");
        while (true) delay(500);
    }

    // 4. Проверяем наличие файла на карте
    if (!SD.exists("/Beatles - Girl.mp3")) {
        Serial.println("[FACTORY TEST КРИТ] Файл /Beatles - Girl.mp3 НЕ НАЙДЕН на SD!");
        while (true) delay(500);
    }
    Serial.println("[FACTORY TEST] Файл найден! Запускаем встроенный MP3-плеер...");

    // =========================================================================
    // БОЕВОЙ ЗАПУСК ВСТРОЕННОГО КАНАЛА M5UNIFIED
    // =========================================================================
    M5.Speaker.setVolume(128); // Громкость 50%
    M5.Speaker.begin();        // Нативно будим чип AW9523 и кодек

    // Официальный метод проигрывания MP3 файлов во встроенном плеере M5Unified!
    // Первым параметром передаем объект смонтированной SD карты, вторым - путь к треку.
    if (M5.Speaker.playMp3(SD, "/Beatles - Girl.mp3")) {
        Serial.println("[FACTORY TEST УСПЕХ] Файл запущен! Пошел чистый I2S-поток...");
    } else {
        Serial.println("[FACTORY TEST КРИТ] Метод playMp3 отклонил воспроизведение!");
    }
}

void loop() {
    // КРИТИЧЕСКИ ВАЖНО: Метод M5.update() обязан крутиться непрерывно!
    // Именно внутри него M5Unified асинхронно читает байты с флешки,
    // декодирует MP3 и подкачивает их в аппаратный DMA-буфер кодека Core2!
    M5.update();

    // Раз в две секунды проверяем статус воспроизведения
    static unsigned long lastLog = 0;
    if (millis() - lastLog >= 2000) {
        lastLog = millis();
        Serial.printf("[AUDIO STATUS] Динамик активен: %s\n", 
                      M5.Speaker.isPlaying() ? "ДА (PLAYING)" : "НЕТ (SILENT)");
    }
    delay(1);
}
