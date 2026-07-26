#ifndef CONFIG_H
#define CONFIG_H

#define PROJECT_NAME "VK_Reader"

// ============================================================
// НАСТРОЙКИ WI-FI ПО УМОЛЧАНИЮ
// (используются, если нет сохраненного конфига)
// ============================================================
#define DEFAULT_SSID             "M5_Reader"
#define DEFAULT_PASSWORD         "12345678"

// ============================================================
// ВРЕМЕННЫЕ ПАРАМЕТРЫ И НАСТРОЙКИ ЖЕСТОВ ГИРОСКОПА
// ============================================================
#define GYRO_SAMPLE_INTERVAL_MS  20   // Период опроса датчиков (мс)
#define GYRO_DEBOUNCE_MS         400  // Защита от повторных срабатываний (мс)
#define TIME_GYROSCOPE_BOUNCE    200  // Окно накопления/анализа жеста (мс)

// Команды GyroJoystick (возвращаемые значения)
#define GYRO_CMD_NONE            0
#define GYRO_CMD_SHAKE           1    // Play / Stop
#define GYRO_CMD_ROLL_DOWN       2    // Volume -
#define GYRO_CMD_ROLL_UP         3    // Volume +
#define GYRO_CMD_PITCH_BWD       4    // Station -
#define GYRO_CMD_PITCH_FWD       5    // Station +

// ============================================================
// АППАРАТНАЯ ПЕРИФЕРИЯ ПЛАТЫ M5STACK CORE2
// ============================================================
// Физические пины SPI-интерфейса для встроенного SD-слота
#define SD_SPI_CS_PIN            4
#define SD_SPI_SCK_PIN           18
#define SD_SPI_MISO_PIN          38
#define SD_SPI_MOSI_PIN          23

// --- ФИЗИЧЕСКИЕ ПИНЫ АППАРАТНОГО ВСТРОЕННОГО ДИНАМИКА CORE2 ---
#define I2S_BCLK_PIN             12   // Тактовая частота шины I2S
#define I2S_LRCK_PIN             0    // Выбор канала (левый/правый)
#define I2S_DOUT_PIN             2    // Цифровой выход аудиоданных

// --- ВИРТУАЛЬНЫЕ ПИНЫ ИЗОЛЯЦИИ ДЛЯ БУДУЩЕГО РЕЖИМА BLUETOOTH ---
#define VIRTUAL_I2S_BCLK         25  
#define VIRTUAL_I2S_LRCK         26  
#define VIRTUAL_I2S_DOUT         27  

// ============================================================
// НАСТРОЙКИ ВРЕМЕНИ (NTP) И СЕТИ
// ============================================================
#define NTP_TIMEZONE_OFFSET      3.0f // Часовой пояс (Москва UTC+3)
#define NTP_SERVER1              "pool.ntp.org"
#define NTP_SERVER2              "time.nist.gov"
#define NTP_SERVER3              "ru.pool.ntp.org"

#define NTP_RETRY_INTERVAL       20   // Интервал повтора в секундах при неудаче
#define NTP_SUCCESS_INTERVAL     3600 // Интервал синхронизации в секундах при успехе

#define WIFI_RETRY_DELAY_MS      500  // Задержка между попытками подключения к Wi-Fi
#define WIFI_TIMEOUT_MS          15000// Таймаут подключения к Wi-Fi (15 секунд)

// =========================================================================
// УДВОЕННЫЕ РАЗМЕРЫ СТЕКА ДЛЯ ЗАДАЧ FREERTOS (БЕЗОПАСНОСТЬ СЕТИ И ПЛЕЕРА)
// =========================================================================
#define AUDIO_TASK_STACK_SIZE    8192 // (С запасом под тяжелые MP3 буферы)
#define CONTROL_TASK_STACK_SIZE  4096 // (Для стабильной отрисовки и логики)
#define NETWORK_TASK_STACK_SIZE  4096 // (Чтобы асинхронный сервер свободно переваривал данные)
#define GYRO_TASK_STACK_SIZE     4096 // (С запасом под высокоточные расчеты джойстика)

#define AUDIO_TASK_PRIORITY      5    // Самый высокий приоритет для плавного звука
#define CONTROL_TASK_PRIORITY    2
#define GYRO_TASK_PRIORITY       4    // Тот же уровень, что и кнопки (опрос параллельный)
#define NETWORK_TASK_PRIORITY    2

#define CONTROL_TASK_DELAY_MS    25
#define NETWORK_TASK_DELAY_MS    100

// ============================================================
// ГЕОМЕТРИЯ ДИСПЛЕЯ M5STACK (ILI9341 / ILI9342C)
// ============================================================
#define SCREEN_WIDTH             320
#define SCREEN_HEIGHT            240

// === ОСТАЛЬНЫЕ СИСТЕМНЫЕ НАСТРОЙКИ ===
#define DEFAULT_BRIGHTNESS       80
#define DEBUG_MODE               1    // 1 = включена отладка в Serial, 0 = выключена

#endif // CONFIG_H
