#include "Arduino.h"
#include <M5Unified.h>  
#include "Audio.h"      
#include "FS.h"         
#include "SD.h"         
#include "SPI.h"        

#define SD_SPI_CS_PIN   4
#define SD_SPI_SCK_PIN  18
#define SD_SPI_MISO_PIN 38
#define SD_SPI_MOSI_PIN 23

#define I2S_DOUT      2
#define I2S_BCLK      12
#define I2S_LRCK      0

Audio audio;
uint32_t lastLogTime = 0; // Для ограничения частоты вывода в Serial

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    // --- АКТУАЛЬНЫЙ СПОСОБ ВКЛЮЧЕНИЯ ЗВУКА НА 100% В M5UNIFIED ---
    M5.Power.Axp192.setGPIO2(true); // Включаем сам чип усилителя NS4168
    
    // Этот метод гарантированно есть в вашей библиотеке. 
    // Мы принудительно переводим шину питания периферии в режим 3.3 Вольта!
    M5.Power.setExtOutput(true); 
    // -----------------------------------------------------------

    delay(1000); 

    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("Core2 Max Volume Init");

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        Serial.println("ОШИБКА: SD-карта 64GB не ответила!");
        M5.Display.setTextColor(RED);
        M5.Display.println("SD Mount Failed!");
    } else {
        Serial.println("УСПЕХ: SD-карта успешно смонтирована!");
        M5.Display.println("SD Card: OK");
    }

    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    audio.setVolume(20); 
    // 3. ПРОГРАММНЫЙ БУСТ (Эквалайзер): 
    // Поднимаем средние и высокие частоты на +6 децибел (максимум для этой библиотеки).
    // Маленькая пищалка Core2 играет только их, и звук станет в два раза громче!
    // Параметры: gainLow (бас), gainMid (средние), gainHigh (высокие)
    audio.setTone(-10, 6, 6); 

    if(!audio.connecttoFS(SD, "/Beatles - Girl.mp3")) {
        M5.Display.setTextColor(RED);
        M5.Display.println("Error: MP3 not found!");
    } else {
        M5.Display.println("Playing MP3...");
    }
}

void loop() {
    audio.loop(); 
    M5.update();

    // Каждую секунду выводим в Монитор Порта прогресс чтения файла
    if (millis() - lastLogTime > 1000) {
        lastLogTime = millis();
        if (audio.isRunning()) {
            Serial.printf("Воспроизведение: %d сек. / Всего: %d сек.\n", 
                          audio.getAudioCurrentTime(), 
                          audio.getAudioFileDuration());
        }
    }
}

// Сюда библиотека сама шлет технические данные о битрейте и читаемых фреймах
void audio_info(const char *info){
    Serial.print("AUDIO_ENGINE: "); Serial.println(info);
}
