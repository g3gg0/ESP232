#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CONFIG_MAGIC 0xE1AAFF00

#define CONFIG_SOFTAPNAME  "esp232-config"
#define CONFIG_OTANAME     "ESP232"

#if defined(ESP232v2)
#define GPIO_RXD 44
#define GPIO_TXD 43
#define GPIO_RTS 0
#define GPIO_CTS 10

#define DEBUG_PRINT(fmt, ...) Serial.printf((fmt), ##__VA_ARGS__)
#define DEBUG_BEGIN() Serial.begin(115200)
#else
#define GPIO_RXD 3
#define GPIO_TXD 1
#define GPIO_RTS -1
#define GPIO_CTS -1
#define GPIO_LED 13
asd
#define DEBUG_PRINT(fmt, ...) while(0)
#define DEBUG_BEGIN() while(0)
#endif



typedef struct
{
    uint32_t magic;
    uint32_t verbose;
    uint32_t baudrate;
    uint32_t databits;
    uint32_t parity;
    uint32_t stopbits;
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
