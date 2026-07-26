// ============================================================
// reader.h - Монолитный управляющий класс VK_Reader
// ============================================================
#ifndef READER_H
#define READER_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <freertos/queue.h>

#include "Audio.h"      // Замена на ESP32-audioI2S

#include "config.h"
#include "classes.h"                 // Содержит WorkSPIFFS::ConfigData

// Структура сообщения для очереди команд FreeRTOS
struct AudioMessage {
    int command;
    int value;
    String text;
};

// ============================================================
// КЛАСС READER
// ============================================================
class Reader {
public:
    Reader(); 
    ~Reader();

    // ---- Публичный интерфейс управления (вызывается из main.cpp / M5StackReader.ino) ----
    bool begin(WorkSPIFFS::ConfigData& cfg); // Старт SD-карты, создание очередей
    void scanFiles();                        // Сканирование флешки и сортировка по алфавиту
    String getCurrentFileName() const;       // Получить имя текущего файла для бегущей строки
    
    bool isPlaying() const { return _isPlaying; } 
    void pressButton(int16_t x, int16_t y);
    void pushButton(int buttonCode);         // НАШ УТВЕРЖДЕННЫЙ МОЗГ УПРАВЛЕНИЯ КНОПКАМИ

    // ---- НАШИ НОВЫЕ МЕТОДЫ ЗВУКОВОГО ДВИЖКА (ESP32-audioI2S) ----
    void initAudioHardware();                // Настройка пинов динамика 12, 0, 2
    void loopAudio();                        // Мост для фонового вызова _audio.loop()
    
    bool startPlaying();                     // ИСПРАВЛЕНО: вариант БЕЗ параметров для pushButton()
    bool startPlaying(const String& filename); // Вариант С параметром для запуска конкретного файла
    void stopPlaying();                      // Метод остановки трека

private:
    // ---- Время и Синхронизация (Пока заглушка) ----
    time_t _lastNtpAttempt;
    bool _ntpSuccess;
    time_t _lastNtpSync;
    struct tm _timeInfo;

    Audio _audio;              // Объект нового аудио-движка ESP32-audioI2S
    bool _isAudioInitialized;  // Флаг, чтобы не настраивать пины при каждом треке

    // ---- Файловый менеджер плеера (Решение нашей первой задачи) ----
    int _currentFileIndex;                    // Текущий номер файла по алфавиту (0 - первый)
    int _totalFiles;                          // Всего допустимых файлов на флешке
    String* _fileList;                        // Динамический массив имен файлов в куче

    // ---- Текущее системное состояние ----
    bool _isPlaying;                          // Флаг: играет сейчас или стоит на паузе
    String _currentMetadata;                  // Буфер для названия текущего файла

    // ---- Аппаратные модули ----
    WorkSPIFFS::ConfigData* _config;          // Указатель на структуру настроек Wi-Fi

    // ---- Потокобезопасная очередь команд FreeRTOS ----
    QueueHandle_t _commandQueue;

    // ---- Текстовый кэш для синхронизации данных дисплея ----
    String _displayStatus;
    String _displayMetadata;
    String _displayTime;

    // ---- Внутренние приватные методы обработки ----
//    void startPlaying();
//    void stopPlaying();
    void processCommand(const AudioMessage& msg); // Разбор очереди на Core 0
    void sortFileList();                          // Пузырьковая сортировка по алфавиту
    void clearFileList(); // декларируем метод очистки памяти динамического массива!
    void syncNTP();

    // Друзья класса для лямбда-функций FreeRTOS задач
    friend void taskAudio(void* parameter);
    friend void taskControl(void* parameter);

    // Память под буфер
    const int preallocateBufferSize = 16 * 1024;
    uint8_t *_bufferMem = nullptr;
};

#endif // READER_H
