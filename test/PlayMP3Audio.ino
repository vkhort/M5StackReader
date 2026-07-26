#include "Arduino.h"
#include <M5Unified.h>  // Оставляем ТОЛЬКО для запуска питания чипа AXP192
#include "Audio.h"      // Ваша проверенная рабочая MP3 библиотека из папки lib
#include "FS.h"         
#include "SD.h"         
#include "SPI.h"        

// Железные пины SPI для SD-карты внутри M5Stack Core2
#define SD_SPI_CS_PIN   4
#define SD_SPI_SCK_PIN  18
#define SD_SPI_MISO_PIN 38
#define SD_SPI_MOSI_PIN 23

// Аппаратные пины I2S для встроенного динамика Core2
#define I2S_DOUT      2
#define I2S_BCLK      12
#define I2S_LRCK      0

Audio audio;
uint32_t lastLogTime = 0; 

void setup() {
    // 1. ВКЛЮЧАЕМ ЖЕЛЕЗО ПЛАТЫ: Используем минимальный запуск M5Unified
    auto cfg = M5.config();
    M5.begin(cfg);

    // Принудительно заставляем чип AXP192 подать питание 3.3В на аудиоусилитель и GPIO
    M5.Power.Axp192.setGPIO2(true); 
    M5.Power.setExtOutput(true);    

    delay(1000); // Даем время цепям питания стабилизироваться
// Регистр 0x27 отвечает за напряжение DCDC3 (аудио/подсветка). 
// Записываем туда код 0x30, что поднимает напряжение на усилителе до максимума в 3.5 Вольта.
// Это даст ощутимый прирост к аппаратной мощности динамика!
//M5.In_I2C.writeRegister8(0x34, 0x27, 0x30, 400000); 
//    delay(1000); // Даем время цепям питания стабилизироваться

    M5.Display.setTextColor(GREEN);
    M5.Display.setTextSize(2);
    M5.Display.println("Core2 Pure MP3 Player");

    // 2. Инициализируем SPI и SD-карту
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        M5.Display.setTextColor(RED);
        M5.Display.println("SD Mount Failed!");
        return;
    } else {
        M5.Display.println("SD Card: OK");
    }

    // 3. ОТДАЕМ ЗВУК БИБЛИОТЕКЕ AUDIO (ESP32-audioI2S)
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
    
    // Включаем софтверное усиление средних и высоких частот (эквалайзер).
    // Это программно увеличит громкость MP3-потока в 2 раза!
    audio.setTone(8, 12, 12); 

    // Выставляем громкость на максимум для этой библиотеки (21 из 21)
    audio.setVolume(21); 

    // 4. Запускаем ваш оригинальный MP3 файл Битлз
    if(!audio.connecttoFS(SD, "/Beatles-Girl.mp3")) {
        M5.Display.setTextColor(RED);
        M5.Display.println("Error: MP3 not found!");
    } else {
        M5.Display.println("Playing MP3 via Engine...");
    }
}

void loop() {
    // Вызываем декодер на максимальной скорости процессора без всяких delay().
    // Библиотека сама знает, когда нужно подлить байты из MP3, музыка пойдет идеально плавно!
    audio.loop(); 
    
    // Обновляем системные задачи экрана
    M5.update();
}

// Этот метод автоматически ловит и выводит логи MP3-движка в Монитор Порта
void audio_info(const char *info){
    Serial.print("AUDIO_ENGINE: "); Serial.println(info);
}
