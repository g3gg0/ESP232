#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define LED_PIN 0
const char *sync_str = "ESP8266-232-UPD";
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#define LED_PIN 13
const char *sync_str = "ESP232-UPD";
#else
#error "This ain't a ESP8266 or ESP32, dumbo!"
#endif

#include <ArduinoOTA.h>
#include <EEPROM.h>

bool ota_enabled = false;
bool ota_active = false;

#define CONFIG_MAGIC 0xE1AAFF02
typedef struct
{
  uint32_t magic;
  uint32_t baudrate;
  char hostname[32];
} t_cfg;


t_cfg current_config;
bool config_mode = false;

void setup()
{
  EEPROM.begin(sizeof(current_config));

  EEPROM.get(0, current_config);
  if (current_config.magic != CONFIG_MAGIC)
  {
    reset_cfg();
  }

  wifi_setup();
  www_setup();
  serial_setup();

  if (has_loopback())
  {
    ota_setup();
    config_mode = true;
  }
}

bool has_loopback()
{
  bool ret = false;
  char receive_buf[32];

  Serial.flush();
  Serial.write((uint8_t *)sync_str, strlen(sync_str));
  Serial.setTimeout(5);

  Serial.readBytes(receive_buf, strlen(sync_str));

  ret = !strncmp(sync_str, receive_buf, strlen(sync_str));

  Serial.flush();

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

  if (!ota_active)
  {
    hasWork |= wifi_loop();
    hasWork |= www_loop();
    hasWork |= serial_loop();
  }
  hasWork |= ota_loop();

  if (!hasWork)
  {
    delay(20);
  }
}

void reset_cfg()
{
  memset(&current_config, 0x00, sizeof(current_config));

  current_config.magic = CONFIG_MAGIC;
  strcpy(current_config.hostname, "ESP232");
  current_config.baudrate = 115200;

  save_cfg();
}

void save_cfg()
{
  EEPROM.put(0, current_config);
  EEPROM.commit();
}
