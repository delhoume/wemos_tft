This is an Arduino sketch to display OpenWeatherMap forecasts on a 320x240 tft display.
It has been created for Wemos D1 Mini ESP8266 and TFT Shield.

It depends on some external libraries:
- TFT_eSPI https://github.com/Bodmer/TFT_eSPI
- Timezone https://github.com/JChristensen/Timezone
- WifiManager https://github.com/tzapu/WiFiManager
- ArduinoJSON https://arduinojson.org/
- uptime https://github.com/YiannisBourkelis/Uptime-Library

It is specifically coded for French language and tries to use very limited memory
on esp8266 while maintaining a smooth double buffered display.
