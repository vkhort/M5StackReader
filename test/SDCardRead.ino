#include <M5Unified.h>
#include <SPI.h>      
#include <SD.h>       

// Функция для вывода списка файлов и папок
void printDirectory(File dir, int numSpaces) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // Файлов больше нет
            break;
        }
        
        // Делаем отступы пробелами для красивого отображения вложенности папок
        for (int i = 0; i < numSpaces; i++) {
            Serial.print("  ");
        }
        
        Serial.print(entry.name());
        
        if (entry.isDirectory()) {
            Serial.println("/");
            M5.Display.printf("[%s]\n", entry.name()); // Вывод папки на экран
            // Если это папка, рекурсивно заходим внутрь (numSpaces + 2)
            printDirectory(entry, numSpaces + 2);
        } else {
            // Выводим имя файла и его размер в байтах
            Serial.printf("    %d байт\n", entry.size());
            M5.Display.printf(" - %s\n", entry.name()); // Вывод файла на экран
        }
        entry.close();
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg); 

    Serial.begin(115200);
    delay(1000);

    Serial.println("=== Сканирование SD-карты ===");
    
    M5.Display.clear();
    M5.Display.setTextSize(1); // Ставим мелкий шрифт, чтобы влезло больше названий
    M5.Display.setCursor(0, 0);

    if (!SD.begin(GPIO_NUM_4)) {
        Serial.println("❌ Ошибка SD-карты!");
        M5.Display.setTextColor(TFT_RED);
        M5.Display.println("SD Card Error!");
        return;
    }

    Serial.println("✅ Карта подключена. Список файлов:");
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.println("Files in root (/) :");

    // Открываем корневой каталог
    File root = SD.open("/");
    
    // Запускаем чтение содержимого (0 — начальный уровень отступа)
    printDirectory(root, 0);
    
    root.close();
    Serial.println("=== Сканирование завершено ===");
}

void loop() {
    M5.update();
}
