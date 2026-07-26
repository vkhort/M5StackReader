// ============================================================
// display.h - Позонный модульный класс экрана M5Stack Core2
// ============================================================
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <M5Unified.h>


// ============================================================
// ЦВЕТОВАЯ ПАЛИТРА ИНТЕРФЕЙСА (АППАРАТНЫЙ ФОРМАТ RGB565)
// ============================================================
#define COLOR_BACKGROUND          TFT_BLACK           // Общий фон экрана
#define COLOR_NEON_FRAME          0x4DB4              // Фирменная контурная рамка (Зелёный)
#define COLOR_TEXT_MAIN           TFT_WHITE           // Основной текст (Часы, Имена файлов)
#define COLOR_TEXT_MUTED          TFT_LIGHTGRAY       // Второстепенный текст (Инструкции)

#define COLOR_BTN_NAV_BACK        0x3DF7              // Кнопки Назад / Тише (Синий)
#define COLOR_BTN_NAV_NEXT        TFT_RED             // Кнопки Вперёд / Громче (Красный)
#define COLOR_BTN_PLAY            0xFDE0              // Кнопка PLAY / Секунды (Жёлтый)
#define COLOR_BTN_STOP            TFT_ORANGE          // Кнопка STOP (Оранжевый)

#define COLOR_VOL_LOW             0xFDE0              // Громкость 0-4 (Жёлтый)
#define COLOR_VOL_MID             0x4DB4              // Громкость 5-15 (Зелёный)
#define COLOR_VOL_HIGH            TFT_RED             // Громкость 16-19 (Красный)

// ============================================================
// НАСТРОЙКА ШРИФТОВ ВРЕМЕНИ (ЧЕСТНЫЕ ВЕКТОРНЫЕ РАЗМЕРЫ)
// ============================================================
#define FONT_CLOCK                fonts::FreeSansBold24pt7b  // ВОЗВРАЩЕНО: Честный максимальный размер (высота 32px)
#define FONT_SECONDS              fonts::FreeSansBold18pt7b  // Крупные гладкие секунды (высота 24px)

#define ZONE_TIME_X        30         
#define ZONE_TIME_Y        55
#define ZONE_TIME_W        120        // ВОЗВРАЩЕНО к исходному удовлетворительному значению
#define ZONE_TIME_H        40

#define ZONE_DATE_X        211        // ВОЗВРАЩЕНО к исходному удовлетворительному значению
#define ZONE_DATE_Y        57
#define ZONE_DATE_W        106        
#define ZONE_DATE_H        60         

#define ZONE_SEC_X         158        // ВОЗВРАЩЕНО к исходному удовлетворительному значению
#define ZONE_SEC_Y         65       
#define ZONE_SEC_W         45        
#define ZONE_SEC_H         32         
  

// ============================================================
// ГЕОМЕТРИЧЕСКАЯ РАЗМЕТКА ЭКРАНА СЕТКИ 320x240 (МАКРОСЫ)
// ============================================================
// 1. Верхняя информационная строка (Status Bar)
#define ZONE_TOP_X         0
#define ZONE_TOP_Y         0
#define ZONE_TOP_W         320
#define ZONE_TOP_H         20

// 2. Вторая строка: Управление громкостью (Volume Controls)
#define ZONE_VOL_CTRL_X    0
#define ZONE_VOL_CTRL_Y    20
#define ZONE_VOL_CTRL_W    320
#define ZONE_VOL_CTRL_H    30

// 3. Третья зона: Вертикальный индикатор громкости (Volume Bar)
#define ZONE_VOL_BAR_X     0
#define ZONE_VOL_BAR_Y     50
#define ZONE_VOL_BAR_W     30
#define ZONE_VOL_BAR_H     100      

// 7. Четвёртая зона: Полноэкранная бегущая строка (Scroll Content)
#define ZONE_SCROLL_X      0
#define ZONE_SCROLL_Y      150
#define ZONE_SCROLL_W      320
#define ZONE_SCROLL_H      40

// 8. Пятая зона: Нижняя строка подсказок тач-кнопок (Bottom Buttons)
#define ZONE_BOTTOM_X      0
#define ZONE_BOTTOM_Y      190
#define ZONE_BOTTOM_W      320
#define ZONE_BOTTOM_H      50

// ============================================================
// КЛАСС УПРАВЛЕНИЯ ДИСПЛЕЕМ M5STACK CORE2
// ============================================================
class Display {
public:
    Display();
    ~Display();

    bool begin();
    void clear();
    void setBrightness(uint8_t percent);

    void showStartup(const String& statusText);
    void showMainInterface(const String& localIP, const String& connectedSSID, bool isAPMode = false);

    void updateTopBar(const String& ssid, const String& ip, bool isAPMode = false, const String& apPassword = "");
    void updateVolumeControls(); 
    void updateVolumeBar(int volume);
    void updateTime(int hours, int minutes, const String& secondsText); // Метод обновления времени
    void updateDate(const String& dateStr, const String& dayStr);
    void updateScrollText(const String& fileName);
    void updateBottomButtons(const String& btnA, const String& btnB, const String& btnC, bool isPlaying = true);

    bool isInitialized() const { return _initialized; }

private:
    bool _initialized;
    String _lastSSID, _lastIP;
    int    _lastVolume;
    int    _lastHours, _lastMinutes;
    String _lastSeconds;
    String _lastDate, _lastDay;
    String _lastScrollText;
    int    _scrollX;

    LGFX_Sprite* _topSprite;       
    LGFX_Sprite* _volCtrlSprite;   
    LGFX_Sprite* _volBarSprite;    
    LGFX_Sprite* _timeSprite;      
    LGFX_Sprite* _dateSprite;      
    LGFX_Sprite* _secSprite;       // ДОБАВЛЕНО: Спрайт под секунды
    LGFX_Sprite* _scrollSprite;    
    LGFX_Sprite* _bottomSprite;    
};

#endif // DISPLAY_H
