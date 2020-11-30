// 2019 Frederic Delhoume

#include <ESP8266WiFi.h>
#include <TFT_eSPI.h> 
//#include <FS.h>
#include <WiFiUdp.h>
#include <Timezone.h>
#include <WiFiManager.h>       
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <uptime.h>
#include <uptime_formatter.h>

//#define USE_TOUCH
/*
 * VCC      3.3V
 * GND      GND
 * CS       D2
 * RESET    RST
 * DC       D1
 * SDI/MOSI D7
 * SCK      D5
 * LED      3.3V 5V ?
 * SDO/MISO D6 optional
 */

 
//#define TFT_CS   PIN_D2  // Chip select control pin D8
//#define TFT_DC   PIN_D1  // Data Command control pin
//#define TFT_RST  PIN_D4  // Reset pin (could connect to NodeMCU RST, see next line)

//for Lolin TFT Shield
//#define TFT_CS PIN_D0  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
//#define TFT_DC PIN_D8  //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
//#define TFT_RST -1 //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
//#define TS_CS PIN_D3   //for D1 mini or TFT I2C Connector Shield (V1.1.0 or later)
//#define TOUCH_CS PIN_D3


ESP8266WebServer webServer(80);       // Create a webserver object that listens for HTTP request on port 80

TFT_eSPI tft; // Invoke custom library

boolean useSprite = true;
TFT_eSprite img = TFT_eSprite(&tft);

const uint16_t bg = TFT_BLACK;
const uint16_t fg = TFT_WHITE;

typedef struct RGB {
  RGB(uint8_t rr, uint8_t gg, uint8_t bb) { setRGB(rr, gg, bb); }
  uint8_t r;
  uint8_t g;
  uint8_t b;
  void setRGB(uint8_t rr, uint8_t gg, uint8_t bb) { r = rr; g = gg; b = bb; }
} RGB;


inline uint8_t lerp(uint8_t v1, uint8_t v2, float p) {
  return v1 + (uint8_t)(p * (v2 - v1));
}

inline uint16_t interpColor(RGB color1, RGB color2, float p) {
   return tft.color565(lerp(color1.r, color2.r, p), lerp(color1.g, color2.g, p), lerp(color1.b, color2.b, p));             
}

// test with very different colors
// blue gradient
RGB topColor = { 45, 85, 205 };
RGB bottomColor = { 105, 155, 230 };

RGB tempGradient[] = {
  RGB(21, 53, 145),
  RGB(22, 94, 149),
  RGB(23, 137, 152),
  RGB(24,156,130),
  RGB(25,159,92),
  RGB(26,163,51),
  RGB(45,167,28),
  RGB(91,170,29),
  RGB(138,174,30),
  RGB(177,168,31),
  RGB(181,125,32),
  RGB(184,81,34),
  RGB(188,35,35),
  RGB(199, 21, 133)
};

int8_t minTemp = -5;
int8_t maxTemp = 40;

void setColorsFromTemp(int8_t temp) {
  if (temp <= -20) {
    topColor = { 45, 85, 205 };
    bottomColor = { 105, 155, 230 };
  } else {
    int16_t maxindex = (sizeof(tempGradient) / sizeof(RGB)) - 1;
    int16_t ct = constrain(temp, minTemp, maxTemp);
    int16_t cti = map(ct, minTemp, maxTemp, 0, maxindex);
    RGB colorRGB = tempGradient[cti];
    topColor.setRGB(colorRGB.r / 2, colorRGB.g / 2, colorRGB.b / 2);
    bottomColor.setRGB(colorRGB.r, colorRGB.g, colorRGB.b);
  }
  initGradient();
}

inline uint16_t getBackgroundColor(uint16_t x, uint16_t y) {
   float p = y / (float)tft.height();
   return interpColor(topColor, bottomColor, p);
}

uint16_t gradient[320];

void initGradient() {
  for (uint16_t y = 0; y < 320; ++y) {
    gradient[y] = getBackgroundColor(0, y);
  }
}

void fillGradientSprite(TFT_eSprite& spr, uint16_t ystart, uint16_t h) {
  uint16_t w = spr.width();
  for (uint16_t y = 0; y < h; ++y) {
    uint16_t color = gradient[ystart + y];
    spr.drawFastHLine(0, y, w, color);
  }
}

void fillGradient(uint16_t y1, uint16_t y2) {
     uint16_t w = tft.width();
    for (uint16_t y = y1; y < y2; ++y) {
      uint16_t color = gradient[y]; // x not used here
      tft.drawFastHLine(0, y, w, color);                     
  }
}

void fillGradientFull() {
  uint16_t w = tft.width();
  uint16_t h = tft.height();
  uint16_t bandHeight = 20;
  uint16_t bands = h / bandHeight;
  img.createSprite(w, bandHeight);
  uint16_t y = 0;
  for (uint16_t b = 0; b < bands; b++, y += bandHeight) {
     fillGradientSprite(img, y, bandHeight);
     img.pushSprite(0, y);
  }
  img.deleteSprite();
}

void fillGradientRect(uint16_t y, uint16_t h) {
    img.createSprite(tft.width(), h);
    fillGradientSprite(img, y, h);
//    img.fillSprite(TFT_YELLOW);
    img.pushSprite(0, y);
    img.deleteSprite();
}

void fillColorFull() {
    uint16_t w = tft.width();
  uint16_t h = tft.height();
  uint16_t bandHeight = 20;
  uint16_t bands = h / bandHeight;
  img.createSprite(w, bandHeight);
  uint16_t y = 0;
  for (uint16_t b = 0; b < bands; b++, y += bandHeight) {
     img.fillSprite(b & 1 ? TFT_GREEN : TFT_RED);
     img.pushSprite(0, y);
  }
  img.deleteSprite();

}

inline uint16_t centerX(uint16_t w) {
  return (tft.width() - w) / 2;
}

inline uint16_t centerY(uint16_t H, uint16_t h) {
  return (H - h) / 2;
}

uint8_t rssiToQuality(long rssi) {
     // -120 to 0 DBm
   uint8_t quality;
   if(rssi <= -100)
     quality = 0;
   else if(rssi >= -50)
     quality = 100;
   else
     quality = 2 * (rssi + 100);
  return quality;
}

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;

const char*   owmApiKey    = "xxxxxxxx";
char   owmCityId[16] = "6613168";
const char*   owmLang      = "fr"; // result JSON is utf8
const uint8_t owmCnt       = 9; // one more in case forecast is late

typedef struct WeatherInfo {
  unsigned long time = 0;
  int8_t  temp = -50;
  char   description[24] = "---";
  char   icon[4] = ""; // TODO use index or enum
} WeatherInfo;

WeatherInfo winfo[owmCnt + 2]; // first is current, others are forecasts


char city[24] = "";

#define CITY_FONT "Roboto-Regular12"
#define CAPTION_FONT "Roboto-Regular12"
#define TIME_FONT "Roboto-Regular48"
#define DATE_FONT "Roboto-Regular22"
#define ICON_FONT "weathericons48"
#define SMALL_ICON_FONT "weathericons20"
#define SMALL_CAPTION_FONT "Roboto-Regular12"
#define BOOT_FONT "PixelMiners10"
#define CONFIG_FONT "Roboto-Regular22"

void initBackground() {
    setColorsFromTemp(winfo[0].temp);
//    fillGradientFull(); // uses sprites
//    fillColorFull(); // debug
}


uint16_t timePos = 14;
uint16_t datePos = 64;
uint16_t forecastPos = 190;

uint16_t cityHeight = 14;

void drawCity(uint16_t x, uint16_t y) {
    img.loadFont(CITY_FONT);
     String ct = city;
#if 0
     {
       int d = uptime::getDays();
        ct += " / ";
        if (d > 0) {
          ct +=  (String)(d) + "d";
        }
        ct += (String)(uptime::getHours()  ) + "h";
        if (d == 0) {
          ct += (String)(uptime::getMinutes()) + "m";
      }
     }
#endif
     uint16_t sprWidth = tft.width();
     img.createSprite(sprWidth, cityHeight);
     img.setTextColor(fg, gradient[y]);
     fillGradientSprite(img, y, cityHeight);
//    img.fillSprite(TFT_GREEN);
     img.setCursor(2, 2);
     img.printToSprite(ct);
     img.pushSprite(x, y);
     img.deleteSprite();
}

const uint16_t sprHeightTime = 50; // time

void drawTime(uint16_t y) {
   img.loadFont(TIME_FONT);
  time_t utc = now();
  time_t local = CE.toLocal(utc, &tcr);
  char buffer[6];
  sprintf(buffer, "%.2d:%.2d", hour(local), minute(local));  
  uint16_t w = img.textWidth(buffer);
   uint16_t sprWidth = tft.width();
   img.createSprite(sprWidth, sprHeightTime);
   fillGradientSprite(img, y, sprHeightTime);
   //img.fillSprite(TFT_GREEN);
  img.setTextColor(TFT_WHITE, gradient[y]);
  uint16_t dateOffset = 5;
  img.setCursor(centerX(w), dateOffset);
  img.printToSprite(buffer);
  img.pushSprite(0, y);
  img.deleteSprite();
}

const char* days[] = { "Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"  };
const char* monthes[] = { "Janvier", "Février", "Mars", "Avril", "Mai", "Juin", "Juillet", 
                          "Août", "Septembre", "Octobre", "Novembre", "Décembre" };

const uint16_t sprHeightDate = 28; // date

void drawDate(uint16_t y) {
   img.loadFont(DATE_FONT);
   time_t utc = now();
   time_t local = CE.toLocal(utc, &tcr);
   char buffer[30];
   sprintf(buffer, "%s %d %s", days[weekday(local) - 1], day(local), monthes[month(local) -1]);
   uint16_t w = img.textWidth(buffer);
    uint16_t sprWidth = tft.width();
   img.createSprite(sprWidth, sprHeightDate);
    fillGradientSprite(img, y, sprHeightDate);
    //img.fillSprite(TFT_RED);
    img.setTextColor(TFT_WHITE, gradient[y]);
    uint16_t dateOffset = 5;
    img.setCursor(centerX(w), dateOffset);
    img.printToSprite(buffer);
    img.pushSprite(0, y);
    img.deleteSprite();
}

const uint8_t DEFAULT_QUALITY = 70;

void drawWifi(uint16_t x, uint16_t y) {
  uint8_t quality = WiFi.isConnected() ? rssiToQuality(WiFi.RSSI()) : DEFAULT_QUALITY;
  uint16_t offset = 4;
  TFT_eSPI& dest = useSprite ? img : tft;
  uint16_t w = 15;
  uint16_t h = 12;
  uint16_t startx = useSprite ? 0 : x;
  uint16_t starty = useSprite ? h : y + h;

  if (useSprite) {
    img.createSprite(w, h);
 //   img.fillSprite(TFT_GREEN);
    fillGradientSprite(img, y, h);
  }
  
  if (quality >= 10)
    dest.fillRect(startx, starty - 3, 3, 3, TFT_WHITE);
  else
    dest.drawRect(startx, starty - 3, 3, 3, TFT_WHITE);
  startx += offset;
  if (quality >= 25)
    dest.fillRect(startx, starty - 5, 3, 5, TFT_WHITE);
  else
    dest.drawRect(startx, starty - 5, 3, 5, TFT_WHITE);
  startx += offset;
   if (quality >= 50)
    dest.fillRect(startx, starty - 7, 3, 7, TFT_WHITE);
  else
    dest.drawRect(startx, starty - 7, 3, 7, TFT_WHITE);
  startx += offset;
   if (quality >= 80)
    dest.fillRect(startx, starty - 9, 3, 9, TFT_WHITE);
  else
    dest.drawRect(startx, starty -9, 3, 9, TFT_WHITE);

    if (useSprite) {
      img.pushSprite(x, y);
      img.deleteSprite();
    }
}

// clear sky 01d f00d
// clear sky 01n f02e
// few clouds 02d 02n f00c
// scattered clouds 03d 03n f002
// broken clouds 04d 04n f013
// shower rain 09d 09n f019
// rain 10d f01a
// rain 10n f029
// thunderstorm 11d 11n f016
// snow 13d 13n f01b f038
// mist 50d 50n f021
// unknown f03e

String getIcon(String icon) {
  if (icon.startsWith("01d"))
    return u8"\uf00d";
 if (icon.startsWith("01n"))
    return u8"\uf02e";
  else if (icon.startsWith("02d"))
    return u8"\uf00c";
    else if (icon.startsWith("02n"))
    return u8"\uf031";
  else if (icon.startsWith("03d"))
    return u8"\uf002";
    else if (icon.startsWith("03n"))
    return u8"\uf031";
     else if (icon.startsWith("04d"))
    return u8"\uf013";
    else if (icon.startsWith("04n"))
    return u8"\uf031";
     else if (icon.startsWith("09d"))
    return u8"\uf019";
    else if (icon.startsWith("09n"))
    return u8"\uf028";
     else if (icon.startsWith("10d"))
    return u8"\uf01a";
    else if (icon.startsWith("10n"))
    return u8"\uf029";
     else if (icon.startsWith("11d"))
    return u8"\uf005";
    else if (icon.startsWith("11n"))
    return u8"\uf025";
     else if (icon.startsWith("13d"))
    return u8"\uf01b";
     else if (icon.startsWith("13n"))
    return u8"\uf038";
     else if (icon.startsWith("50d"))
    return u8"\uf012";   
    else if (icon.startsWith("50n"))
    return u8"\uf023";   
  return u8"\uf03e";
}

void drawWeatherAndTemp(uint16_t y) {  
  WeatherInfo info = winfo[0];
  int8_t temp = info.temp;
  String t = String(temp) + "°";
  String caption = info.description;
  String icon = getIcon(info.icon);
  uint16_t separator = 40;
  // center all
    TFT_eSPI& dest = useSprite ? img : tft;
    dest.loadFont(TIME_FONT);
    uint16_t tempWidth = dest.textWidth(t);
    uint16_t tempHeight = dest.fontHeight();
    dest.loadFont(ICON_FONT);
    uint16_t iconWidth = dest.textWidth(icon);
    uint16_t iconHeight = dest.fontHeight();
    uint16_t allWidth = iconWidth + separator + tempWidth;

    uint16_t x = (tft.width() - allWidth) / 2;
    uint16_t centerXIcon = x + (iconWidth / 2);
 
    // init sprite
    uint16_t sprWidth = tft.width() - 20;
    uint16_t sprHeight4 = iconHeight;
    if (useSprite) {
     img.createSprite(sprWidth, sprHeight4);
      fillGradientSprite(img, y, sprHeight4);
      //img.fillSprite(TFT_BLACK);
      img.setTextColor(TFT_WHITE, gradient[y]);   // not pixel perfect but close enough   
    }
   // display icon
    if (useSprite) {
      img.setCursor(x, 0);
      img.printToSprite(icon);
    } else {
      tft.setCursor(x, y);
      tft.print(icon);
    }
    // display temp
    dest.loadFont(TIME_FONT);
    uint16_t xpos = centerXIcon + (iconWidth / 2) + separator;
    // try to center vertically, works not very well because no textHeight(), uses fontHeight()
//    uint16_t ypos = (iconHeight - tempHeight) / 2; 
    uint16_t ypos = (sprHeight4 - tempHeight) / 2; 
    if (useSprite) {
      img.setCursor(xpos, ypos + 4);
      img.printToSprite(t);
      img.pushSprite(0, y);
      // fill space at right
      fillGradientSprite(img, y, img.height());
      img.pushSprite(sprWidth, y);
     } else {
      tft.setCursor(xpos, y + ypos);
      tft.print(t);
    }
    // display caption
    dest.loadFont(CAPTION_FONT);
    /// get caption centered over icon x
    uint16_t captionWidth = dest.textWidth(caption);
    xpos = centerXIcon - (captionWidth / 2);

    if (useSprite) {
      // re-create sprite
      img.deleteSprite();
      img.createSprite(sprWidth, img.fontHeight());
      fillGradientSprite(img, y + iconHeight, img.fontHeight());
      //img.fillSprite(TFT_RED);
      img.setCursor(xpos, 0);
      img.printToSprite(caption);
    } else {
      tft.setCursor(xpos, y + iconHeight);
      tft.print(caption);
    }
    // display sprite
    uint16_t gapOffset = img.height();
    if (useSprite) {
      img.pushSprite(0, y + iconHeight);
      // fill space at right
      fillGradientSprite(img, y + iconHeight, img.height());
      img.pushSprite(sprWidth, y + iconHeight);
      img.deleteSprite();
    }
    // fill gap before forecasts
    uint16_t gapStart = y + iconHeight + gapOffset;
    fillGradientRect(gapStart, forecastPos - gapStart); 
}

void drawForecasts(uint16_t y) {
  uint16_t numForecastPerLine = 4;
  uint16_t numLines = 2;
  TFT_eSPI& dest = useSprite ? img : tft;
  dest.loadFont(SMALL_CAPTION_FONT);
  uint16_t additionalHeight = dest.fontHeight();
  uint16_t spacing = 4;
  uint16_t rowSpacing = 0; // 6;

  dest.loadFont(SMALL_ICON_FONT);
  uint16_t iconHeight = dest.fontHeight();
  uint16_t spriteHeight = iconHeight + additionalHeight * 2 + spacing * 2 + 4;
  // distribute centers horizontally
  uint16_t borderX = 30;
  uint16_t spaceX = (tft.width() - (2 * borderX)) / (numForecastPerLine - 1);

  // create only one sprite per row
  if (useSprite) {
    img.createSprite(tft.width(), spriteHeight);
  }
  uint16_t ypos = y;
  uint8_t owmIndex = 1;
  for (uint16_t l = 0; l < numLines; ++l, y += 60, ypos += spriteHeight + rowSpacing) {
     uint16_t xiconCenter = borderX;
     if (useSprite) {
        fillGradientSprite(img, ypos, spriteHeight);
        //img.fillSprite(TFT_BLACK);
        img.setTextColor(TFT_WHITE, gradient[ypos]);      
    }
    for (uint16_t idx = 0; idx < numForecastPerLine; ++idx, ++owmIndex, xiconCenter += spaceX) {
     WeatherInfo info = winfo[owmIndex];
      String icon = getIcon(info.icon);
    // print icon
      dest.loadFont(SMALL_ICON_FONT);
      uint16_t iconWidth = dest.textWidth(icon);
      if (useSprite) {
          img.setCursor(xiconCenter - (iconWidth / 2), additionalHeight);
          img.printToSprite(icon);
          // DEBUG
          //img.drawFastVLine(xiconCenter, 0, spriteHeight, TFT_RED);
       } else {
         tft.setCursor(xiconCenter - (iconWidth / 2), y);
         tft.print(icon);
      }
      dest.loadFont(SMALL_CAPTION_FONT);
      int8_t temp = info.temp;
        String t = String(temp) + "°";
        // TODO convert to local time
        time_t utc = info.time;
        time_t local = CE.toLocal(utc, &tcr);
        char tt[6];
        sprintf(tt, "%d:%.2d", hour(local), minute(local));  

        // find center
        uint16_t tWidth = dest.textWidth(t);
        uint16_t ttWidth = dest.textWidth(tt);
        // print temp and caption
        if (useSprite) {
          img.setCursor(xiconCenter - (ttWidth / 2), 0);
          img.printToSprite(tt);
          img.setCursor(xiconCenter - (tWidth / 2), iconHeight + additionalHeight + spacing);
          img.printToSprite(t);
        } else {
          tft.setCursor(xiconCenter - (ttWidth / 2), y);
          tft.print(tt);
          tft.setCursor(xiconCenter - (tWidth / 2), y + iconHeight + additionalHeight + spacing);
          tft.print(t);
         }
     }
    if (useSprite) {
      img.pushSprite(0, ypos);
    }
  }
  if (useSprite) {
    img.deleteSprite();
  }
}

unsigned int localPort = 2390;      // local port to listen for UDP packets
const char* ntpServerName = "time.google.com";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE] = { 0 };
WiFiUDP udp;

void sendNTPpacket(IPAddress& address) {
  if (WiFi.isConnected()) {
    if (packetBuffer[0] == 0) { // it once for all
      memset(packetBuffer, 0, NTP_PACKET_SIZE);
      packetBuffer[0] = 0b11100011;   // LI, Version, Mode
      packetBuffer[1] = 0;     // Stratum, or type of clock
      packetBuffer[2] = 6;     // Polling Interval
      packetBuffer[3] = 0xEC;  // Peer Clock Precision
      // 8 bytes of zero for Root Delay & Root Dispersion
      packetBuffer[12] = 49;
      packetBuffer[13] = 0x4E;
      packetBuffer[14] = 49;
      packetBuffer[15] = 52;
    }
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
  }
}

const time_t DEFAULT_TIME = 0;

time_t getNtpTime() {
  if (WiFi.isConnected()) {
    IPAddress timeServerIP; // time.nist.gov NTP server address
    WiFi.hostByName(ntpServerName, timeServerIP);
    while (udp.parsePacket() > 0) ; // discard any previously received packets
    sendNTPpacket(timeServerIP);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
      int size = udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
 //       Serial.println("Receive NTP Response");
        udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
        return secsSince1900 - 2208988800UL;
      }
    }
  }
//  Serial.println("No NTP Response :-(");
  return DEFAULT_TIME; // return 0 if unable to get the time
}

void configModeCallback (WiFiManager *myWiFiManager) {
  tft.println("Config mode...");
  tft.print("connect to SSID ");
  tft.println(myWiFiManager->getConfigPortalSSID());
}

void boot1() {
  boolean spiffs = SPIFFS.begin();
    tft.begin();

    tft.fillScreen(TFT_BLACK);
    tft.setRotation(2);  // portrait
   
    tft.loadFont(BOOT_FONT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.println("Wemos TFT weather");
    tft.println();
    tft.println("2019 Frédéric Delhoume");
    tft.println();
    if (!spiffs) {
       tft.println("SPIFFS initialization failed!");
      while (1) yield(); // Stay here twiddling thumbs waiting
  } else {
    tft.println("SPIFFS started");
  }
    tft.println("Connecting to Wifi...");
}

void boot2() {
  if (WiFi.isConnected()) {
    tft.print("IP: ");
    tft.println(WiFi.localIP());
  } else {
    tft.println("No connection");
  }
  delay(2000);
}

typedef enum Mode {
  None = -1,
  Weather = 1,
  Config = 2
} Mode;

Mode currentMode = Weather;

uint8_t lastMinute = 100;
uint8_t lastDay = 10;
uint8_t lastSecond = 100;
unsigned long lastOWMUpdate = 0;

void setupWeather() {
    // reset everything
    // different gradient depending on current temperature...
    lastSecond = 100;
    lastMinute = 100;
    lastDay = 10;
    lastOWMUpdate = 0;
}

void loopWeather();

void loopConfig() {
}

void initOTA() {
  ArduinoOTA.setHostname("WemosWeather");
  ArduinoOTA.setPassword("wemos");
  ArduinoOTA.onStart([]() {
    tft.fillScreen(TFT_BLACK);
    tft.loadFont(BOOT_FONT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  });
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    unsigned int realprogress = (progress / (total / 100));
    char buffer[16];
    sprintf(buffer, "OTA: %.2d %%", realprogress);
    uint16_t height = tft.fontHeight();
    uint16_t width = tft.textWidth(buffer);
    tft.setCursor(centerX(width), (tft.height() - height) / 2);
    tft.print(buffer);
  });
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();
}

/*
   "id": 3018354, "name": "Fleac",
    "id": 6613168,"name": "Arcueil",
    "id": 6431033,"name": "Quimper",
*/

struct City {
  const char* name;
  const char* id;
};

struct City cities[] = {
  { "arcueil", "6613168" },
  { "fleac", "3018354" },
  { "quimper", "6431033" }
};

const char* SPIFFS_CITIES = "/cities.json";

String getCityId(const char* name) {
  // try known cities
  uint16_t nCities = sizeof(cities) / sizeof(struct City);
  for (uint16_t idx = 0; idx < nCities; ++idx) {
    if (!strcasecmp(cities[idx].name, name)) {
      return String(cities[idx].id);
    }
  }
  // try SPIFFS
  fs::File f = SPIFFS.open(SPIFFS_CITIES, "r");
  if (f) {
    // TODO: use streaming parser for large files
    String contents = f.readString();
    f.close();
    DynamicJsonDocument doc(1024);
    auto error = deserializeJson(doc, contents);
    if (!error) {
        const char* id = doc[name];
        if (id) {
          return String(id);
        }
      }
    }
  return String("");
}

const char* TFT_FILENAME = "/tft.bmp";

void generateBMPFile(const char* name) {
  fs::File file = SPIFFS.open(name, "w");
  uint16_t w = tft.width();
  uint16_t h = tft.height();
  int rowSize = 4 * ((3*w + 3) / 4); // with padding
  int filesize = 54 + rowSize * h;
  unsigned char bmpFileHeader[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0 };
  unsigned char bmpInfoHeader[40] = { 40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0 };

 bmpFileHeader[ 2] = (unsigned char)(filesize    );
 bmpFileHeader[ 3] = (unsigned char)(filesize>> 8);
 bmpFileHeader[ 4] = (unsigned char)(filesize>>16);
 bmpFileHeader[ 5] = (unsigned char)(filesize>>24);

 bmpInfoHeader[ 4] = (unsigned char)(       w    );
 bmpInfoHeader[ 5] = (unsigned char)(       w>> 8);
 bmpInfoHeader[ 6] = (unsigned char)(       w>>16);
 bmpInfoHeader[ 7] = (unsigned char)(       w>>24);
 bmpInfoHeader[ 8] = (unsigned char)(       h    );
 bmpInfoHeader[ 9] = (unsigned char)(       h>> 8);
 bmpInfoHeader[10] = (unsigned char)(       h>>16);
 bmpInfoHeader[11] = (unsigned char)(       h>>24);

 file.write(bmpFileHeader, sizeof(bmpFileHeader));    // write file header
 file.write(bmpInfoHeader, sizeof(bmpInfoHeader));    // " info header

  uint8_t* buffer = (uint8_t*)malloc(rowSize);
  for (uint16_t y = 0; y < h; y++) {
    // upside down
    tft.readRectRGB(0, h - y - 1, w, 1, buffer);
    file.write(buffer, rowSize);
  }
  free(buffer);
  file.close(); 
}

void setup() {
  Serial.begin(115200); 
  boot1();
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(60); // DEBUG MODE
  // wifiManager.setDebugOutput(true);
  wifiManager.setMinimumSignalQuality(30);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect("wemos_tft"); //  no password

  if (WiFi.isConnected()) {
    udp.begin(localPort);
    webServer.on("/city", HTTP_POST, []() {
      String city = webServer.arg("value");
      String id = getCityId(city.c_str());
      if (id.length() > 0) {
        strcpy(owmCityId, id.c_str());
      } else {
        // check if it is a number
        if (id.toInt() != 0) {
          strcpy(owmCityId, city.c_str());
        }
      }
      setupWeather();
    });
    webServer.on("/all", HTTP_GET, []() {
      DynamicJsonDocument doc(256);
      doc["uptime"] = uptime_formatter::getUptime();
      doc["city"] = owmCityId;
      char json[256];
      serializeJsonPretty(doc, json);
      webServer.send(200, "text/json", json);
    });
    webServer.on("/tft", HTTP_GET, []() {
      generateBMPFile(TFT_FILENAME);
      if (SPIFFS.exists(TFT_FILENAME)) {                            // If the file exists
         fs::File file = SPIFFS.open(TFT_FILENAME, "r");                 // Open it
         size_t sent = webServer.streamFile(file, "image/bmp"); // And send it to the client
         file.close(); 
      }                                      
    });
    webServer.begin();
   }
   setSyncInterval(300);
   setSyncProvider(getNtpTime);
 
    boot2();
    initGradient();

    initButtons();

    if (currentMode == Weather)
      setupWeather();
    else if (currentMode == Config)
      setupConfig();
}

HTTPClient owm;
WiFiClient wclient;

const unsigned long UPDATE_MINUTES = 2;
const unsigned long UPDATE_SECONDS = UPDATE_MINUTES * 60;
const unsigned long UPDATE_MILLIS = UPDATE_SECONDS * 1000;

void getOWMForecast() {
   if (WiFi.isConnected()) {
      char owmURL[128];
      sprintf(owmURL, "http://api.openweathermap.org/data/2.5/forecast?id=%s&lang=%s&appid=%s&units=metric&cnt=%d", 
              owmCityId, owmLang, owmApiKey, owmCnt);
   owm.begin(wclient, owmURL);
    if (owm.GET()) {
      String json = owm.getString();
 //     Serial.println(json);
 //     Serial.println(json.length());
      DynamicJsonDocument doc(8192);
      auto error = deserializeJson(doc, json);
      if (!error) {
        uint8_t idx = 1;
        unsigned long n = now();
        JsonArray list = doc["list"];
        for (JsonObject info : list) {
          unsigned long dt = info["dt"];
          // sometimes forecast are late
          if (dt > (n + UPDATE_SECONDS)) {
            winfo[idx].time = dt;
            winfo[idx].temp = (int8_t)round(info["main"]["temp"]);
             JsonObject weather = info["weather"][0];
             // not used
             // strcpy(winfo[idx].description, weather["description"]);
            strcpy(winfo[idx].icon,        weather["icon"]);
            ++idx;
          } else {
            time_t local = CE.toLocal(n, &tcr);
            time_t forec = CE.toLocal(dt, &tcr);
            char tt[16];
            sprintf(tt, "now %d:%.2d forecast %d:%.2d", hour(local), minute(local), hour(forec), minute(forec));
            Serial.println(String("Late :") + String(tt));
          }
        }
      }
    }
    owm.end();
  }
}

void getOWMCurrent() {
   if (WiFi.isConnected()) {
    char owmURL[128];
    sprintf(owmURL, "http://api.openweathermap.org/data/2.5/weather?id=%s&lang=%s&appid=%s&units=metric", 
            owmCityId, owmLang, owmApiKey);
    owm.begin(wclient, owmURL);
    if (owm.GET()) {
      String json = owm.getString();
 //     Serial.println(json);
 //     Serial.println(json.length());
      DynamicJsonDocument doc(2048);
      auto result = deserializeJson(doc, json);
      if (!result) {
         winfo[0].temp = (int8_t)round(doc["main"]["temp"]);
          JsonObject weather = doc["weather"][0];
          strcpy(winfo[0].description, weather["description"]);
          winfo[0].description[0] = toupper(winfo[0].description[0]);
          strcpy(winfo[0].icon, weather["icon"]);
          strcpy(city, doc["name"]);
        }
      }
    owm.end();
  }
}

typedef struct Button {
  uint16_t x;
  uint16_t y;
  uint16_t w;
  uint16_t h;
  uint16_t color;
  String text;
} Button;


uint16_t numButtons = 0;
Button buttons[20];

void initButton(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color, String text) {
  Button& button = buttons[numButtons];
  button.x = x;
  button.y = y;
  button.w = w;
  button.h = h;
  button.color = color;
  button.text = text;
  numButtons++;
}

uint16_t border = 5;
uint16_t separator = 10;
uint16_t lightblue = tft.color565(31, 31, 236);
uint16_t lightred = tft.color565(236, 31, 31);
uint16_t lightgreen = tft.color565(31, 200, 31);
uint16_t lightgray = tft.color565(31, 31, 31);

String buttonText[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "OK" };

void initButtons() {
    tft.loadFont(CONFIG_FONT);
    uint16_t height = tft.fontHeight() + 10;
    uint16_t width = tft.width() - 2 * border;
    numButtons = 0;

    uint16_t ypos = border;
    initButton(border, ypos, width, height, lightblue, "Arcueil");
    ypos += border + height;
    initButton(border, ypos, width, height, lightblue, "Fléac");
    ypos += border + height;
    initButton(border, ypos, width, height, lightblue, "Quimper");
    ypos += border + height + separator;
    initButton(border, ypos, width, height, lightblue, "");

    ypos += border + height;
    
    // draw numpad
    uint16_t cols = 3;
    uint16_t rows = 4;

    uint16_t colw = (tft.width() - ((cols + 1) * border)) / cols;

    uint8_t num = 0;
    for (uint16_t y = 0; y < rows; ++y, ypos += border + height) {
      uint16_t xpos = border;
      for (uint8_t x = 0; x < cols; ++x, xpos += (colw + border), ++num) {
        uint16_t color = lightblue;
        if (y == (rows - 1)) {
          if (x == 0) {
            color = lightred;
          } else if (x == 2) {
            color = lightgreen;
          }
        }
        initButton(xpos, ypos, colw, height, color, buttonText[num]);
      }
    }
}

uint16_t radius = 4;

void drawButton(struct Button& button) {
  uint16_t x = button.x;
  uint16_t y = button.y;
  uint16_t w = button.w;
  uint16_t h = button.h;
  uint16_t color = button.color;
  String text = button.text;
//  Serial.println(String( "Draw ") + text + " " + x + " " + y + " " + w + " " + h);
   tft.fillRoundRect(x, y, w, h, radius, color);
   // center text in button
   uint16_t texth = tft.fontHeight();
   uint16_t textw = tft.textWidth(text);
   uint16_t xtext = x + (w - textw) / 2;
   uint16_t ytext = y + (h - texth) / 2;
   tft.setCursor(xtext, ytext);
   tft.setTextColor(TFT_WHITE, color);
   tft.print(text);
}


void drawButtons() {
  for (uint16_t idx = 0; idx < numButtons; ++idx) {
   drawButton(buttons[idx]);
  }
}

int16_t buttonClicked(uint16_t x, uint16_t y) {
    for (uint16_t idx = 0; idx < numButtons; ++idx) {
      Button button = buttons[idx];
      if ((button.x <= x) && ((button.x + button.w) <= x) &&
          (button.y <= y) && ((button.y + button.h) <= y)) {
          return idx;
       }
    }
    return -1;
}

void setupConfig() {
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(2);  // portrait
    tft.loadFont(CONFIG_FONT);
    drawButtons();
}

void loop() {
  if (currentMode == Weather) {
      loopWeather();
  } else if (currentMode == Config) {
      loopConfig();
  }
 #if defined(USE_TOUCH)   
   uint16_t x, y;
  // See if there's any touch data for us
  if (tft.getTouch(&x, &y)) {
    Serial.println(String("Touch: ") + x + " " + y);
    if (currentMode == Weather) {
        currentMode = Config;
        setupConfig();
     } else {
        // TODO handle config events, find clicked button
        currentMode = Weather;
        setupWeather();
        int16_t b = buttonClicked(x, y);
        if (b >= 0) {
          Button button = buttons[b];
          Serial.println(buttons[b].text);
          //update color
          uint16_t oldColor = button.color;
          button.color = lightgray;
          drawButton(button);
          delay(100);
          button.color = oldColor;
          drawButton(button);
          // TODO call callback
        }
    }
  }
 #endif 
 webServer.handleClient();
 ArduinoOTA.handle();
}

int8_t lastTemp = -50;

void loopWeather() {
    time_t utc = now();
    time_t local = CE.toLocal(utc, &tcr);
    uint8_t m = minute(local); 
    uint8_t d = day(local);
    uint8_t s = second(local);
    
    if ((lastOWMUpdate == 0) || ((millis() - lastOWMUpdate) >= UPDATE_MILLIS)) { // every X minute check weather
      lastOWMUpdate = millis();
      getOWMCurrent();
       getOWMForecast();
       if (winfo[0].temp != lastTemp) {
            initBackground();
            lastTemp = winfo[0].temp;
        }
         // draw all
         drawCity(0, 0);
        drawWifi(222, 0);
        drawTime(timePos); 
       drawDate(datePos);
       drawWeatherAndTemp(92);
         drawForecasts(forecastPos);
     }
    if (s != lastSecond) {
         drawWifi(222, 0);
        lastSecond = s;
    }
    if (m != lastMinute) {
        drawTime(timePos); 
         lastMinute = m; 
    }
        if (d != lastDay) {
          // no need to refresh city
//         drawCity(2, 2);
         drawDate(datePos);
          lastDay = d;
    }
    uptime::calculateUptime();
}
