#include "display.h"
#include "config.h"   
#include "classes.h"  
#include "Reader.h"   

Display::Display() 
    : _initialized(false), _lastVolume(-1), _lastHours(-1), _lastMinutes(-1)
    , _scrollX(ZONE_SCROLL_W), _topSprite(nullptr), _volCtrlSprite(nullptr), _volBarSprite(nullptr)
    , _timeSprite(nullptr), _dateSprite(nullptr), _secSprite(nullptr), _scrollSprite(nullptr), _bottomSprite(nullptr) {}

Display::~Display() {
    if (_topSprite)     delete _topSprite;
    if (_volCtrlSprite) delete _volCtrlSprite;
    if (_volBarSprite)  delete _volBarSprite;
    if (_timeSprite)    delete _timeSprite;
    if (_dateSprite)    delete _dateSprite;
    if (_secSprite)     delete _secSprite; // ДОБАВЛЕНО
    if (_scrollSprite)  delete _scrollSprite;
    if (_bottomSprite)  delete _bottomSprite;
}

bool Display::begin() {
    if (!M5.Display.getBoard()) return false;

    M5.Display.setRotation(1);
    M5.Display.setColorDepth(16);
    M5.Display.fillScreen(COLOR_BACKGROUND); 

    _topSprite = new LGFX_Sprite(&M5.Display);     _topSprite->createSprite(ZONE_TOP_W, ZONE_TOP_H);
    _volCtrlSprite = new LGFX_Sprite(&M5.Display); _volCtrlSprite->createSprite(ZONE_VOL_CTRL_W, ZONE_VOL_CTRL_H);
    _volBarSprite = new LGFX_Sprite(&M5.Display);  _volBarSprite->createSprite(ZONE_VOL_BAR_W, ZONE_VOL_BAR_H);
    _timeSprite = new LGFX_Sprite(&M5.Display);    _timeSprite->createSprite(ZONE_TIME_W, ZONE_TIME_H);
    _dateSprite = new LGFX_Sprite(&M5.Display);    _dateSprite->createSprite(ZONE_DATE_W, ZONE_DATE_H);
    _secSprite = new LGFX_Sprite(&M5.Display);     _secSprite->createSprite(ZONE_SEC_W, ZONE_SEC_H); // ДОБАВЛЕНО
    _scrollSprite = new LGFX_Sprite(&M5.Display);  _scrollSprite->createSprite(ZONE_SCROLL_W, ZONE_SCROLL_H);
    _bottomSprite = new LGFX_Sprite(&M5.Display);  _bottomSprite->createSprite(ZONE_BOTTOM_W, ZONE_BOTTOM_H);

    _initialized = true;
    return true;
}

void Display::clear() {
    if (_initialized) M5.Display.fillScreen(COLOR_BACKGROUND);
}

void Display::setBrightness(uint8_t percent) {
    if (_initialized) M5.Display.setBrightness(percent);
}

void Display::showStartup(const String& statusText) {
    if (!_initialized) return;
    M5.Display.fillScreen(COLOR_BACKGROUND); 

    M5.Display.setTextColor(COLOR_TEXT_MAIN);
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.setFont(&fonts::Font0); M5.Display.setTextSize(2); 
    
    M5.Display.drawString(PROJECT_NAME, 15, 15);
    
    M5.Display.setTextColor(COLOR_TEXT_MUTED);
    M5.Display.drawString(statusText.c_str(), 15, 45);
}

void Display::updateTopBar(const String& ssid, const String& ip, bool isAPMode, const String& apPassword) {
    if (!_initialized) return;

    _topSprite->fillSprite(COLOR_BACKGROUND); 
    _topSprite->setFont(&fonts::Font0); _topSprite->setTextSize(1);
    _topSprite->setTextColor(COLOR_TEXT_MAIN);
    
    _topSprite->setTextDatum(textdatum_t::top_left);
    _topSprite->drawString((" SSID: " + ssid).c_str(), 5, 5);
    
    _topSprite->setTextDatum(textdatum_t::top_right);
    if (isAPMode) {
        _topSprite->drawString(("IP: " + ip + " (pass: " + apPassword + ")").c_str(), ZONE_TOP_W - 5, 5);
    } else {
        _topSprite->drawString(("IP: " + ip).c_str(), ZONE_TOP_W - 5, 5);
    }
    _topSprite->pushSprite(ZONE_TOP_X, ZONE_TOP_Y);
}

void Display::updateVolumeControls() {
    if (!_initialized) return;
    _volCtrlSprite->fillSprite(COLOR_BACKGROUND); 
    _volCtrlSprite->drawFastHLine(0, 0, ZONE_VOL_CTRL_W, COLOR_NEON_FRAME);
    _volCtrlSprite->setFont(&fonts::Font0); _volCtrlSprite->setTextSize(2);
    
    _volCtrlSprite->setTextColor(COLOR_BTN_NAV_BACK); 
    _volCtrlSprite->setTextDatum(textdatum_t::top_left);
    _volCtrlSprite->drawString(" [ < ]  ↓VOL", 15, 8);
    
    _volCtrlSprite->setTextColor(COLOR_BTN_NAV_NEXT); 
    _volCtrlSprite->setTextDatum(textdatum_t::top_right);
    _volCtrlSprite->drawString("VOL↑  [ > ] ", ZONE_VOL_CTRL_W - 15, 8);
    
    _volCtrlSprite->drawFastHLine(0, ZONE_VOL_CTRL_H - 1, ZONE_VOL_CTRL_W, COLOR_NEON_FRAME);
    _volCtrlSprite->pushSprite(ZONE_VOL_CTRL_X, ZONE_VOL_CTRL_Y);
}

void Display::updateVolumeBar(int volume) {
    if (!_initialized || _lastVolume == volume) return;
    _lastVolume = constrain(volume, 0, 19);

    _volBarSprite->fillSprite(COLOR_BACKGROUND); 
    int barHeight = _lastVolume * 5;
    int yPos = ZONE_VOL_BAR_H - barHeight;

    uint16_t color = M5.Display.color565(255, 193, 7);
    if (_lastVolume >= 5 && _lastVolume <= 15)      color = COLOR_VOL_MID;  
    else if (_lastVolume >= 16)                     color = COLOR_VOL_HIGH; 

    if (barHeight > 0) _volBarSprite->fillRect(5, yPos, ZONE_VOL_BAR_W - 10, barHeight, color);
    _volBarSprite->drawRect(5, 0, ZONE_VOL_BAR_W - 10, ZONE_VOL_BAR_H, COLOR_NEON_FRAME);
    _volBarSprite->pushSprite(ZONE_VOL_BAR_X, ZONE_VOL_BAR_Y);
}

// 4. Раздельное обновление часов и минут (СТРОГО ЧЕСТНЫЕ ВЕКТОРНЫЕ ШРИФТЫ)
// 4. Обновление Часов, Минут и Секунд (СТРОГО ОТ УГЛА 0,0)
void Display::updateTime(int hours, int minutes, const String& secondsText) {
    if (!_initialized) return;

    // Отрисовка Часов и Минут
    if (_lastHours != hours || _lastMinutes != minutes) {
        _lastHours = hours; _lastMinutes = minutes;
        _timeSprite->fillSprite(COLOR_BACKGROUND); // Очищаем чёрный прямоугольник часов
        _timeSprite->setTextColor(COLOR_TEXT_MAIN);
        
        // Включаем режим: Рисовать строго от верхнего левого угла холста!
        _timeSprite->setTextDatum(textdatum_t::top_left);
        _timeSprite->setFont(&FONT_CLOCK);
        
        char timeBuf[6];
        sprintf(timeBuf, "%02d:%02d", hours, minutes);
        
        // Никакой математики по вертикали и горизонтали! Печатаем прямо в угол (0, 0)
        _timeSprite->drawString(timeBuf, 0, 0); 
        _timeSprite->pushSprite(ZONE_TIME_X, ZONE_TIME_Y); // Выталкиваем прямоугольник на экран
    }

    // Отрисовка Секунд
    if (_lastSeconds != secondsText) {
        _lastSeconds = secondsText;
        _secSprite->fillSprite(COLOR_BACKGROUND); // Очищаем чёрный прямоугольник секунд
        _secSprite->setTextColor(COLOR_VOL_LOW);   // Наш фирменный жёлтый цвет
        
        _secSprite->setTextDatum(textdatum_t::top_left);
        _secSprite->setFont(&FONT_SECONDS);
        
        // Печатаем секунды прямо в угол (0, 0) их личного прямоугольника!
        _secSprite->drawString(secondsText.c_str(), 0, 0);
        _secSprite->pushSprite(ZONE_SEC_X, ZONE_SEC_Y); // Выталкиваем на экран
    }
}

// 5. Обновление Даты и Дня недели (СТРОГО ОДИН ПОД ДРУГИМ ОТ УГЛА 0,0)
void Display::updateDate(const String& dateStr, const String& dayStr) {
    if (!_initialized || (_lastDate == dateStr && _lastDay == dayStr)) return;
    _lastDate = dateStr; _lastDay = dayStr;

    _dateSprite->fillSprite(COLOR_BACKGROUND); // Очищаем чёрный прямоугольник даты
    _dateSprite->setFont(&fonts::Font0); _dateSprite->setTextSize(2);
    
    // Включаем режим верхнего левого угла для даты
    _dateSprite->setTextDatum(textdatum_t::top_left);
    
    // 1-я строка: Дата (Жёлтый). Пишем прямо в левый верхний угол (0, 0)
    _dateSprite->setTextColor(COLOR_VOL_LOW);
    _dateSprite->drawString(dateStr.c_str(), 0, 0);
    
    // 2-я строка: День недели. Опускаем ровно на высоту шрифта (например, на 20 пикселей)
    _dateSprite->setTextColor(COLOR_VOL_MID);
    _dateSprite->drawString(dayStr.c_str(), 0, 20);
    
    _dateSprite->pushSprite(ZONE_DATE_X, ZONE_DATE_Y);
}

void Display::updateScrollText(const String& fileName) {
    if (!_initialized) return;

    extern Reader reader;
    bool isPlaying = reader.isPlaying(); 

    if (_lastScrollText != fileName) {
        _lastScrollText = fileName;
        _scrollX = 5; 
    }

    _scrollSprite->fillSprite(COLOR_BACKGROUND); 
    _scrollSprite->setFont(&fonts::Font0); _scrollSprite->setTextSize(2);
    _scrollSprite->setTextColor(COLOR_TEXT_MAIN);
    _scrollSprite->setTextDatum(textdatum_t::middle_left);

    _scrollSprite->drawString((">>> " + _lastScrollText).c_str(), _scrollX, ZONE_SCROLL_H / 2);
    
    if (isPlaying) {
        _scrollX -= 2;
        int textLength = _lastScrollText.length() * 12 + 40;
        if (_scrollX < -textLength) _scrollX = ZONE_SCROLL_W;
    } else {
        _scrollX = 5; 
    }

    _scrollSprite->pushSprite(ZONE_SCROLL_X, ZONE_SCROLL_Y);
}

void Display::updateBottomButtons(const String& btnA, const String& btnB, const String& btnC, bool isPlaying) {
    if (!_initialized) return;
    _bottomSprite->fillSprite(COLOR_BACKGROUND); 
    _bottomSprite->drawFastHLine(0, 0, ZONE_BOTTOM_W, COLOR_NEON_FRAME);
    _bottomSprite->setFont(&fonts::Font0); _bottomSprite->setTextSize(2);
    _bottomSprite->setTextDatum(textdatum_t::middle_center);
    
    _bottomSprite->setTextColor(COLOR_BTN_NAV_BACK); 
    _bottomSprite->drawString(btnA.c_str(), 53, ZONE_BOTTOM_H / 2);
    
    _bottomSprite->setTextColor(isPlaying ? COLOR_BTN_PLAY : COLOR_BTN_STOP);
    _bottomSprite->drawString(btnB.c_str(), 160, ZONE_BOTTOM_H / 2);
    
    _bottomSprite->setTextColor(COLOR_BTN_NAV_NEXT); 
    _bottomSprite->drawString(btnC.c_str(), 266, ZONE_BOTTOM_H / 2);
    _bottomSprite->pushSprite(ZONE_BOTTOM_X, ZONE_BOTTOM_Y);
}

void Display::showMainInterface(const String& localIP, const String& connectedSSID, bool isAPMode) {
    if (!_initialized) return;
    clear();
    M5.Display.drawRect(0, 0, 320, 240, COLOR_NEON_FRAME);

    extern WorkSPIFFS::ConfigData config;

    updateTopBar(connectedSSID, localIP, isAPMode, config.password); 
    updateVolumeControls();
    updateVolumeBar(10);
    updateTime(0, 0, "oo");             
    updateDate("24.07.26", "FRIDAY");   
    updateBottomButtons("[ << ]", "[ || ]", "[ >> ]", true);
}
