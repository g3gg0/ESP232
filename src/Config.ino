
#include <FS.h>
#include <SPIFFS.h>

#include "Config.h"

t_cfg current_config;
bool config_valid = false;

void cfg_save()
{
    File file = SPIFFS.open("/config.dat", "w");
    if (!file || file.isDirectory())
    {
        return;
    }

    if (strlen(current_config.hostname) < 2)
    {
        strcpy(current_config.hostname, "ESP232");
    }

    file.write((uint8_t *)&current_config, sizeof(current_config));
    file.close();
}

void cfg_reset()
{
    memset(&current_config, 0x00, sizeof(current_config));

    current_config.magic = CONFIG_MAGIC;
    strcpy(current_config.hostname, "ESP232");
    
    strcpy(current_config.mqtt_server, "");
    current_config.mqtt_port = 11883;
    strcpy(current_config.mqtt_user, "");
    strcpy(current_config.mqtt_password, "");
    strcpy(current_config.mqtt_client, "ESP232");
    current_config.mqtt_publish = 0;
    current_config.mqtt_publish_rate = 5000;

    
    current_config.baudrate = 115200;
    current_config.verbose = 0;
    strcpy(current_config.connect_string, "");
    strcpy(current_config.disconnect_string, "");
    

    
    strcpy(current_config.wifi_ssid, "(not set)");
    strcpy(current_config.wifi_password, "(not set)");
}

void cfg_read()
{
    File file = SPIFFS.open("/config.dat", "r");

    config_valid = false;

    if (!file || file.isDirectory())
    {
        cfg_reset();
    }
    else
    {
        file.read((uint8_t *)&current_config, sizeof(current_config));
        file.close();

        if (current_config.magic != CONFIG_MAGIC)
        {
            cfg_reset();
            return;
        }
        config_valid = true;
    }
}
