// ============================================================
// reader.h - Монолитный управляющий класс VK_Reader
// ============================================================
#ifndef READER_H
#define READER_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <freertos/queue.h>

// Инклуды аудио-движка ESP8266Audio
#include "AudioFileSourceSD.h"       // Для чтения локальных файлов с флешки
#include "AudioFileSourceBuffer.h"   // Буферизация для стабильности
#include "AudioGeneratorMP3.h"       // Декодер MP3
#include "AudioGeneratorWAV.h"       // Декодер WAV
#include "AudioOutputI2S.h"          // Вывод на ЦАП

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
    Reader();  // ИСПРАВЛЕНО: Убрана опечатка Readder
    ~Reader();

    // ---- Публичный интерфейс управления (вызывается из main.cpp) ----
    bool begin(WorkSPIFFS::ConfigData& cfg); // Старт SD-карты, создание очередей
    void scanFiles();                         // Сканирование флешки и сортировка по алфавиту
    
    String getCurrentFileName() const;       // Получить имя текущего файла для бегущей строки

    bool isPlaying() const { return _isPlaying; }
    void pushButton(int buttonCode); // НАШ УТВЕРЖДЕННЫЙ МОЗГ УПРАВЛЕНИЯ КНОПКАМИ

private:
    // ---- Время и Синхронизация (Пока заглушка) ----
    time_t _lastNtpAttempt;
    bool _ntpSuccess;
    time_t _lastNtpSync;
    struct tm _timeInfo;

    // ---- Аудио-движок (Динамические указатели ESP8266Audio) ----
//    AudioGenerator*       _decoder = nullptr; // Универсальный указатель на MP3 или WAV
//    AudioFileSourceSD*    _file = nullptr;    // Источник данных: файл на SD-карте
//    AudioFileSourceBuffer* _buff = nullptr;    // Буфер в оперативной памяти
//    AudioOutput*          _out = nullptr;     // Базовый класс вывода звука
    // ИСПРАВЛЕНО: Вместо AudioOutputI2S* используем универсальный базовый класс вывода библиотеки
    AudioFileSourceSD*     _file;
    AudioFileSourceBuffer* _buff;
    AudioGeneratorMP3*     _decoder;
    AudioOutput*           _out;      // Изменено на базовый AudioOutput, чтобы подключить наш мост!

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
    void startPlaying();
    void stopPlaying();
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
