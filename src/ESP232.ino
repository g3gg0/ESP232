#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define GPIO_LED 0
const char *sync_str = "ESP8266-232-UPD";
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>


const char *sync_str = "ESP232-UPD\n";
#else
#error "This ain't a ESP8266 or ESP32, dumbo!"
#endif


#include <FS.h>
#include <SPIFFS.h>
#include "Config.h"

bool config_mode = false;

void setup()
{
    DEBUG_BEGIN();
    DEBUG_PRINT("\n\n\n");

    DEBUG_PRINT("[i] SDK:          '%s'\n", ESP.getSdkVersion());
    DEBUG_PRINT("[i] CPU Speed:    %d MHz\n", ESP.getCpuFreqMHz());
    DEBUG_PRINT("[i] Chip Id:      %06llX\n", ESP.getEfuseMac());
    DEBUG_PRINT("[i] Flash Mode:   %08X\n", ESP.getFlashChipMode());
    DEBUG_PRINT("[i] Flash Size:   %08X\n", ESP.getFlashChipSize());
    DEBUG_PRINT("[i] Flash Speed:  %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
    DEBUG_PRINT("[i] Heap          %d/%d\n", ESP.getFreeHeap(), ESP.getHeapSize());
    DEBUG_PRINT("[i] SPIRam        %d/%d\n", ESP.getFreePsram(), ESP.getPsramSize());
    DEBUG_PRINT("\n");
    DEBUG_PRINT("[i] Starting\n");
    DEBUG_PRINT("[i]   Setup SPIFFS\n");
    if (!SPIFFS.begin(true))
    {
        DEBUG_PRINT("[E]   SPIFFS Mount Failed\n");
    }

    cfg_read();
    scpi_setup();
    wifi_setup();
    www_setup();
    serial_setup();
    dbg_setup();
    led_setup();

    led_set(0, 255, 0, 0);

    if (has_loopback())
    {
        DEBUG_PRINT("[i] Loopback detected, entering config mode");
        ota_enable();
        config_mode = true;
    }
}

bool has_loopback()
{
    return false;

    bool ret = false;
    char receive_buf[32];

    Serial.write((uint8_t *)sync_str, strlen(sync_str));
    Serial.setTimeout(5);
    Serial.readBytes(receive_buf, strlen(sync_str));

    ret = !strncmp(sync_str, receive_buf, strlen(sync_str));

    return ret;
}

char receive_buffer[128];
int receive_pos = 0;

void loop()
{
    bool hasWork = false;

    if (config_mode)
    {
#if defined(ESP232v2)
        led_set(0, (millis() % 500) > 250 ? 255 : 0, 0, 0);
#else
        pinMode(GPIO_LED, OUTPUT);
        digitalWrite(GPIO_LED, (millis() % 500) > 250);
#endif

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
