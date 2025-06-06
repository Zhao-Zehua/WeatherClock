//==================================================================================
//                               LIBRARY INCLUDES
//==================================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include "ArduinoJson.h"
#include <TimeLib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "lvgl.h"

// Custom assets
#include "weathernum.h"
#include "img/temperature.h"
#include "img/humidity.h"
#include "img/ccmelogo.h"

// Custom fonts
#include "font/FxLED_32.h"
#include "font/zkyyt12.h"
#include "font/city10.h"
#include "font/tq10.h"
#include "font/ALBB10.h"
#include "font/ALBB12.h"

//==================================================================================
//                              USER CONFIGURATION
//==================================================================================
// --- WiFi Credentials ---
const char *ssid = "your-ssid";
const char *password = "your-password";

// --- Custom Display Text ---
String motto = "Pek. Univ. CCME";

// --- Weather Configuration ---
String cityCode = "101010100"; // Default: Beijing (can be auto-detected)

//==================================================================================
//                                   CONSTANTS
//==================================================================================
// Screen dimensions
static const uint16_t SCREEN_WIDTH = 128;
static const uint16_t SCREEN_HEIGHT = 128;

// Network
static const char NTP_SERVER[] = "time.nist.gov";
const int TIME_ZONE = 8; // UTC+8
const int NTP_PORT = 8000;

// Update Intervals (in milliseconds)
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 20000;  // 20 seconds
const unsigned long NTP_SYNC_INTERVAL_MS = 20 * 60 * 1000; // 20 minutes
const unsigned long WEATHER_UPDATE_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes
const unsigned long INFO_SCROLL_INTERVAL_MS = 3500;       // 3.5 seconds
const unsigned long TEMP_HUMID_SWITCH_INTERVAL_MS = 5000; // 5 seconds

// Pin & System
const int LCD_BL_PWM_CHANNEL = 2;

// Colors (565 format)
const uint16_t COLOR_BG = 0x0000;
const uint16_t COLOR_WHITE = 0xFFFF;
const uint16_t COLOR_RED = 0xF800;
const uint16_t COLOR_GREEN = 0x07E0;
const uint16_t COLOR_BLUE = 0x001F;
const uint16_t COLOR_MOTTO = 0x9021; // PKU red

// Theme Colors
const uint16_t color_TimeHM_Font = COLOR_WHITE;
const uint16_t color_TimeS_Font = COLOR_WHITE;
const uint16_t color_Week_Font = COLOR_WHITE;
const uint16_t color_Month_Font = COLOR_WHITE;
const uint16_t color_TH_Font = COLOR_WHITE; // Temp/Humidity
const uint16_t color_Tip_Font = COLOR_WHITE;
const uint16_t color_City_Font = COLOR_WHITE;
const uint16_t color_Air_Font = COLOR_WHITE;
const uint16_t color_Frame = COLOR_WHITE;

//==================================================================================
//                                GLOBAL OBJECTS
//==================================================================================
TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);
TFT_eSprite clk = TFT_eSprite(&tft); // Sprite for most UI elements
TFT_eSprite clkb = TFT_eSprite(&tft); // Sprite for animations

WiFiClient wifiClient;
HTTPClient httpClient;
WiFiUDP wifiUdp;

WeatherNum weatherIconRenderer;

LV_IMG_DECLARE(ASSASSIN);
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * SCREEN_HEIGHT / 10];
static lv_obj_t *logo_imga = NULL;

//==================================================================================
//                          STATE & TIMING VARIABLES
//==================================================================================
// State Management
enum AppState { STATE_INIT, STATE_WIFI_CONNECTING, STATE_RUNNING, STATE_WIFI_LOST };
AppState currentState = STATE_INIT;
time_t prevDisplayTime = 0;

// Weather & Info
String currentTemperature = "--";
String currentHumidity = "--";
String scrollText[7];
int scrollTextIndex = 0;

// Non-Blocking Timers
unsigned long lastWifiCheckTime = 0;
unsigned long lastNtpSyncTime = 0;
unsigned long lastWeatherUpdateTime = 0;
unsigned long lastScrollTime = 0;
unsigned long lastTempHumidSwitchTime = 0;

// Animation State
enum AnimState { ANIM_IDLE, ANIM_RUNNING };
AnimState tempHumidAnimState = ANIM_IDLE;
unsigned long tempHumidAnimStartTime = 0;
int tempHumidDisplayState = 0; // 0 for temp, 1 for humid

//==================================================================================
//                          FUNCTION PROTOTYPES
//==================================================================================
// Core State Management
void manageWifiConnection();
void manageNtpSync();
void manageWeatherUpdates();

// Setup functions
void setupDisplay();
void startWifi();
void setupLVGL();
void drawStaticUI();

// Network functions
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
void fetchCityCode();
void fetchWeatherData();

// Parsing and Data Handling
void parseAndDisplayWeatherData(String *cityDZ, String *dataSK, String *dataFC);

// UI Update Functions
void updateClockDisplay();
void triggerScrollingText();
void triggerTempHumidSwitch();
void handleAnimations();
void displayFollowerInfo();
void updateLVGL();

// String/Time Helpers
String getWeekString();
String getMonthDayString();
String getHourMinuteString();
String formatDigits(int digits);

// LVGL Callbacks
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

//==================================================================================
//                                    SETUP
//==================================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n--- Weather Clock Starting Up (Non-Blocking Version) ---");

    // 1. 立即初始化和绘制静态UI
    setupDisplay();
    setupLVGL();
    drawStaticUI();         // <-- 移到这里，立即绘制UI框架
    displayFollowerInfo();  // <-- 移到这里，立即绘制底部信息

    // 2. 在后台开始连接WiFi，并显示非阻塞提示
    startWifi();
    currentState = STATE_WIFI_CONNECTING;
    lastWifiCheckTime = millis();
    
    Serial.println("\n--- Setup Complete, Handing Over to Main Loop ---");
}

//==================================================================================
//                                 MAIN LOOP
//==================================================================================
void loop() {
    // Core state and connection managers
    manageWifiConnection();
    manageNtpSync();
    manageWeatherUpdates();
    
    // Update time display only when the second changes
    if (now() != prevDisplayTime) {
        prevDisplayTime = now();
        updateClockDisplay();
    }

    // Trigger UI updates based on their timers if the system is running
    if (currentState == STATE_RUNNING) {
        if (millis() - lastScrollTime > INFO_SCROLL_INTERVAL_MS) {
            triggerScrollingText();
        }
        if (millis() - lastTempHumidSwitchTime > TEMP_HUMID_SWITCH_INTERVAL_MS) {
            triggerTempHumidSwitch();
        }
    }
    
    // These handlers run continuously to drive animations
    handleAnimations();
    updateLVGL();

    delay(10); // Yield to other tasks
}

//==================================================================================
//                          CORE STATE MANAGEMENT
//==================================================================================

//==================================================================================
//                          CORE STATE MANAGEMENT
//==================================================================================


void manageWifiConnection() {
    // 只有在初次连接时，或者距离上次检查超过一定间隔时，才检查WiFi状态。
    if (millis() - lastWifiCheckTime < (currentState == STATE_WIFI_CONNECTING ? 500 : WIFI_RECONNECT_INTERVAL_MS)) {
        return;
    }
    lastWifiCheckTime = millis();

    if (WiFi.status() != WL_CONNECTED) {
        if (currentState == STATE_RUNNING) {
            Serial.println("\nWiFi connection lost. Attempting to reconnect...");
            currentState = STATE_WIFI_CONNECTING;
            WiFi.reconnect();
        } else if (currentState == STATE_WIFI_CONNECTING) {
            Serial.print(".");
        }
    } 
    else if (currentState != STATE_RUNNING) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        // 清除顶部的 "Connecting..." 消息，并触发初始滚动文本
        triggerScrollingText(); 
        
        currentState = STATE_RUNNING;

        // 立即触发NTP和天气更新
        manageNtpSync();
        fetchCityCode();
    }
}

void manageNtpSync() {
    if (currentState != STATE_RUNNING) return; // Only sync if WiFi is connected

    if (timeStatus() == timeNotSet || millis() - lastNtpSyncTime > NTP_SYNC_INTERVAL_MS) {
        Serial.println("Attempting NTP synchronization...");
        wifiUdp.begin(NTP_PORT);
        setSyncProvider(getNtpTime);
        setSyncInterval(NTP_SYNC_INTERVAL_MS / 1000); // setSyncInterval is in seconds
        lastNtpSyncTime = millis();
    }
}

void manageWeatherUpdates() {
    if (currentState != STATE_RUNNING) return; // Only update if WiFi is connected

    if (millis() - lastWeatherUpdateTime > WEATHER_UPDATE_INTERVAL_MS) {
        fetchWeatherData();
        lastWeatherUpdateTime = millis();
    }
}

//==================================================================================
//                            SETUP FUNCTIONS
//==================================================================================

void setupDisplay() {
    tft.begin();
    tft.setRotation(2);
    tft.invertDisplay(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_WHITE, COLOR_BG);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    Serial.println("Display initialized.");
}

void startWifi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password); // This is non-blocking
}

void setupLVGL() {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_HEIGHT * SCREEN_WIDTH / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_obj_add_style(lv_scr_act(), &style, 0);

    logo_imga = lv_gif_create(lv_scr_act());
    lv_obj_center(logo_imga);
    lv_obj_align(logo_imga, LV_ALIGN_LEFT_MID, 50, 0);
    lv_gif_set_src(logo_imga, &ASSASSIN);

    updateLVGL();
    Serial.println("LVGL initialized for animation.");
}

void drawStaticUI() {
    tft.drawFastHLine(0, 0, 128, color_Frame);
    tft.drawFastHLine(0, 18, 128, color_Frame);
    tft.drawFastHLine(0, 88, 128, color_Frame);
    tft.drawFastHLine(0, 106, 128, color_Frame);
    tft.drawFastHLine(0, 127, 128, color_Frame);

    tft.drawFastVLine(80, 0, 18, color_Frame);
    tft.drawFastVLine(32, 88, 18, color_Frame);
    tft.drawFastVLine(85, 88, 18, color_Frame);
    Serial.println("Static UI frame drawn.");
}

//==================================================================================
//                           NETWORK FUNCTIONS
//==================================================================================

void fetchCityCode() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot fetch city code, WiFi not connected.");
        return;
    }
    // Set a short timeout for DNS and connection to avoid long blocks
    httpClient.setTimeout(5000); 
    String url = "http://wgeo.weather.com.cn/ip/?_=" + String(now());
    httpClient.begin(wifiClient, url);
    // ... rest of the function is the same ...
    int httpCode = httpClient.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = httpClient.getString();
        int idPos = payload.indexOf("id=");
        if (idPos > -1) {
            cityCode = payload.substring(idPos + 4, idPos + 4 + 9);
            Serial.print("Auto-detected City Code: ");
            Serial.println(cityCode);
        } else {
            Serial.println("Failed to find city code in response. Using default.");
        }
    } else {
        Serial.printf("Failed to get city code, HTTP error: %d. Using default.\n", httpCode);
    }
    httpClient.end();
    fetchWeatherData(); // Fetch weather with the (new or default) city code
}

void fetchWeatherData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot fetch weather, WiFi not connected.");
        currentTemperature = "XX"; currentHumidity = "XX"; // Indicate data is stale
        return;
    }

    Serial.println("Fetching new weather data...");
    httpClient.setTimeout(8000); // 8-second timeout for the entire request
    String url = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_=" + String(now());
    httpClient.begin(wifiClient, url);
    httpClient.addHeader("Referer", "http://www.weather.com.cn/");
    httpClient.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/116.0.0.0 Safari/537.36 Edg/116.0.1938.76");

    int httpCode = httpClient.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = httpClient.getString();
        // Extract the relevant JSON parts
        int indexStart = payload.indexOf("weatherinfo\":");
        int indexEnd = payload.indexOf("};var alarmDZ");
        if(indexStart == -1 || indexEnd ==-1) { httpClient.end(); return; }
        String jsonCityDZ = payload.substring(indexStart + 13, indexEnd);

        indexStart = payload.indexOf("dataSK =");
        indexEnd = payload.indexOf(";var dataZS");
        if(indexStart == -1 || indexEnd ==-1) { httpClient.end(); return; }
        String jsonDataSK = payload.substring(indexStart + 8, indexEnd);

        indexStart = payload.indexOf("\"f\":[");
        indexEnd = payload.indexOf(",{\"fa");
        if(indexStart == -1 || indexEnd ==-1) { httpClient.end(); return; }
        String jsonFC = payload.substring(indexStart + 5, indexEnd);

        parseAndDisplayWeatherData(&jsonCityDZ, &jsonDataSK, &jsonFC);
        Serial.println("Weather data fetch and display successful.");
    } else {
        Serial.printf("Failed to get weather data, HTTP error: %d\n", httpCode);
        currentTemperature = "ER"; currentHumidity = "ER"; // Indicate error
    }
    httpClient.end();
}

void parseAndDisplayWeatherData(String *cityDZ, String *dataSK, String *dataFC) {
    // This function remains largely the same as it's just parsing and drawing.
    // The key is that it's only called after a successful, non-blocking fetch.
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, *dataSK);
    // ... function content is identical to your original ...
    JsonObject sk = doc.as<JsonObject>();
    if (sk.isNull()) { Serial.println("Failed to parse SK JSON"); return; }

    currentTemperature = sk["temp"].as<String>();
    currentHumidity = sk["SD"].as<String>();

    clk.setColorDepth(8); clk.loadFont(city10);
    clk.createSprite(46, 16); clk.fillSprite(COLOR_BG);
    clk.setTextDatum(ML_DATUM); clk.setTextColor(color_City_Font, COLOR_BG);
    clk.drawString(sk["cityname"].as<String>(), 1, 8);
    clk.pushSprite(81, 2); clk.deleteSprite(); clk.unloadFont();

    int weatherCode = atoi((sk["weathercode"].as<String>()).substring(1, 3).c_str());
    weatherIconRenderer.printfweather(1, 47, weatherCode);

    String aqiTxt = "优"; uint16_t pm25BgColor = tft.color565(156, 202, 127);
    int pm25V = sk["pm25"].as<int>();
    if (pm25V > 200) { aqiTxt = "重度"; pm25BgColor = tft.color565(136, 11, 32); }
    else if (pm25V > 150) { aqiTxt = "中度"; pm25BgColor = tft.color565(186, 55, 121); }
    else if (pm25V > 100) { aqiTxt = "轻度"; pm25BgColor = tft.color565(242, 159, 57); }
    else if (pm25V > 50) { aqiTxt = "良"; pm25BgColor = tft.color565(247, 219, 100); }

    clk.setColorDepth(8); clk.loadFont(tq10);
    clk.createSprite(36, 14); clk.fillSprite(COLOR_BG);
    clk.fillRoundRect(0, 0, 32, 16, 4, pm25BgColor);
    clk.setTextDatum(MC_DATUM); clk.setTextColor(color_Air_Font);
    clk.drawString(aqiTxt, 16, 8); clk.pushSprite(93, 68); clk.deleteSprite(); clk.unloadFont();
    
    scrollText[0] = "实时天气 " + sk["weather"].as<String>();
    scrollText[1] = "空气质量 " + aqiTxt;
    scrollText[2] = "风向 " + sk["WD"].as<String>() + sk["WS"].as<String>();
    
    deserializeJson(doc, *cityDZ); JsonObject dz = doc.as<JsonObject>();
    if (!dz.isNull()) { scrollText[3] = "今日 " + dz["weather"].as<String>(); }
    
    deserializeJson(doc, *dataFC); JsonObject fc = doc.as<JsonObject>();
    if (!fc.isNull()) {
        scrollText[4] = "低温 " + fc["fd"].as<String>() + "℃";
        scrollText[5] = "高温 " + fc["fc"].as<String>() + "℃";
    }
}

//==================================================================================
//                       NON-BLOCKING UI & ANIMATIONS
//==================================================================================

//==================================================================================
//                       NON-BLOCKING UI & ANIMATIONS
//==================================================================================

void updateClockDisplay() {
    clk.setColorDepth(8);

    // --- Hour and Minute ---
    clk.createSprite(75, 28);
    clk.fillSprite(COLOR_BG);
    clk.loadFont(FxLED_32);
    clk.setTextDatum(ML_DATUM);
    clk.setTextColor(color_TimeHM_Font, COLOR_BG);
    if (timeStatus() == timeSet) {
        clk.drawString(getHourMinuteString(), 1, 14);
    } else {
        clk.drawString("00:00", 1, 14); // 占位符
    }
    clk.pushSprite(10, 19);
    clk.deleteSprite();

    // --- Seconds ---
    clk.createSprite(50, 28);
    clk.fillSprite(COLOR_BG);
    // 字体已加载，无需重复加载
    clk.setTextDatum(ML_DATUM);
    clk.setTextColor(color_TimeS_Font, COLOR_BG);
     if (timeStatus() == timeSet) {
        clk.drawString(formatDigits(second()), 1, 14);
    } else {
        clk.drawString("00", 1, 14); // 占位符
    }
    clk.pushSprite(86, 19);
    clk.deleteSprite();
    clk.unloadFont();

    // --- Weekday and Date ---
    clk.loadFont(zkyyt12);
    // Weekday
    clk.createSprite(30, 16);
    clk.fillSprite(COLOR_BG);
    clk.setTextDatum(ML_DATUM);
    clk.setTextColor(color_Week_Font, COLOR_BG);
    if (timeStatus() == timeSet) {
        clk.drawString(getWeekString(), 1, 8);
    } else {
        clk.drawString("周一", 1, 8); // 占位符
    }
    clk.pushSprite(1, 89);
    clk.deleteSprite();

    // Month and Day
    clk.createSprite(52, 16);
    clk.fillSprite(COLOR_BG);
    // 字体已加载
    clk.setTextDatum(ML_DATUM);
    clk.setTextColor(color_Month_Font, COLOR_BG);
    if (timeStatus() == timeSet) {
        clk.drawString(getMonthDayString(), 1, 8);
    } else {
        clk.drawString("一月一日", 1, 8); // 占位符
    }
    clk.pushSprite(33, 89);
    clk.deleteSprite();
    clk.unloadFont();
}

void triggerScrollingText() {
    lastScrollTime = millis();
    // 在下一个循环之前提前增加索引
    int nextIndex = (scrollTextIndex + 1) % 6; 

    // 检查当前文本是否存在，不存在则跳过绘制
    if (scrollText[scrollTextIndex].isEmpty()) {
        scrollTextIndex = nextIndex;
        return;
    }
    
    // Animate the text sliding up
    clkb.setColorDepth(8);
    clkb.loadFont(tq10);
    clkb.createSprite(78, 16);
    clkb.fillSprite(COLOR_BG);
    clkb.setTextWrap(false);
    clkb.setTextDatum(ML_DATUM);
    clkb.setTextColor(color_Tip_Font, COLOR_BG);
    // 直接绘制当前索引的文本
    clkb.drawString(scrollText[scrollTextIndex], 1, 8);
    clkb.pushSprite(1, 1);
    clkb.deleteSprite();
    clkb.unloadFont();

    // 更新索引到下一个
    scrollTextIndex = nextIndex;
}

void triggerTempHumidSwitch() {
    // This function just sets the state to start the animation.
    // The actual drawing happens in handleAnimations().
    if (tempHumidAnimState == ANIM_IDLE) {
        tempHumidDisplayState = 1 - tempHumidDisplayState; // Toggle state
        tempHumidAnimState = ANIM_RUNNING;
        tempHumidAnimStartTime = millis();
        lastTempHumidSwitchTime = millis();
    }
}

void handleAnimations() {
    if (tempHumidAnimState != ANIM_RUNNING) return;

    const unsigned long animDuration = 400; // 400ms for the slide-in animation
    unsigned long elapsedTime = millis() - tempHumidAnimStartTime;

    String valueToShow;
    const uint8_t *iconToShow;
    size_t iconSize;

    if (tempHumidDisplayState == 0) { // Temperature
        valueToShow = currentTemperature + "C";
        iconToShow = temperature;
        iconSize = sizeof(temperature);
    } else { // Humidity
        valueToShow = currentHumidity;
        iconToShow = humidity;
        iconSize = sizeof(humidity);
    }

    // Calculate Y position for smooth animation
    int startY = 24;
    int endY = 8;
    int currentY = (elapsedTime < animDuration) ? (startY - ((float)elapsedTime / animDuration) * (startY - endY)) : endY;

    // Draw the icon first
    TJpgDec.drawJpg(88, 89, iconToShow, iconSize);

    // Then draw the text sprite next to it
    clkb.setColorDepth(8);
    clkb.loadFont(ALBB10);
    clkb.createSprite(40, 16);
    clkb.fillSprite(COLOR_BG);
    clkb.setTextDatum(ML_DATUM);
    clkb.setTextColor(color_TH_Font, COLOR_BG);
    clkb.drawString(valueToShow, 1, currentY); // Draw at calculated position
    
    // *** FIX: Push the sprite to the CORRECT location, next to the icon ***
    clkb.pushSprite(105, 89); 

    clkb.deleteSprite();
    clkb.unloadFont();

    if (elapsedTime >= animDuration) {
        // Animation finished
        tempHumidAnimState = ANIM_IDLE;
    }
}

void displayFollowerInfo() {
    // This is static info, drawn once at startup or after screen clear
    TJpgDec.drawJpg(1, 108, ccmelogo, sizeof(ccmelogo));
    clk.createSprite(104, 18);
    clk.fillSprite(COLOR_BG);
    clk.loadFont(ALBB12);
    clk.setTextDatum(MC_DATUM);
    clk.setTextColor(COLOR_MOTTO, COLOR_BG);
    clk.drawString(motto, 52, 9);
    clk.pushSprite(20, 108);
    clk.deleteSprite();
    clk.unloadFont();
}

void updateLVGL() {
    lv_timer_handler();
}

//==================================================================================
//                     HELPERS & CALLBACKS (Unchanged)
//==================================================================================

String getWeekString() {
    String days[] = {"日", "一", "二", "三", "四", "五", "六"};
    return "周" + days[weekday() - 1];
}

String getMonthDayString() {
    return String(month()) + "月" + String(day()) + "日";
}

String getHourMinuteString() {
    return formatDigits(hour()) + ":" + formatDigits(minute());
}

String formatDigits(int digits) {
    if (digits < 10) return "0" + String(digits);
    return String(digits);
}

time_t getNtpTime() {
    IPAddress ntpServerIP;
    if(!WiFi.hostByName(NTP_SERVER, ntpServerIP)) {
      Serial.println("NTP host lookup failed.");
      return 0;
    }
    
    sendNTPpacket(ntpServerIP);

    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = wifiUdp.parsePacket();
        if (size >= 48) {
            Serial.println("NTP response received, time synchronized!");
            byte packetBuffer[48];
            wifiUdp.read(packetBuffer, 48);
            unsigned long secsSince1900 = (unsigned long)packetBuffer[40] << 24 | (unsigned long)packetBuffer[41] << 16 | (unsigned long)packetBuffer[42] << 8 | (unsigned long)packetBuffer[43];
            return secsSince1900 - 2208988800UL + TIME_ZONE * 3600;
        }
    }

    Serial.println("NTP sync failed (timeout).");
    return 0; 
}

void sendNTPpacket(IPAddress &address) {
    byte packetBuffer[48];
    memset(packetBuffer, 0, 48);
    packetBuffer[0] = 0b11100011; packetBuffer[2] = 6; packetBuffer[3] = 0xEC;
    packetBuffer[12] = 49; packetBuffer[13] = 0x4E; packetBuffer[14] = 49; packetBuffer[15] = 52;
    wifiUdp.beginPacket(address, 123);
    wifiUdp.write(packetBuffer, 48);
    wifiUdp.endPacket();
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}