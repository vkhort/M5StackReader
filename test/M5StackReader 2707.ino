// ============================================================
// M5StackReader.ino - Высококлассная 5-задачная архитектура FreeRTOS
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include "XPowersLib.h"    // Библиотека питания
#include "config.h"
#include "classes.h"
#include "display.h"
#include "html_const.h"
#include "Reader.h" 

// === ГЛОБАЛЬНЫЕ ОБЪЕКТЫ (RAM) ===
XPowersAXP192 power;       
Reader reader;    
Display display;
WorkSPIFFS myFS;
WorkSPIFFS::ConfigData config;
WiFiConnect wifi;

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
    Serial.println("\n=== ЧИСТЫЙ СТАРТ МЕДИАЦЕНТРА M5STACK CORE2 ===");

    // Инициализируем шину I2C через константы из config.h
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); 

    // =========================================================================
    // НОВЫЙ ЧИСТЫЙ ШАГ 1: Старт питания и усилителя напрямую через XPowersLib
    // =========================================================================
    if (!power.begin(Wire, AXP192_I2C_ADDRESS, I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("[ОШИБКА] Контроллер питания AXP192 не отвечает!");
    } else {
        power.setDC3Voltage(3300);   // Ток на экран и подсветку LovyanGFX (3.3В)
        power.setLDO2Voltage(3300);  // Ток на слот SD-карты (3.3В)
        power.setLDO3Voltage(2000);  // Ток на доп. периферию (2.0В)
        power.enableLDOio();         // Физически включаем встроенный аудиоусилитель NS4168
        Serial.println("[УСПЕХ] Ток на периферию, SD-карту и усилитель подан.");
    }

    // 2. ЗАПУСК ЭКРАНА LOVYANGFX И ВНУТРЕННИХ СТРУКТУР ПЛАТЫ
    display.begin();
    display.showStartup("System Loading...");

    // 3. МОНТИРУЕМ ВНУТРЕННЮЮ ПАМЯТЬ LITTLEFS / SPIFFS
    Serial.println("[SYSTEM] Монтируем внутреннюю память LittleFS...");
    if (!SPIFFS.begin(true)) { 
        Serial.println("[ОШИБКА] Файловая система LittleFS повреждена!");
        display.showStartup("LittleFS Error!");
        delay(2000);
    } else {
        Serial.println("[УСПЕХ] Внутренняя память LittleFS смонтирована.");
        myFS.loadConfig(config); // Загружаем сохраненный конфиг Wi-Fi и оператора
    }

        // ШАГ 3.5: Сканируем флешку, нумеруем и сортируем файлы по алфавиту
    if (reader.begin(config)) {
        reader.scanFiles(); 
    } else {
        display.showStartup("SD CARD FAILED!");
        while (true) delay(100);
    }

    // 4. ИНИЦИАЛИЗАЦИЯ ЖЕЛЕЗА SD-КАРТЫ ПО ШИНЕ SPI
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("[ОШИБКА] Карта памяти MicroSD не обнаружена!");
        display.showStartup("SD Card Error!");
    } else {
        Serial.println("[УСПЕХ] MicroSD карта успешно смонтирована.");
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

// ============================================================================
// КОД ОПРОСА ТАЧСКРИНА И КНОПОК ПОД ЭКРАНОМ (ЯДРО 1)
// ============================================================================
void TaskButtonCode(void* pvParameters) {
    // 🔥 СООБЩАЕМ КОМПИЛЯТОРУ, ЧТО ПЛЕЕР СУЩЕСТВУЕТ В ДРУГОМ ФАЙЛЕ
    extern Reader* globalReader;
    int16_t touchX = 0;
    int16_t touchY = 0;
    bool isButtonPressed = false;

    while (true) {
        // Если есть физическое касание, забираем сырые координаты X и Y
        if (display.getTouch(&touchX, &touchY)) {
            if (!isButtonPressed) {
                isButtonPressed = true;
                
                // 🔥 НАПРЯМУЮ вызываем наш глобальный объект плеера!
                // Никаких путаниц с pvParameters и nullptr
                if (globalReader != nullptr) {
                    globalReader->pressButton(touchX, touchY);
                }
            }
        } else {
            isButtonPressed = false; // Палец поднят — сбрасываем блокировку
        }
        vTaskDelay(50 / portTICK_PERIOD_MS); // Разгрузочная пауза 50мс
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