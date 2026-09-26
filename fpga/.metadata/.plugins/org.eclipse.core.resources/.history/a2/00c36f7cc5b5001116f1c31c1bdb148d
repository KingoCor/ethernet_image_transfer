#ifndef SERVER_H
#define SERVER_H

#include "lwipopts.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "lwip/inet.h"
#include "xil_printf.h"
#include "platform.h"
#include <stdint.h>

#define UDP_CONN_PORT 5001

extern struct netif server_netif;

void set_image(uint8_t *data, uint16_t width, uint16_t height, uint8_t channels);
void start_application(void);
void print_app_header(void);

#endif
