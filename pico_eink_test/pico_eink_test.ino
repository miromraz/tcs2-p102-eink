// TCM-P102-220X Image Receiver
// Receives EPD image data over serial, streams to display
//
// Protocol:
//   PC sends "IMG\n" to start
//   Pico responds "READY\n"
//   PC sends 163856 bytes (16 header + 163840 image)
//   Pico responds "OK\n" after each 250-byte chunk uploaded
//   After all data: Pico sends "REFRESH\n" and triggers display update
//   After refresh complete: Pico sends "DONE\n"

#include <SPI.h>

#define LED_PIN          25
#define TCM2_BUSY_PIN    16
#define TCM2_ENABLE_PIN  17
#define TCM2_SPI_CS      13

#define SPI_SPEED        1000000
#define MAX_CHUNK        250
#define EPD_TOTAL_SIZE   163856UL  // 16 header + 163840 image

bool waitBusyHigh(uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        if (digitalRead(TCM2_BUSY_PIN) == HIGH) return true;
        delayMicroseconds(50);
    }
    return false;
}

uint16_t uploadChunk(const uint8_t *data, uint8_t len)
{
    if (!waitBusyHigh(3000)) return 0xFFFF;

    SPI1.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
    digitalWrite(TCM2_SPI_CS, LOW);
    delayMicroseconds(10);
    SPI1.transfer(0x20);  // INS: UploadImageData
    SPI1.transfer(0x01);  // P1
    SPI1.transfer(0x00);  // P2
    SPI1.transfer(len);   // Lc
    for (uint8_t i = 0; i < len; i++) {
        SPI1.transfer(data[i]);
    }
    delayMicroseconds(10);
    digitalWrite(TCM2_SPI_CS, HIGH);
    SPI1.endTransaction();

    // Wait for controller to process the chunk
    // BUSY goes LOW briefly then HIGH; with pull-up we may miss the LOW pulse
    delay(10);
    // If BUSY is LOW, wait for it to go HIGH
    if (digitalRead(TCM2_BUSY_PIN) == LOW) {
        uint32_t t = millis();
        while (digitalRead(TCM2_BUSY_PIN) == LOW && millis() - t < 5000);
    }
    if (!waitBusyHigh(5000)) return 0xFFFE;

    SPI1.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
    digitalWrite(TCM2_SPI_CS, LOW);
    delayMicroseconds(10);
    uint8_t sw1 = SPI1.transfer(0x00);
    uint8_t sw2 = SPI1.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(TCM2_SPI_CS, HIGH);
    SPI1.endTransaction();

    delayMicroseconds(1200);
    waitBusyHigh(1000);
    return (sw1 << 8) | sw2;
}

void resetDataPointer()
{
    if (!waitBusyHigh(3000)) return;
    SPI1.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
    digitalWrite(TCM2_SPI_CS, LOW);
    delayMicroseconds(10);
    SPI1.transfer(0x20);
    SPI1.transfer(0x0D);
    SPI1.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(TCM2_SPI_CS, HIGH);
    SPI1.endTransaction();
    delay(10);
    waitBusyHigh(3000);
    // Read status
    SPI1.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
    digitalWrite(TCM2_SPI_CS, LOW);
    delayMicroseconds(10);
    SPI1.transfer(0x00);
    SPI1.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(TCM2_SPI_CS, HIGH);
    SPI1.endTransaction();
    delayMicroseconds(1200);
    waitBusyHigh(1000);
}

void displayUpdate()
{
    if (!waitBusyHigh(3000)) return;
    SPI1.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
    digitalWrite(TCM2_SPI_CS, LOW);
    delayMicroseconds(10);
    SPI1.transfer(0x24);
    SPI1.transfer(0x01);
    SPI1.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(TCM2_SPI_CS, HIGH);
    SPI1.endTransaction();

    // Wait for refresh (can take 2-6 seconds)
    delayMicroseconds(200);
    uint32_t t = millis();
    while (millis() - t < 10000) {
        if (digitalRead(TCM2_BUSY_PIN) == LOW) break;
        delayMicroseconds(100);
    }
    while (digitalRead(TCM2_BUSY_PIN) == LOW && millis() - t < 10000) {
        delay(50);
    }
    // Read status
    SPI1.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE3));
    digitalWrite(TCM2_SPI_CS, LOW);
    delayMicroseconds(10);
    SPI1.transfer(0x00);
    SPI1.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(TCM2_SPI_CS, HIGH);
    SPI1.endTransaction();
    delayMicroseconds(1200);
    waitBusyHigh(1000);
}

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    Serial.begin(115200);
    delay(1000);

    // Init pins
    pinMode(TCM2_SPI_CS, OUTPUT);
    digitalWrite(TCM2_SPI_CS, HIGH);
    pinMode(TCM2_ENABLE_PIN, OUTPUT);
    digitalWrite(TCM2_ENABLE_PIN, HIGH);
    pinMode(TCM2_BUSY_PIN, INPUT_PULLUP);

    // Init SPI1
    SPI1.setRX(12);
    SPI1.setTX(11);
    SPI1.setSCK(10);
    SPI1.begin();

    // Enable controller
    digitalWrite(TCM2_ENABLE_PIN, LOW);
    delay(300);
    waitBusyHigh(2000);

    Serial.println("EINK_READY");
}

void loop()
{
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "IMG") {
            // Reset data pointer
            resetDataPointer();
            Serial.println("READY");

            // Receive and upload data
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
                        timeout = millis();  // Reset timeout on each byte
                    }
                }

                if (received < chunkSize) {
                    Serial.println("TIMEOUT");
                    error = true;
                    break;
                }

                // Upload to display controller
                uint16_t res = uploadChunk(chunk, chunkSize);
                if (res != 0x9000) {
                    Serial.print("ERR:");
                    Serial.println(res, HEX);
                    error = true;
                    break;
                }

                totalReceived += chunkSize;
                Serial.println("OK");

                // Blink LED
                digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            }

            if (!error) {
                Serial.println("REFRESH");
                displayUpdate();
                Serial.println("DONE");
                digitalWrite(LED_PIN, HIGH);
            }
        }
        else if (cmd == "PING") {
            Serial.println("PONG");
        }
    }

    delay(10);
}
