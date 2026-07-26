// ============================================================
// M5StackReader.ino - Высококлассная 5-задачная архитектура FreeRTOS
// ============================================================
#include <Arduino.h>
#include <M5Unified.h>
#include "config.h"
#include "classes.h"
#include "display.h"
#include "html_const.h"
#include "Reader.h" 

// === ГЛОБАЛЬНЫЕ ОБЪЕКТЫ (RAM) ===
Display display;
WorkSPIFFS myFS;
WorkSPIFFS::ConfigData config;
WiFiConnect wifi;
Reader reader; 

// Прототипы пяти параллельных FreeRTOS-задач
void TaskAudioCode(void* pvParameters);
void TaskButtonCode(void* pvParameters);
void TaskGyroCode(void* pvParameters);
void TaskControlCode(void* pvParameters);
void TaskNetworkCode(void* pvParameters);

// Внешние обработчики низкого уровня из Reader.cpp
extern void taskAudio(void* parameter);
extern void taskControl(void* parameter);

// ============================================================
// SETUP - СТРОГО СИНХРОННЫЙ СТАРТ ЖЕЛЕЗА ДО ЗАПУСКА ЯДЕР
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // ШАГ 1: Старт чипа питания, шин и экрана Core2
    auto cfg = M5.config();
    M5.begin(cfg);
    // ============================================================
    // АППАРАТНЫЙ ФИКС ПИТАНИЯ ДЛЯ M5UNIFIED (ЗАЩИТА ОТ ПЕРЕЗАГРУЗОК)
    // ============================================================
    // Жестко обесточиваем встроенный усилитель звука (динамик) на старте.
    // Это полностью разгрузит USB-шину, чтобы Wi-Fi спокойно и без просадок поднял сеть!
    M5.Power.setExtOutput(false);
    // ============================================================
    display.begin();
    display.showStartup("Booting...");

    // ШАГ 2-3: Спокойно монтируем SPIFFS и читаем настройки Wi-Fi в RAM
    if (myFS.begin()) {
        myFS.loadConfig(config);
    } else {
        display.showStartup("SPIFFS FAILED!");
        while (true) delay(100);
    }

    // ШАГ 3.5: Сканируем флешку, нумеруем и сортируем файлы по алфавиту
    if (reader.begin(config)) {
        reader.scanFiles(); 
    } else {
        display.showStartup("SD CARD FAILED!");
        while (true) delay(100);
    }

    // =========================================================================
    // ШАГ 4: ВСЁ ЖЕЛЕЗО И ПАМЯТЬ ГОТОВЫ. ЗАПУСКАЕМ 5 ДВИГАТЕЛЕЙ FREERTOS!
    // =========================================================================
#if DEBUG_MODE
    Serial.println("[SYSTEM] Инициализация завершена. Распределяем задачи по ядрам...");
#endif

    // 1. ЗВУКОВОЙ КОНВЕЙЕР (Ядро 0, Максимальный приоритет 5)
    xTaskCreatePinnedToCore(TaskAudioCode, "AudioTask", AUDIO_TASK_STACK_SIZE, NULL, AUDIO_TASK_PRIORITY, NULL, 0);

    // 2. ОПРОС ТАЧСКРИНА (Ядро 1, Высокий приоритет 4)
    xTaskCreatePinnedToCore(TaskButtonCode, "ButtonTask", CONTROL_TASK_STACK_SIZE, NULL, 4, NULL, 1);

    // 3. ОПРОС ЖЕСТОВ И ГИРОСКОПА (Ядро 1, Высокий приоритет 4)
    xTaskCreatePinnedToCore(TaskGyroCode, "GyroTask", GYRO_TASK_STACK_SIZE, NULL, GYRO_TASK_PRIORITY, NULL, 1);

    // 4. ИНТЕРФЕЙС И АНИМАЦИЯ ЭКРАНА (Ядро 1, Средний приоритет 2)
    xTaskCreatePinnedToCore(TaskControlCode, "ControlTask", CONTROL_TASK_STACK_SIZE, NULL, CONTROL_TASK_PRIORITY, NULL, 1);

    // 5. СЕТЬ, СЕРВЕР И ЧАСЫ NTP (Ядро 1, Низкий приоритет 1)
    xTaskCreatePinnedToCore(TaskNetworkCode, "NetworkTask", NETWORK_TASK_STACK_SIZE, NULL, NETWORK_TASK_PRIORITY, NULL, 1);
}

void loop() {
    vTaskDelete(NULL); // Самоуничтожение loopTask ради экономии памяти
}

// ============================================================
// 1. КОД АУДИО-ЗАДАЧИ (ЯДРО 0 - ВЫДЕЛЕННЫЙ ЗВУКОВОЙ ТРАКТ)
// ============================================================
void TaskAudioCode(void* pvParameters) {
    while (true) {
        taskAudio(NULL); 
        vTaskDelay(1 / portTICK_PERIOD_MS); 
    }
}

// ============================================================
// 2. ИСПРАВЛЕНО: ОТДЕЛЬНАЯ ЗАДАЧА ТАЧСКРИНА (ЯДРО 1, ОПРОС РАЗ В 50мс)
// ============================================================
void TaskButtonCode(void* pvParameters) {
#if DEBUG_MODE
    Serial.println("[FreeRTOS] TaskButton запущена на Ядре 1 с интервалом 50мс");
#endif
    while (true) {
        // Аппаратно опрашиваем емкостное стекло Core2
        M5.update(); 

        // Проверяем нажатия на три нижних стеклянных кружка под экраном
        if (M5.BtnA.wasPressed()) reader.pushButton(1); // Левый кружок
        if (M5.BtnB.wasPressed()) reader.pushButton(2); // Центральный кружок (Play/Stop)
        if (M5.BtnC.wasPressed()) reader.pushButton(3); // Правый кружок

        vTaskDelay(50 / portTICK_PERIOD_MS); // Ваша идеальная разгрузочная пауза 50мс!
    }
}

// ============================================================
// 3. КОД ЗАДАЧИ ГИРОСКОПА (ЯДРО 1 - ДЛЯ БУДУЩИХ ЖЕСТОВ НАКЛОНА)
// ============================================================
void TaskGyroCode(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(GYRO_SAMPLE_INTERVAL_MS);
    while (true) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Высокоточный шаг
        // Сюда мы завтра внедрим метод reader.runGyroJoystick() для наклонов устройства
    }
}

// ============================================================
// 4. КОД ЗАДАЧИ ИНТЕРФЕЙСА И АНИМАЦИИ (ЯДРО 1 - СТРОГО 40мс НА ШАГ)
// ============================================================
void TaskControlCode(void* pvParameters) {
    unsigned long lastScrollTime = 0;
    while (true) {
        // Обновление бегущей строки (~25 кадров в секунду)
        if (millis() - lastScrollTime >= 40) {
            lastScrollTime = millis();
            String currentTrack = reader.getCurrentFileName();
            display.updateScrollText(currentTrack); // Метод сам знает, бежать или стоять!
        }
        vTaskDelay(40 / portTICK_PERIOD_MS);
    }
}

// ============================================================
// 5. КОД СЕТЕВОЙ ЗАДАЧИ И ЧАСОВ (ЯДРО 1 - РАЗГРУЖЕННЫЙ ФОН)
// ============================================================
void TaskNetworkCode(void* pvParameters) {
    // Сеть стартует один раз строго при входе в таску
    if (!wifi.isSTA() && !wifi.isAP()) {
        wifi.setupWiFi(config, WIFI_TIMEOUT_MS);
    }
    while (true) {
        taskControl(NULL); // Ход автономных часов и поддержание NTP-статусов
        vTaskDelay(100 / portTICK_PERIOD_MS); // Сети спешить некуда, опрашиваем раз в 100 мс
    }
}
