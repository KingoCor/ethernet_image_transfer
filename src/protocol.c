#include "protocol.h"
#include <stdio.h>
#include <string.h>

// type+PacketData/PacketInfo+data
uint8_t buf[1+6+MAX_PACKET_DATA_SIZE] = {0};
uint8_t ackBuf;
uint8_t ACK = PACKET_ACK;

Error Packet_Send(const Packet *p, Socket *s) {
	if (!p || !s) return ERROR_ARG;

	int bufSize = 0;

	buf[0] = (uint8_t)p->type;
	
	if (p->type==PACKET_GET_IMAGE) {
		bufSize = 1;
	} else if (p->type==PACKET_SET_IMAGE_INFO) {
		uint16_t width = htons(p->info.width);
		uint16_t height = htons(p->info.height);
		memcpy(&buf[1], &width, 2);
		memcpy(&buf[3], &height, 2);
		buf[5] = p->info.channels;
		bufSize = 6;
	} else if (p->type==PACKET_SET_IMAGE) {
		uint32_t start = htonl(p->data.start);
		uint16_t size = htons(p->data.size);
		memcpy(&buf[1], &start, 4);
		memcpy(&buf[5], &size, 2);
		memcpy(&buf[7], p->data.data, p->data.size);
		bufSize = 8+p->data.size;
	} else return ERROR_ARG;
	
	for (int i=0; i<REPEATS; ++i) {
		Error err = Socket_Send(s, (char *)buf, bufSize, NULL);
		if (err) continue;
		err = Socket_Recv(s, (char *)&ackBuf, 1, NULL, 10);
		if (err) continue;
		if (ackBuf==PACKET_ACK) return ERROR_OK;
	}

	return ERROR_WRITE;
}

Error Packet_Recv(Packet *p, Socket *s, int timeout) {
	if (!p || !s) return ERROR_ARG;
	
	int bufSize = 0;
	Error err = Socket_Recv(s, (char *)&buf, 1+6+MAX_PACKET_DATA_SIZE, &bufSize, timeout);
	if (err) return err;
	if (bufSize==0) return ERROR_READ;

	p->type = (PacketType)buf[0];

	if (p->type==PACKET_GET_IMAGE) {
	} else if (p->type==PACKET_SET_IMAGE_INFO) {
		if (bufSize<6) return ERROR_READ;
		uint16_t net;
		memcpy(&net, &buf[1], 2);
		p->info.width = ntohs(net);
		memcpy(&net, &buf[3], 2);
		p->info.height = ntohs(net);
		p->info.channels = buf[5];
	} else if (p->type==PACKET_SET_IMAGE) {
		if (bufSize<8) return ERROR_READ;
		uint32_t start;
		memcpy(&start, &buf[1], 4);
		p->data.start = ntohl(start);
		uint16_t size;
		memcpy(&size, &buf[5], 2);
		p->data.size = ntohs(size);
		if (bufSize-7<p->data.size) return ERROR_READ;
		memcpy(p->data.data, &buf[7], p->data.size);
	} else return ERROR_ARG;

	Packet ack;
	ack.type = PACKET_ACK;
	int sentLen = 0;
	for (int i=0; i<REPEATS; ++i) {
		Error err = Socket_Send(s, (char *)&ACK, 1, &sentLen);
		if (!err && sentLen>0) return ERROR_OK;
	}

	return ERROR_READ;
}


Error ImageTranferer_Send(ImageTranferer *img, Socket *s) {
	if (!img || !s) return ERROR_ARG;

	Packet p;
	p.type = PACKET_SET_IMAGE_INFO;
	p.info.width = img->width;
	p.info.height = img->height;
	p.info.channels = img->channels;
	Packet_Send(&p, s);

	p.type = PACKET_SET_IMAGE;

	int size = img->width*img->height*img->channels;
	int count = 0;
	for (int i=0; i<size; i+=MAX_PACKET_DATA_SIZE) {
		p.data.start = i;
		p.data.size = size-i;
		if (p.data.size>MAX_PACKET_DATA_SIZE) p.data.size = MAX_PACKET_DATA_SIZE;
		p.data.data = &img->data[i];
		Packet_Send(&p, s);
		count += 1;
	}

	return ERROR_OK;
}

Error ImageTranferer_Recv(ImageTranferer *img, Socket *s, int timeout) {
	if (!img || !s) return ERROR_ARG;

	uint8_t data[MAX_PACKET_DATA_SIZE];

	Packet p;
	p.type = PACKET_GET_IMAGE;
	p.data.data = data;

	Error err = Packet_Send(&p, s);
	if (err) return err;

	int count = 0;
	int estimate = -1;
	while (count<estimate || estimate<0) {
		err = Packet_Recv(&p, s, timeout);
		if (err==ERROR_TIMEOUT) {
			printf("Received only %d/%d\n",count,estimate);
			return ERROR_TIMEOUT;
		}

		if (p.type==PACKET_SET_IMAGE_INFO) {
			int size = p.info.width*p.info.height*p.info.channels;
			estimate = size/MAX_PACKET_DATA_SIZE;
			if (size%MAX_PACKET_DATA_SIZE>0) estimate+=1;

			img->width = p.info.width;
			img->height = p.info.height;
			img->channels = p.info.channels;
			p.data.data = data;
		} else if (p.type==PACKET_SET_IMAGE) {
			int size = p.data.size;
			if (size+p.data.start>img->dataCapacity) {
				size = img->dataCapacity-p.data.start;
				if (size<0) size = 0;
			}

			memcpy(img->data+p.data.start, p.data.data, size);

			count+=1;
		}
	}

	return ERROR_OK;
}
