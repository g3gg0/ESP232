
WiFiServer debug_server(2323);
WiFiClient debug_client;
char debug_buf[1024];
char dbg_status[1024];

bool debug_started = false;
bool debug_connected = false;
int dbg_last_activity = 0;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define COUNT(x) (sizeof(x)/sizeof(x[0]))

typedef struct
{
    const char *name;
    const char *desc;
    uint32_t address;
    const char *access;
} t_dbg_per;


t_dbg_per dbg_registers[] =  {
    // regex (UART_[A-Z0-9_]*) ([a-zA-Z0-9_\- ]+) (0x[0-9A-F]{8}) (0x[0-9A-F]{8}) (0x[0-9A-F]{8}) ([RWO/]+)
    // Configuration registers 
    { "UART_CONF0_REG", "Configuration register 0", 0x3FF40020, "R/W" }, 
    { "UART_CONF1_REG", "Configuration register 1", 0x3FF40024, "R/W" }, 
    { "UART_CLKDIV_REG", "Clock divider configuration", 0x3FF40014, "R/W" }, 
    { "UART_FLOW_CONF_REG", "Software flow-control configuration", 0x3FF40034, "R/W" }, 
    { "UART_SWFC_CONF_REG", "Software flow-control character configuration", 0x3FF4003C, "R/W" }, 
    { "UART_SLEEP_CONF_REG", "Sleep-mode configuration", 0x3FF40038, "R/W" }, 
    { "UART_IDLE_CONF_REG", "Frame-end idle configuration", 0x3FF40040, "R/W" }, 
    { "UART_RS485_CONF_REG", "RS485 mode configuration", 0x3FF40044, "R/W" }, 
    // Status registers 
    { "UART_STATUS_REG", "UART status register", 0x3FF4001C, "RO" }, 
    // Autobaud registers 
    { "UART_AUTOBAUD_REG", "Autobaud configuration register", 0x3FF40018, "R/W" }, 
    { "UART_LOWPULSE_REG", "Autobaud minimum low pulse duration register", 0x3FF40028, "RO" }, 
    { "UART_HIGHPULSE_REG", "Autobaud minimum high pulse duration register", 0x3FF4002C, "RO" }, 
    { "UART_POSPULSE_REG", "Autobaud high pulse register", 0x3FF40068, "RO" }, 
    { "UART_NEGPULSE_REG", "Autobaud low pulse register", 0x3FF4006C, "RO" }, 
    { "UART_RXD_CNT_REG", "Autobaud edge change count register", 0x3FF40030, "RO" }, 
    // AT escape seqence detection configuration 
    { "UART_AT_CMD_PRECNT_REG", "Pre-sequence timing configuration", 0x3FF40048, "R/W" }, 
    { "UART_AT_CMD_POSTCNT_REG", "Post-sequence timing configuration", 0x3FF4004C, "R/W" }, 
    { "UART_AT_CMD_GAPTOUT_REG", "Timeout configuration", 0x3FF40050, "R/W" }, 
    { "UART_AT_CMD_CHAR_REG", "AT escape sequence detection configuration", 0x3FF40054, "R/W" }, 
    //FIFO configuration
    { "UART_FIFO_REG", "FIFO data register", 0x3FF40000, "R/W" }, 
    { "UART_MEM_CONF_REG", "UART threshold and allocation configuration", 0x3FF40058, "R/W" }, 
    { "UART_MEM_CNT_STATUS_REG", "Receive and transmit memory configuration", 0x3FF40064, "RO" }, 
    //Interrupt registers 
    { "UART_INT_RAW_REG", "Raw interrupt status", 0x3FF40004, "RO" }, 
    { "UART_INT_ST_REG", "Masked interrupt status", 0x3FF40008, "RO" }, 
    { "UART_INT_ENA_REG", "Interrupt enable bits", 0x3FF4000C, "R/W" }, 
    { "UART_INT_CLR_REG", "Interrupt clear bits", 0x3FF40010, "WO" }    
};

uint32_t *dbg_last_values = NULL;
uint32_t *dbg_last_update = NULL;

void dbg_setup()
{
    if (!debug_started)
    {
        debug_server.begin();
        debug_server.setNoDelay(true);
    }
    dbg_last_values = (uint32_t *)malloc(sizeof(uint32_t) * COUNT(dbg_registers));
    dbg_last_update = (uint32_t *)malloc(sizeof(uint32_t) * COUNT(dbg_registers));
    strcpy(dbg_status, "");
    debug_started = true;
}

uint32_t dbg_read(uint32_t address)
{
    volatile uint32_t *ptr = (volatile uint32_t *)address;

    return *ptr;
}

void dbg_write(uint32_t address, uint32_t value)
{
    volatile uint32_t *ptr = (volatile uint32_t *)address;

    *ptr = value;
}


bool dbg_loop()
{
    uint32_t time = millis();

    /* network part */
    if (debug_server.hasClient())
    {
        debug_connected = true;

        if (debug_client != NULL)
        {
            debug_client.stop();
        }

        debug_client = debug_server.available();
        char remote[32];

        sprintf(remote, "%s:%d", debug_client.remoteIP().toString().c_str(), debug_client.remotePort());
    }

    if (!debug_connected)
    {
        return false;
    }

    if (!debug_client.connected())
    {
        debug_connected = false;
        debug_client.stop();
        return false;
    }

    int avail = debug_client.available();
    if (avail > 0)
    {
        dbg_last_activity = time;

        int net_avail = MIN(sizeof(debug_buf), avail);
        int net_read = debug_client.read((uint8_t *)debug_buf, net_avail);

        if(net_read > 0)
        {
            dbg_parse(debug_buf);
        }
    }

    static int nextTime = 0;
    static int nextTimeRefresh = 0;

    if(time >= nextTime)
    {
        bool update = false;
        bool refresh = (time >= nextTimeRefresh);

        for(int pos = 0; pos < COUNT(dbg_registers); pos++)
        {
            uint32_t value = dbg_read(dbg_registers[pos].address);
            if(value != dbg_last_values[pos])
            {
                dbg_last_values[pos] = value;
                dbg_last_update[pos] = time;
                update = true;
            }
        }

        if(refresh)
        {
            nextTimeRefresh = time + 10000;
        }

        if(update)
        {
            debug_client.printf("\x1b[0;0H\n Debug interface\n");
            debug_client.printf("-----------------\n");
            for(int pos = 0; pos < COUNT(dbg_registers); pos++)
            {
                const char *high_str = " ";
                const char *norm_str = " ";
                bool draw = refresh;

                if(time - dbg_last_update[pos] < 1000)
                {
                    high_str = "\x1b[36m>";
                    norm_str = "\x1b[0m";
                    draw = true;
                }

                if(draw)
                {
                    debug_client.printf("%s  %32s  0x%08X '%s' %s\n", high_str, dbg_registers[pos].name, dbg_last_values[pos], dbg_registers[pos].desc, norm_str);
                }
                else
                {
                    debug_client.printf("\x1b[1B");
                }
            }
            debug_client.printf("\n Status> %s\n", dbg_status);
        }
        nextTime = time + 250;
    }

    bool activity = (time - dbg_last_activity) < 1000;

    return activity;
}

void dbg_parse(char *buf)
{
    int start = 0;

    while(buf[start] && buf[start] == ' ')
    {
        start++;
    }

    switch(buf[start])
    {
        case 'w':
            start++;
            start++;
            int len = 0;

            while(buf[start + len] && (buf[start + len] != ' '))
            {
                len++;
            }

            int reg = -1;
            for(int pos = 0; pos < COUNT(dbg_registers); pos++)
            {
                if(!strncmp(&buf[start], dbg_registers[pos].name, len))
                {
                    reg = pos;
                }
            }

            if(reg == -1)
            {
                sprintf(dbg_status, "Unknown register in command '%s'", buf);
                return;
            }

            int value_start = start + len;

            while(buf[value_start] && (buf[value_start] != ' '))
            {
                value_start++;
            }
            value_start++;

//w UART_INT_CLR_REG 0xFFFF
//w UART_INT_ENA_REG 0x195
//w UART_FLOW_CONF_REG 0x01
//w UART_FLOW_CONF_REG 0x00


            uint32_t value = (uint32_t)strtol(&buf[value_start], NULL, 0);

            sprintf(dbg_status, "Writing 0x%08X to %s", value, dbg_registers[reg].name);
            dbg_write(dbg_registers[reg].address, value);

            break;
    }
}