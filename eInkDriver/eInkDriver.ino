#include <WiFi.h>
#include <HTTPClient.h>
#include "TFT_eSPI.h"
#include "driver.h"
#include <Preferences.h>
#include "secrets.h"

Preferences prefs;
EPaper epaper;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const char* url1 = URL1;
const char* url2 = URL2;
const char* url3 = URL3;

const uint8_t ROTATION = 2; // Rotation of 2 rotates 180

#define SRC_W 800
#define SRC_H 480
#define DST_W 800
#define DST_H 480

#define BYTES_PER_ROW (DST_W / 2)
#define IMAGE_SIZE (BYTES_PER_ROW * DST_H)

// 256-entry lookup table
static uint8_t gammaLUT[256];
uint32_t lastUpdateMs = 0, startupTime=0;
uint16_t updateCount = 0;
uint32_t imageCount = 0;
uint8_t displayIndex = 0;
bool freshBoot = true;
bool deepsleepEnabled = false;
uint16_t deepSleepDurationSeconds = 300;

const float CALIBRATION_FACTOR = 0.968;
#define BATTERY_PIN 1       // GPIO1 (A0) - BAT_ADC
#define ADC_EN_PIN 6        // GPIO6 (A5) - ADC_EN

const int BUTTON_D1 = D1;  // First user button
const int BUTTON_D2 = D2;  // Second user button
const int BUTTON_D4 = D4;  // Third user button
volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
volatile bool button3Pressed = false;

enum PageSelection{
    PAGE_IMAGE,
    PAGE_HOMEASSISTANT,
    PAGE_NUMPAGES
};

enum BatteryState{
    BATT_FULL,
    BATT_GOOD,
    BATT_MEDIUM,
    BATT_LOW,
    BATT_CRITICAL,
}batteryStatus;

/*
 * Display index handles which page we're currently displaying. 
 * These helpers ensure that the value is stored in persistent memory
 * (to reduce boot up updates)
 */
int getDisplayIndex()
{
    return prefs.getInt("displayIndex",PAGE_HOMEASSISTANT);
}
void setDisplayIndex(PageSelection index)
{
    prefs.putInt("displayIndex",(int) index);
}
void setDisplayIndex(int index)
{
    prefs.putInt("displayIndex", index);
}
void incrementDisplayIndex()
{
    int next_page = (getDisplayIndex()+1);
    if(next_page == PAGE_NUMPAGES)
    {
        next_page = 0;
    }
    setDisplayIndex(next_page);
    Serial.printf("Increment Page to: %d\n",(int)next_page);
}
void decrementDisplayIndex()
{
    int next_page = (getDisplayIndex()-1);
    if(next_page < 0)
    {
        next_page = PAGE_NUMPAGES - 1;
    }
    setDisplayIndex(next_page);
    Serial.printf("Decrement Page to: %d\n",(int)next_page);
}

/*
 * Helper functions to compute time for deepsleep
 */
constexpr uint64_t deepSleepSeconds(uint64_t s) {
  return s * 1000000ULL;
}
constexpr uint64_t deepSleepMinutes(uint64_t m) {
  return deepSleepSeconds(m * 60ULL);
}

//ISR for handling buttons
void IRAM_ATTR button1ISR()
{    button1Pressed = true;}
void IRAM_ATTR button2ISR()
{    button2Pressed = true;}
void IRAM_ATTR button3ISR()
{    button3Pressed = true;}

#define ADC_SAMPLES 30
#define ADC_COUNTS 4095.0
#define ADC_REF_VOLTAGE 3.6
#define VOLTAGE_DIVIDER 2.0
float readBatteryVoltage() {
    // Enable ADC
    digitalWrite(ADC_EN_PIN, HIGH);
    delay(10);  // Short delay to stabilize

    // Read 30 times and average for more stable readings
    long sum = 0;
    for(int i = 0; i < ADC_SAMPLES; i++) {
        sum += analogRead(BATTERY_PIN);
        delayMicroseconds(100);
    }

    // Disable ADC to save power
    digitalWrite(ADC_EN_PIN, LOW);

    // Calculate voltage
    float adc_avg = sum / (ADC_SAMPLES*1.0);
    float voltage = (adc_avg / ADC_COUNTS) * ADC_REF_VOLTAGE * VOLTAGE_DIVIDER * CALIBRATION_FACTOR;

    // Determine battery level
    if (voltage >= 4.0) {
        batteryStatus = BATT_FULL;
    } else if (voltage >= 3.7) {
        batteryStatus = BATT_GOOD;
    } else if (voltage >= 3.5) {
        batteryStatus = BATT_MEDIUM;
    } else if (voltage >= 3.2) {
        batteryStatus = BATT_LOW;
    } else {
        batteryStatus = BATT_CRITICAL;
    }
    return voltage;
}
// Call this once in setup() to initialize the LUT
void setupGammaLUT(float gamma = 0.4) {
    for (int i = 0; i < 256; i++) {
        float normalized = i / 255.0f;             // 0..1
        float corrected = pow(normalized, 1.0f / gamma);  // gamma correction
        uint8_t level = corrected * 4;             // 0..3
        if (level > 3) level = 3;                  // clamp
        gammaLUT[i] = level;
    }
}

void downloadAndConvertBMP(const char* url) {
    HTTPClient http;
    WiFiClient client;
    Serial.println("Starting BMP Download and Conversion.");
    http.setTimeout(30000);  // 30 seconds
    // http.setBufferSize(1024);  // or smaller
    http.setReuse(false);
    http.begin(client, url);
    int code = http.GET();
    if (code != 200) {
        Serial.printf("HTTP GET failed: %d\n", code);
        Serial.printf("Content-Length: %d\n", http.getSize());
        Serial.printf("Transfer-Encoding: %s\n", http.header("Transfer-Encoding").c_str());
        http.end();
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        Serial.println("Failed to get stream");
        http.end();
        return;
    }

    uint8_t junk[32];
    // Read BITMAPFILEHEADER (14 bytes)
    uint8_t fileHeader[14];
    if (stream->readBytes(fileHeader, 14) != 14) {
        Serial.println("Failed to read BMP file header");
        return;
    }

    // Extract pixel data offset
    uint32_t bfOffBits = *(uint32_t*)&fileHeader[10];

    // We already consumed 14 bytes
    uint32_t toSkip = bfOffBits - 14;

    // Skip DIB header + palette (whatever size)
    while (toSkip > 0) {
        uint16_t n = stream->readBytes(junk, min(toSkip, (uint32_t)sizeof(junk)));
        if (n == 0) {
            Serial.println("Failed skipping BMP header/palette");
            return;
        }
        toSkip -= n;
    }

    // Process row by row
    uint16_t rowStride = (SRC_W + 3) & ~3; // BMP row padded to 4 bytes
    uint16_t padding = rowStride - SRC_W;
    // Row buffer
    static uint8_t grayRow[SRC_W];
    static uint8_t row4bpp[DST_W/2];
    // Clear the buffer.
    epaper.fillScreen(TFT_GRAY_3);
    // Read the BMP Image row by row, starting from the bottom. 
    for (int y = 0; y < DST_H; y++) {
        // 1. Read one BMP row into grayRow[SRC_W]
        stream->readBytes(grayRow, SRC_W);
        
        /* 2. Convert 2x8-bit → 2x4-level → 4bpp packed
        *      This is confusing, but it is a proprietary (and perhaps industry) standard for
        *      packing images for the Eink. The library mandates 4 bits per pixel, even though 
        *      it is only 2 bits of information (4 levels of gray)
        */
       for (int x = 0; x < DST_W; x += 2) {
           uint8_t level0 = gammaLUT[grayRow[x]];       // first pixel
           uint8_t level1 = gammaLUT[grayRow[x + 1]];   // second pixel
           uint8_t packed = (level0 << 4) | (level1 & 0x0F); // pack 2 pixels into 1 byte
           row4bpp[x / 2] = packed;
        }
        
        // 3. Push this row to the display in reversed y order
        epaper.pushImage(0, DST_H - 1 - y, DST_W, 1, (uint16_t*) row4bpp, 4);
        if (y % 100 == 0) Serial.printf("Processed row %d\n", y);
        
        // 4. Skip padding. This is poorly tested, because most rows aren't padded.
        if (padding) {
            stream->readBytes(junk, padding);
        }
    }

    //Write to the display
    epaper.update();
    //This var is for future use if we ever succeed with partial updates.
    updateCount++;
    // Write persistent memory with image count, indicating that we've successfully displayed the image.
    prefs.putInt("imageCount",imageCount);

    Serial.println("BMP streamed and converted to 4-bit buffer");
    if (client.connected()) {
        http.end();  // safe
    } else {
        Serial.println("Client disconnected, skipping http.end()");
    }
}

/*
 * Ping a server to get the image index, which should increment every time the image on the server updates.
 * This pairs with the custom image server written for this device.
 * Returns True if the local image counter doesn't match the server image counter.
 * Retruns False if the local image counter matches the server's image counter (indicating no new image)
 */

bool checkForNewImage(const char* url) {
    
    HTTPClient http;
    WiFiClient client;
    Serial.println("Checking Server for new image.");
    http.setTimeout(30000);  // 30 seconds
    // http.setBufferSize(1024);  // or smaller
    http.setReuse(false);
    http.begin(client, url);
    int code = http.GET();
    int len = http.getSize();
    if (code != 200) {
        Serial.printf("HTTP GET failed: %d\n", code);
        Serial.printf("Content-Length: %d\n", len);
        Serial.printf("Transfer-Encoding: %s\n", http.header("Transfer-Encoding").c_str());

        http.end();
        return false;
    }else{
        Serial.printf("Expected bytes: %d\n", (1));
        Serial.printf("HTTP size: %d\n", len);
    }

    /*
     * Read from the http client what should be a few bytes of a number indicating which image we're on.
     */
    if (len <= 0 || len > 16)
    {
        return false;
    }
    char imageCountStr[16] = {0};
    client.readBytes(imageCountStr, len);
    int serverImageCount = atoi(imageCountStr);
    
    Serial.printf("Checked for new image, received %s\n",(serverImageCount != imageCount)?"New Image Ready":"No New Image Ready");
    if (client.connected()) {
        http.end();  // safe
    } else {
        Serial.println("Client disconnected, skipping http.end()");
    }

    if (serverImageCount != imageCount)
    {
        imageCount = serverImageCount;
        Serial.printf("New image is ready at the server.\n");
        return true;
    }
    else
    {
        return false;
    }

}
// Broke this out into a helper in case I ever want to light-sleep
void initWifi()
{
    uint32_t start = millis();
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > 15000) {
            initDeepSleep();
        }
    }
    Serial.println("Wifi Initialized.");
}
void initDeepSleep()
{
    #define WAKE_MASK ((1ULL << BUTTON_D1) | (1ULL << BUTTON_D2) | 1ULL << BUTTON_D4)
    esp_sleep_enable_ext1_wakeup(WAKE_MASK, ESP_EXT1_WAKEUP_ALL_LOW);
    esp_sleep_enable_timer_wakeup(deepSleepSeconds(deepSleepDurationSeconds));
    esp_deep_sleep_start();
}
void setup() {

    /*
     * Review why we are now waking up. If a button woke us up from a deepsleep,
     * let's try to do the action that was requested.
     * This is poorly tested.
     */
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    uint64_t wakePins = esp_sleep_get_ext1_wakeup_status();

    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT1:

            if (wakePins & (1ULL << BUTTON_D1)) 
            {
                Serial.println("Woke: BUTTON_D1");
                button1Pressed = true;
            }
            if (wakePins & (1ULL << BUTTON_D2)) 
            {
                Serial.println("Woke: BUTTON_D2");
                button2Pressed = true;
            }
            if (wakePins & (1ULL << BUTTON_D4)) 
            {
                Serial.println("Woke: BUTTON_D4");
                button3Pressed = true;
            }
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Woke from timer");
            break;
        default:
            Serial.println("Cold boot");
    }

    /*
     * It is important to initialize the eInk display before the WiFi. 
     * I think it has to do with memory allocation during wifi init, but I'm not sure;
     * if you try to init wifi before epaper-grayscale, the board will crash.
     */
    Serial.begin(115200);
    setupGammaLUT(.4);
    epaper.begin();
    delay(500);
    epaper.initGrayMode(GRAY_LEVEL4);
    epaper.setRotation(ROTATION);  // Landscape
    delay(500);
    Serial.println("Epaper initialized");
    
    /*
     * Okay, now we can configure the GPIO, including interrupt polling for the buttons.
     */
    pinMode(BUTTON_D1, INPUT_PULLUP);
    pinMode(BUTTON_D2, INPUT_PULLUP);
    pinMode(BUTTON_D4, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_D1),
        button1ISR,
        FALLING   // or RISING / CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_D2),
        button2ISR,
        FALLING   // or RISING / CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_D4),
        button3ISR,
        FALLING   // or RISING / CHANGE
    );

    // Configure the battery management GPIO
    // Configure ADC_EN
    pinMode(ADC_EN_PIN, OUTPUT);
    digitalWrite(ADC_EN_PIN, LOW);  // Start with ADC disabled to save power
    
    // Configure ADC
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    Serial.println("GPIO initialized");

    // Configure persistent memory. Not clear this is working perfectly (check the image count value.)
    prefs.begin("myapp", false);   // namespace, read/write
    imageCount = prefs.getInt("imageCount");
    displayIndex = getDisplayIndex();
    deepsleepEnabled = prefs.getBool("deepsleepEnabled");
    Serial.printf("Image Count: %d, display index: %d\n",imageCount,displayIndex);

    // Initialize Wifi.
    initWifi();
    startupTime = millis();
}

// If we're not asleep, let's run stuff at varying frequency.
const uint32_t period = 10;
const uint32_t ticksPerSecond = 1000/period;
void handleButtons(){
    /*
     * These booleans are set in ISR that handle button presses. 
     * If a button was pressed, let's do the action here.
     */
    if(button1Pressed)
    {
        incrementDisplayIndex();
        button1Pressed = false;
    }
    if(button2Pressed)
    {
        decrementDisplayIndex();
        button2Pressed = false;
    }
    if(button3Pressed)
    {
        decrementDisplayIndex();
        button3Pressed = false;
    }
}


/* Update the display:
 *  Every sixty seconds.
 *  Or if the page just updated
 *  Or if we just booted.
 */ 
bool shouldUpdateDisplay()
{
    if (freshBoot) return true;
    if (getDisplayIndex() != displayIndex) return true;
    if ((millis() - lastUpdateMs) > deepSleepDurationSeconds*ticksPerSecond) return true;
    return false;
}

/*
 * After twenty seconds awake, let's go to sleep (if other conditions have 
 * enabled deepsleep). At writing, this happens if the battery is not at full.
 */
bool shouldDeepSleep()
{
    if (!deepsleepEnabled) return false;
    if (millis() < 20*ticksPerSecond) return false;
    return true;
}
void updateDisplay()
{
    if(readBatteryVoltage() < 4.0)
    {
        deepsleepEnabled = true;
    }
    switch(getDisplayIndex())
    {
        // Locally hosted image display.
        case PAGE_IMAGE:
            // Check if the image has updated on the server.
            if(checkForNewImage(url3) || getDisplayIndex() != displayIndex )
            {
                // Download and display that BMP.
                downloadAndConvertBMP(url2);
            }
            deepSleepDurationSeconds = 1200;
            break;
        case PAGE_HOMEASSISTANT:
            // Go get a URL (homeassistant page from Puppet)
            downloadAndConvertBMP(url1);
            deepSleepDurationSeconds = 200;
            break;
        default:
            // How did I get here? How do I work this?
            setDisplayIndex(PAGE_IMAGE);
            Serial.printf("Invalid page\n");
            break;
    }
    displayIndex = getDisplayIndex();
    lastUpdateMs = millis();
}
void loop() {
    handleButtons();
    if(shouldUpdateDisplay()) updateDisplay();
    if(shouldDeepSleep()) initDeepSleep();
    // The freshBoot variable makes sure we update on our first pass through if we just started.
    if(freshBoot)
    {
        freshBoot = false;
    }
    delay(period);
}

