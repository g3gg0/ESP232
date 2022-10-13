#ifndef __SCPI_H__
#define __SCPI_H__

#define SCPI_STATE_IDLE          0
#define SCPI_STATE_STB           1
#define SCPI_STATE_STB_RESPONSE  2
#define SCPI_STATE_OPC           3
#define SCPI_STATE_OPC_RESPONSE  4
#define SCPI_STATE_COMMAND       5
#define SCPI_STATE_RESPONSE      6

#define SCPI_MAX_COMMAND  64

typedef struct
{
    char command[SCPI_MAX_COMMAND];
    bool has_response;
    bool check_error;
    void (*resp)(bool success, const char *line);
} t_scpi_command;


void scpi_setup(void);
bool scpi_loop(void);
bool scpi_command(const char *command, bool has_response, bool check_error, void (*cbr)(bool success, const char *response));

#endif