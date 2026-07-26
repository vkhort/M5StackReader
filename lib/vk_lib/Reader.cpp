// ============================================================
// Reader.cpp - Реализация аудио-движка и файлового менеджера
// ============================================================
#include "Reader.h"
#include "display.h"
#include "classes.h"
#include "config.h"


// Подключаем внешние глобальные объекты для вывода данных
extern Display display;
extern WorkSPIFFS myFS;

// Глобальный указатель на наш класс для фоновых задач FreeRTOS
Reader* globalReader = nullptr;

// ============================================================
// БЕЗОПАСНЫЙ ФОНОВЫЙ ЗВУКОВОЙ КОНВЕЙЕР (ЯДРО 0)
// ============================================================
void taskAudio(void* parameter) {
    // Извлекаем указатель на наш экземпляр класса Reader, переданный при создании таски
    Reader* readerInstance = (Reader*)parameter;
    
    #if DEBUG_MODE
    Serial.println("[FreeRTOS] Звуковой конвейер TaskAudio запущен на Ядре 0.");
    #endif

    // Бесконечный цикл высокоприоритетной звуковой задачи FreeRTOS
    while (true) {
        // Вызываем неблокирующий конечный автомат декодера.
        // Мы вызываем метод объекта _audio напрямую через геттер, 
        // либо, если _audio лежит в приватной секции, мы просто добавим в класс Reader метод loop()
        // и вызовем его здесь: readerInstance->loopAudio();
        
        // Для примера, если мы сделаем публичный метод loopAudio() в Reader:
        readerInstance->loopAudio();
        
        // Никаких тяжелых delay() сюда не ставим, FreeRTOS сама отдаст микросекунды другим таскам,
        // но короткий пустой пропуск цикла (yield) защитит от срабатывания сторожевого таймера (Watchdog)
        yield();
    }
}

void Reader::loopAudio() {
    _audio.loop(); 
}

// Фоновый FreeRTOS метод-работяга для Ядра 1 (Вызывается из taskControl)
void taskControl(void* parameter) {
    if (!globalReader) return;

    static time_t lastSec = 0;

    if (globalReader->_commandQueue != nullptr) {
        AudioMessage msg;
        if (xQueueReceive(globalReader->_commandQueue, &msg, 0) == pdTRUE) {
            globalReader->processCommand(msg);
        }
    }

    globalReader->syncNTP();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        time_t currentSec = time(nullptr);
        if (currentSec != lastSec) {
            lastSec = currentSec;

            extern Display display;
            
            // ИСПРАВЛЕНО: Массив на 3 символа под секунды типа "00" + ноль-терминатор
            char secBuf[3];
            sprintf(secBuf, "%02d", timeinfo.tm_sec);

            display.updateTime(timeinfo.tm_hour, timeinfo.tm_min, String(secBuf));

            if (timeinfo.tm_sec == 0) {
                // ИСПРАВЛЕНО: Корректные размеры массивов под дату и день недели
                char dateBuf[12];
                char dayBuf[16];
                
                strftime(dateBuf, sizeof(dateBuf), "%d.%m.%y", &timeinfo);
                strftime(dayBuf, sizeof(dayBuf), "%A", &timeinfo);
                
                String dayStr = String(dayBuf);
                dayStr.toUpperCase();
                display.updateDate(dateBuf, dayStr);
            }
        }
    }
}

// ============================================================
// КОНСТРУКТОР И ДЕСТРУКТОР КЛАССА
// ============================================================
Reader::Reader() 
    : _lastNtpAttempt(0), _ntpSuccess(false), _lastNtpSync(0)
    , _currentFileIndex(0), _totalFiles(0), _fileList(nullptr)
    , _isPlaying(false), _currentMetadata(""), _config(nullptr)
    , _commandQueue(nullptr), _bufferMem(nullptr) 
{
    globalReader = this; // Регистрируем указатель для FreeRTOS
}

Reader::~Reader() {
    stopPlaying();
    clearFileList();
    if (_bufferMem) {
        free(_bufferMem);
        _bufferMem = nullptr;
    }
    if (_commandQueue) {
        vQueueDelete(_commandQueue);
        _commandQueue = nullptr;
    }
}

// ============================================================
// ИНИЦИАЛИЗАЦИЯ И СТАРТ СИСТЕМЫ
// ============================================================
bool Reader::begin(WorkSPIFFS::ConfigData& cfg) {
    _config = &cfg;

    #if DEBUG_MODE
    Serial.println("[Reader] Инициализация SD-карты...");
    #endif

    // Выделяем память под статический буфер воспроизведения ESP8266Audio
    if (!_bufferMem) {
        _bufferMem = (uint8_t*)malloc(preallocateBufferSize);
    }

    // Создаем защищенную очередь команд FreeRTOS (размер: 10 сообщений)
    if (!_commandQueue) {
        _commandQueue = xQueueCreate(10, sizeof(AudioMessage));
    }

    // Монтируем физическую SD-карту. В Core2 пин CS флешки — это всегда GPIO 4
    if (!SD.begin(GPIO_NUM_4)) {
        #if DEBUG_MODE
        Serial.println("[Reader] КРИТИЧЕСКАЯ ОШИБКА: SD-карта не найдена!");
        #endif
        return false;
    }

    #if DEBUG_MODE
    Serial.println("[Reader] SD-карта успешно смонтирована.");
    #endif
    return true;
}

// ============================================================
// СКАНИРОВАНИЕ КОРНЯ И ФОРМИРОВАНИЕ СПИСКА ПО АЛФАВИТУ
// ============================================================
void Reader::scanFiles() {
    clearFileList(); // Очищаем кучу перед новым сканированием

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        #if DEBUG_MODE
        Serial.println("[Reader] Ошибка: Не удалось открыть корень SD!");
        #endif
        return;
    }

    // ШАГ 1: Считаем общее количество валидных аудиофайлов
    int tempCount = 0;
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            name.toLowerCase();
            // Отбираем строго MP3 и WAV, как договаривались
            if (name.endsWith(".mp3") || name.endsWith(".wav")) {
                tempCount++;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.rewindDirectory(); // Возвращаем указатель в начало папки

    _totalFiles = tempCount;
    if (_totalFiles == 0) {
        #if DEBUG_MODE
        Serial.println("[Reader] Предупреждение: На SD-карте нет поддерживаемых аудиофайлов!");
        #endif
        _fileList = nullptr; // Массив не выделяем
        _currentFileIndex = -1;
        return;
    }

    // ШАГ 2: Выделяем в куче (Heap) массив строк ровно под наше число файлов
    _fileList = new String[_totalFiles];

    // ШАГ 3: Записываем имена файлов в созданный динамический массив
    int index = 0;
    file = root.openNextFile();
    while (file && index < _totalFiles) {
        if (!file.isDirectory()) {
            String name = file.name();
            String lowerName = name;
            lowerName.toLowerCase();
            
            if (lowerName.endsWith(".mp3") || lowerName.endsWith(".wav")) {
                _fileList[index] = name; // Сохраняем оригинальное имя (слэш в начале уберётся)
                if (_fileList[index].startsWith("/")) {
                    _fileList[index] = _fileList[index].substring(1);
                }
                index++;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    // ШАГ 4: Сортируем собранный динамический массив по алфавиту
    sortFileList();

    // ШАГ 5: Назначаем индекс 0 (первый трек по алфавиту) текущим
    _currentFileIndex = 0;
    _displayMetadata = getCurrentFileName();

    #if DEBUG_MODE
    Serial.printf("[Reader] Сканирование завершено. Найдено файлов: %d\n", _totalFiles);
    for (int i = 0; i < _totalFiles; i++) {
        Serial.printf("  Файл [%d]: %s\n", i, _fileList[i].c_str());
    }
    Serial.printf("[Reader] Текущий трек №0 назначен: %s\n\n", _displayMetadata.c_str());
    #endif
}

// ============================================================
// АЛГОРИТМ ПУЗЫРЬКОВОЙ СОРТИРОВКИ СТРОК ПО АЛФАВИТУ
// ============================================================
void Reader::sortFileList() {
    if (_totalFiles <= 1) return;
    
    for (int i = 0; i < _totalFiles - 1; i++) {
        for (int j = 0; j < _totalFiles - i - 1; j++) {
            // Классическое посимвольное сравнение строк C++
            if (_fileList[j].compareTo(_fileList[j + 1]) > 0) {
                // Меняем элементы местами через временный буфер
                String temp = _fileList[j];
                _fileList[j] = _fileList[j + 1];
                _fileList[j + 1] = temp;
            }
        }
    }
}

// ============================================================
// ГЕТТЕРЫ И СЕРВИСНЫЕ МЕТОДЫ ОЧИСТКИ ПАМЯТИ
// ============================================================
// Обновляем геттер имени файла для обработки пустой флешки
String Reader::getCurrentFileName() const {
    if (_fileList != nullptr && _currentFileIndex >= 0 && _currentFileIndex < _totalFiles) {
        return _fileList[_currentFileIndex];
    }
    return "No Files"; // ИСПРАВЛЕНО: Если файлов нет, вернется строго эта надпись
}

void Reader::clearFileList() {
    if (_fileList != nullptr) {
        delete[] _fileList; // Освобождаем динамический массив строк из кучи
        _fileList = nullptr;
        _totalFiles = 0;
        #if DEBUG_MODE
        Serial.println("[Reader] Динамическая память списка файлов очищена");
        #endif
    }
}

#include "Reader.h"

// =========================================================================
// АППАРАТНАЯ ИНИЦИАЛИЗАЦИЯ ДИНАМИКА (Вызывается один раз при старте системы)
// =========================================================================
void Reader::initAudioHardware() {
    // Настраиваем физические пины встроенного динамика M5Stack Core2 из config.h
    _audio.setPinout(I2S_BCLK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN);
    
    // Выставляем громкость встроенного MP3-декодера (шкала от 0 до 21)
    _audio.setVolume(21); 
    
    // Включаем частотную коррекцию под мелкий динамик платы:
    // Срезаем хриплый бас (-15) и поднимаем средние и высокие частоты на максимум (+6)
    _audio.setTone(8, 12, 12);
    
    _isAudioInitialized = true;
    
    #if DEBUG_MODE
    Serial.println("[AUDIO] Железо встроенного динамика инициализировано (12, 0, 2).");
    #endif
}

// =========================================================================
// ОБНОВЛЕННЫЙ МЕТОД ЗАПУСКА ПРОИГРЫВАНИЯ MP3
// =========================================================================
bool Reader::startPlaying(const String& filename) {
    // На всякий случай проверяем, инициализировано ли железо
    if (!_isAudioInitialized) {
        initAudioHardware();
    }

    // Если прямо сейчас уже что-то играло — принудительно останавливаем
    if (_audio.isRunning()) {
        stopPlaying();
    }

    #if DEBUG_MODE
    Serial.print("[READER] Попытка запустить файл: ");
    Serial.println(filename);
    #endif

    // Передаем объект файловой системы (SD) и полный путь к файлу на флешке.
    // Библиотека сама откроет файл, прочитает ID3-теги и запустит поток декодирования.
    if (!_audio.connecttoFS(SD, filename.c_str())) {
        #if DEBUG_MODE
        Serial.printf("[ОШИБКА] Движок не смог открыть файл: %s\n", filename.c_str());
        #endif
        return false;
    }

    #if DEBUG_MODE
    Serial.println("[УСПЕХ] MP3-поток успешно запущен в шину I2S встроенного динамика!");
    #endif
    return true;
}

// =========================================================================
// МЕТОД ПОЛНОЙ ОСТАНОВКИ ЗВУКА
// =========================================================================
void Reader::stopPlaying() {
    if (_audio.isRunning()) {
        _audio.stopSong();
        #if DEBUG_MODE
        Serial.println("[READER] Воспроизведение принудительно остановлено.");
        #endif
    }
}

// ============================================================
// УМНАЯ СИНХРОНИЗАЦИЯ NTP И ОБНОВЛЕНИЕ ЧАСОВ (КЛАСС READER)
// ============================================================
void Reader::syncNTP() {
    // Подключаем внешний класс сети, чтобы знать режим работы (STA или AP)
    extern WiFiConnect wifi; 
    time_t now = time(nullptr);

    if (wifi.isSTA()) {
        // Логика интервалов, которую вы заложили в config.h
        uint32_t interval = _ntpSuccess ? NTP_SUCCESS_INTERVAL : NTP_RETRY_INTERVAL;

        if (now - _lastNtpSync >= interval || _lastNtpSync == 0) {
            _lastNtpSync = now;
            #if DEBUG_MODE
            Serial.println("[Reader NTP] Запрос точного времени с серверов...");
            #endif
            
            // Настраиваем системный RTC на работу с серверами из config.h
            configTime(NTP_TIMEZONE_OFFSET * 3600, 0, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
            
            // Проверяем, ответил ли сервер (год должен стать больше 1970)
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 1000)) { // Ждем ответа максимум 1 секунду
                _ntpSuccess = true;
                #if DEBUG_MODE
                Serial.println("[Reader NTP] Успешная синхронизация!");
                #endif
            } else {
                _ntpSuccess = false;
                #if DEBUG_MODE
                Serial.println("[Reader NTP] Сервер времени не ответил, повтор через 20 сек.");
                #endif
            }
        }
    } else {
        // Если мы в режиме AP — сервер времени физически недоступен
        _ntpSuccess = false;
    }
}

// Точка разбора FreeRTOS сообщений
void Reader::processCommand(const AudioMessage& msg) {
    // Сюда будут падать команды тачскрина
}

// ============================================================
// ЕДИНЫЙ ЦЕНТР ОБРАБОТКИ СОБЫТИЙ ТАЧСКРИНА (МЕТОД КЛАССА)
// ============================================================
void Reader::pushButton(int buttonCode) {
    extern Display display; 

    switch (buttonCode) {
        case 1: // Левая кнопка Button A [ << ]
            break;

        case 2: // ЦЕНТРАЛЬНАЯ КНОПКА BUTTON B [ Play / Stop ]
            _isPlaying = !_isPlaying; 
            
            #if DEBUG_MODE
            Serial.printf("[Reader] Кнопка нажата. Перерисовка интерфейса...\n");
            #endif

            // СНАЧАЛА полностью рисуем кнопку на экране, пока шина SPI абсолютно свободна!
            display.updateBottomButtons(
                "[ << ]", 
                _isPlaying ? "[ || ]" : "[  ▶  ]", 
                "[ >> ]", 
                _isPlaying
            );

            // И ТОЛЬКО ПОСЛЕ ЭТОГО запускаем или глушим аудио-движок на Ядре 0!
            if (_isPlaying) {
                startPlaying();
            } else {
                stopPlaying();
            }
            break;

        case 3: // Правая кнопка Button C [ >> ]
            break;
    }
}

