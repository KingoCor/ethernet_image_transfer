#include "protocol.h"
#include <string.h>

// type+PacketData/PacketInfo+data
uint8_t buf[1+6+MAX_PACKET_DATA_SIZE] = {0};

Error Packet_Send(const Packet *p, Socket *s) {
	if (!p || !s) return ERROR_ARG;

	int bufSize = 0;

	buf[0] = (uint8_t)p->type;
	
	if (p->type==PACKET_GET_IMAGE_INFO) {
		bufSize = 1;
	} else if (p->type==PACKET_SET_IMAGE_INFO) {
		uint16_t width = htons(p->info.width);
		uint16_t height = htons(p->info.height);
		memcpy(&buf[1], &width, 2);
		memcpy(&buf[3], &height, 2);
		buf[5] = p->info.channels;
		bufSize = 6;
	} else if (p->type==PACKET_GET_IMAGE_DATA) {
		uint32_t start = htonl(p->data.start);
		uint16_t size = htons(p->data.size);
		memcpy(&buf[1], &start, 4);
		memcpy(&buf[5], &size, 2);
		bufSize = 7;
	} else if (p->type==PACKET_SET_IMAGE_DATA) {
		uint32_t start = htonl(p->data.start);
		uint16_t size = htons(p->data.size);
		memcpy(&buf[1], &start, 4);
		memcpy(&buf[5], &size, 2);
		memcpy(&buf[7], p->data.data, p->data.size);
		bufSize = 7+p->data.size;
	} else return ERROR_ARG;
	
	return Socket_Send(s, (char *)buf, bufSize, NULL);
}

Error Packet_Recv(Packet *p, Socket *s) {
	if (!p || !s) return ERROR_ARG;
	
	int bufSize = 0;
	Error err = Socket_Recv(s, (char *)&buf, 1+6+MAX_PACKET_DATA_SIZE, &bufSize);
	if (err) return err;
	if (bufSize==0) return ERROR_READ;

	p->type = (PacketType)buf[0];

	if (p->type==PACKET_SET_IMAGE_INFO) {
		if (bufSize<6) return ERROR_READ;
		uint16_t net;
		memcpy(&net, &buf[1], 2);
		p->info.width = ntohs(net);
		memcpy(&net, &buf[3], 2);
		p->info.height = ntohs(net);
		p->info.channels = buf[5];
	} else if (p->type==PACKET_GET_IMAGE_DATA) {
		if (bufSize<7) return ERROR_READ;
		uint32_t start;
		memcpy(&start, &buf[1], 4);
		p->data.start = ntohl(start);
		uint16_t size;
		memcpy(&size, &buf[5], 2);
		p->data.size = ntohs(size);
	} else if (p->type==PACKET_SET_IMAGE_DATA) {
		if (bufSize<7) return ERROR_READ;
		uint32_t start;
		memcpy(&start, &buf[1], 4);
		p->data.start = ntohl(start);
		uint16_t size;
		memcpy(&size, &buf[5], 2);
		p->data.size = ntohs(size);
		if (bufSize-7<p->data.size) return ERROR_READ;
		memcpy(p->data.data, &buf[7], p->data.size);
	}

	return ERROR_OK;
}

Error Packet_Req(const Packet *req, Socket *s, int repeats, Packet *p) {
	if (!s || !p) return ERROR_ARG;

	for (int i=0; i<repeats || repeats<=0; ++i) {
		Error err = Packet_Send(req, s);
		if (err) continue;
		err = Packet_Recv(p, s);
		if (err) continue;
		return ERROR_OK;
	}

	return ERROR_TIMEOUT;
}

Error ImageTranferer_Req(ImageTranferer *img, Socket *s, int repeats) {
	if (!img || !s) return ERROR_ARG;

	uint8_t data[MAX_PACKET_DATA_SIZE];
	Packet req, res;

	req.type = PACKET_GET_IMAGE_INFO;
	Error err = Packet_Req(&req, s, repeats, &res);
	if (err) return err;

	img->width = res.info.width;
	img->height = res.info.height;
	img->channels = res.info.channels;

	req.type = PACKET_GET_IMAGE_DATA;
	res.data.data = data;

	uint32_t size = img->width*img->height*img->channels;
	int remains = img->dataCapacity>size ? size : img->dataCapacity;
	uint32_t offset = 0;
	while (remains>0) {
		req.data.start = offset;
		req.data.size = remains>MAX_PACKET_DATA_SIZE ? MAX_PACKET_DATA_SIZE : remains;

		err = Packet_Req(&req, s, repeats, &res);
		if (err) return err;

		memcpy(img->data+res.data.start, res.data.data, res.data.size);

		offset += res.data.size;
		remains -= res.data.size;
	}

	return ERROR_OK;
}

Error ImageTranferer_Res(ImageTranferer *img, Socket *s) {
	if (!img || !s) return ERROR_ARG;

	Packet p;
	Error err = Packet_Recv(&p, s);
	if (err) return err;

	if (p.type==PACKET_GET_IMAGE_INFO) {
		p.type = PACKET_SET_IMAGE_INFO;
		p.info.width = img->width;
		p.info.height = img->height;
		p.info.channels = img->channels;
	} else if (p.type==PACKET_GET_IMAGE_DATA) {
		p.type = PACKET_SET_IMAGE_DATA;
		uint32_t size = img->width*img->height*img->channels;
		if (p.data.start>=size) p.data.start = size-1;
		if (p.data.start+p.data.size>size) p.data.size = size-p.data.start;
		p.data.data = img->data+p.data.start;
	}

	return Packet_Send(&p, s);
}

