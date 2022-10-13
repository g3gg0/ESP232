
#include "scpi.h"

static int line_pos = 0;
static uint8_t line_buffer[64];
static bool line_queued = false;


void disp_init()
{

}

bool disp_loop()
{
    uint32_t time = millis();
    static int nextTime = 0;

    if(current_config.mqtt_publish)
    {
        if(time < nextTime)
        {
            return false;
        }
        if(line_queued && time > nextTime + 500)
        {
            line_queued = false;
        }
        if(line_queued)
        {
            return false;
        }
        nextTime = time + current_config.mqtt_publish_rate;
        line_queued = true;

        if((current_config.mqtt_publish & 8))
        {
            scpi_command(":MEAS?", true, true, &disp_parse_meas);
        }
        else
        {
            if((current_config.mqtt_publish & 4) == 0)
            {
                scpi_command(":INIT", false, true, NULL);
            }
            scpi_command("DISP:WIND1:DATA?", true, true, &disp_parse);
        }
    }
    return line_queued;
}

double disp_parse_float(const char *line, char *unit)
{
    int pos = 0;
    double sign = 1;
    double value = 0;
    int decimal = 10;

    while(line[pos] == ' ')
    {
        pos++;
    }

    if(line[pos] == '-')
    {
        sign = -1;
        pos++;
    }
    else if(line[pos] == '+')
    {
        pos++;
    }

    while(isdigit(line[pos]))
    {
        value *= 10.0f;
        value += (double)(line[pos] - '0');
        pos++;
    }

    if(line[pos] == '.')
    {
        pos++;
        while(isdigit(line[pos]))
        {
            value += (double)(line[pos] - '0') / (double)decimal;
            pos++;
            decimal *= 10;
        }
    }

    switch(line[pos])
    {
        case 'n':
            value /= 1000000000.0f;
            break;
        case 'u':
            value /= 1000000.0f;
            break;
        case 'm':
            value /= 1000.0f;
            break;
        case 'k':
            value *= 1000.0f;
            break;
        case 'M':
            value *= 1000000.0f;
            break;
        case 'G':
            value *= 1000000000.0f;
            break;
    }

    value *= sign;

    pos++;

    *unit = line[pos];

    return value;
}

void disp_parse(bool success, const char *src_line)
{
    int start = 0;
    char line[64];

        mqtt_publish_string("debug/string/%s/src_line", src_line);
        mqtt_publish_string("debug/string/%s/success", success ? "true" : "false");

    if(!success)
    {
        return;
    }
    
    strncpy(line, src_line, sizeof(line));

    if(line[0] != '"')
    {
        return;
    }

    /* quit RS232 mode again */
    if(current_config.mqtt_publish & 4)
    {
        scpi_command(":SYST:LOC", false, false, NULL);
    }

    line_queued = false;


    /* skip first quotation mark and spaces */
    while((line[start] == ' ') || (line[start] == '"'))
    {
        start++;
    }

    /* translate symbols and skip trailing quot */
    int pos = start;

    while(line[pos])
    {
        if(line[pos] == '\x10')
        {
            line[pos] = 'u';
        }
        else if(line[pos] == '\x11')
        {
            line[pos] = '+';
        }
        else if(line[pos] == '\x12')
        {
            line[pos] = 'O';
        }
        if(line[pos] == '"')
        {
            line[pos] = '\000';
        }

        pos++;
    }
    
    if(current_config.mqtt_publish & 1)
    {
        mqtt_publish_string("feeds/string/%s/display", &line[start]);
    }

    if(current_config.mqtt_publish & 2)
    {
        char path[32];
        char valstr[32];
        char unit;
        double value = disp_parse_float(&line[start], &unit);

        strcpy(path, "feeds/float/%s/value_X");
        path[21] = unit;

        sprintf(valstr, "%g", value);

        mqtt_publish_string(path, valstr);
    }
}

void disp_parse_meas(bool success, const char *src_line)
{
    int start = 0;
    char line[64];

    if(!success)
    {
        return;
    }
}
