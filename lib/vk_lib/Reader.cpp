// ============================================================
// Reader.cpp - Реализация аудио-движка и файлового менеджера
// ============================================================
#include "Reader.h"
#include "display.h"
#include "classes.h"
#include "config.h"

// ============================================================
// ИСПРАВЛЕННЫЙ ПРОГРАММНЫЙ КЛАСС-МОСТ ДЛЯ M5.SPEAKER
// ============================================================
class AudioOutputM5 : public AudioOutput {
public:
    AudioOutputM5() {
        auto cfg = M5.Speaker.config();
        cfg.sample_rate = 44100; 
        cfg.stereo = true;       
        M5.Speaker.config(cfg);
    }

    virtual bool begin() override { 
        return true; 
    }

    // ИСПРАВЛЕНО СИНТАКСИС: Теперь это строго указатель на массив (int16_t *sample)
    virtual bool ConsumeSample(int16_t *sample) override {
        // Передаем указатель на стерео-пару (левый и правый каналы),
        // длина массива = 2 сэмпла, частота = 44100 Гц, stereo = true
        return M5.Speaker.playRaw(sample, 2, 44100, true);
    }
    
    virtual bool stop() override { 
        M5.Speaker.stop(); 
        return true; 
    }
};

// Подключаем внешние глобальные объекты для вывода данных
extern Display display;
extern WorkSPIFFS myFS;

// Глобальный указатель на наш класс для фоновых задач FreeRTOS
Reader* globalReader = nullptr;

// ============================================================
// БЕЗОПАСНЫЙ ФОНОВЫЙ ЗВУКОВОЙ КОНВЕЙЕР (ЯДРО 0)
// ============================================================
void taskAudio(void* parameter) {
    while(true) {
        // КРИТИЧЕСКИЙ ФИКС: Пауза FreeRTOS ОБЯЗАНА стоять на самом входе в цикл!
        // Она сбрасывает сторожевой таймер (Watchdog) Ядра 0 и полностью исключает перезагрузки!
        vTaskDelay(1 / portTICK_PERIOD_MS); 

        if (globalReader && globalReader->isPlaying() && globalReader->_decoder != nullptr) {
            if (globalReader->_decoder->isRunning()) {
                // Крутим один микро-шаг декодирования MP3 фрейма
                if (!globalReader->_decoder->loop()) {
                    // Если трек доиграл до конца — мягко останавливаем
                    globalReader->stopPlaying();
                }
            }
        }
    }
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
    , _decoder(nullptr), _file(nullptr), _buff(nullptr), _out(nullptr)
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

// ============================================================
// БЕЗОПАСНЫЙ ЗАПУСК СВЯЗКИ ДЕКОДЕРА И M5.SPEAKER
// ============================================================
void Reader::startPlaying() {
    stopPlaying(); // Очищаем прошлые хвосты

    // Включаем силовое питание периферии Core2
    M5.Power.setExtOutput(true);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Выставляем безопасную начальную громкость встроенного динамика (от 0 до 255)
    M5.Speaker.setVolume(32); 

    String fullPath = "/" + getCurrentFileName();
    #if DEBUG_MODE
    Serial.printf("[Audio] Старт MP3 через M5.Speaker: %s\n", fullPath.c_str());
    #endif

    // Инициализируем программный конвейер
    _file = new AudioFileSourceSD(fullPath.c_str());
    _buff = new AudioFileSourceBuffer(_file, 4096); // 4Кб буфер в куче
    _out = new AudioOutputM5();                     // ПОДКЛЮЧАЕМ НАШ УТВЕРЖДЕННЫЙ МОСТ!
    
    _decoder = new AudioGeneratorMP3();
    _decoder->begin(_buff, _out);
}

void Reader::stopPlaying() {
    if (_decoder) {
        if (_decoder->isRunning()) _decoder->stop();
        delete _decoder; _decoder = nullptr;
    }
    if (_buff) { delete _buff; _buff = nullptr; }
    if (_file) { delete _file; _file = nullptr; }
    if (_out)  { delete _out;  _out = nullptr;  }
    
    M5.Speaker.stop(); // Гасим встроенный звук
    M5.Power.setExtOutput(false); // Обесточиваем усилитель в тишине

    #if DEBUG_MODE
    Serial.println("[Audio] Звуковой тракт M5.Speaker успешно очищен.");
    #endif
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

