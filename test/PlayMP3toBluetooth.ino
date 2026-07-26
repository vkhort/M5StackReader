#include "Arduino.h"
#define LGFX_AUTODETECT 
#include <LovyanGFX.hpp>
#include "XPowersLib.h" 
#include "BluetoothA2DPSource.h" 
#include "Audio.h"      
#include "FS.h"         
#include "SD.h"         
#include "SPI.h"        

#define SD_SPI_CS_PIN   4
#define SD_SPI_SCK_PIN  18
#define SD_SPI_MISO_PIN 38
#define SD_SPI_MOSI_PIN 23

#define VIRTUAL_I2S_BCLK 25
#define VIRTUAL_I2S_LRCK 26
#define VIRTUAL_I2S_DOUT 27

static LGFX lcd;           
XPowersAXP192 power;       
BluetoothA2DPSource a2dp_source;
Audio audio; 

bool isBluetoothConnected = false;
bool isMp3Playing = false;
bool waitingForUserTrigger = false;
uint32_t debugTimer = 0;

// ИСПРАВЛЕНО: Меняем статический массив на указатель. Буфер выделим в PSRAM!
const size_t RING_BUFFER_SIZE = 32768; // 32 Килобайта — идеальный баланс
uint8_t* ringBuffer = nullptr;
volatile size_t head = 0; 
volatile size_t tail = 0; 

size_t get_buffer_available() {
    if (head >= tail) return head - tail;
    return RING_BUFFER_SIZE - tail + head;
}

size_t get_buffer_free_space() {
    return RING_BUFFER_SIZE - get_buffer_available() - 1;
}

int32_t get_sound_data(uint8_t *data, int32_t byteCount) {
    if (ringBuffer == nullptr) return 0;
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
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== СТАРТ СИСТЕМЫ ESP32 (PSRAM BUFFER) ===");

    // ИСПРАВЛЕНО: Выделяем 32 КБ буфера в быстрой внешней PSRAM памяти
    if (psramInit()) {
        ringBuffer = (uint8_t*)ps_malloc(RING_BUFFER_SIZE);
        if (ringBuffer != nullptr) {
            Serial.println("[УСПЕХ] 32KB Кольцевой буфер успешно создан в PSRAM!");
        } else {
            Serial.println("[ОШИБКА] Не удалось выделить память в PSRAM!");
        }
    } else {
        Serial.println("[ОШИБКА] Чип PSRAM не найден или не инициализирован!");
    }

    // Инициализация питания
    if (!power.begin(Wire, AXP192_SLAVE_ADDRESS, 21, 22)) {
        Serial.println("[ОШИБКА] Чип питания AXP192 не ответил!");
    } else {
        power.setDC3Voltage(3300);   
        power.setLDO2Voltage(3300);  
        power.setLDO3Voltage(2000);  
        power.enableLDOio();         
        Serial.println("[УСПЕХ] Чип питания настроен, ток подан.");
    }

    // Инициализация LovyanGFX
    lcd.init();
    lcd.setRotation(1);
    lcd.fillScreen(TFT_BLACK);
    lcd.setTextColor(TFT_GREEN);
    lcd.setTextSize(2);
    lcd.println("LovyanGFX + PSRAM Buffer");

    // Инициализация SD-карты
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        lcd.setTextColor(TFT_RED);
        lcd.println("SD Mount Failed!");
        Serial.println("[ОШИБКА] SD-карта не смонтирована!");
        return;
    }
    Serial.println("[УСПЕХ] SD-карта готова.");

    // Инициализируем аудио-движок на изолированных виртуальных пинах
    // Теперь внутренней SRAM для DMA-буферов хватит с избытком!
    audio.setPinout(VIRTUAL_I2S_BCLK, VIRTUAL_I2S_LRCK, VIRTUAL_I2S_DOUT);

    a2dp_source.set_data_callback(get_sound_data);
    Serial.println("[BT] Ищем колонку JBL OnBeat Venue...");
    
    a2dp_source.start("JBL OnBeat Venue"); 
    a2dp_source.set_volume(127); 
}

void loop() {
    static bool lastState = false;
    if (a2dp_source.is_connected() != lastState) {
        lastState = a2dp_source.is_connected();
        isBluetoothConnected = lastState;
        lcd.clearDisplay();
        lcd.setCursor(0, 0);
        if (isBluetoothConnected) {
            lcd.setTextColor(TFT_GREEN);
            lcd.println("CONNECTED TO JBL!");
            lcd.setTextColor(TFT_WHITE);
            lcd.println("\nSend '1' in Serial\nto start music!");
            Serial.println("\n[BT STATUS] Колонка JBL успешно подключена!");
            waitingForUserTrigger = true;
        } else {
            lcd.setTextColor(TFT_YELLOW);
            lcd.println("JBL Disconnected...");
            isMp3Playing = false;
            waitingForUserTrigger = false;
        }
    }

    if (waitingForUserTrigger && Serial.available() > 0) {
        char incomingChar = Serial.read();
        if (incomingChar == '1') {
            waitingForUserTrigger = false;
            lcd.clearDisplay();
            lcd.setCursor(0, 0);
            lcd.setTextColor(TFT_GREEN);
            lcd.println("CONNECTED TO JBL!");
            lcd.println("Streaming MP3...");
            Serial.println("[START] Сигнал получен! Открываем MP3 файл...");
            
            audio.setVolume(21);
            if(!audio.connecttoFS(SD, "/Beatles-Girl.mp3")) {
                lcd.setTextColor(TFT_RED);
                lcd.println("Error: MP3 not found!");
            } else {
                isMp3Playing = true;
            }
        }
    }

    if (isBluetoothConnected && isMp3Playing) {
        audio.loop(); 
    }

    if (millis() - debugTimer > 1000) {
        debugTimer = millis();
        Serial.printf("[DEBUG] Буфер ОЗУ: %d байт | Свободно: %d\n", 
                      get_buffer_available(), get_buffer_free_space());
    }
}

// Наш официальный перехватчик с мягким тормозом
void audio_process_extern(int16_t* buff, uint16_t len, bool *continueI2S) {
    if (ringBuffer == nullptr) return;
    size_t bytesToWrite = len * 4; 

    // Если 32 КБ буфер в PSRAM заполняется, плавно притормаживаем декодер по 1 мс
    while (get_buffer_free_space() < bytesToWrite) {
        delay(1); 
    }

    uint8_t* bytePtr = (uint8_t*)buff;
    for (size_t i = 0; i < bytesToWrite; i++) {
        ringBuffer[head] = bytePtr[i];
        head = (head + 1) % RING_BUFFER_SIZE;
    }
    *continueI2S = false; 
}

void audio_info(const char *info){
    Serial.print("[MP3 ENGINE]: "); Serial.println(info);
}
