// ============================================================
// html_const.h - Оптимизированные константы HTML страниц
// ============================================================
#ifndef HTML_CONST_H
#define HTML_CONST_H

// ============================================================
// ЧАСТЬ 1: СТИЛИ И ШАПКА СТРАНИЦЫ (Хранятся строго в Flash/PROGMEM)
// ============================================================
static const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VK Музыкальный Комбайн</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #1a1a2e; color: white; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
        .card { background: #16162a; padding: 30px; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.3); width: 100%; max-width: 400px; box-sizing: border-box; border: 1px solid #4CAF50; }
        h2 { margin-top: 0; color: #4CAF50; text-align: center; margin-bottom: 25px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 8px; color: #4CAF50; font-weight: bold; font-size: 14px; }
        
        /* Стили для текстовых полей */
        input[type="text"], input[type="password"], input[type="number"] { 
            width: 100%; 
            padding: 10px; 
            border-radius: 8px; 
            border: 1px solid #4CAF50; 
            background: #0f3460; 
            color: white; 
            font-size: 1em; 
            outline: none; 
            box-sizing: border-box;
        }
        
        .password-wrapper { position: relative; display: flex; align-items: center; }
        .toggle-password { position: absolute; right: 10px; background: none; border: none; cursor: pointer; font-size: 18px; padding: 0; }
        
        /* Стильный ползунок громкости (Range Slider) */
        input[type="range"].vol-slider { 
            width: 100%; 
            -webkit-appearance: none; 
            background: #0f3460; 
            height: 8px; 
            border-radius: 5px; 
            outline: none; 
            border: 1px solid #4CAF50; 
            margin: 15px 0; 
        }
        input[type="range"].vol-slider::-webkit-slider-thumb { 
            -webkit-appearance: none; 
            background: #4CAF50; 
            width: 22px; 
            height: 22px; 
            border-radius: 50%; 
            cursor: pointer; 
            transition: transform 0.1s; 
            box-shadow: 0 0 10px rgba(76, 175, 80, 0.5); 
        }
        input[type="range"].vol-slider::-webkit-slider-thumb:hover { 
            transform: scale(1.2); 
        }
        
        /* Группа кнопок управления */
        .btn-group { 
            display: flex; 
            gap: 10px; 
            margin-top: 15px; 
            flex-wrap: wrap; 
            justify-content: center; 
        }
        button { 
            flex: 1; 
            background: #4CAF50; 
            color: white; 
            border: none; 
            padding: 12px 20px; 
            border-radius: 8px; 
            cursor: pointer; 
            font-size: 1em; 
            transition: all 0.2s; 
            min-width: 80px; 
            font-weight: bold;
        }
        button:hover { background: #45a049; }
        button:disabled { opacity: 0.5; cursor: not-allowed; }
        button.secondary { background: #0f3460; border: 1px solid #4CAF50; }
        button.secondary:hover { background: #1a4a80; }
        button.danger { background: #dc3545; }
        button.danger:hover { background: #c82333; }
        button.load { background: #ffc107; color: #1a1a2e; }
        button.load:hover { background: #e0a800; }
        
        .status { text-align: center; margin-top: 15px; font-size: 14px; font-weight: bold; color: #ffc107; }
    </style>
</head>
<body>
    <div class="card">
        <h2>VK Музыкальный Комбайн</h2>
        
        <div class="form-group">
            <label>Wi-Fi SSID</label>
            <input type="text" id="ssid" placeholder="Enter Wi-Fi network name">
        </div>
        
        <div class="form-group">
            <label>Wi-Fi Password</label>
            <div class="password-wrapper">
                <input type="password" id="password" placeholder="Enter Wi-Fi password">
                <button type="button" class="toggle-password" onclick="togglePassword(this)">👁️</button>
            </div>
        </div>
        
        <div class="btn-group">
            <button class="load" onclick="loadConfig()">Read</button>
            <button onclick="saveConfig()">Save</button>
        </div>

        <div id="statusText" class="status"></div>
    </div>

    <script>
        // Функция переключения видимости пароля (звездочки в символы)
        function togglePassword(btn) {
            const input = document.getElementById('password');
            if (input.type === 'password') {
                input.type = 'text';
                btn.textContent = '🙈';
            } else {
                input.type = 'password';
                btn.textContent = '👁️';
            }
        }

        // Функция нажатия на кнопку Read (Запрашиваем данные с ESP32)
        function loadConfig() {
            document.getElementById('statusText').textContent = "Чтение настроек из памяти...";
            fetch('/get-config')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('ssid').value = data.ssid || "";
                    document.getElementById('password').value = data.password || "";
                    document.getElementById('statusText').textContent = "Настройки успешно прочитаны!";
                })
                .catch(err => {
                    document.getElementById('statusText').textContent = "Ошибка чтения данных!";
                    console.error(err);
                });
        }

        // Функция нажатия на кнопку Save (Шлем POST запрос на ESP32)
        function saveConfig() {
            const ssidValue = document.getElementById('ssid').value;
            const passValue = document.getElementById('password').value;
            
            if (!ssidValue) {
                document.getElementById('statusText').textContent = "Укажите имя сети Wi-Fi!";
                return;
            }

            document.getElementById('statusText').textContent = "Сохранение конфигурации...";

            // Собираем стандартную HTML-форму для POST в теле запроса
            const formData = new FormData();
            formData.append('ssid', ssidValue);
            formData.append('password', passValue);

            fetch('/save', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(text => {
                document.getElementById('statusText').textContent = "Сохранено! Рестарт устройства...";
            })
            .catch(err => {
                document.getElementById('statusText').textContent = "Ошибка записи!";
                console.error(err);
            });
        }

        // Автоматически читаем конфиг при открытии страницы в браузере
        window.onload = loadConfig;
    </script>
</body>
</html>
)rawliteral";

#endif
