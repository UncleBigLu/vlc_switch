#ifndef _TCP_CLIENT_H_
#define _TCP_CLIENT_H_
#include <string.h>
#include "myadc.h"

#define SERVER_HOSTNAME "ublhw.local"
#define SERVER_PORT "8888"

void vtcp_client_task(void *pvParameters);
void tcp_server();
static int connect_to_server(void *dest_addr);

typedef struct {
    adc_continuous_handle_t adc_handle;
    char *dest_addr;
}tcp_client_param_t;

#endif