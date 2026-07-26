// ============================================================
// classes.cpp - Упрощенная реализация WorkSPIFFS (БЕЗ JSON)
// ============================================================
#include "classes.h"
#include "config.h"
#include "html_const.h"
#include <WiFi.h>
#include "display.h" 
#include "Reader.h"

extern WorkSPIFFS myFS;
extern Display display;
extern WorkSPIFFS::ConfigData config;

// ============================================================
// РЕАЛИЗАЦИЯ ConfigData
// ============================================================

// Все методы загрузки станций и получения URL полностью УДАЛЕНЫ,
// так как в файле /config теперь хранятся только две строки Wi-Fi!

// ============================================================
// РЕАЛИЗАЦИЯ КЛАССА WorkSPIFFS (БЕЗ ИСПОЛЬЗОВАНИЯ JSON)
// ============================================================
WorkSPIFFS::WorkSPIFFS() : _mounted(false) 
{
#if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Конструктор вызван");
#endif
}

WorkSPIFFS::~WorkSPIFFS() {
    if (_mounted) {
        SPIFFS.end(); // Безопасно закрываем файловую систему при уничтожении объекта
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] SPIFFS успешно размонтирован");
#endif
    }
}


// ============================================================
// БАЗОВЫЕ ОПЕРАЦИИ С ПАМЯТЬЮ
// ============================================================

bool WorkSPIFFS::begin() {
#if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Монтируем файловую систему SPIFFS...");
#endif
    // Передаем true в качестве параметра. Если файловая система повреждена,
    // ESP32 автоматически отформатирует её, предотвращая вечный сбой (Crash).
    _mounted = SPIFFS.begin(true);
    if (!_mounted) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] КРИТИЧЕСКАЯ ОШИБКА: Не удалось смонтировать SPIFFS!");
#endif
        return false;
    }
#if DEBUG_MODE
    Serial.println("[WorkSPIFFS] SPIFFS успешно смонтирован");
#endif

    // Проверяем, существует ли файл конфигурации на Flash-памяти
    if (!SPIFFS.exists(_configFile)) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Файл конфигурации не найден, создаем дефолтный...");
#endif
        ConfigData defaultData; 
        
        // Заполняем дефолтными строками Wi-Fi
        setDefaults(defaultData);
        
        // Записываем чистые текстовые настройки в файл
        saveConfig(defaultData);
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Текстовый файл конфигурации успешно создан");
#endif
    } else {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Файл конфигурации обнаружен на диске");
#endif
    }
    return true;
}

void WorkSPIFFS::setDefaults(ConfigData& data) {
    // Больше нет станций, громкости и индексов — только 2 базовые текстовые строки
    data.ssid = DEFAULT_SSID;
    data.password = DEFAULT_PASSWORD;

#if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Установлен дефолтный сетевой конфиг из config.h");
#endif
}

// ============================================================
// БЕЗОПАСНАЯ ЗАГРУЗКА КОНФИГА ИЗ ТЕКСТОВОГО ФАЙЛА (Core 1)
// ============================================================
bool WorkSPIFFS::loadConfig(ConfigData& data) {
    if (!_mounted) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Ошибка: SPIFFS не смонтирован, берем дефолт");
#endif
        setDefaults(data);
        return false;
    }

    if (!SPIFFS.exists(_configFile)) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Файл конфигурации не найден, берем дефолт");
#endif
        setDefaults(data);
        return false;
    }

    File file = SPIFFS.open(_configFile, "r");
    if (!file) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Не удалось открыть файл конфигурации");
#endif
        setDefaults(data);
        return false;
    }

#ifdef DEBUG_MODE
    // ============================================================
    // ВРЕМЕННО: Вывод содержимого текстового файла в консоль
    // ============================================================
    Serial.println("[WorkSPIFFS] ===== CONFIG FILE CONTENT =====");
    while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println(line);
    }
    Serial.println("[WorkSPIFFS] ===== END OF CONFIG FILE =====");
    file.seek(0); // Возвращаем указатель чтения в начало файла
#endif

    // Основной цикл построчного текстового парсинга формата "ключ=значение"
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        // Пропускаем пустые строки и закомментированные строчки (через #)
        if (line.length() == 0 || line.startsWith("#")) continue;

        int eqPos = line.indexOf('=');
        if (eqPos < 0) continue;

        String key = line.substring(0, eqPos);
        String value = line.substring(eqPos + 1);

        // Парсим только сетевые настройки, всё лишнее удалено
        if (key == "ssid") {
            data.ssid = value;
        } else if (key == "password") {
            data.password = value;
        }
    }
    file.close();

#if DEBUG_MODE
    Serial.printf("[WorkSPIFFS] Успех! Сетевой конфиг загружен. SSID: '%s'\n\n", data.ssid.c_str());
#endif
    return true;
}

// ============================================================
// БЕЗОПАСНОЕ СОХРАНЕНИЕ КОНФИГА В ТЕКСТОВЫЙ ФАЙЛ (Core 1)
// ============================================================
bool WorkSPIFFS::saveConfig(const ConfigData& data) {
    if (!_mounted) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Ошибка сохранения: SPIFFS не смонтирован");
#endif
        return false;
    }

    // Открываем файл в режиме записи ("w" полностью перезаписывает файл с нуля)
    File file = SPIFFS.open(_configFile, "w");
    if (!file) {
#if DEBUG_MODE
        Serial.println("[WorkSPIFFS] Не удалось открыть файл конфигурации для записи");
#endif
        return false;
    }

    // Записываем только сетевые настройки, всё лишнее удалено
    file.println("ssid=" + data.ssid);
    file.println("password=" + data.password);

    // Физически закрываем файл, фиксируя данные на Flash-диске ESP32
    file.close();

#if DEBUG_MODE
    Serial.println("[WorkSPIFFS] Сетевой конфиг успешно сохранен на диске\n");
#endif
    return true;
}


// ============================================================
// РЕАЛИЗАЦИЯ СЕТЕВОГО КЛАССА WiFiConnect
// ============================================================

void WiFiConnect::setAPCredentials(const String& ssid, const String& password) {
    apSSID = ssid;
    apPassword = password;
#if DEBUG_MODE
    Serial.printf("[WiFiConnect] Изменены настройки встроенной AP: SSID=%s\n", apSSID.c_str());
#endif
}

bool WiFiConnect::setupWiFi(WorkSPIFFS::ConfigData& config, unsigned long timeoutMs) {
    // ЗАЩИТА: Если сеть уже поднята — повторно ничего не делаем
    if ((_mode == Mode::STA && isConnected()) || _mode == Mode::AP) {
        return true;
    }

    targetSSID = config.ssid;
    targetPassword = config.password;

    // САМ КЛАСС ОДИН РАЗ РАЗВОРАЧИВАЕТ СЕТКУ ИНТЕРФЕЙСА НА ЧЕРНОМ ФОНЕ ДО СТАРТА СЕТИ
    display.showMainInterface("", config.ssid, false);

    // Если в сохраненных настройках SSID пустой — сразу запускаем аварийную точку доступа
    if (targetSSID.isEmpty()) {
        #if DEBUG_MODE
        Serial.println("[WiFi] SSID пуст в памяти, запуск режима AP...");
        #endif
        startAPMode();
        return false;
    }

    #if DEBUG_MODE
    Serial.printf("[WiFi] Пробуем подключиться к: %s...\n", targetSSID.c_str());
    #endif

    // Запускаем неблокирующий цикл ожидания домашней сети
    if (connectToWiFi(targetSSID, targetPassword, timeoutMs)) {
        _mode = Mode::STA;
        #if DEBUG_MODE
        Serial.printf("[WiFi] Успешно подключено! Системный IP: %s\n", WiFi.localIP().toString().c_str());
        #endif
        
        // КРИТИЧЕСКИ ВАЖНО: Отключаем режим сна антенны, чтобы убрать заикания звука
        WiFi.setSleep(false); 

        // ИСПРАВЛЕНО: Выводим домашний IP в топ-бар и включаем NTP-время
        display.updateTopBar(config.ssid, WiFi.localIP().toString(), false);
        configTime(NTP_TIMEZONE_OFFSET * 3600, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

        // Настраиваем роуты управления плеером и запускаем асинхронный веб-сервер на Core 1
        setupWebServer(myFS, config);
        return true;
    }

    // Если за 15 секунд роутер не ответил — уходим в аварийный WiFi Manager
    #if DEBUG_MODE
    Serial.println("[WiFi] Ошибка подключения к роутеру, переходим в режим AP...");
    #endif
    
    _mode = Mode::AP;
    startAPMode(); // РАСКОММЕНТИРОВАНО: Переходим в точку доступа
    return false;
}

bool WiFiConnect::connectToWiFi(const String& ssid, const String& password, unsigned long timeoutMs) {
    WiFi.disconnect(true); // Полностью очищаем прошлые сетевые сокеты
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    
    WiFi.mode(WIFI_STA); // Переводим чип строго в режим клиента домашней сети
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    
    WiFi.begin(ssid.c_str(), password.c_str());
    unsigned long start = millis(); 

    // Во время ожидания сети процессорное ядро Core 1 полностью свободно!
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        #if DEBUG_MODE
        Serial.print(".");
        #endif
    }
    #if DEBUG_MODE
    Serial.println();
    #endif
    
    return WiFi.status() == WL_CONNECTED;
}

void WiFiConnect::startAPMode() {
    WiFi.disconnect(true);
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    
    WiFi.mode(WIFI_AP); // Переводим передатчик строго в изолированный режим раздачи Wi-Fi
    vTaskDelay(100 / portTICK_PERIOD_MS); 

    // Поднимаем аварийную Wi-Fi сеть для смартфона
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    
    // Берем имя сети и пароль из живого конфига
    WiFi.softAP(config.ssid.c_str(), config.password.c_str());
    _mode = Mode::AP;

    // ИСПРАВЛЕНО: Выводим IP-адрес и пароль точки доступа на наш новый топ-бар плеера!
    display.updateTopBar(config.ssid, "192.168.4.1", true, config.password);

    // Настраиваем аварийную страницу ввода паролей и запускаем сервер настроек
    setupWebServer(myFS, config);
}

    // ============================================================
    // ИНИЦИАЛИЗАЦИЯ АСИНХРОННЫХ РОУТОВ ВЕБ-СЕРВЕРА (Core 1)
    // ============================================================
    void WiFiConnect::setupWebServer(WorkSPIFFS& spiffs, WorkSPIFFS::ConfigData& config) {
        _server.reset();

        // 1. Отдача главной страницы настроек из Flash (PROGMEM)
        _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            extern const char CONFIG_HTML[] PROGMEM;
            AsyncWebServerResponse *response = request->beginResponse(200, "text/html", (const uint8_t*)CONFIG_HTML, strlen_P(CONFIG_HTML));
            response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
            request->send(response);
        });

        // 2. Роут для кнопки "Read" (отдает текущий конфиг в браузер)
        _server.on("/get-config", HTTP_GET, [&config](AsyncWebServerRequest *request){
            String json = "{\"ssid\":\"" + config.ssid + "\",\"password\":\"" + config.password + "\"}";
            request->send(200, "application/json", json);
        });

        // 3. Роут для кнопки "Save" (принимает данные из JavaScript)
        _server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
            extern WorkSPIFFS myFS;
            extern WorkSPIFFS::ConfigData config;

            if (request->hasParam("ssid", true)) {
                config.ssid = request->getParam("ssid", true)->value();
                config.ssid.trim();
            }
            if (request->hasParam("password", true)) {
                config.password = request->getParam("password", true)->value();
            }

            myFS.saveConfig(config);
            request->send(200, "text/plain", "OK");

            vTaskDelay(2000 / portTICK_PERIOD_MS);
            ESP.restart(); 
        });

        // Физический старт асинхронного сервера
        _server.begin();
    #if DEBUG_MODE
        Serial.println("[WebServer] Асинхронный сервер успешно запущен и слушает порт 80.");
    #endif
    }
 // Закрывающая скобка метода void WiFiConnect::setupWebServer()

 // ============================================================
// ГЕТТЕРЫ СЕТЕВОГО СТАТУСА И IP АДРЕСОВ (КЛАСС WiFiConnect)
// ============================================================
bool WiFiConnect::isConnected() const {
    return (WiFi.status() == WL_CONNECTED);
}

String WiFiConnect::getIPAddress() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

String WiFiConnect::getAPIPAddress() {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

String WiFiConnect::getCurrentSSID() {
    return targetSSID;
}

void WiFiConnect::disconnect() {
    WiFi.disconnect(true);
    _mode = Mode::DISCONNECTED;
#if DEBUG_MODE
    Serial.println("[WiFi] Сетевые интерфейсы закрыты, сокеты STA очищены");
#endif
}
