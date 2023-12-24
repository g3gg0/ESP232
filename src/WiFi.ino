
#include <DNSServer.h>
#include "Config.h"

DNSServer dnsServer;

bool connecting = false;
bool wifi_captive = false;
int wifi_rssi = 0;
char wifi_error[64];

void wifi_setup()
{
    DEBUG_PRINT("[WiFi] Connecting to '%s', password '%s'...\n", current_config.wifi_ssid, current_config.wifi_password);
    sprintf(wifi_error, "");
    WiFi.begin(current_config.wifi_ssid, current_config.wifi_password);
    connecting = true;
#if defined(ESP232v2)
    led_set(0, 8, 8, 0);
#else
    pinMode(GPIO_LED, OUTPUT);
    digitalWrite(GPIO_LED, LOW);
#endif
}

void wifi_off()
{
    connecting = false;
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void wifi_enter_captive()
{
    wifi_off();
    WiFi.softAP(CONFIG_SOFTAPNAME);
    dnsServer.start(53, "*", WiFi.softAPIP());
    DEBUG_PRINT("[WiFi] Local IP: %s\n", WiFi.softAPIP().toString().c_str());

    wifi_captive = true;

    /* reset captive idle timer */
    www_activity();
}

bool wifi_loop(void)
{
    int status = WiFi.status();
    uint32_t curTime = millis();
    static uint32_t nextTime = 0;
    static uint32_t stateCounter = 0;

    if (wifi_captive)
    {
        dnsServer.processNextRequest();
        
#if defined(ESP232v2)
        led_set(0, 0, ((millis() % 250) > 125) ? 0 : 255, 0);
#else
        digitalWrite(GPIO_LED, ((millis() % 250) > 125) ? LOW : HIGH);
#endif

        /* captive mode, but noone cares */
        if (!www_is_captive_active())
        {
            DEBUG_PRINT("[WiFi] Timeout in captive, trying known networks again\n");
            sprintf(wifi_error, "Timeout in captive, trying known networks again");
            dnsServer.stop();
            wifi_off();
            wifi_captive = false;
            stateCounter = 0;
            sprintf(wifi_error, "");
        }
        return true;
    }

    if (nextTime > curTime)
    {
        return false;
    }

    /* standard refresh time */
    nextTime = curTime + 500;

    /* when stuck at a state, disconnect */
    if (++stateCounter > 20)
    {
        DEBUG_PRINT("[WiFi] Timeout connecting\n");
        sprintf(wifi_error, "Timeout - incorrect password?");
        wifi_off();
    }

    if (strcmp(wifi_error, ""))
    {
        DEBUG_PRINT("[WiFi] Entering captive mode. Reason: '%s'\n", wifi_error);

        wifi_enter_captive();

        stateCounter = 0;
        return false;
    }

    switch (status)
    {
        case WL_CONNECTED:
            if (connecting)
            {
                led_set(0, 0, 4, 0);
                connecting = false;
                DEBUG_PRINT("[WiFi] Connected, IP address: %s\n", WiFi.localIP().toString().c_str());
                //serial_println("[WiFi] Connected, IP address: ");
                //serial_println(WiFi.localIP().toString().c_str());
                stateCounter = 0;
                sprintf(wifi_error, "");
                
                
#if defined(ESP232v2)
                led_set(0, 0, 8, 0);
#else
                digitalWrite(GPIO_LED, HIGH);
#endif
            }
            else
            {
                wifi_rssi = WiFi.RSSI();

                /* happy with this state, reset counter */
                stateCounter = 0;
            }
            break;

        case WL_CONNECTION_LOST:
            DEBUG_PRINT("[WiFi] Connection lost\n");
            sprintf(wifi_error, "Network found, but connection lost");
            led_set(0, 32, 8, 0);
            wifi_off();
            break;

        case WL_CONNECT_FAILED:
            DEBUG_PRINT("[WiFi] Connection failed\n");
            sprintf(wifi_error, "Network found, but connection failed");
            wifi_off();
            break;

        case WL_NO_SSID_AVAIL:
            DEBUG_PRINT("[WiFi] No SSID with that name\n");
            sprintf(wifi_error, "Network not found");
            wifi_off();
            break;

        case WL_SCAN_COMPLETED:
            DEBUG_PRINT("[WiFi] Scan completed\n");
            wifi_off();
            break;

        case WL_DISCONNECTED:
            if(!connecting)
            {
                DEBUG_PRINT("[WiFi] Disconnected\n");
                led_set(0, 255, 0, 255);
                wifi_off();
            }
            break;

        case WL_IDLE_STATUS:
            if (!connecting)
            {
                connecting = true;
                DEBUG_PRINT("[WiFi]  Idle, connect to '%s'\n", current_config.wifi_ssid);
                WiFi.mode(WIFI_STA);
                WiFi.begin(current_config.wifi_ssid, current_config.wifi_password);
            }
            else
            {
                DEBUG_PRINT("[WiFi]  Idle, connecting...\n");
            }
            break;

        case WL_NO_SHIELD:
            if (!connecting)
            {
                connecting = true;
                DEBUG_PRINT("[WiFi]  Disabled (%d), connecting to '%s'\n", status, current_config.wifi_ssid);
                WiFi.mode(WIFI_STA);
                WiFi.begin(current_config.wifi_ssid, current_config.wifi_password);
            }
            break;

        default:
            DEBUG_PRINT("[WiFi]  unknown (%d), disable\n", status);
            wifi_off();
            break;
    }

    return false;
}
