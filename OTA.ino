
#include <ArduinoOTA.h>

void ota_setup()
{
  if(ota_enabled)
  {
    return;
  }
  
  ArduinoOTA.setHostname(current_config.hostname);

  ArduinoOTA
    .onStart([]() {
      Serial.printf("[OTA] active");
      ota_active = true;
    })
    .onEnd([]() {
      Serial.printf("[OTA] inactive");
      ota_active = false;
    })
    .onProgress([](unsigned int progress, unsigned int total) {
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    
  Serial.printf("[OTA] started\n");
  ArduinoOTA.begin();
  ota_enabled = true;
}

bool ota_loop()
{
  if(ota_enabled)
  {
    ArduinoOTA.handle();
  }

  return ota_enabled;
}

