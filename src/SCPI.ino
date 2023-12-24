
#include "scpi.h"
#include "Config.h"

static int scpi_line_pos = 0;
static uint8_t scpi_line_buffer[128];
static int scpi_state = SCPI_STATE_IDLE;
static uint32_t scpi_request_time = 0;
static uint32_t scpi_opc_timeout = 100;
static uint32_t scpi_request_timeout = 500;
static uint32_t scpi_retries = 0;
static uint32_t scpi_opc_retries_max = 100;
static bool scpi_state_changed = false;

static t_scpi_command scpi_current;

QueueHandle_t scpi_commands;


void scpi_debug(const char *msg)
{
    if(1)
    {
        DEBUG_PRINT("> %s\n", msg);
    }
}

void scpi_setup()
{
    scpi_commands = xQueueCreate(10, sizeof(t_scpi_command));
    scpi_state = SCPI_STATE_IDLE;
}

bool scpi_loop()
{
    uint32_t time = millis();
    static int nextTime = 0;

    t_scpi_command dummy;

    bool item_waiting = (xQueuePeek(scpi_commands, &dummy, 0) == pdTRUE) && (scpi_state == SCPI_STATE_IDLE);

    if(item_waiting || scpi_state_changed)
    {
        nextTime = time;
    }

    while(time >= nextTime)
    {
        nextTime = time + 1000;

        switch(scpi_state)
        {
            case SCPI_STATE_IDLE:
                if(xQueuePeek(scpi_commands, &dummy, 0) == pdFALSE)
                {
                    return false;
                }
                if(xQueueReceiveFromISR(scpi_commands, &scpi_current, NULL) == pdFALSE)
                {
                    return false;
                }

                /* immediately restart loop */
                scpi_state = SCPI_STATE_OPC;
                scpi_retries = 0;
                nextTime = time;
                scpi_debug("Fetched new command");
                continue;

            case SCPI_STATE_OPC:
                /* too many failures already? */
                if(scpi_retries++ > scpi_opc_retries_max)
                {
                    scpi_debug("SCPI_STATE_OPC (scpi_retries++ > scpi_opc_retries_max) -> SCPI_STATE_IDLE");
                    scpi_current.resp(false, NULL);

                    /* immediately restart loop */
                    scpi_state = SCPI_STATE_IDLE;
                    nextTime = time;
                }
                else
                {
                    serial_println("*OPC?");
                    scpi_retries++;
                    scpi_request_time = millis();
                    nextTime = scpi_request_time + scpi_opc_timeout;

                    scpi_state = SCPI_STATE_OPC_RESPONSE;
                }
                break;

            case SCPI_STATE_OPC_RESPONSE:
                scpi_debug("> OPC timed out");
                //while(scpi_retries > 2);
                
                /* immediately restart loop, sending OPC again */
                scpi_state = SCPI_STATE_COMMAND;
                scpi_retries = 0;
                nextTime = time;
                break;

            case SCPI_STATE_COMMAND:
                scpi_debug("SCPI_STATE_COMMAND");
                serial_println("*CLS");
                delay(5);
                serial_println(scpi_current.command);

                if(scpi_current.has_response)
                {
                    scpi_debug("SCPI_STATE_COMMAND -> SCPI_STATE_RESPONSE");

                    scpi_state = SCPI_STATE_RESPONSE;
                    scpi_request_time = millis();
                    nextTime = scpi_request_time + scpi_request_timeout;
                }
                else
                {
                    scpi_debug("SCPI_STATE_COMMAND -> SCPI_STATE_IDLE");
                    scpi_current.resp(true, NULL);

                    /* immediately restart loop */
                    nextTime = time;
                    if(scpi_current.check_error)
                    {
                        delay(5);
                        scpi_state = SCPI_STATE_STB;
                        scpi_retries = 0;
                    }
                    else
                    {
                        scpi_state = SCPI_STATE_IDLE;
                    }
                }
                break;

            case SCPI_STATE_RESPONSE:
                scpi_debug("> RESPONSE timed out");
                //while(scpi_retries > 2);
                
                scpi_debug("SCPI_STATE_RESPONSE (timeout)");
                /* query timed out */
                if(scpi_current.resp)
                {
                    scpi_current.resp(false, NULL);
                }

                /* immediately restart loop */
                scpi_state = SCPI_STATE_IDLE;
                nextTime = time;
                break;

            case SCPI_STATE_STB:
                /* too many failures already? */
                if(scpi_retries++ > scpi_opc_retries_max)
                {
                    scpi_debug("SCPI_STATE_STB (scpi_retries++ > scpi_opc_retries_max) -> SCPI_STATE_IDLE");
                    if(scpi_current.resp)
                    {
                        scpi_current.resp(false, NULL);
                    }

                    /* immediately restart loop */
                    scpi_state = SCPI_STATE_IDLE;
                    nextTime = time;
                }
                else
                {
                    serial_println("*STB?");
                    scpi_retries++;
                    scpi_request_time = millis();
                    nextTime = scpi_request_time + scpi_opc_timeout;

                    scpi_state = SCPI_STATE_STB_RESPONSE;
                }
                break;

            case SCPI_STATE_STB_RESPONSE:
                scpi_debug("> STB timed out");
                //while(scpi_retries > 2);

                /* immediately restart loop, sending STB again */
                scpi_state = SCPI_STATE_STB;
                nextTime = time;
                break;

        }
    }

    scpi_state_changed = false;

    return scpi_state != SCPI_STATE_IDLE;
}

void dummy_cbr(bool success, const char *response)
{
}

bool scpi_command(const char *command, bool has_response, bool check_error, void (*cbr)(bool success, const char *response))
{
    t_scpi_command entry;

    strncpy(entry.command, command, sizeof(entry.command));
    entry.has_response = has_response;
    entry.check_error = check_error;
    entry.resp = cbr ? cbr : &dummy_cbr;

    DEBUG_PRINT("Queue new command: '%s'", command);

    return xQueueSend(scpi_commands, &entry, 0) == pdTRUE;
}

void scpi_parse(const char *line)
{
    switch(scpi_state)
    {
        case SCPI_STATE_OPC_RESPONSE:
            if(line[0] == '1')
            {
                scpi_state = SCPI_STATE_COMMAND;
                scpi_retries = 0;
                scpi_state_changed = true;
                break;
            }
            break;

        case SCPI_STATE_RESPONSE:
            scpi_current.resp(true, line);
            if(scpi_current.check_error)
            {
                scpi_state = SCPI_STATE_STB;
            }
            else
            {
                scpi_state = SCPI_STATE_IDLE;
            }
            scpi_retries = 0;
            scpi_state_changed = true;
            break;

        case SCPI_STATE_STB_RESPONSE:
            int status = atoi(line);

            if(status & 4)
            {
                scpi_state = SCPI_STATE_COMMAND;
                scpi_state_changed = true;
                break;
            }
            scpi_state = SCPI_STATE_IDLE;
            scpi_state_changed = true;
            break;
    }
}

void scpi_cb(uint8_t ch)
{
    if(ch == '\r')
    {
    }
    else if(ch == '\n')
    {
        scpi_line_buffer[scpi_line_pos] = 0;
        scpi_line_pos = 0;
        scpi_parse((const char *)scpi_line_buffer);
    }
    else if((scpi_line_pos + 1) < sizeof(scpi_line_buffer))
    {
        scpi_line_buffer[scpi_line_pos++] = ch;
    }
}