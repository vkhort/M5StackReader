#include "Arduino.h"
#include <M5Unified.h>  
#include "BluetoothA2DPSource.h" 

BluetoothA2DPSource a2dp_source;

// Создаем кольцевой буфер в оперативной памяти (16 Килобайт)
const size_t RING_BUFFER_SIZE = 16384;
uint8_t ringBuffer[RING_BUFFER_SIZE];

volatile size_t head = 0; // Указатель записи
volatile size_t tail = 0; // Указатель чтения Bluetooth

// Создаем массив для хранения одного заранее вычисленного периода ноты "Ля"
// Для частоты 440 Гц при дискретизации 44100 один период — это ровно 100 семплов
const size_t TONE_SAMPLES = 100;
int16_t precalculatedTone[TONE_SAMPLES];

size_t get_buffer_available() {
    if (head >= tail) return head - tail;
    return RING_BUFFER_SIZE - tail + head;
}

size_t get_buffer_free_space() {
    return RING_BUFFER_SIZE - get_buffer_available() - 1;
}

// 1. ПОТОК BLUETOOTH: Мгновенно забирает байты из ОЗУ без задержек
int32_t get_sound_data(uint8_t *data, int32_t byteCount) {
    size_t availableBytes = get_buffer_available();
    int32_t toCopy = (availableBytes < (size_t)byteCount) ? availableBytes : byteCount;

    if (toCopy == 0) return 0;

    for (int32_t i = 0; i < toCopy; i++) {
        data[i] = ringBuffer[tail];
        tail = (tail + 1) % RING_BUFFER_SIZE;
    }
    return toCopy;
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    delay(1000); 

    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("JBL Fast Tone Test");

    // --- ПРЕДВАРИТЕЛЬНЫЙ РАСЧЕТ ЗВУКА В setup() ---
    // Генерируем тихую, чистую и мягкую синусоиду один раз при старте
    for (size_t i = 0; i < TONE_SAMPLES; i++) {
        float angle = (2.0 * PI * i) / TONE_SAMPLES;
        // Умножаем на 2000 (вместо 32000), чтобы звук был тихим и не хрипел
        precalculatedTone[i] = (int16_t)(sin(angle) * 2000.0);
    }

    // Регистрируем коллбэк Bluetooth
    a2dp_source.set_data_callback(get_sound_data);

    // Подключаемся к вашей колонке
    M5.Display.println("Connecting to JBL...");
    a2dp_source.start("JBL OnBeat Venue"); 
    
    // Громкость Bluetooth-канала (0.5 из 1.0)
    a2dp_source.set_volume(0.5); 
}

void loop() {
    M5.update();

    // Переменная для отслеживания текущего индекса в массиве готового звука
    static size_t toneIndex = 0;

    // 2. ОСНОВНОЙ ПОТОК: Заполняет буфер готовыми байтами на максимальной скорости
    size_t freeSpace = get_buffer_free_space();

    // Если в кольцевом буфере есть свободное место под один 16-битный Стерео фрейм (4 байта)
    while (freeSpace >= 4) {
        int16_t sample = precalculatedTone[toneIndex];

        // Разделяем 16-битное число на два байта
        uint8_t lowByte  = sample & 0xFF;
        uint8_t highByte = (sample >> 8) & 0xFF;

        // Записываем Левый канал
        ringBuffer[head] = lowByte;
        head = (head + 1) % RING_BUFFER_SIZE;
        ringBuffer[head] = highByte;
        head = (head + 1) % RING_BUFFER_SIZE;

        // Записываем точно такой же Правый канал (Стерео-поток)
        ringBuffer[head] = lowByte;
        head = (head + 1) % RING_BUFFER_SIZE;
        ringBuffer[head] = highByte;
        head = (head + 1) % RING_BUFFER_SIZE;

        // Переходим к следующему кусочку нашей звуковой волны
        toneIndex = (toneIndex + 1) % TONE_SAMPLES;

        // Пересчитываем свободное место внутри цикла, чтобы забить буфер до отказа
        freeSpace = get_buffer_free_space();
    }

    // Микро-пауза в 1 миллисекунду, чтобы процессор успевал обрабатывать системные задачи связи
    delay(1);
}
