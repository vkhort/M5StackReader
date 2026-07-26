#include "Arduino.h"
#include <M5Unified.h>  
#include "FS.h"         
#include "SD.h"         
#include "SPI.h"        

#define SD_SPI_CS_PIN   4
#define SD_SPI_SCK_PIN  18
#define SD_SPI_MISO_PIN 38
#define SD_SPI_MOSI_PIN 23

File wavFile;
bool isPlaying = false;

// Буфер на 4 Килобайта (4096 байт)
uint8_t audioBuffer[4096]; 

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    delay(1000); 

    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("Core2 Standard WAV Player");

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        M5.Display.setTextColor(RED);
        M5.Display.println("SD Mount Failed!");
        return;
    }

    M5.Speaker.begin();
    M5.Speaker.setVolume(255); // Максимальная громкость железа

    // Открываем созданный в Audacity файл
    wavFile = SD.open("/Beatles-Girl.wav", FILE_READ);
    if (!wavFile) {
        M5.Display.setTextColor(RED);
        M5.Display.println("Error: File not found!");
    } else {
        // Пропускаем ровно 44 байта стандартного WAV-заголовка Microsoft
        wavFile.seek(44);
        M5.Display.println("Playing Standard 44.1kHz...");
        isPlaying = true;
    }
}

void loop() {
    M5.update();

    if (isPlaying && wavFile.available()) {
        size_t bytesRead = wavFile.read(audioBuffer, sizeof(audioBuffer));
        
        // Отправляем эталонные параметры: 
        // 44100 Гц, true (Stereo) — так как мы экспортировали файл из Audacity именно так
        M5.Speaker.playRaw((const int16_t*)audioBuffer, bytesRead / 2, 44100, true);
        
        // Наша проверенная пауза, которая убирает шумы
        delay(20); 
        
    } else if (isPlaying && !wavFile.available()) {
        M5.Display.println("\nTrack Finished!");
        wavFile.close();
        isPlaying = false;
    }
}
