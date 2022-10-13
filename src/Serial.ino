
#include <driver\uart.h>

WiFiServer telnet(23);
WiFiClient client;
uint8_t serial_buf[1024];
bool client_connected = false;

const char *udpAddress = "192.168.1.255";
const int udpPort = 2323;
WiFiUDP udp_out;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

uint8_t terminator = 0x0A;
bool serial_started = false;
unsigned long last_activity = 0;
uint32_t delay_us_per_byte = 0;

#define IDF
const uart_port_t uart_num = UART_NUM_0;

#ifdef IDF
const int uart_buffer_size = (1024 * 2);
QueueHandle_t uart_queue;
#endif

void serial_setup()
{
    if (!serial_started)
    {
#ifdef IDF
        uart_config_t uart_config =
        {
            .baud_rate = (int)current_config.baudrate,
            .data_bits = (uart_word_length_t)(current_config.databits - 5),
            .parity = (uart_parity_t)(current_config.parity ? current_config.parity + 1 : 0),
            .stop_bits = (uart_stop_bits_t)(current_config.stopbits + 1),
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
        };

        pinMode(3, INPUT);

        uart_param_config(uart_num, &uart_config);
        uart_set_pin(uart_num, 1, 3, -1, -1);
        uart_driver_install(uart_num, uart_buffer_size, 0, 100, &uart_queue, 0);
        uart_set_sw_flow_ctrl(uart_num, false, 0, 0);
        uart_set_hw_flow_ctrl(uart_num, UART_HW_FLOWCTRL_DISABLE, 0);

#else
        Serial.begin(current_config.baudrate);
#endif
        telnet.begin();
        telnet.setNoDelay(true);
    }

    uart_set_baudrate(uart_num, current_config.baudrate);

    if (current_config.baudrate > 0)
    {
        delay_us_per_byte = (10 * 1000000 / current_config.baudrate);
    }

#if defined(ESP8266)
    Serial.swap();
#endif
    serial_started = true;

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
}

void serial_print(const char *str, int length)
{
    char buf[128];

#ifdef IDF
    uart_write_bytes(uart_num, str, length);
#else
    Serial.write(str, length);
#endif
    if(WiFi.status() == WL_CONNECTED)
    {
        udp_out.beginPacket(udpAddress, udpPort);
        udp_out.write((const uint8_t *)"> ", 2);
        udp_out.write((const uint8_t *)str, length);
        udp_out.endPacket();

        if (client.connected())
        {
            client.write("> ", 2);
            client.write(str, length);
        }
    }
}

void serial_println(const char *str)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "%s\n", str);
    serial_print(buf, strlen(buf));
}

bool serial_loop_rx()
{
    /* network part */
    if (telnet.hasClient())
    {
        client_connected = true;

        if (client != NULL)
        {
            client.stop();
        }

        client = telnet.available();
        char remote[32];

        sprintf(remote, "%s:%d", client.remoteIP().toString().c_str(), client.remotePort());

#ifndef IDF
        if (current_config.verbose)
        {
            Serial.printf("[Serial] New client connected: %s\n", remote);
        }

        if (strstr(current_config.connect_string, "%s"))
        {
            Serial.printf(current_config.connect_string, remote);
        }
        else
        {
            Serial.printf(current_config.connect_string);
        }
#endif
    }

    if (!client_connected)
    {
        return false;
    }

    if (!client.connected())
    {
        client_connected = false;
#ifndef IDF
        Serial.printf(current_config.disconnect_string);
#endif

        client.stop();
        return false;
    }

    int avail = client.available();
    if (avail <= 0)
    {
        return false;
    }

    int net_avail = MIN(sizeof(serial_buf), avail);
    unsigned long current_millis = millis();

    if (avail > 0)
    {
        digitalWrite(LED_PIN, HIGH);
        int net_read = client.read(serial_buf, net_avail);
        // Serial.printf("r: %d/%d", net_avail, net_read);
#ifdef IDF
        uart_write_bytes(uart_num, (const char *)serial_buf, net_read);
#else
        Serial.write(serial_buf, net_read);
#endif

        /* no delay needed anymore, deactivated Tx buffer. enabling it causes Rx loss on IDF. sigh. */
        //uint32_t delay_ms = net_read * delay_us_per_byte / 1000;
        //delay(delay_ms);
        digitalWrite(LED_PIN, LOW);

        last_activity = current_millis;
    }

    bool activity = (current_millis - last_activity) < 1000;

    return activity;
}

bool serial_loop_tx()
{
    /* UART part */
    int rcv_pos = 0;
    uint32_t rcv_timeout = micros();
    uint32_t current_millis = micros();

    while (rcv_timeout > 0)
    {
        static uint8_t buffer[1024];
        int pos = 0;
#ifdef IDF
        int32_t length = uart_read_bytes(uart_num, buffer, sizeof(buffer), 1);
#else
        int32_t length = Serial.available() ? 1 : 0;
#endif

        if (current_config.verbose & 2)
        {
            if (length < 0)
            {
                udp_out.beginPacket(udpAddress, udpPort);
                udp_out.write((const uint8_t *)"ERROR:", 6);
                udp_out.write((const uint8_t *)length, 4);
                udp_out.endPacket();
            }
        }

        if (length > 0)
        {
            if (current_config.verbose & 2)
            {
                udp_out.beginPacket(udpAddress, udpPort);
            }
            while (pos < length)
            {
                uint8_t ch = 0;
                digitalWrite(LED_PIN, HIGH);

#ifdef IDF
                ch = buffer[pos++];
#else
                ch = Serial.read();
#endif
                if (current_config.verbose & 2)
                {
                    udp_out.write((const uint8_t *)&ch, 1);
                }

                digitalWrite(LED_PIN, LOW);

                serial_buf[rcv_pos++] = ch;
                scpi_cb(ch);

                /* received a line terminator? if not wait for it or until timeout happened */
                if ((terminator != 0) && (ch == terminator))
                {
                    rcv_timeout = 0;
                }
                else
                {
                    rcv_timeout = micros() + 2 * delay_us_per_byte;
                }
#ifndef IDF
                length += Serial.available() ? 1 : 0;
#endif
            }
            if (current_config.verbose & 2)
            {
                udp_out.endPacket();
            }
        }

        if (micros() >= rcv_timeout || rcv_pos >= sizeof(serial_buf))
        {
            rcv_timeout = 0;
        }
    }

    if (rcv_pos > 0 && client.connected())
    {
        last_activity = current_millis;
        client.write(serial_buf, rcv_pos);
    }

    bool activity = (current_millis - last_activity) < 1000;

    return activity;
}
