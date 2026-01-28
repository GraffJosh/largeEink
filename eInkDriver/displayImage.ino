/*This is a 4-color electronic ink screen, which can only display 4 colors. 

Here is the 4 colors you can display:
1.TFT_GRAY_0    black
2.TFT_GRAY_1      |
3.TFT_GRAY_2      |
4.TFT_GRAY_3    white

*/
#include <WiFi.h>
#include <HTTPClient.h>
#include "TFT_eSPI.h"
#include "driver.h"
#include <Preferences.h>
#include "secrets.h"

Preferences prefs;
// #include "image.h"
// #ifdef EPAPER_ENABLE  // Only compile this code if the EPAPER_ENABLE is defined in User_Setup.h
EPaper epaper;
// #endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

const char* url1 = URL1;
const char* url2 = URL2;
const char* url3 = URL3;


#define SRC_W 800
#define SRC_H 480
#define DST_W 800
#define DST_H 480

#define BYTES_PER_ROW (DST_W / 2)
#define IMAGE_SIZE (BYTES_PER_ROW * DST_H)

// 256-entry lookup table
uint8_t gammaLUT[256];
uint16_t updateCount = 0;
uint16_t imageCount = 0;
uint8_t displayIndex = 0;
bool freshBoot = true;
bool button1Pressed = false;
bool button2Pressed = false;
bool button3Pressed = false;
bool deepsleepEnabled = false;

#define BATTERY_PIN 1       // GPIO1 (A0) - BAT_ADC
#define ADC_EN_PIN 6        // GPIO6 (A5) - ADC_EN

const float CALIBRATION_FACTOR = 0.968;

const int BUTTON_D1 = D1;  // First user button
const int BUTTON_D2 = D2;  // Second user button
const int BUTTON_D4 = D4;  // Third user button

struct buttonState{
    bool button1;
    bool button2;
    bool button3;
};
volatile buttonState ButtonStates,LastButtonStates;

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
};
int getDisplayIndex()
{
    return prefs.getInt("displayIndex");
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

void IRAM_ATTR pollButtons()
{

    // Read button states (buttons are LOW when pressed because of pull-up resistors)
    ButtonStates.button1 = !digitalRead(BUTTON_D1);
    ButtonStates.button2 = !digitalRead(BUTTON_D2);
    ButtonStates.button3 = !digitalRead(BUTTON_D4);

    if(ButtonStates.button1 && !LastButtonStates.button1)
    {
        button1Pressed = true;
        // incrementDisplayIndex();
    }
    if(ButtonStates.button3)
    {
        button3Pressed = true;
        // decrementDisplayIndex();
    }
}

float readBatteryVoltage() {
  // Enable ADC
  digitalWrite(ADC_EN_PIN, HIGH);
  delay(10);  // Short delay to stabilize
  
  // Read 30 times and average for more stable readings
  long sum = 0;
  for(int i = 0; i < 30; i++) {
    sum += analogRead(BATTERY_PIN);
    delayMicroseconds(100);
  }
  
  // Disable ADC to save power
  digitalWrite(ADC_EN_PIN, LOW);
  
  // Calculate voltage
  float adc_avg = sum / 30.0;
  float voltage = (adc_avg / 4095.0) * 3.6 * 2.0 * CALIBRATION_FACTOR;
  
// Determine battery level
    BatteryState batteryStatus;
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
  
    // Serial.printf("Battery Voltage: %f\n",voltage);
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
    Serial.println("Starting HTTP GET (stream mode)");
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
    }else{
        Serial.printf("Check for new image: HTTP size: %d\n", http.getSize());
    }

    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        Serial.println("Failed to get stream");
        http.end();
        return;
    }

    // // Allocate 2-bit-per-pixel buffer (still needed)
    // if (!quadGrayscaleImage) {
    //   Serial.println("Failed to allocate 2-bit buffer");
    //   http.end();
    //   return;
    // }

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

    Serial.printf("Skipped to pixel data (offset %lu)\n", bfOffBits);


    // Process row by row
    uint16_t rowStride = (SRC_W + 3) & ~3; // BMP row padded to 4 bytes
    uint16_t padding = rowStride - SRC_W;
    // Row buffer
    static uint8_t grayRow[SRC_W];
    static uint8_t row4bpp[DST_W/2];
    epaper.fillScreen(TFT_GRAY_3);
    for (int y = 0; y < DST_H; y++) {
        // 1. Read one BMP row into grayRow[SRC_W]
        stream->readBytes(grayRow, SRC_W);

        // 2. Convert 2x8-bit → 2x4-level → 4bpp packed
        for (int x = 0; x < DST_W; x += 2) {
            uint8_t level0 = gammaLUT[grayRow[x]];       // first pixel
            uint8_t level1 = gammaLUT[grayRow[x + 1]];   // second pixel

            uint8_t packed = (level0 << 4) | (level1 & 0x0F); // pack 2 pixels into 1 byte

            row4bpp[x / 2] = packed;
        }


        // 3. Push this row to the display    // Push to display **in reversed y order**
        epaper.pushImage(0, DST_H - 1 - y, DST_W, 1, (uint16_t*) row4bpp, 4);
        if (y % 50 == 0) Serial.printf("Processed row %d\n", y);

    }

    Serial.println("Full Update!");
    epaper.update();
    updateCount++;
    prefs.putInt("imageCount",imageCount);

    Serial.println("BMP streamed and converted to 4-bit buffer");
    if (client.connected()) {
        http.end();  // safe
    } else {
        Serial.println("Client disconnected, skipping http.end()");
    }
}

bool checkForNewImage(const char* url) {
    
    HTTPClient http;
    WiFiClient client;
    Serial.println("Checking Server for new image.");
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
        return false;
    }else{
        Serial.printf("Expected bytes: %d\n", (1));
        Serial.printf("HTTP size: %d\n", http.getSize());
    }
    char* imageCountStr = (char*) malloc(http.getSize());
    for (int i = 0;i<http.getSize();i++) {
        imageCountStr[i] = client.read();
    }
    int serverImageCount = atoi(imageCountStr);
    
    Serial.printf("Checked for new image, received %s\n",(serverImageCount != imageCount)?"New Image Ready":"No New Image Ready");
    if (client.connected()) {
        http.end();  // safe
    } else {
        Serial.println("Client disconnected, skipping http.end()");
    }
    free(imageCountStr);
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
void initWifi()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Wifi Initialized.");
}
void setup() {

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

    Serial.begin(115200);
    setupGammaLUT(.4);
    epaper.begin();
    delay(500);
    epaper.initGrayMode(GRAY_LEVEL4);
    delay(500);
    Serial.println("Epaper initialized");
    
    pinMode(BUTTON_D1, INPUT_PULLUP);
    pinMode(BUTTON_D2, INPUT_PULLUP);
    pinMode(BUTTON_D4, INPUT_PULLUP);
    
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_D1),
        pollButtons,
        FALLING   // or RISING / CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_D2),
        pollButtons,
        FALLING   // or RISING / CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_D4),
        pollButtons,
        FALLING   // or RISING / CHANGE
    );

    // Configure ADC_EN
    pinMode(ADC_EN_PIN, OUTPUT);
    digitalWrite(ADC_EN_PIN, LOW);  // Start with ADC disabled to save power
    
    // Configure ADC
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    Serial.println("GPIO initialized");

    prefs.begin("myapp", false);   // namespace, read/write
    imageCount = prefs.getInt("imageCount");
    displayIndex = getDisplayIndex();
    deepsleepEnabled = prefs.getBool("deepsleepEnabled");
    Serial.printf("Image Count: %d, display index: %d\n",imageCount,displayIndex);

    initWifi();
  
}
uint32_t loopCounter = 0;
const uint32_t period = 10;
const uint32_t seconds = 1000/period;

void loop() {
    // if(!(loopCounter % 2))
    // {
    //     pollButtons();
    // }
    if(!(loopCounter % 30*seconds) || freshBoot)
    {
        if(readBatteryVoltage() < 4.0)
        {
            deepsleepEnabled = true;
        }
    }
    if(!(loopCounter % (60*seconds)) || getDisplayIndex() != displayIndex || freshBoot )
    {
        switch(getDisplayIndex())
        {
            case PAGE_IMAGE:
                if(checkForNewImage(url3) || getDisplayIndex() != displayIndex )
                {
                    downloadAndConvertBMP(url2);
                }
                break;
            case PAGE_HOMEASSISTANT:
                downloadAndConvertBMP(url1);
                break;
            default:
                Serial.printf("Invalid page\n");
                break;
        }
        displayIndex = getDisplayIndex();
    }
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
    if(loopCounter % 20*seconds && deepsleepEnabled)
    {
        #define WAKE_MASK ((1ULL << BUTTON_D1) | (1ULL << BUTTON_D2) | 1ULL << BUTTON_D4)
        esp_sleep_enable_ext1_wakeup(WAKE_MASK, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_sleep_enable_timer_wakeup(5ULL * 60ULL * 1000000ULL);
        esp_deep_sleep_start();
    }
  delay(period);
  if(freshBoot)
  {
    freshBoot = false;
  }
  loopCounter++;
}

