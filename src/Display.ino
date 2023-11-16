
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
    static uint32_t nextTime = 0;

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

    mqtt_publish_string("debug/string/%s/success", success ? "true" : "false");
    
    if(!success)
    {
        return;
    }
    
    mqtt_publish_string("debug/string/%s/src_line", src_line);

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
    if(!success)
    {
        return;
    }
    
    /* +4.016779E+00,+1.001000E+00,+9.910000E+37,+6.041777E+02,+3.584400E+04 */
    float meas_u, meas_i, meas_r, meas_t, status;
    /*
        Bit 0 (OFLO) — Set to 1 if measurement was made while in over-range.
        Bit 1 (Filter) — Set to 1 if measurement was made with the filter enabled.
        Bit 2 (Front/Rear) — Set to 1 if FRONT terminals are selected.
        Bit 3 (Compliance) — Set to 1 if in real compliance.
        Bit 4 (OVP) — Set to 1 if the over voltage protection limit was reached.
        Bit 5 (Math) — Set to 1 if math expression (calc1) is enabled.
        Bit 6 (Null) — Set to 1 if Null is enabled.
        Bit 7 (Limits) — Set to 1 if a limit test (calc2) is enabled.
        Bits 8 and 9 (Limit Results) — Provides limit test results (see grading and sorting modes below).
        Bit 10 (Auto-ohms) — Set to 1 if auto-ohms enabled.
        Bit 11 (V-Meas) — Set to 1 if V-Measure is enabled.
        Bit 12 (I-Meas) — Set to 1 if I-Measure is enabled.
        Bit 13 (Ω-Meas) — Set to 1 if Ω-Measure is enabled.
        Bit 14 (V-Sour) — Set to 1 if V-Source used.
        Bit 15 (I-Sour) — Set to 1 if I-Source used.
        Bit 16 (Range Compliance) — Set to 1 if in range compliance.
        Bit 17 (Offset Compensation) — Set to 1 if Offset Compensated Ohms is enabled.
        Bit 18 — Contact check failure (see Appendix F).
        Bits 19, 20 and 21 (Limit Results) — Provides limit test results
        (see grading and sorting modes below).
        Bit 22 (Remote Sense) — Set to 1 if 4-wire remote sense selected.
        Bit 23 (Pulse Mode) — Set to 1 if in the Pulse Mode.
    */
    
    int items_parsed = sscanf(src_line, "%f,%f,%f,%f,%f", 
                              &meas_u, &meas_i, &meas_r, &meas_t, &status);

    if(items_parsed == 5)
    {
        mqtt_publish_float("feeds/float/%s/meas_U", meas_u);
        mqtt_publish_float("feeds/float/%s/meas_I", meas_i);
        mqtt_publish_float("feeds/float/%s/meas_R", meas_r);
        mqtt_publish_float("feeds/float/%s/meas_t", meas_t);
        mqtt_publish_float("feeds/float/%s/meas_status", status);
    }

}
