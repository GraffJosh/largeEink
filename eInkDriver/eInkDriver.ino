#include <WiFi.h>
#include <HTTPClient.h>
#include "TFT_eSPI.h"
#include "driver.h"
#include <Preferences.h>
#include "secrets.h"
#include "icons.h"

#include<PubSubClient.h>
#include<HaMqttEntities.h>



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
static const uint8_t bayer8[8][8] = {
 { 0,48,12,60, 3,51,15,63},
 {32,16,44,28,35,19,47,31},
 { 8,56, 4,52,11,59, 7,55},
 {40,24,36,20,43,27,39,23},
 { 2,50,14,62, 1,49,13,61},
 {34,18,46,30,33,17,45,29},
 {10,58, 6,54, 9,57, 5,53},
 {42,26,38,22,41,25,37,21}
};

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

const int BUTTON_1_PIN = D1;  // First user button
const int BUTTON_2_PIN = D2;  // Second user button
const int BUTTON_3_PIN = D4;  // Third user button
volatile bool button1Pressed = false;
volatile bool button2Pressed = false;
volatile bool button3Pressed = false;
#define BUTTON_DEBOUNCE_TIME_MS 250

WiFiClient client;
PubSubClient mqtt_client(client);


enum PageSelection{
    PAGE_IMAGE,
    PAGE_HOMEASSISTANT,
    PAGE_AUX_PAGE,
    PAGE_NUMPAGES
};
const char * select_options[PAGE_NUMPAGES] PROGMEM = {
     "Image Page",
     "HA Page",
     "Aux Page"
     };
     
const char * page_urls[PAGE_NUMPAGES] PROGMEM = {
     URL2,
     URL1,
     URL4,
     };

enum BatteryState{
    BATT_FULL,
    BATT_GOOD,
    BATT_MEDIUM,
    BATT_LOW,
    BATT_CRITICAL,
}batteryStatus;

#define HA_DEVICE_ID            "smart_display_5"
#define HA_DEVICE_FRIENDLY_NAME "7.5in Epaper Smart Display"

#define ENTITIES_COUNT 5
HADevice ha_device = HADevice(HA_DEVICE_ID,HA_DEVICE_FRIENDLY_NAME,"1.0");
// HANumber haPageNumber = HANumber("page_num","Page Number",ha_device,1,100,1);
HASelect haPageSelect = HASelect("page_select","Page Select",ha_device,PAGE_NUMPAGES,select_options);
HAText haURL = HAText("aux_url","Aux URL",ha_device,150);
HAButton haRefreshButton = HAButton("refresh_button","Refresh Page",ha_device);
HASensorNumeric haBatteryVoltage = HASensorNumeric("battery_voltage","Battery Voltage",ha_device,"V",2);
HASensorNumeric haBatteryState = HASensorNumeric("battery_state","Battery State",ha_device,"%",0);
// HASensorBinary haDeepSleepActive = HASensorBinary("deep_sleep_active","Deep Sleep Active");
// HASensorBinary haDeviceOnline = HASensorBinary("device_online","Device Online");


int getPageIndexFromSelect(const char* receivedSelectState,const char** list, uint16_t listSize)
{
    for(int index=0;index<listSize;index++)
    {
        if(!strcmp(list[index],receivedSelectState))
        {
            return index;
        }
    }
    return -1;
}

/* Callback from HA-MQTT entities. It is called when an entity changes its state.

    Entity can be NULL when the received topic is not related to any entity.
    This is useful to handle other topics with the same mqtt client.
*/
void ha_callback(HAEntity *entity, char *topic, byte *payload, unsigned int length){
    // if(entity == &haPageNumber){
    //     Serial.printf("Changed number state to %d \n",haPageNumber.getState());
    // }
    // else 
    if(entity == &haPageSelect)
    {
        setDisplayIndex(haPageSelect.getState());
    }
    else if(entity == &haRefreshButton)
    {
        Serial.printf("Refresh Button Pressed\n");
        freshBoot = true;
    }
    else if(entity == &haURL)
    {
        String prefix(HA_URL_PREFIX);
        String suffix(HA_URL_SUFFIX);
        String data(haURL.getState());
        static String auxURL = (prefix + data + suffix);
        page_urls[PAGE_AUX_PAGE] = auxURL.c_str();
        Serial.printf("Loading new Aux URL: %s\n",page_urls[PAGE_AUX_PAGE]);
        haURL.setState(data.c_str());
        setDisplayIndex(PAGE_AUX_PAGE);
        freshBoot = true;
    }
    else
        Serial.println("Callback called from other subscription");

}

/*
 * Display index handles which page we're currently displaying. 
 * These helpers ensure that the value is stored in persistent memory
 * (to reduce boot up updates)
 */
int getDisplayIndex()
{
    return prefs.getInt("displayIndex",PAGE_HOMEASSISTANT);
}
void setDisplayIndex(const char* receivedSelectState)
{
    Serial.printf("Select state: %s\n",receivedSelectState);
    int pageIndex = getPageIndexFromSelect(receivedSelectState,select_options,PAGE_NUMPAGES);
    if(pageIndex == -1)
    {
        Serial.printf("Failed to extract page index from state?\n");
    }
    else
    {
        setDisplayIndex(pageIndex);
        Serial.printf("Page Index updated to: %d\n",pageIndex);
    }
}
void setDisplayIndex(PageSelection index)
{
    setDisplayIndex((int) index);
}
void setDisplayIndex(int index)
{
    prefs.putInt("displayIndex", index);
    haPageSelect.setState(select_options[index]);
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
{    
    button1Pressed = true; 
}
void IRAM_ATTR button2ISR()
{    
    button2Pressed = true; 
}
void IRAM_ATTR button3ISR()
{    
    button3Pressed = true; 
}

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
    haBatteryVoltage.setState(voltage);
    haBatteryState.setState(map(voltage,4.2,3.1,100,0));
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
// Nominal centers of each gray level (tune if needed)
#define G0  0     // black
#define G1  85    // dark gray
#define G2  170   // light gray
#define G3  255   // white

#define SNAP_THRESH 1   // +/- range to lock in (tune this!)
#define DITHER_STRENGTH 85

uint8_t ditheredLevel(uint8_t gray, int x, int y)
{
    // ----- PHASE 1: SNAP INPUT VALUES TO KNOWN-GOOD LEVELS -----
    // If the incoming pixel value is already very close to one of the
    // canonical gray centers (black, dark gray, light gray, white),
    // we lock it to that level immediately and skip dithering entirely.
    //
    // This prevents dithering noise in flat areas like text, icons,
    // UI backgrounds, and large uniform regions.
    if (abs(gray - G0) <= SNAP_THRESH) return 0;  // Near black → force black
    if (abs(gray - G1) <= SNAP_THRESH) return 1;  // Near dark gray → force level 1
    if (abs(gray - G2) <= SNAP_THRESH) return 2;  // Near light gray → force level 2
    if (abs(gray - G3) <= SNAP_THRESH) return 3;  // Near white → force white

    // ----- PHASE 2: ORDERED DITHER THRESHOLD LOOKUP -----
    // Look up a value from the 8×8 Bayer matrix using the pixel's (x, y)
    // coordinates. The "(x + y)" term decorrelates columns so that
    // vertical banding does not appear on the e-ink panel.
    //
    // The Bayer matrix values are in the range 0–63.
    int t = bayer8[y & 7][(x + y) & 7];

    // Shift the Bayer value so it is centered around zero.
    // This converts the range:
    //     0–63  →  -32…+31
    //
    // Centering is important so the dither adds *positive and negative*
    // bias evenly around the original pixel value.
    t -= 32;

    // ----- PHASE 3: APPLY DITHER AS A SMALL BIAS -----
    // Each output gray level is separated by ~85 (255 / 3).
    // We scale the dither so it nudges the pixel by roughly ±½
    // of one quantization step (~±42).
    //
    // This is enough to push pixels across level boundaries
    // without creating visible structure or banding.
    int adjusted = gray + (t * 85) / 64;

    // Clamp the adjusted value to the valid 8-bit grayscale range
    // to prevent underflow or overflow.
    if (adjusted < 0)   adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    // ----- PHASE 4: SNAP AGAIN AT THE EXTREMES -----
    // After dithering, the adjusted value might drift slightly
    // into or out of the black/white regions. We re-snap here
    // to ensure that true blacks and whites remain stable and
    // do not acquire dither noise at their edges.
    if (adjusted <= G0 + SNAP_THRESH) return 0;  // Force black
    if (adjusted >= G3 - SNAP_THRESH) return 3;  // Force white

    // ----- PHASE 5: FINAL QUANTIZATION -----
    // Convert the final 8-bit grayscale value into one of the
    // 4 e-ink gray levels (0–3) using the gamma correction LUT.
    //
    // This LUT handles panel-specific response nonlinearity.
    return gammaLUT[adjusted];
}

void downloadAndConvertBMP(const char* url) {
    
    HTTPClient http;
    Serial.println("Starting BMP Download and Conversion.");
    http.setTimeout(30000);  // 30 seconds
    // http.setBufferSize(1024);  // or smaller
    http.setReuse(false);
    http.setRedirectLimit(10);
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
    static uint8_t level0 = 0;
    static uint8_t level1 = 0;
    static uint8_t packed = 0;
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
        //    uint8_t level0 = gammaLUT[grayRow[x]];       // first pixel
        //    uint8_t level1 = gammaLUT[grayRow[x + 1]];   // second pixel
        //    uint8_t packed = (level0 << 4) | (level1 & 0x0F); // pack 2 pixels into 1 byte
        //    row4bpp[x / 2] = packed;

            level0 = ditheredLevel(grayRow[x],     x,     y);
            level1 = ditheredLevel(grayRow[x + 1], x + 1, y);

            packed = (level0 << 4) | (level1 & 0x0F);
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
    addBatteryIndicator();
    //Write to the display
    epaper.update();
    
    // updateScreenInChunks(128);
    //This var is for future use if we ever succeed with partial updates.
    updateCount++;
    // Write persistent memory with image count, indicating that we've successfully displayed the image.
    prefs.putInt("imageCount",imageCount);

    while (stream->available()) {
        uint8_t tmp[256];
        stream->readBytes(tmp, min(stream->available(), (int)sizeof(tmp)));
    }

    Serial.println("BMP streamed and converted to 4-bit buffer");
    http.end();  // safe
}
void updateScreenInChunks(int chunkSize) {
    for (int y = 0; y < DST_H; y += chunkSize) {
        for (int x = 0; x < DST_W; x += chunkSize) {

            int w = chunkSize;
            int h = chunkSize;

            // Clamp width/height at screen edges
            if (x + w > DST_W) w = DST_W - x;
            if (y + h > DST_H) h = DST_H - y;

            epaper.updataPartial(x, y, w, h);
        }
    }
}

void addBatteryIndicator()
{
    uint16_t iconHeight=50,iconWidth = 50;
    uint16_t* icon;
    switch(batteryStatus){
        // case BATT_FULL:
        //     iconWidth = battery_charging_iconWidth;
        //     iconHeight = battery_charging_iconHeight;
        //     icon = (uint16_t*) battery_charging;
        //     break;
        // case BATT_GOOD:
        //     iconWidth = battery_90_iconWidth;
        //     iconHeight = battery_90_iconHeight;
        //     icon = (uint16_t*) battery_90;
        //     break;
        // case BATT_MEDIUM:
        //     iconWidth = battery_50_iconWidth;
        //     iconHeight = battery_50_iconHeight;
        //     icon = (uint16_t*) battery_50;
        //     break;
        case BATT_LOW:
            iconWidth = battery_30_iconWidth;
            iconHeight = battery_30_iconHeight;
            icon = (uint16_t*) battery_30;
            break;
        case BATT_CRITICAL:
            iconWidth = battery_10_iconWidth;
            iconHeight = battery_10_iconHeight;
            icon = (uint16_t*) battery_10;
            break;
    }
    if(icon != NULL)
    {
        uint16_t xpos = TFT_WIDTH-(15+iconWidth);
        uint16_t ypos = TFT_HEIGHT-(15+iconWidth);
        epaper.fillCircle(xpos+(iconWidth/2), ypos+(iconHeight/2), iconHeight, TFT_GRAY_0);
        epaper.pushImage(xpos,ypos,iconWidth,iconHeight, icon);
    }
}

/*
 * Ping a server to get the image index, which should increment every time the image on the server updates.
 * This pairs with the custom image server written for this device.
 * Returns True if the local image counter doesn't match the server image counter.
 * Retruns False if the local image counter matches the server's image counter (indicating no new image)
 */

bool checkForNewImage(const char* url)
{
    HTTPClient http;
    // WiFiClient client;

    Serial.println("Checking Server for new image.");

    http.setTimeout(30000);
    http.setReuse(false);
    http.setRedirectLimit(10);

    if (!http.begin(client, url)) {
        return false;
    }

    int code = http.GET();
    if (code != 200) {
        Serial.printf("HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }

    // Read response safely
    char imageCountStr[16] = {0};
    size_t idx = 0;

    while (client.connected() || client.available()) {
        if (client.available()) {
            char c = client.read();
            if (idx < sizeof(imageCountStr) - 1) {
                imageCountStr[idx++] = c;
            }
        }
        delay(1);  // yield to LWIP
    }

    http.end();  // ALWAYS

    int serverImageCount = atoi(imageCountStr);

    Serial.printf(
        "Checked for new image, received %s\n",
        (serverImageCount != imageCount) ? "New Image Ready" : "No New Image Ready"
    );

    if (serverImageCount != imageCount) {
        imageCount = serverImageCount;
        Serial.println("New image is ready at the server.");
        return true;
    }

    return false;
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
void initMqtt()
{
    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);

    HAMQTT.begin(mqtt_client,ENTITIES_COUNT);
    // HAMQTT.addEntity(haPageNumber);
    // haPageNumber.setState(displayIndex);
    HAMQTT.addEntity(haPageSelect);
    HAMQTT.addEntity(haURL);
    HAMQTT.addEntity(haRefreshButton);
    HAMQTT.addEntity(haBatteryState);
    HAMQTT.addEntity(haBatteryVoltage);
    // HAMQTT.addEntity(haDeepSleepActive);
    // HAMQTT.addEntity(haDeviceOnline);


    haPageSelect.setState(select_options[displayIndex]);
    haURL.setState(HA_URL_DATA);

    HAMQTT.setCallback(ha_callback);
    Serial.printf("MQTT initialized\n");
}
void initDeepSleep()
{

    // haDeviceOnline.setState(false);
    #define WAKE_MASK ((1ULL << BUTTON_1_PIN) | (1ULL << BUTTON_2_PIN) | 1ULL << BUTTON_3_PIN)
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

            if (wakePins & (1ULL << BUTTON_1_PIN)) 
            {
                Serial.println("Woke: BUTTON_1_PIN");
                button1Pressed = true;
            }
            if (wakePins & (1ULL << BUTTON_2_PIN)) 
            {
                Serial.println("Woke: BUTTON_2_PIN");
                button2Pressed = true;
            }
            if (wakePins & (1ULL << BUTTON_3_PIN)) 
            {
                Serial.println("Woke: BUTTON_3_PIN");
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
    setupGammaLUT(.8);
    epaper.begin();
    delay(500);
    epaper.initGrayMode(GRAY_LEVEL4);
    epaper.setRotation(ROTATION);  // Landscape
    delay(500);
    Serial.println("Epaper initialized");
    
    /*
     * Okay, now we can configure the GPIO, including interrupt polling for the buttons.
     */
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
    pinMode(BUTTON_3_PIN, INPUT_PULLUP);
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_1_PIN),
        button1ISR,
        FALLING   // or RISING / CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_2_PIN),
        button2ISR,
        FALLING   // or RISING / CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_3_PIN),
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

    // Initialize Wifi.
    initWifi();
    initMqtt();
    delay(100);
    // haDeviceOnline.setState(true);
    startupTime = millis();
    Serial.println("End Setup");
}

// If we're not asleep, let's run stuff at varying frequency.
const uint32_t period = 10;
const uint32_t ticksPerSecond = 1000;
static uint32_t lastButtonPress = millis();
void handleButtons(){
    /*
     * These booleans are set in ISR that handle button presses. 
     * If a button was pressed, let's do the action here.
     */
    if(button1Pressed)
    {
        if(digitalRead(BUTTON_1_PIN))
        {
            lastButtonPress = millis();
            incrementDisplayIndex();
            button1Pressed = false;
        }
    }
    if(button2Pressed)
    {
        if(digitalRead(BUTTON_2_PIN))
        {
            lastButtonPress = millis();
            freshBoot = true;
            button2Pressed = false;
        }
    }
    if(button3Pressed)
    {
        if(digitalRead(BUTTON_3_PIN))
        {
            lastButtonPress = millis();
            decrementDisplayIndex();
            button3Pressed = false;
        }
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
    if ((millis() - lastUpdateMs) > (deepSleepDurationSeconds*ticksPerSecond)) return true;
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
    Serial.printf("Loading Page: %s\n",select_options[getDisplayIndex()]);  
    if(readBatteryVoltage() < 4.1)
    {
        deepsleepEnabled = true;
        // haDeepSleepActive.setState(true);
    }else{
        // haDeepSleepActive.setState(false);
    }
    // Clear the buffer.
    epaper.fillScreen(TFT_GRAY_3);
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
        // case PAGE_HOMEASSISTANT:
        //     // Go get a URL (homeassistant page from Puppet)
        //     downloadAndConvertBMP(page_urls[getDisplayIndex]);
        //     break;
        default:
            // "other" we just index into the URLs list. This will probably never cause any problems ha
            downloadAndConvertBMP(page_urls[getDisplayIndex()]);
            deepSleepDurationSeconds = 200;
            // setDisplayIndex(PAGE_IMAGE);
            // Serial.printf("Invalid page\n");
            break;
    }
    displayIndex = getDisplayIndex();
    lastUpdateMs = millis();
}
void checkMqtt()
{
    if(WiFi.status() == WL_CONNECTED && !HAMQTT.connected()) {
        Serial.println("Connecting to MQTT");
         if( HAMQTT.connect("Device",MQTT_USER,MQTT_PASSWORD))
            Serial.println("Connected to MQTT");
         else
         {
            Serial.println("Failed to connect to MQTT");
            delay(5000);
         }
    }
    //If I add the binary sensors for deep sleep, this starts crashing. Don't know why. 
    // Serial.println("Start MQTT Loop");
    HAMQTT.loop();
    // Serial.println("End MQTT Loop");
    
}
void loop() {
    checkMqtt();
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

