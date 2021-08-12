
WiFiServer telnet(23);
WiFiClient client;

bool use_xonxoff = true;
bool paused = false;
uint8_t terminator = 0x0A;
uint32_t buffer_timeout = 30;
bool serial_started = false;
uint8_t buf[1024];
unsigned long last_activity = 0;

void serial_setup()
{
  if (serial_started)
  {
    Serial.end();
  }
  Serial.begin(current_config.baudrate);
#if defined(ESP8266)
#pragma message "ESP8266 stuff happening!"
  Serial.swap();
#elif defined(ESP32)
#pragma message "ESP32 stuff happening!"
#else
#error "This ain't a ESP8266 or ESP32, dumbo!"
#endif

  serial_started = true;

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  telnet.begin();
  telnet.setNoDelay(true);
}

bool serial_loop()
{
  if (telnet.hasClient())
  {
    if (client != NULL)
    {
      client.stop();
      //client = NULL;
    }

    client = telnet.available();
    paused = false;
  }

  if (!client.connected())
  {
    client.stop();
    return false;
  }

  int net_avail = client.available();
  unsigned long current_millis = millis();

  if (net_avail > 0 && !paused)
  {
    last_activity = current_millis;
    digitalWrite(LED_PIN, HIGH);
    int net_read = client.read(buf, net_avail);
    Serial.write(buf, net_avail);
    digitalWrite(LED_PIN, LOW);
  }

  int rcv_pos = 0;
  int rcv_timeout = millis();

  while (rcv_timeout > 0 && rcv_pos < sizeof(buf))
  {
    if (Serial.available())
    {
      digitalWrite(LED_PIN, HIGH);
      uint8_t ch = Serial.read();
      digitalWrite(LED_PIN, LOW);

      if (use_xonxoff)
      {
        if (ch == 0x13)
        {
          paused = true;
          continue;
        }
        if (ch == 0x11)
        {
          paused = false;
          continue;
        }
      }

      buf[rcv_pos++] = ch;

      /* received a line terminator? if not wait for it or until timeout happened */
      if (terminator != 0 && ch == terminator)
      {
        rcv_timeout = 0;
      }
      else
      {
        rcv_timeout = millis() + buffer_timeout;
      }
    }

    if (millis() >= rcv_timeout)
    {
      rcv_timeout = 0;
    }
  }

  if (rcv_pos > 0)
  {
    last_activity = current_millis;
    client.write(buf, rcv_pos);
  }

  bool activity = (current_millis - last_activity) < 1000;

  return activity;
}
