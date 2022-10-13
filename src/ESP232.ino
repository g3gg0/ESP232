#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define LED_PIN 0
const char *sync_str = "ESP8266-232-UPD";
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#define LED_PIN 13
const char *sync_str = "ESP232-UPD\n";
#else
#error "This ain't a ESP8266 or ESP32, dumbo!"
#endif

#include <FS.h>
#include <SPIFFS.h>

bool config_mode = false;

void setup()
{
    // Serial.begin(115200);
    // Serial.printf("\n\n\n");

    // Serial.printf("[i] SDK:          '%s'\n", ESP.getSdkVersion());
    // Serial.printf("[i] CPU Speed:    %d MHz\n", ESP.getCpuFreqMHz());
    // Serial.printf("[i] Chip Id:      %06llX\n", ESP.getEfuseMac());
    // Serial.printf("[i] Flash Mode:   %08X\n", ESP.getFlashChipMode());
    // Serial.printf("[i] Flash Size:   %08X\n", ESP.getFlashChipSize());
    // Serial.printf("[i] Flash Speed:  %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
    // Serial.printf("[i] Heap          %d/%d\n", ESP.getFreeHeap(), ESP.getHeapSize());
    // Serial.printf("[i] SPIRam        %d/%d\n", ESP.getFreePsram(), ESP.getPsramSize());
    // Serial.printf("\n");
    // Serial.printf("[i] Starting\n");
    // Serial.printf("[i]   Setup SPIFFS\n");
    if (!SPIFFS.begin(true))
    {
        // Serial.println("[E]   SPIFFS Mount Failed");
    }

    cfg_read();
    scpi_setup();
    wifi_setup();
    www_setup();
    serial_setup();
    dbg_setup();

    if (has_loopback())
    {
        ota_enable();
        config_mode = true;
    }
}

bool has_loopback()
{
    return false;

    bool ret = false;
    char receive_buf[32];

    // Serial.write((uint8_t *)sync_str, strlen(sync_str));
    // Serial.setTimeout(5);

    // Serial.readBytes(receive_buf, strlen(sync_str));

    ret = !strncmp(sync_str, receive_buf, strlen(sync_str));

    return ret;
}

void loop()
{
    bool hasWork = false;

    if (config_mode)
    {
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, (millis() % 500) > 250);
        wifi_loop();
        www_loop();
        ota_loop();
        return;
    }

    // if (!ota_active)
    {
        hasWork |= dbg_loop();
        hasWork |= scpi_loop();
        hasWork |= wifi_loop();
        hasWork |= www_loop();
        hasWork |= serial_loop_rx();
        hasWork |= serial_loop_tx();
        hasWork |= mqtt_loop();
        hasWork |= disp_loop();
    }
    hasWork |= ota_loop();

    if (!hasWork)
    {
        delay(10);
    }
}
