#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <raylib.h>

#include "error.h"
#include "socket.h"
#include "protocol.h"

#define SERVER_IP    "0.0.0.0"
#define SERVER_PORT  12345
#define CLIENT_IP    "127.0.0.1"
#define CLIENT_PORT  12346
#define TIMEOUT_MS   1

#define IMG_W        160
#define IMG_H        128
#define IMG_CH       3
#define SHOW_SCALE   3
#define STREAM_FPS   30

static void RenderSourceImage(uint8_t *buffer, int width, int height,
                              int channels, float timeSec, uint32_t frameNo)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * channels;
            buffer[idx + 0] = (uint8_t)(x * 255 / (width  - 1));
            buffer[idx + 1] = (uint8_t)(y * 255 / (height - 1));
            buffer[idx + 2] = (uint8_t)(frameNo & 0xFF);
        }
    }

    int cx = width  / 2 + (int)((width  / 3) * cosf(timeSec));
    int cy = height / 2 + (int)((height / 3) * sinf(timeSec * 1.3f));
    int r  = 14;
    for (int y = cy - r; y <= cy + r; ++y) {
        if (y < 0 || y >= height) continue;
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x < 0 || x >= width) continue;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > r * r) continue;
            int idx = (y * width + x) * channels;
            buffer[idx + 0] = 255;
            buffer[idx + 1] = 255;
            buffer[idx + 2] = 0;
        }
    }
}

int main(void)
{
    if (Socket_Init() != ERROR_OK) {
        fprintf(stderr, "Socket_Init failed\n");
        return 1;
    }

    InitWindow(720, 360, "PROTOCOL — server");

    Socket sock;
    Addr   localAddr, peerAddr;

    if (Socket_Open(&sock) != ERROR_OK) {
        fprintf(stderr, "Socket_Open failed\n");
        goto closeWindow;
    }
    Socket_SetTimeout(&sock, TIMEOUT_MS);

    if (Addr_SetIp(&localAddr, SERVER_IP) != ERROR_OK ||
        Addr_SetPort(&localAddr, SERVER_PORT) != ERROR_OK) {
        fprintf(stderr, "Addr setup failed\n");
        goto closeSocket;
    }
    if (Socket_Bind(&sock, &localAddr) != ERROR_OK) {
        fprintf(stderr, "Bind failed\n");
        goto closeSocket;
    }

    if (Addr_SetIp(&peerAddr, CLIENT_IP) != ERROR_OK ||
        Addr_SetPort(&peerAddr, CLIENT_PORT) != ERROR_OK) {
        fprintf(stderr, "Peer setup failed\n");
        goto closeSocket;
    }
    if (Socket_Connect(&sock, &peerAddr) != ERROR_OK) {
        fprintf(stderr, "Connect failed\n");
        goto closeSocket;
    }

    uint32_t frameSize = (uint32_t)IMG_W * IMG_H * IMG_CH;
    uint8_t *sourceBuffer = (uint8_t *)malloc(frameSize);
    if (!sourceBuffer) {
        fprintf(stderr, "Out of memory\n");
        goto closeSocket;
    }
    RenderSourceImage(sourceBuffer, IMG_W, IMG_H, IMG_CH, 0.0f, 0);

    Image sourceImage = {
        .data    = sourceBuffer,
        .width   = IMG_W,
        .height  = IMG_H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8
    };
    Texture2D sourceTexture = LoadTextureFromImage(sourceImage);
    SetTextureFilter(sourceTexture, TEXTURE_FILTER_POINT);

    Broadcast broadcast = {0};
    broadcast.width     = IMG_W;
    broadcast.height    = IMG_H;
    broadcast.channels  = IMG_CH;
    broadcast.data      = sourceBuffer;
    broadcast.streaming = 0;
    broadcast.frame     = 0;
    broadcast.offset    = 0;

    uint8_t  rxBuffer[2048];
    uint8_t  txBuffer[2048];
    uint32_t frameCounter = 0;
    float    timeSec      = 0.0f;

    SetTargetFPS(STREAM_FPS);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        timeSec += dt;
        frameCounter++;

        RenderSourceImage(sourceBuffer, IMG_W, IMG_H, IMG_CH,
                          timeSec, frameCounter);

        /* ---- приём запросов от клиента ---- */
        for (;;) {
            int receivedLen = 0;
            Error err = Socket_Recv(&sock, (char *)rxBuffer,
                                    (int)sizeof(rxBuffer), &receivedLen);
            if (err != ERROR_OK || receivedLen <= 0) break;

            Packet request;
            if (Packet_Deserialize(rxBuffer, (uint32_t)receivedLen, &request) != ERROR_OK)
                continue;

            if (request.type == PACKET_START_STREAM ||
                request.type == PACKET_END_STREAM)
            {
                Broadcast_Responde(&broadcast, peerAddr, &request, NULL);
                if (request.type == PACKET_START_STREAM) {
                    broadcast.frame  = (uint8_t)(frameCounter & 0xFF);
                    broadcast.offset = 0;
                }
            } else {
                Packet response;
                if (Broadcast_Responde(&broadcast, peerAddr, &request, &response) != ERROR_OK)
                    continue;

                uint32_t used = 0;
                if (Packet_Serialize(&response, txBuffer, (uint32_t)sizeof(txBuffer), &used) != ERROR_OK)
                    continue;

                int sentLen = 0;
                Socket_Send(&sock, (const char *)txBuffer, (int)used, &sentLen);
            }
        }

        /* ---- стриминг текущего кадра ---- */
        if (broadcast.streaming) {
            uint8_t currentFrame = (uint8_t)(frameCounter & 0xFF);
            if (broadcast.frame != currentFrame) {
                broadcast.frame  = currentFrame;
                broadcast.offset = 0;
            }

            Packet chunk;
            while (Broadcast_Sream(&broadcast, &chunk) == ERROR_OK) {
                uint32_t used = 0;
                if (Packet_Serialize(&chunk, txBuffer,
                                     (uint32_t)sizeof(txBuffer), &used) != ERROR_OK)
                    break;

                int sentLen = 0;
                if (Socket_Send(&sock, (const char *)txBuffer,
                                (int)used, &sentLen) != ERROR_OK)
                    break;
            }
        }

        /* ---- рендер ---- */
        UpdateTexture(sourceTexture, sourceBuffer);

        BeginDrawing();
        ClearBackground((Color){20, 20, 26, 255});

        DrawTextureEx(sourceTexture, (Vector2){10, 10}, 0.0f,
                      (float)SHOW_SCALE, WHITE);

        int infoY = 10 + IMG_H * SHOW_SCALE + 12;
        DrawText("SERVER", 10, infoY, 22, RAYWHITE);
        DrawText(TextFormat("streaming: %s", broadcast.streaming ? "yes" : "no"),
                 10, infoY + 28, 18, LIGHTGRAY);
        DrawText(TextFormat("frame: %u", frameCounter),
                 10, infoY + 50, 18, LIGHTGRAY);
        DrawText(TextFormat("offset: %u / %u", broadcast.offset, frameSize),
                 10, infoY + 72, 18, LIGHTGRAY);
        DrawFPS(10, infoY + 94);

        EndDrawing();
    }

    UnloadTexture(sourceTexture);
    free(sourceBuffer);

closeSocket:
    Socket_Close(&sock);
closeWindow:
    CloseWindow();
    Socket_Deinit();
    return 0;
}
