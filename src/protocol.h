#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#include "error.h"
#include "socket.h"

#define MAX_PACKET_DATA_SIZE 1024

typedef enum {
	PACKET_GET_IMAGE_INFO,
	PACKET_SET_IMAGE_INFO,
	PACKET_GET_IMAGE_DATA,
	PACKET_SET_IMAGE_DATA
} PacketType;

typedef struct {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
} PacketInfo;

typedef struct {
	uint32_t start;
	uint16_t size;
	uint8_t *data;
} PacketData;

typedef struct {
	PacketType type;
	union {
		PacketInfo info;
		PacketData data;
	};
} Packet;

Error Packet_Send(const Packet *p, Socket *s);
Error Packet_Recv(Packet *p, Socket *s);
Error Packet_Req(const Packet *req, Socket *s, int repeats, Packet *p);

typedef struct {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
	uint8_t *data;
	uint32_t dataCapacity;
} ImageTranferer;

Error ImageTranferer_Req(ImageTranferer *img, Socket *s, int repeats);
Error ImageTranferer_Res(ImageTranferer *img, Socket *s);

#endif
