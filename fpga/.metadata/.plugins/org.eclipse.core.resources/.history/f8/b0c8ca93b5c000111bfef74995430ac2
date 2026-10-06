#include "protocol.h"
#include <string.h>

static void write_u16(uint8_t *buff, uint16_t value) {
    buff[0] = (uint8_t)( value        & 0xFF);
    buff[1] = (uint8_t)((value >> 8)  & 0xFF);
}

static void write_u32(uint8_t *buff, uint32_t value) {
    buff[0] = (uint8_t)( value        & 0xFF);
    buff[1] = (uint8_t)((value >> 8)  & 0xFF);
    buff[2] = (uint8_t)((value >> 16) & 0xFF);
    buff[3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint16_t read_u16(const uint8_t *buff) {
    return (uint16_t)buff[0] | ((uint16_t)buff[1] << 8);
}

static uint32_t read_u32(const uint8_t *buff) {
    return  (uint32_t)buff[0]
         | ((uint32_t)buff[1] << 8)
         | ((uint32_t)buff[2] << 16)
         | ((uint32_t)buff[3] << 24);
}

Error Packet_Serialize(const Packet *p, uint8_t *buff, uint32_t size, uint32_t *sizeUsed) {
    if (!p || !buff) return ERROR_ARG;
    if (size < 1) return ERROR_ARG;
    if (p->type > PACKET_SET_STREAM_DATA) return ERROR_ARG;

    buff[0] = (uint8_t)p->type;
    uint32_t offset = 1;

    switch (p->type) {

    case PACKET_GET_STREAM_INFO:
        if (sizeUsed) *sizeUsed = offset;
        return ERROR_OK;

    case PACKET_SET_STREAM_INFO:
        if (size < offset + 5) return ERROR_ARG;
        write_u16(&buff[offset], p->info.width);  offset += 2;
        write_u16(&buff[offset], p->info.height); offset += 2;
        buff[offset++] = p->info.channels;
        if (sizeUsed) *sizeUsed = offset;
        return ERROR_OK;

    case PACKET_START_STREAM:
        if (sizeUsed) *sizeUsed = offset;
        return ERROR_OK;

    case PACKET_END_STREAM:
        if (sizeUsed) *sizeUsed = offset;
        return ERROR_OK;

    case PACKET_GET_STREAM_DATA:
        if (size < offset + 10) return ERROR_ARG;
        write_u32(&buff[offset], p->data.frame); offset += 4;
        write_u32(&buff[offset], p->data.start); offset += 4;
        write_u16(&buff[offset], p->data.size);  offset += 2;
        if (sizeUsed) *sizeUsed = offset;
        return ERROR_OK;

    case PACKET_SET_STREAM_DATA:
        if (p->data.size > 0 && !p->data.data) return ERROR_ARG;
        if (size < offset + 10u + p->data.size) return ERROR_ARG;
        write_u32(&buff[offset], p->data.frame); offset += 4;
        write_u32(&buff[offset], p->data.start); offset += 4;
        write_u16(&buff[offset], p->data.size);  offset += 2;
        if (p->data.size) {
            memcpy(&buff[offset], p->data.data, p->data.size);
            offset += p->data.size;
        }
        if (sizeUsed) *sizeUsed = offset;
        return ERROR_OK;
    }

    return ERROR_ARG;
}

Error Packet_Deserialize(const uint8_t *buff, uint32_t size, Packet *p) {
    if (!buff || !p) return ERROR_ARG;
    if (size<1) return ERROR_ARG;

    uint8_t rawType = buff[0];
    if (rawType > PACKET_SET_STREAM_DATA) return ERROR_READ;

    p->type = (PacketType)rawType;
    uint32_t offset = 1;

    switch (p->type) {

    case PACKET_GET_STREAM_INFO:
        return ERROR_OK;

    case PACKET_SET_STREAM_INFO:
        if (size < offset + 5) return ERROR_READ;
        p->info.width    = read_u16(&buff[offset]); offset += 2;
        p->info.height   = read_u16(&buff[offset]); offset += 2;
        p->info.channels = buff[offset++];
        return ERROR_OK;

    case PACKET_START_STREAM:
        return ERROR_OK;

    case PACKET_END_STREAM:
        return ERROR_OK;

    case PACKET_GET_STREAM_DATA:
        if (size < offset + 10) return ERROR_READ;
        p->data.frame = read_u32(&buff[offset]); offset += 4;
        p->data.start = read_u32(&buff[offset]); offset += 4;
        p->data.size  = read_u16(&buff[offset]); offset += 2;
        p->data.data  = NULL;
        return ERROR_OK;

    case PACKET_SET_STREAM_DATA:
        if (size < offset + 10) return ERROR_READ;
        p->data.frame = read_u32(&buff[offset]); offset += 4;
        p->data.start = read_u32(&buff[offset]); offset += 4;
        p->data.size  = read_u16(&buff[offset]); offset += 2;

        if (p->data.size == 0) {
            p->data.data = NULL;
            return ERROR_OK;
        }
        if (size < offset + p->data.size) return ERROR_READ;
        p->data.data = (uint8_t *)&buff[offset];
        return ERROR_OK;
    }

    return ERROR_READ;
}

static uint32_t Broadcast_FrameSize(const Broadcast *b) {
    return (uint32_t)b->width
         * (uint32_t)b->height
         * (uint32_t)b->channels;
}

Error Broadcast_Responde(Broadcast *b, Addr addr, const Packet *req, Packet *res) {
    if (!b || !req) return ERROR_ARG;

    switch (req->type) {
		case PACKET_GET_STREAM_INFO:
			if (!res) return ERROR_ARG;
			res->type          = PACKET_SET_STREAM_INFO;
			res->info.width    = b->width;
			res->info.height   = b->height;
			res->info.channels = b->channels;
			return ERROR_OK;

		case PACKET_START_STREAM:
			b->addr      = addr;
			b->streaming = 1;
			b->frame     = 0;
			return ERROR_OK;

		case PACKET_END_STREAM:
			b->streaming = 0;
			return ERROR_OK;

		case PACKET_GET_STREAM_DATA: {
			if (!res || !b->data) return ERROR_ARG;

			uint32_t frameSize = Broadcast_FrameSize(b);
			if (frameSize == 0) return ERROR_ARG;

			uint32_t start = req->data.start;
			uint32_t length = req->data.size;

			if (start>=frameSize) return ERROR_ARG;
			if (start+length>frameSize) length = frameSize - start;

			res->type       = PACKET_SET_STREAM_DATA;
			res->data.frame = req->data.frame;
			res->data.start = start;
			res->data.size  = (uint16_t)length;
			res->data.data  = b->data+start;
			return ERROR_OK;
		}

		default:
			return ERROR_ARG;
    }
}

Error Broadcast_Stream(Broadcast *b, Packet *p) {
    if (!b || !p) return ERROR_ARG;
    if (!b->streaming) return ERROR_ARG;
    if (!b->data) return ERROR_ARG;

    uint32_t frameSize = Broadcast_FrameSize(b);

    if (frameSize==0) return ERROR_ARG;
    if (b->offset>=frameSize)  return ERROR_READ;

    uint32_t remaining = frameSize-b->offset;
    uint16_t size = (remaining>MAX_PACKET_DATA_SIZE) ? (uint16_t)MAX_PACKET_DATA_SIZE : (uint16_t)remaining;

    p->type       = PACKET_SET_STREAM_DATA;
    p->data.frame = b->frame;
    p->data.start = b->offset;
    p->data.size  = size;
    p->data.data  = b->data+b->offset;

    b->offset += size;

    return ERROR_OK;
}

Error Broadcast_Receive(Broadcast *b, const Packet *p, int *frameReceived, Packet *request) {
    if (!b) return ERROR_ARG;
    if (frameReceived) *frameReceived = 0;
    if (request) memset(request, 0, sizeof(*request));
    if (!b->data) return ERROR_ARG;

    switch (p->type) {
        case PACKET_SET_STREAM_INFO:
            b->width     = p->info.width;
            b->height    = p->info.height;
            b->channels  = p->info.channels;
            return ERROR_OK;

        case PACKET_SET_STREAM_DATA: {
            uint32_t frameSize = Broadcast_FrameSize(b);
            if (frameSize == 0) return ERROR_ARG;

            uint8_t newFrame = (uint8_t)p->data.frame;

            if (newFrame != b->frame) {
                b->frame  = newFrame;
                b->offset = 0;
                if (frameReceived) *frameReceived = 1;
            }

            uint32_t start  = p->data.start;
            uint32_t length = p->data.size;

            if (start >= frameSize) return ERROR_READ;
            if (start + length > frameSize) length = frameSize - start;

            if (start > b->offset && request) {
                uint32_t gapStart = b->offset;
                uint32_t gapSize  = start - b->offset;
                if (gapSize > MAX_PACKET_DATA_SIZE)
                    gapSize = MAX_PACKET_DATA_SIZE;

                request->type       = PACKET_GET_STREAM_DATA;
                request->data.frame = b->frame;
                request->data.start = gapStart;
                request->data.size  = (uint16_t)gapSize;
                request->data.data  = NULL;
            }

            if (p->data.data && length) {
                memcpy(b->data + start, p->data.data, length);
            }

            if (start + length > b->offset) {
                b->offset = start + length;
            }

            if (b->offset >= frameSize) {
                if (frameReceived) *frameReceived = 1;
            }

            return ERROR_OK;
        }

        case PACKET_END_STREAM:
            b->streaming = 0;
            if (frameReceived) *frameReceived = 1;
            return ERROR_OK;

        default:
            return ERROR_ARG;
    }
}
