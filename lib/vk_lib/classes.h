#pragma once

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h> // Асинхронный сервер
#include "config.h"

#define CONFIG_FILE "/config"

// ============================================================
// КЛАСС ДЛЯ РАБОТЫ С ЭНЕРГОНЕЗАВИСИМОЙ ПАМЯТЬЮ SPIFFS
// ============================================================
class WorkSPIFFS {
public:
    // Упрощенная структура данных — только Wi-Fi параметры
    struct ConfigData {
        String ssid;
        String password;
        ConfigData() : ssid(""), password("") {}
    };

    WorkSPIFFS();
    ~WorkSPIFFS();

    bool begin();
    bool loadConfig(ConfigData& data);
    bool saveConfig(const ConfigData& data);
    void setDefaults(ConfigData& data);

private:
    String _configFile = CONFIG_FILE;
    bool _mounted = false;
};

// ============================================================
// КЛАСС ДЛЯ УПРАВЛЕНИЯ WI-FI И АСИНХРОННЫМ СЕРВЕРОМ
// ============================================================
class WiFiConnect {
public:
    enum class Mode { STA, AP, DISCONNECTED, ERROR };

    WiFiConnect() : _server(80), _mode(Mode::DISCONNECTED) {}

    void setAPCredentials(const String& ssid, const String& password);
    bool setupWiFi(WorkSPIFFS::ConfigData& config, unsigned long timeoutMs = 15000);
    
    Mode getMode() const { return _mode; }
    bool isSTA() const { return _mode == Mode::STA; }
    bool isAP() const { return _mode == Mode::AP; }
    bool isConnected() const;
    
    String getIPAddress();
    String getAPIPAddress();
    String getCurrentSSID();
    void disconnect();
    
    void setupWebServer(WorkSPIFFS& spiffs, WorkSPIFFS::ConfigData& config);
    void startAPMode();

private:
    Mode _mode;
    String targetSSID;
    String targetPassword;
    String apSSID;
    String apPassword;

    AsyncWebServer _server; // Асинхронный веб-сервер внутри класса

    bool connectToWiFi(const String& ssid, const String& password, unsigned long timeoutMs);
};
