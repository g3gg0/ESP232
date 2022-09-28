
#include <esp_rom_gpio.h>

#define TEST_ESP_OK(x,ret)              do { if((x) != ESP_OK) return ret; } while(0)
#define TEST_ASSERT_NOT_EQUAL(x,y) 
#define TEST_ASSERT_EQUAL(x,y)
#define TEST_ASSERT_NOT_NULL(x) 

const uart_port_t uart_echo = UART_NUM_1;
char sim_rxbuf[1024];
int sim_rxpos = 0;

int32_t sim_setup()
{
    uart_config_t uart_config =
    {
        .baud_rate = (int)current_config.baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    const int uart0_tx_signal = U0TXD_OUT_IDX;
    const int uart1_tx_signal = U1TXD_OUT_IDX;
    const int uart0_tx = 1;
    const int uart0_rx = 13;
    const int uart1_tx = 14;
    const int uart1_rx = 1;
    const int buf_size = 256;
    const int intr_alloc_flags = 0;


    TEST_ESP_OK(uart_driver_install(uart_echo, buf_size * 2, 0, 0, NULL, intr_alloc_flags), -1);
    TEST_ESP_OK(uart_param_config(uart_echo, &uart_config), -2);
    TEST_ESP_OK(uart_set_pin(uart_echo, uart1_tx, uart1_rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), -3);
    

    esp_rom_gpio_connect_out_signal(uart0_rx, uart1_tx_signal, false, false);
    esp_rom_gpio_connect_out_signal(uart1_rx, uart0_tx_signal, false, false);
    

    return 0;
}

void sim_parse(char *line)
{
    const char *response = "";

    if(line[0] == '>')
    {
        return;
    }

    if(!strcmp(line, "*STB?"))
    {
        response = "0\n";
        //serial_println("Received STB");
    }
    else if(!strcmp(line, "*OPC?"))
    {
        response = "1\n";
        //serial_println("Received OPC");
    }
    else if(!strcmp(line, "*CLS"))
    {
        response = "";
        //serial_println("Received CLS");
    }
    else if(!strcmp(line, "DISP:WIND1:DATA?"))
    {
        response = "\" -0.0000 A\"\n";
        //serial_println("Received DISP");
    }

    uart_write_bytes(uart_echo, response, strlen(response));
}


bool sim_loop()
{
    const uart_port_t uart_echo = UART_NUM_1;
    const int buf_size = 256;

    while(true)
    {
        char rcvd;
        int len = uart_read_bytes(uart_echo, &rcvd, 1, 1);

        if(len <= 0)
        {
            break;
        }

        char msg[2];

        msg[0] = rcvd;
        msg[1] = 0;

        if(rcvd == '\n')
        {
            sim_rxbuf[sim_rxpos] = '\000';
            sim_parse(sim_rxbuf);
            sim_rxpos = 0;
        }
        else if(rcvd == '\r')
        {
        }
        else
        {
            sim_rxbuf[sim_rxpos++] = rcvd;
            sim_rxpos %= sizeof(sim_rxbuf);
        }
    }

    return true;
}
