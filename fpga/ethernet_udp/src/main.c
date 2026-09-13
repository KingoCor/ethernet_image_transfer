#include "xparameters.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "platform_config.h"
#include "lwipopts.h"
#include "xil_printf.h"
#include "sleep.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/init.h"
#include "lwip/inet.h"
#include "xil_cache.h"

#include "lwip/dhcp.h"
extern volatile int dhcp_timoutcntr;

extern volatile int TcpFastTmrFlag;
extern volatile int TcpSlowTmrFlag;

#define DEFAULT_IP_ADDRESS	"192.168.1.10"
#define DEFAULT_IP_MASK		"255.255.255.0"
#define DEFAULT_GW_ADDRESS	"192.168.1.1"

struct netif server_netif;

#define IMAGE_WIDTH 640
#define IMAGE_HEIGHT 640
#define IMAGE_CHANNELS 3
#define IMAGE_SIZE IMAGE_WIDTH*IMAGE_HEIGHT*IMAGE_CHANNELS
static uint8_t data[IMAGE_SIZE];

static void print_ip(char *msg, ip_addr_t *ip) {
	print(msg);
	xil_printf("%d.%d.%d.%d\r\n", ip4_addr1(ip), ip4_addr2(ip),
			ip4_addr3(ip), ip4_addr4(ip));
}

static void print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw) {
	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}

static void assign_default_ip(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw) {
	int err;

	xil_printf("Configuring default IP %s \r\n", DEFAULT_IP_ADDRESS);

	err = inet_aton(DEFAULT_IP_ADDRESS, ip);
	if (!err) xil_printf("Invalid default IP address: %d\r\n", err);

	err = inet_aton(DEFAULT_IP_MASK, mask);
	if (!err) xil_printf("Invalid default IP MASK: %d\r\n", err);

	err = inet_aton(DEFAULT_GW_ADDRESS, gw);
	if (!err) xil_printf("Invalid default gateway address: %d\r\n", err);
}

void init_image() {
	for (int y=0; y<IMAGE_HEIGHT; ++y) {
		for (int x=0; x<IMAGE_WIDTH; ++x) {
			int i = (y*IMAGE_WIDTH+x)*IMAGE_CHANNELS;
			data[i+0] = x%256;
			data[i+1] = y%256;
			data[i+2] = 0;
		}
	}
}

int main(void) {
	struct netif *netif;

	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] = {0x00, 0x0a, 0x35, 0x00, 0x01, 0x02};

	netif = &server_netif;

	init_platform();

	xil_printf("\r\n\r\n");
	xil_printf("-----lwIP RAW Mode UDP Server Application-----\r\n");

	/* initialize lwIP */
	lwip_init();

	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(netif, NULL, NULL, NULL, mac_ethernet_address,PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\r\n");
		return -1;
	}
	netif_set_default(netif);

	/* now enable interrupts */
	platform_enable_interrupts();

	/* specify that the network if is up */
	netif_set_up(netif);

	/* Create a new DHCP client for this interface.
	 * Note: you must call dhcp_fine_tmr() and dhcp_coarse_tmr() at
	 * the predefined regular intervals after starting the client.
	 */
	dhcp_start(netif);
	dhcp_timoutcntr = 24;
	while (((netif->ip_addr.addr)==0) && (dhcp_timoutcntr > 0)) xemacif_input(netif);

	if (dhcp_timoutcntr<=0) {
		if ((netif->ip_addr.addr)==0) {
			xil_printf("ERROR: DHCP request timed out\r\n");
			assign_default_ip(&(netif->ip_addr),&(netif->netmask),&(netif->gw));
		}
	}

	/* print IP address, netmask and gateway */
	print_ip_settings(&(netif->ip_addr), &(netif->netmask), &(netif->gw));

	xil_printf("\r\n");

	/* print app header */
	print_app_header();

	/* start the application*/
	init_image();
	set_image(data, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);
	start_application();
	xil_printf("\r\n");

	while (1) {
		if (TcpFastTmrFlag) {
			tcp_fasttmr();
			TcpFastTmrFlag = 0;
		}
		if (TcpSlowTmrFlag) {
			tcp_slowtmr();
			TcpSlowTmrFlag = 0;
		}
		xemacif_input(netif);
	}

	/* never reached */
	cleanup_platform();

	return 0;
}
