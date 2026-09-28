#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <raylib.h>

#include "error.h"
#include "socket.h"
#include "protocol.h"

#define SERVER_IP        "127.0.0.1"
#define SERVER_PORT      12345
#define LOCAL_IP         "0.0.0.0"
#define LOCAL_PORT       12346
#define TIMEOUT_MS       1
#define HANDSHAKE_TRIES  30
#define HANDSHAKE_WAIT   0.1

#define SHOW_SCALE       3

int main(void) {
    if (Socket_Init() != ERROR_OK) {
        fprintf(stderr, "Socket_Init failed\n");
        return 1;
    }

    InitWindow(640, 700, "PROTOCOL — client");

    Socket sock;
    Addr   localAddr, serverAddr;

    if (Socket_Open(&sock) != ERROR_OK) {
        fprintf(stderr, "Socket_Open failed\n");
        goto closeWindow;
    }

    Socket_SetTimeout(&sock, TIMEOUT_MS);

    if (Addr_SetIp(&localAddr, LOCAL_IP) != ERROR_OK || Addr_SetPort(&localAddr, LOCAL_PORT) != ERROR_OK) {
        fprintf(stderr, "Local addr setup failed\n");
        goto closeSocket;
    }

    if (Socket_Bind(&sock, &localAddr) != ERROR_OK) {
        fprintf(stderr, "Bind failed\n");
        goto closeSocket;
    }

    if (Addr_SetIp(&serverAddr, SERVER_IP) != ERROR_OK || Addr_SetPort(&serverAddr, SERVER_PORT) != ERROR_OK) {
        fprintf(stderr, "Server addr setup failed\n");
        goto closeSocket;
    }

    if (Socket_Connect(&sock, &serverAddr) != ERROR_OK) {
        fprintf(stderr, "Connect failed\n");
        goto closeSocket;
    }

    uint8_t rxBuffer[2048];
    uint8_t txBuffer[2048];
    uint32_t used = 0;
    int      sentLen = 0;

    Packet request = {0};
    request.type = PACKET_GET_STREAM_INFO;
    Packet_Serialize(&request, txBuffer, (uint32_t)sizeof(txBuffer), &used);

    Broadcast broadcast = {0};
    int gotInfo = 0;

    for (int attempt = 0; attempt<HANDSHAKE_TRIES && !gotInfo; ++attempt) {
        Socket_Send(&sock, (const char *)txBuffer, (int)used, &sentLen);

        double deadline = GetTime()+HANDSHAKE_WAIT;
        while (GetTime()<deadline && !gotInfo) {
            int receivedLen = 0;
            Error err = Socket_Recv(&sock, (char *)rxBuffer, (int)sizeof(rxBuffer), &receivedLen);
            if (err != ERROR_OK) {
                WaitTime(0.001);
                continue;
            }
            if (receivedLen <= 0) continue;

            Packet response;
            if (Packet_Deserialize(rxBuffer, (uint32_t)receivedLen, &response) != ERROR_OK)
                continue;
            if (response.type != PACKET_SET_STREAM_INFO)
                continue;

            broadcast.width    = response.info.width;
            broadcast.height   = response.info.height;
            broadcast.channels = response.info.channels;
            gotInfo = 1;
        }
    }

    if (!gotInfo) {
        fprintf(stderr, "client: no SET_STREAM_INFO from server\n");
        goto closeSocket;
    }

    uint32_t frameSize = (uint32_t)broadcast.width*broadcast.height*broadcast.channels;

    broadcast.data = (uint8_t *)malloc(frameSize);
    if (!broadcast.data) {
        fprintf(stderr, "Out of memory\n");
        goto closeSocket;
    }
    memset(broadcast.data, 0, frameSize);

    broadcast.frame     = 0xFF;
    broadcast.offset    = 0;
    broadcast.streaming = 1;

    request.type = PACKET_START_STREAM;
    Packet_Serialize(&request, txBuffer, (uint32_t)sizeof(txBuffer), &used);
    Socket_Send(&sock, (const char *)txBuffer, (int)used, &sentLen);

    /* текстура, отображающая принимаемый кадр */
    Image frameImage = {
        .data    = broadcast.data,
        .width   = broadcast.width,
        .height  = broadcast.height,
        .mipmaps = 1,
        .format  = (broadcast.channels == 3)
                       ? PIXELFORMAT_UNCOMPRESSED_R8G8B8
                       : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D frameTexture = LoadTextureFromImage(frameImage);
    SetTextureFilter(frameTexture, TEXTURE_FILTER_POINT);

    uint8_t  expectedFrame  = 0xFF;
    uint32_t expectedOffset = 0;
    int      framesReceived = 0;
    uint32_t retriesSent    = 0;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        for (;;) {
            int receivedLen = 0;
            Error err = Socket_Recv(&sock, (char *)rxBuffer, (int)sizeof(rxBuffer), &receivedLen);
            if (err != ERROR_OK || receivedLen <= 0) break;

            Packet response;
            if (Packet_Deserialize(rxBuffer, (uint32_t)receivedLen, &response) != ERROR_OK)
                continue;

            if (response.type == PACKET_SET_STREAM_DATA) {
                uint8_t  newFrame = (uint8_t)response.data.frame;
                uint32_t start    = response.data.start;
                uint32_t length   = response.data.size;

                if (newFrame != expectedFrame) {
                    expectedFrame  = newFrame;
                    expectedOffset = 0;
                }

                if (start > expectedOffset) {
                    uint32_t gap = start - expectedOffset;
                    if (gap > 0xFFFF) gap = 0xFFFF;

                    Packet retry = {0};
                    retry.type       = PACKET_GET_STREAM_DATA;
                    retry.data.frame = response.data.frame;
                    retry.data.start = expectedOffset;
                    retry.data.size  = (uint16_t)gap;

                    uint32_t retryUsed = 0;
                    if (Packet_Serialize(&retry, txBuffer,
                                         (uint32_t)sizeof(txBuffer),
                                         &retryUsed) == ERROR_OK) {
                        int retrySent = 0;
                        Socket_Send(&sock, (const char *)txBuffer,
                                    (int)retryUsed, &retrySent);
                        retriesSent++;
                    }
                }
                if (start+length>expectedOffset)
                    expectedOffset = start+length;
            }

            int frameReceived = 0;
            Broadcast_Receive(&broadcast, &response, &frameReceived);
            if (frameReceived)
                framesReceived++;
        }

        UpdateTexture(frameTexture, broadcast.data);

        BeginDrawing();
        ClearBackground((Color){20, 20, 26, 255});

		DrawTexturePro(
			frameTexture, 
			(Rectangle){0,0,broadcast.width,broadcast.height}, 
			(Rectangle){0,0,640,512}, 
			(Vector2){0,0}, 
			0,
			WHITE
		);

        int infoY = 512+12;
        DrawText("CLIENT", 10, infoY, 22, RAYWHITE);
        DrawText(TextFormat("frame: %d", broadcast.frame), 10, infoY + 28, 18, LIGHTGRAY);
        DrawText(TextFormat("received: %u / %u", broadcast.offset, frameSize), 10, infoY + 50, 18, LIGHTGRAY);
        DrawText(TextFormat("frames rx: %d", framesReceived), 10, infoY + 72, 18, LIGHTGRAY);
        DrawText(TextFormat("retries: %u", retriesSent), 10, infoY + 94, 18, LIGHTGRAY);
        DrawFPS(10, infoY + 116);

        EndDrawing();
    }

    request.type = PACKET_END_STREAM;
    Packet_Serialize(&request, txBuffer, (uint32_t)sizeof(txBuffer), &used);
    Socket_Send(&sock, (const char *)txBuffer, (int)used, &sentLen);

    UnloadTexture(frameTexture);
    free(broadcast.data);

closeSocket:
    Socket_Close(&sock);
closeWindow:
    CloseWindow();
    Socket_Deinit();
    return 0;
}
