#include "Arduino.h"
#include <M5Unified.h>  
#include "BluetoothA2DPSource.h" 
#include "FS.h"         
#include "SD.h"         
#include "SPI.h"        

#define SD_SPI_CS_PIN   4
#define SD_SPI_SCK_PIN  18
#define SD_SPI_MISO_PIN 38
#define SD_SPI_MOSI_PIN 23

BluetoothA2DPSource a2dp_source;
File wavFile;
bool fileReady = false;

// Оставляем наш рабочий кольцевой буфер на 16 Килобайт в ОЗУ
const size_t RING_BUFFER_SIZE = 16384;
uint8_t ringBuffer[RING_BUFFER_SIZE];

volatile size_t head = 0; // Указатель записи с SD-карты
volatile size_t tail = 0; // Указатель чтения Bluetooth в JBL

size_t get_buffer_available() {
    if (head >= tail) return head - tail;
    return RING_BUFFER_SIZE - tail + head;
}

size_t get_buffer_free_space() {
    return RING_BUFFER_SIZE - get_buffer_available() - 1;
}

// 1. ПОТОК BLUETOOTH (Коллбэк): Без изменений, мгновенно забирает байты из памяти
int32_t get_sound_data(uint8_t *data, int32_t byteCount) {
    if (!fileReady) return 0;

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
    M5.Display.println("JBL Player Fixed");

    // Инициализируем SD-карту
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        M5.Display.setTextColor(RED);
        M5.Display.println("SD Mount Failed!");
        return;
    }

    // Открываем подготовленный в Audacity Стерео WAV-файл без пробелов
    wavFile = SD.open("/Beatles-Girl.wav", FILE_READ);
    if (!wavFile) {
        M5.Display.setTextColor(RED);
        M5.Display.println("WAV File not found!");
    } else {
        wavFile.seek(44); // Пропускаем заголовок Microsoft WAV, встаем на аудио-байты
        fileReady = true;
        M5.Display.println("Filling buffer...");
    }

    // Регистрируем наш байтовый коллбэк
    a2dp_source.set_data_callback(get_sound_data);

    // Подключаемся к колонке JBL
    M5.Display.println("Connecting to JBL...");
    a2dp_source.start("JBL OnBeat Venue"); 
    
    // Ставим громкость Bluetooth-передачи на максимум (1.0). 
    // Регулировать громкость Битлз теперь можно кнопками самой колонки!
    a2dp_source.set_volume(1.0); 
}

void loop() {
    M5.update();

    // Заполнение буфера кусками из файла (ваш рабочий код)
    if (fileReady && wavFile.available()) {
        size_t freeSpace = get_buffer_free_space();
        if (freeSpace > 512) {
            uint8_t readBlock[512]; 
            size_t bytesRead = wavFile.read(readBlock, sizeof(readBlock));
            for (size_t i = 0; i < bytesRead; i++) {
                ringBuffer[head] = readBlock[i];
                head = (head + 1) % RING_BUFFER_SIZE;
            }
        }
    } else if (fileReady && !wavFile.available()) {
        M5.Display.println("\nTrack Finished!");
        wavFile.close();
        fileReady = false;
    }

    // БЛОК ОТСЛЕЖИВАНИЯ ПОДКЛЮЧЕНИЯ В loop():
    static bool lastState = false;
    if (a2dp_source.is_connected() != lastState) {
        lastState = a2dp_source.is_connected();
        M5.Display.clearDisplay();
        M5.Display.setCursor(0, 0);
        
        if (lastState) {
            M5.Display.setTextColor(GREEN);
            M5.Display.println("CONNECTED TO JBL!");
            M5.Display.println("Playing Beatles...");
            
            // Ждем 300 мс, чтобы завершился Bluetooth-хендшейк профилей звука
            delay(300);
            
            // --- ВОТ ОН, ТОТ САМЫЙ КЛЮЧ ДЛЯ СНЯТИЯ БЛОКИРОВКИ С JBL ---
            // Эта команда шлет пакет AVRCP, имитируя, что мы физически смахнули ползунок 
            // громкости на телефоне. Шкала от 0 до 127. Ставим 100 (около 80% громкости).
            // Это сорвет аппаратную заглушку Mute с процессора колонки!
            a2dp_source.set_volume(127); 
            
        } else {
            M5.Display.setTextColor(YELLOW);
            M5.Display.println("JBL Disconnected.\nReconnecting...");
        }
    }
    delay(1); 
}
