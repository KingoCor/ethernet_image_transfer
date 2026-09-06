#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#include "error.h"
#include "socket.h"

#define MAX_PACKET_DATA_SIZE 1024

typedef enum {
	PACKET_GET_IMAGE,
	PACKET_SET_IMAGE_INFO,
	PACKET_SET_IMAGE
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
Error Packet_Recv(Packet *p, Socket *s, int timeout);

typedef struct {
	uint16_t width;
	uint16_t height;
	uint8_t channels;
	uint8_t *data;
	uint32_t dataCapacity;
} ImageTranferer;

Error ImageTranferer_Send(ImageTranferer *img, Socket *s);
Error ImageTranferer_Recv(ImageTranferer *img, Socket *s, int timeout);

#endif
