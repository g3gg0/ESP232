
#include <PubSubClient.h>
#include <ESP32httpUpdate.h>
#include <Config.h>


WiFiClient mqtt_client;
PubSubClient mqtt(mqtt_client);


int mqtt_last_publish_time = 0;
int mqtt_lastConnect = 0;
int mqtt_retries = 0;
bool mqtt_fail = false;

char command_topic[64];
char response_topic[64];


void callback(char *topic, byte *payload, unsigned int length)
{
    // Serial.print("Message arrived [");
    // Serial.print(topic);
    // Serial.print("] ");
    // Serial.print("'");
    for (int i = 0; i < length; i++)
    {
        // Serial.print((char)payload[i]);
    }
    // Serial.print("'");
    // Serial.println();

    payload[length] = 0;

    if (!strcmp(topic, command_topic))
    {
        char *command = (char *)payload;
        char buf[1024];

        if (!strncmp(command, "http", 4))
        {
            snprintf(buf, sizeof(buf)-1, "updating from: '%s'", command);
            // Serial.printf("%s\n", buf);

            mqtt.publish(response_topic, buf);
            ESPhttpUpdate.rebootOnUpdate(false);
            t_httpUpdate_return ret = ESPhttpUpdate.update(command);

            switch (ret)
            {
                case HTTP_UPDATE_FAILED:
                    snprintf(buf, sizeof(buf)-1, "HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                    mqtt.publish(response_topic, buf);
                    // Serial.printf("%s\n", buf);
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    snprintf(buf, sizeof(buf)-1, "HTTP_UPDATE_NO_UPDATES");
                    mqtt.publish(response_topic, buf);
                    // Serial.printf("%s\n", buf);
                    break;

                case HTTP_UPDATE_OK:
                    snprintf(buf, sizeof(buf)-1, "HTTP_UPDATE_OK");
                    mqtt.publish(response_topic, buf);
                    // Serial.printf("%s\n", buf);
                    delay(500);
                    ESP.restart();
                    break;

                default:
                    snprintf(buf, sizeof(buf)-1, "update failed");
                    mqtt.publish(response_topic, buf);
                    // Serial.printf("%s\n", buf);
                    break;
            }
        }
        else
        {
            snprintf(buf, sizeof(buf)-1, "unknown command: '%s'", command);
            mqtt.publish(response_topic, buf);
            // Serial.printf("%s\n", buf);
        }
    }
}

void mqtt_setup()
{
    mqtt.setCallback(callback);
}

void mqtt_publish_string(const char *name, const char *value)
{
    char path_buffer[128];

    sprintf(path_buffer, name, current_config.mqtt_client);

    if (!mqtt.publish(path_buffer, value))
    {
        mqtt_fail = true;
    }
    // Serial.printf("Published %s : %s\n", path_buffer, value);
}

void mqtt_publish_float(const char *name, float value)
{
    char path_buffer[128];
    char buffer[32];

    sprintf(path_buffer, name, current_config.mqtt_client);
    sprintf(buffer, "%0.2f", value);

    if (!mqtt.publish(path_buffer, buffer))
    {
        mqtt_fail = true;
    }
    // Serial.printf("Published %s : %s\n", path_buffer, buffer);
}

void mqtt_publish_int(const char *name, uint32_t value)
{
    char path_buffer[128];
    char buffer[32];

    if (value == 0x7FFFFFFF)
    {
        return;
    }
    sprintf(path_buffer, name, current_config.mqtt_client);
    sprintf(buffer, "%d", value);

    if (!mqtt.publish(path_buffer, buffer))
    {
        mqtt_fail = true;
    }
    // Serial.printf("Published %s : %s\n", path_buffer, buffer);
}

bool mqtt_loop()
{
    uint32_t time = millis();
    static int nextTime = 0;

#ifdef TESTMODE
    return false;
#endif
    if (mqtt_fail)
    {
        mqtt_fail = false;
        mqtt.disconnect();
    }

    MQTT_connect();

    if (!mqtt.connected())
    {
        return false;
    }

    mqtt.loop();

    if (time >= nextTime)
    {
        bool do_publish = false;

        if ((time - mqtt_last_publish_time) > 60000)
        {
            do_publish = true;
        }

        if (do_publish)
        {
            mqtt_last_publish_time = time;

            if (current_config.mqtt_publish & 1)
            {
            }
            if (current_config.mqtt_publish & 2)
            {
            }
            if (current_config.mqtt_publish & 4)
            {
            }
            if (current_config.mqtt_publish & 8)
            {
            }
        }

        if(current_config.mqtt_publish & 1)
        {
            nextTime = time + 1000;
        }
        if(current_config.mqtt_publish & 2)
        {
            nextTime = time + 250;
        }
    }

    return false;
}

void MQTT_connect()
{
    int curTime = millis();
    int8_t ret;

    if (strlen(current_config.mqtt_server) == 0)
    {
        return;
    }

    mqtt.setServer(current_config.mqtt_server, current_config.mqtt_port);
    
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    if (mqtt.connected())
    {
        return;
    }

    if ((mqtt_lastConnect != 0) && (curTime - mqtt_lastConnect < (1000 << mqtt_retries)))
    {
        return;
    }

    mqtt_lastConnect = curTime;

    // Serial.println("MQTT: Connecting to MQTT... ");
    
    sprintf(command_topic, "tele/%s/command", current_config.mqtt_client);
    sprintf(response_topic, "tele/%s/response", current_config.mqtt_client);

    ret = mqtt.connect(current_config.mqtt_client, current_config.mqtt_user, current_config.mqtt_password);

    if (ret == 0)
    {
        mqtt_retries++;
        if (mqtt_retries > 8)
        {
            mqtt_retries = 8;
        }
        // Serial.printf("MQTT: (%d) ", mqtt.state());
        // Serial.println("MQTT: Retrying MQTT connection");
        mqtt.disconnect();
    }
    else
    {
        /* discard counts till then */
        // Serial.println("MQTT Connected!");
        mqtt.subscribe(command_topic);
        mqtt_publish_string((char *)"feeds/string/%s/error", "");
    }
}
