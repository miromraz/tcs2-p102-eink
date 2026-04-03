// ESP32-S3 + TCS2-P102 E-Ink Display Driver
// Phase 1: Display init, checkerboard test pattern, serial image upload
//
// Hardware: Seeed Studio XIAO ESP32-S3 Sense + MpicoSys TCS2-P102-231
// Display:  10.2" 1024x1280 1-bit B&W (EZ102CT011)
//
// Wiring (XIAO header pins):
//   D8  (GPIO7)  -> SCK
//   D10 (GPIO9)  -> MOSI
//   D9  (GPIO8)  -> MISO
//   D0  (GPIO1)  -> CS
//   D1  (GPIO2)  -> BUSY
//   D2  (GPIO3)  -> ENABLE
//
// Serial protocol (same as pico_eink_test):
//   PC sends "IMG\n" -> MCU responds "READY\n"
//   PC sends 163856 bytes (16 header + 163840 image)
//   MCU responds "OK\n" after each 250-byte chunk
//   After all data: "REFRESH\n" then "DONE\n"

#include <Arduino.h>
#include <SPI.h>
#include <TCM2.h>

// Pin definitions - XIAO ESP32-S3 Sense
#define PIN_SCK     7   // D8
#define PIN_MOSI    9   // D10
#define PIN_MISO    8   // D9
#define PIN_CS      1   // D0
#define PIN_BUSY    2   // D1
#define PIN_ENABLE  3   // D2

// Display constants
#define DISPLAY_WIDTH   1024
#define DISPLAY_HEIGHT  1280
#define EPD_HEADER_SIZE 16
#define EPD_IMAGE_SIZE  ((DISPLAY_WIDTH / 8) * DISPLAY_HEIGHT)  // 163840
#define EPD_TOTAL_SIZE  (EPD_HEADER_SIZE + EPD_IMAGE_SIZE)      // 163856
#define MAX_CHUNK       TCM2_MAX_CHUNK_SIZE                     // 250

// EPD header for 10.2" 1024x1280 1-bit display
static const uint8_t EPD_HEADER[EPD_HEADER_SIZE] = {
    0x3D,       // panel type: 10.2"
    0x04, 0x00, // X resolution: 1024 (big-endian)
    0x05, 0x00, // Y resolution: 1280 (big-endian)
    0x01,       // color depth: 1-bit
    0x00,       // pixel data format: type 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

TCM2 tcm(PIN_BUSY, PIN_ENABLE, PIN_CS);

void printDeviceInfo()
{
    uint8_t buffer[32];

    Serial.println("--- Device Info ---");

    TCM2Response res = tcm.getDeviceInfo(buffer);
    if (res == TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("Device info: ");
        Serial.println((char *)buffer);
    } else {
        Serial.print("getDeviceInfo failed: 0x");
        Serial.println(res, HEX);
        return;
    }

    res = tcm.getDeviceId(buffer);
    if (res == TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("Device ID: ");
        for (int i = 0; i < TCM2_LE_GET_DEVICE_ID; i++) {
            if (buffer[i] < 0x10) Serial.print("0");
            Serial.print(buffer[i], HEX);
        }
        Serial.println();
    }

    float temperature;
    res = tcm.getTemperature(&temperature);
    if (res == TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.println(" C");
    }

    Serial.println("-------------------");
}

void displayCheckerboard()
{
    Serial.println("Drawing checkerboard...");

    // Reset data pointer and erase framebuffer
    TCM2Response res = tcm.resetDataPointer();
    if (res != TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("resetDataPointer failed: 0x");
        Serial.println(res, HEX);
        return;
    }

    // Upload EPD header first
    uint8_t header[EPD_HEADER_SIZE];
    memcpy(header, EPD_HEADER, EPD_HEADER_SIZE);
    res = tcm.uploadImageData(header, EPD_HEADER_SIZE);
    if (res != TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("Upload header failed: 0x");
        Serial.println(res, HEX);
        return;
    }

    // Generate and upload checkerboard pattern
    // 8-pixel-high bands: each row within a band is the same,
    // alternating between 0xFF (white) and 0x00 (black) every 8 pixels wide
    // Row bytes = 1024/8 = 128 bytes per row
    uint8_t chunk[MAX_CHUNK];
    uint32_t bytesUploaded = 0;
    uint32_t pixelRow = 0;
    uint8_t chunkPos = 0;
    const uint32_t bytesPerRow = DISPLAY_WIDTH / 8;  // 128

    while (bytesUploaded < EPD_IMAGE_SIZE) {
        // Which row are we on?
        pixelRow = bytesUploaded / bytesPerRow;
        uint32_t colByte = bytesUploaded % bytesPerRow;

        // Determine checkerboard square: 8 pixels high x 8 pixels wide
        // Row band: pixelRow / 8, Col band: colByte (each byte = 8 pixels)
        bool rowBandEven = ((pixelRow / 8) % 2) == 0;
        bool colBandEven = (colByte % 2) == 0;

        // XOR to create checkerboard: white (0xFF) or black (0x00)
        if (rowBandEven == colBandEven) {
            chunk[chunkPos] = 0xFF; // white
        } else {
            chunk[chunkPos] = 0x00; // black
        }

        chunkPos++;
        bytesUploaded++;

        // Upload when chunk is full or we've reached the end
        if (chunkPos == MAX_CHUNK || bytesUploaded == EPD_IMAGE_SIZE) {
            res = tcm.uploadImageData(chunk, chunkPos);
            if (res != TCM2_EP_SW_NORMAL_PROCESSING) {
                Serial.print("Upload failed at byte ");
                Serial.print(bytesUploaded);
                Serial.print(": 0x");
                Serial.println(res, HEX);
                return;
            }
            chunkPos = 0;
        }
    }

    Serial.println("Refreshing display...");
    res = tcm.displayUpdate();
    if (res != TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("displayUpdate failed: 0x");
        Serial.println(res, HEX);
        return;
    }
    Serial.println("Checkerboard displayed.");
}

void handleSerialImageUpload()
{
    // Reset data pointer
    TCM2Response res = tcm.resetDataPointer();
    if (res != TCM2_EP_SW_NORMAL_PROCESSING) {
        Serial.print("ERR:RESET:");
        Serial.println(res, HEX);
        return;
    }
    Serial.println("READY");

    // Receive and upload data in chunks
    uint32_t totalReceived = 0;
    uint8_t chunk[MAX_CHUNK];
    bool error = false;

    while (totalReceived < EPD_TOTAL_SIZE && !error) {
        uint32_t remaining = EPD_TOTAL_SIZE - totalReceived;
        uint8_t chunkSize = (remaining > MAX_CHUNK) ? MAX_CHUNK : remaining;

        // Read exactly chunkSize bytes from serial
        uint8_t received = 0;
        uint32_t timeout = millis();
        while (received < chunkSize && millis() - timeout < 5000) {
            if (Serial.available()) {
                chunk[received++] = Serial.read();
                timeout = millis();
            }
        }

        if (received < chunkSize) {
            Serial.println("TIMEOUT");
            error = true;
            break;
        }

        // Upload to display controller
        res = tcm.uploadImageData(chunk, chunkSize);
        if (res != TCM2_EP_SW_NORMAL_PROCESSING) {
            Serial.print("ERR:");
            Serial.println(res, HEX);
            error = true;
            break;
        }

        totalReceived += chunkSize;
        Serial.println("OK");
    }

    if (!error) {
        Serial.println("REFRESH");
        tcm.displayUpdate();
        Serial.println("DONE");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);  // Allow USB CDC enumeration, don't use while(!Serial)

    Serial.println("ESP32-S3 E-Ink Display Driver");
    Serial.println("Initializing...");

    // Pre-configure BUSY pin with pull-up before controller init
    pinMode(PIN_BUSY, INPUT_PULLUP);

    // Ensure clean reset: hold ENABLE HIGH (disabled) before library init
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, HIGH);
    delay(100);

    // Initialize SPI with custom pin mapping (must be before tcm.begin())
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

    // Initialize TCM2 controller (sets ENABLE LOW, waits for BUSY)
    tcm.begin();
    Serial.println("Controller initialized.");

    // Query and display device info to verify SPI communication
    printDeviceInfo();

    // Display checkerboard test pattern
    displayCheckerboard();

    Serial.println("EINK_READY");
}

void loop()
{
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "IMG") {
            handleSerialImageUpload();
        } else if (cmd == "PING") {
            Serial.println("PONG");
        }
    }

    delay(10);
}
