#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_MAGIC 0xE1AAFF00

#define CONFIG_SOFTAPNAME  "esp232-config"
#define CONFIG_OTANAME     "ESP232"


typedef struct
{
    uint32_t magic;
    uint32_t verbose;
    uint32_t baudrate;
    char hostname[32];
    char wifi_ssid[32];
    char wifi_password[32];
    char mqtt_server[32];
    int mqtt_port;
    char mqtt_user[32];
    char mqtt_password[32];
    char mqtt_client[32];
    int mqtt_publish;
    int mqtt_publish_rate;
    char connect_string[128];
    char disconnect_string[128];
} t_cfg;


extern t_cfg current_config;


#endif
