
#include <ArduinoOTA.h>
#include "Config.h"

bool ota_active = false;
bool ota_setup_done = false;
long ota_offtime = 0;

void ota_setup()
{
    if (ota_setup_done)
    {
        ota_enable();
        return;
    }
    ArduinoOTA.setHostname(CONFIG_OTANAME);

    ArduinoOTA.onStart([]() {
        String type;
        ota_active = true; 
        ota_offtime = millis() + 600000;
    })
    .onEnd([]() { 
        ota_active = false; 
    })
    .onProgress([](unsigned int progress, unsigned int total) { 
    })
    .onError([](ota_error_t error) {
        DEBUG_PRINT("Error[%u]\n", error);
        //if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        //else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        //else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        //else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        //else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    
    ArduinoOTA.begin();

    DEBUG_PRINT("[OTA] Setup finished\n");
    ota_setup_done = true;
    ota_enable();
}

void ota_enable()
{
    DEBUG_PRINT("[OTA] Enabled\n");
    ota_offtime = millis() + 600000;
}

bool ota_enabled()
{
    return (ota_offtime > millis() || ota_active);
}

bool ota_loop()
{
    if (ota_enabled())
    {
        ArduinoOTA.handle();
    }

    return ota_active;
}
