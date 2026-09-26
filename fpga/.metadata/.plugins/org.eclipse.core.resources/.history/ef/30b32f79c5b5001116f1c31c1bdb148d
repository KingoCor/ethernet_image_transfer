#include "server.h"

#define MAX_PACKET_DATA_SIZE 1024

#define PACKET_INFO_SIZE 6 // type + w(2) + h(2) + ch
#define PACKET_DATA_SIZE 7 // type + start(4) + size(2)

typedef enum {
    PACKET_GET_IMAGE_INFO = 0,
    PACKET_SET_IMAGE_INFO = 1,
    PACKET_GET_IMAGE_DATA = 2,
    PACKET_SET_IMAGE_DATA = 3
} PacketType;

static struct udp_pcb *pcb;
static struct {
	uint8_t *data;
	uint16_t width;
	uint16_t height;
	uint8_t channels;
} image;



static inline uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
static inline uint32_t rd_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}
static inline void wr_u16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v;
}
static inline void wr_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)v;
}

void set_image(uint8_t *data, uint16_t width, uint16_t height, uint8_t channels) {
	image.data = data;
	image.width = width;
	image.height = height;
	image.channels = channels;
}

static void send_pbuf(struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (!p) return;
    err_t e = udp_sendto(pcb, p, addr, port);
    if (e != ERR_OK) xil_printf("udp_sendto err=%d\r\n", e);
    pbuf_free(p);
}

static void handle_get_info(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, PACKET_INFO_SIZE, PBUF_RAM);
    if (!p) return;

    uint8_t *b = (uint8_t *)p->payload;
    b[0] = (uint8_t)PACKET_SET_IMAGE_INFO;
    wr_u16(&b[1], (uint16_t)image.width);
    wr_u16(&b[3], (uint16_t)image.height);
    b[5] = image.channels;

    send_pbuf(pcb, p, addr, port);
}

static void handle_get_data(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port,  const uint8_t *payload, uint16_t len) {
    if (len<PACKET_DATA_SIZE) return;
    if (!image.data) return;

    uint32_t start    = rd_u32(&payload[1]);
    uint16_t req_size = rd_u16(&payload[5]);

    uint32_t total = image.width*image.height*image.channels;

    uint16_t size;
    if (start>=total) {
        size = 0;
    } else {
        uint32_t rem = total-start;
        size = req_size;
        if (size>rem) size = (uint16_t)rem;
        if (size>MAX_PACKET_DATA_SIZE) size = MAX_PACKET_DATA_SIZE;
    }

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, PACKET_DATA_SIZE+size, PBUF_RAM);
    if (!p) return;

    uint8_t *b = (uint8_t *)p->payload;
    b[0] = (uint8_t)PACKET_SET_IMAGE_DATA;
    wr_u32(&b[1], start);
    wr_u16(&b[5], size);
    if (size) memcpy(&b[7], image.data+start, size);

    send_pbuf(pcb, p, addr, port);
}

static void udp_recv_cb(void *arg, struct udp_pcb *tpcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
	if (!p) return;

	uint8_t buf[1+PACKET_DATA_SIZE+MAX_PACKET_DATA_SIZE];
	uint16_t len = p->tot_len > sizeof(buf) ? sizeof(buf) : p->tot_len;
	pbuf_copy_partial(p, buf, len, 0);
	pbuf_free(p);

	if (len < 1) return;

	switch (buf[0]) {
		case PACKET_GET_IMAGE_INFO:
			handle_get_info(pcb, addr, port);
			break;

		case PACKET_GET_IMAGE_DATA:
			handle_get_data(pcb, addr, port, buf, len);
			break;

		default:
			break;
	}
}

void print_app_header(void) {
	xil_printf("UDP server listening on port %d\r\n", UDP_CONN_PORT);
}

void start_application(void) {
	err_t err;

	pcb = udp_new();
	if (!pcb) {
		xil_printf("UDP server: Error creating PCB. Out of Memory\r\n");
		return;
	}

	err = udp_bind(pcb, IP_ADDR_ANY, UDP_CONN_PORT);
	if (err != ERR_OK) {
		xil_printf("UDP server: Unable to bind to port");
		xil_printf(" %d: err = %d\r\n", UDP_CONN_PORT, err);
		udp_remove(pcb);
		return;
	}

	udp_recv(pcb, udp_recv_cb, NULL);

	return;
}
