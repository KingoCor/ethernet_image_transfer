#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "raylib.h"

#include "error.h"
#include "socket.h"
#include "protocol.h"

#define DEFAULT_SERVER_IP    "127.0.0.1"
#define DEFAULT_SERVER_PORT  12345
#define DEFAULT_LOCAL_IP     "0.0.0.0"
#define DEFAULT_LOCAL_PORT   12346
#define DEFAULT_TIMEOUT_MS   100
#define DEFAULT_FRAMES       0
#define DEFAULT_FPS          60
#define DEFAULT_TITLE        "Video Stream Client"

typedef struct {
    const char *server_ip;
    int         server_port;
    const char *local_ip;
    int         local_port;
    int         timeout_ms;
    int         frames;       /* 0 = бесконечно */
    int         window_w;     /* 0 = по размеру потока */
    int         window_h;
    int         fps;
    const char *title;
} Config;

typedef struct {
    Socket    sock;
    Broadcast bcast;
    Texture2D texture;
    int       textureReady;
} Client;

static void PrintUsage(const char *prog) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -s, --server-ip <IP>      Server IP              (default: %s)\n"
        "  -p, --server-port <PORT>  Server port            (default: %d)\n"
        "  -l, --local-ip <IP>       Local bind IP          (default: %s)\n"
        "  -b, --local-port <PORT>   Local bind port        (default: %d)\n"
        "  -t, --timeout <MS>        Socket timeout (ms)    (default: %d)\n"
        "  -n, --frames <N>          Stop after N frames; 0 = infinite (default: %d)\n"
        "      --width <PX>          Window width; 0 = auto (default: 0)\n"
        "      --height <PX>         Window height; 0 = auto (default: 0)\n"
        "      --fps <N>             Target FPS             (default: %d)\n"
        "      --title <TITLE>       Window title           (default: \"%s\")\n"
        "  -h, --help                Show this help\n",
        prog,
        DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT,
        DEFAULT_LOCAL_IP,  DEFAULT_LOCAL_PORT,
        DEFAULT_TIMEOUT_MS, DEFAULT_FRAMES,
        DEFAULT_FPS, DEFAULT_TITLE);
}

static int ParseInt(const char *s, int *out, int min, int max) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);

    if (errno != 0)   return -1;
    if (end == s)     return -1;
    if (*end != '\0') return -1;
    if (v < min)      return -1;
    if (v > max)      return -1;

    *out = (int)v;
    return 0;
}

static int ParseArgs(int argc, char **argv, Config *cfg) {
    cfg->server_ip   = DEFAULT_SERVER_IP;
    cfg->server_port = DEFAULT_SERVER_PORT;
    cfg->local_ip    = DEFAULT_LOCAL_IP;
    cfg->local_port  = DEFAULT_LOCAL_PORT;
    cfg->timeout_ms  = DEFAULT_TIMEOUT_MS;
    cfg->frames      = DEFAULT_FRAMES;
    cfg->window_w    = 0;
    cfg->window_h    = 0;
    cfg->fps         = DEFAULT_FPS;
    cfg->title       = DEFAULT_TITLE;

    enum { OPT_WIDTH = 1000, OPT_HEIGHT, OPT_FPS, OPT_TITLE };

    static struct option long_opts[] = {
        {"server-ip",   required_argument, 0, 's'},
        {"server-port", required_argument, 0, 'p'},
        {"local-ip",    required_argument, 0, 'l'},
        {"local-port",  required_argument, 0, 'b'},
        {"timeout",     required_argument, 0, 't'},
        {"frames",      required_argument, 0, 'n'},
        {"width",       required_argument, 0, OPT_WIDTH},
        {"height",      required_argument, 0, OPT_HEIGHT},
        {"fps",         required_argument, 0, OPT_FPS},
        {"title",       required_argument, 0, OPT_TITLE},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int rc;

    while ((opt = getopt_long(argc, argv, "s:p:l:b:t:n:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 's':
            cfg->server_ip = optarg;
            break;
        case 'l':
            cfg->local_ip = optarg;
            break;

        case 'p':
            rc = ParseInt(optarg, &cfg->server_port, 0, 65535);
            if (rc != 0) {
                printf("Invalid --server-port: %s\n", optarg);
                return -1;
            }
            break;
        case 'b':
            rc = ParseInt(optarg, &cfg->local_port, 0, 65535);
            if (rc != 0) {
                printf("Invalid --local-port: %s\n", optarg);
                return -1;
            }
            break;
        case 't':
            rc = ParseInt(optarg, &cfg->timeout_ms, 1, 60000);
            if (rc != 0) {
                printf("Invalid --timeout: %s\n", optarg);
                return -1;
            }
            break;
        case 'n':
            rc = ParseInt(optarg, &cfg->frames, 0, 1 << 30);
            if (rc != 0) {
                printf("Invalid --frames: %s\n", optarg);
                return -1;
            }
            break;
        case OPT_WIDTH:
            rc = ParseInt(optarg, &cfg->window_w, 0, 16384);
            if (rc != 0) {
                printf("Invalid --width: %s\n", optarg);
                return -1;
            }
            break;
        case OPT_HEIGHT:
            rc = ParseInt(optarg, &cfg->window_h, 0, 16384);
            if (rc != 0) {
                printf("Invalid --height: %s\n", optarg);
                return -1;
            }
            break;
        case OPT_FPS:
            rc = ParseInt(optarg, &cfg->fps, 0, 1000);
            if (rc != 0) {
                printf("Invalid --fps: %s\n", optarg);
                return -1;
            }
            break;
        case OPT_TITLE:
            cfg->title = optarg;
            break;

        case 'h':
            PrintUsage(argv[0]);
            exit(0);
        case '?':
        default:
            PrintUsage(argv[0]);
            return -1;
        }
    }

    if (optind < argc) {
        printf("Unexpected positional arguments:");
        for (int i = optind; i < argc; ++i)
            printf(" %s", argv[i]);
        printf("\n");
        return -1;
    }
    return 0;
}

static int InitSocket(Client *c, const Config *cfg) {
    Addr local, server;
    Error err;

    err = Socket_Init();
    if (err != ERROR_OK) {
        printf("Socket_Init failed: %d\n", err);
        return -1;
    }

    err = Socket_Open(&c->sock);
    if (err != ERROR_OK) {
        printf("Socket_Open failed: %d\n", err);
        Socket_Deinit();
        return -1;
    }

    err = Socket_SetTimeout(&c->sock, cfg->timeout_ms);
    if (err != ERROR_OK) {
        printf("Socket_SetTimeout failed: %d\n", err);
        Socket_Close(&c->sock);
        Socket_Deinit();
        return -1;
    }

    Addr_SetIp(&local, cfg->local_ip);
    Addr_SetPort(&local, (unsigned short)cfg->local_port);

    err = Socket_Bind(&c->sock, &local);
    if (err != ERROR_OK) {
        printf("Bind %s:%d failed: %d\n", cfg->local_ip, cfg->local_port, err);
        Socket_Close(&c->sock);
        Socket_Deinit();
        return -1;
    }

    Addr_SetIp(&server, cfg->server_ip);
    Addr_SetPort(&server, (unsigned short)cfg->server_port);

    err = Socket_Connect(&c->sock, &server);
    if (err != ERROR_OK) {
        printf("Connect to %s:%d failed: %d\n",
               cfg->server_ip, cfg->server_port, err);
        Socket_Close(&c->sock);
        Socket_Deinit();
        return -1;
    }
    return 0;
}

static void DeinitSocket(Client *c) {
    Socket_Close(&c->sock);
    Socket_Deinit();
}

static int InitBroadcast(Client *c) {
    memset(&c->bcast, 0, sizeof(c->bcast));

    uint8_t *buf = (uint8_t *)malloc(1);
    if (buf == NULL) return -1;

    c->bcast.data = buf;
    return 0;
}

static int ResizeBroadcastBuffer(Client *c, uint32_t frameSize) {
    uint8_t *buf = (uint8_t *)realloc(c->bcast.data, frameSize);
    if (buf == NULL) return -1;

    c->bcast.data = buf;
    return 0;
}

static void DeinitBroadcast(Client *c) {
    if (c->bcast.data != NULL) {
        free(c->bcast.data);
        c->bcast.data = NULL;
    }
}

static int InitRaylib(Client *c, const Config *cfg) {
    int w = cfg->window_w > 0 ? cfg->window_w : (int)c->bcast.width;
    int h = cfg->window_h > 0 ? cfg->window_h : (int)c->bcast.height;

    if (w <= 0) w = 800;
    if (h <= 0) h = 600;

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(w, h, cfg->title);

    int ready = IsWindowReady();
    if (ready == 0) return -1;

    if (cfg->fps > 0) SetTargetFPS(cfg->fps);
    return 0;
}

static void DeinitRaylib(void) {
    CloseWindow();
}

static int InitTexture(Client *c) {
    int fmt;
    int ok = 1;

    switch (c->bcast.channels) {
    case 1: fmt = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE; ok = 1; break;
    case 2: fmt = PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA; ok = 1; break;
    case 3: fmt = PIXELFORMAT_UNCOMPRESSED_R8G8B8;    ok = 1; break;
    case 4: fmt = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;  ok = 1; break;
    default:
        fmt = 0;
        ok = 0;
    }

    if (ok == 0) {
        printf("Unsupported channel count: %u\n", c->bcast.channels);
        return -1;
    }

    Image img = {
        .data    = c->bcast.data,
        .width   = (int)c->bcast.width,
        .height  = (int)c->bcast.height,
        .mipmaps = 1,
        .format  = fmt,
    };

    c->texture = LoadTextureFromImage(img);

    if (c->texture.id == 0) {
        printf("LoadTextureFromImage failed\n");
        return -1;
    }
    c->textureReady = 1;
    return 0;
}

static void DeinitTexture(Client *c) {
    if (c->textureReady != 0) {
        UnloadTexture(c->texture);
        c->textureReady = 0;
    }
}

static Error SendPacket(Client *c, const Packet *p) {
    uint8_t  buf[64 + MAX_PACKET_DATA_SIZE];
    uint32_t used = 0;

    Error err = Packet_Serialize(p, buf, sizeof(buf), &used);
    if (err != ERROR_OK) return err;

    err = Socket_Send(&c->sock, buf, used, NULL);
    return err;
}

static Error RecvPacket(Client *c, Packet *resp, uint8_t *scratch, uint32_t scratchSize) {
    uint32_t got = 0;

    Error err = Socket_Recv(&c->sock, scratch, scratchSize, &got);
    if (err != ERROR_OK) return err;

    err = Packet_Deserialize(scratch, got, resp);
    return err;
}

static int RequestStreamInfo(Client *c) {
    Packet req = {0};
    req.type = PACKET_GET_STREAM_INFO;

    uint8_t scratch[64 + MAX_PACKET_DATA_SIZE];

    for (int attempt = 0; attempt < 5; ++attempt) {
        Error err = SendPacket(c, &req);
        if (err != ERROR_OK) continue;

        Packet resp;
        err = RecvPacket(c, &resp, scratch, sizeof(scratch));
        if (err != ERROR_OK) continue;

        if (resp.type != PACKET_SET_STREAM_INFO) {
            printf("Unexpected response type: %d\n", (int)resp.type);
            return -1;
        }

        err = Broadcast_Receive(&c->bcast, &resp, NULL, NULL);
        if (err != ERROR_OK) return -1;

        return 0;
    }
    return -1;
}

static int StartStream(Client *c) {
    Packet req = {0};
    req.type = PACKET_START_STREAM;

    Error err = SendPacket(c, &req);
    if (err != ERROR_OK) return -1;

    return 0;
}

static int EndStream(Client *c) {
    Packet req = {0};
    req.type = PACKET_END_STREAM;

    Error err = SendPacket(c, &req);
    if (err != ERROR_OK) return -1;

    return 0;
}

static void DrawFrameScaled(Texture2D tex) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float scaleX = sw / (float)tex.width;
    float scaleY = sh / (float)tex.height;
    float scale  = scaleX < scaleY ? scaleX : scaleY;

    float dw = (float)tex.width  * scale;
    float dh = (float)tex.height * scale;

    Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
    Rectangle dst = { (sw - dw) * 0.5f, (sh - dh) * 0.5f, dw, dh };

    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

static void RunStreamLoop(Client *c, const Config *cfg) {
    uint8_t scratch[64 + MAX_PACKET_DATA_SIZE];

    double budgetSec = cfg->fps > 0 ? 1.0 / (double)cfg->fps : 0.016;

    int closing = 0;
    while (closing == 0) {
        closing = WindowShouldClose();
        if (closing != 0) break;

        double budgetStart = GetTime();

		int frameDone = 0;
        while (frameDone == 0) {
            closing = WindowShouldClose();
            if (closing != 0) break;

            double now = GetTime();
            if ((now - budgetStart) > budgetSec) break;

            Packet resp;
            Error err = RecvPacket(c, &resp, scratch, sizeof(scratch));
            if (err != ERROR_OK) continue;

			Packet request;
			err = Broadcast_Receive(&c->bcast, &resp, &frameDone, &request);
            if (err != ERROR_OK) continue;

			if (request.type!=PACKET_NONE) SendPacket(c, &request);

            if (frameDone) UpdateTexture(c->texture, c->bcast.data);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        if (c->textureReady != 0) DrawFrameScaled(c->texture);

        DrawFPS(10, 10);
        DrawText(TextFormat("Frame: %d", c->bcast.frame), 10, 32, 20, GREEN);
        DrawText(TextFormat("Resolution: %ux%u x%u", c->bcast.width, c->bcast.height, c->bcast.channels), 10, 56, 20, GREEN);

        EndDrawing();

        if (cfg->frames>0 && c->bcast.frame >= cfg->frames) break;
    }
}

int main(int argc, char **argv) {
    Config cfg;
    int    rc;

    rc = ParseArgs(argc, argv, &cfg);
    if (rc != 0) return 2;

    Client client;
    memset(&client, 0, sizeof(client));

    rc = InitSocket(&client, &cfg);
    if (rc != 0) return 1;

    rc = InitBroadcast(&client);
    if (rc != 0) {
        printf("InitBroadcast failed\n");
        DeinitSocket(&client);
        return 1;
    }

    rc = RequestStreamInfo(&client);
    if (rc != 0) {
        printf("Failed to get stream info\n");
        DeinitBroadcast(&client);
        DeinitSocket(&client);
        return 1;
    }

    printf("Stream: %ux%u, %u channel(s)\n", client.bcast.width, client.bcast.height, client.bcast.channels);

    uint32_t frameSize = (uint32_t)client.bcast.width
                       * (uint32_t)client.bcast.height
                       * (uint32_t)client.bcast.channels;
    if (frameSize == 0) {
        printf("Invalid frame size\n");
        DeinitBroadcast(&client);
        DeinitSocket(&client);
        return 1;
    }

    rc = ResizeBroadcastBuffer(&client, frameSize);
    if (rc != 0) {
        printf("Out of memory for frame buffer\n");
        DeinitBroadcast(&client);
        DeinitSocket(&client);
        return 1;
    }

    rc = InitRaylib(&client, &cfg);
    if (rc != 0) {
        printf("InitRaylib failed\n");
        DeinitBroadcast(&client);
        DeinitSocket(&client);
        return 1;
    }

    rc = InitTexture(&client);
    if (rc != 0) {
        DeinitRaylib();
        DeinitBroadcast(&client);
        DeinitSocket(&client);
        return 1;
    }

    rc = StartStream(&client);
    if (rc != 0) {
        printf("StartStream failed\n");
        DeinitTexture(&client);
        DeinitRaylib();
        DeinitBroadcast(&client);
        DeinitSocket(&client);
        return 1;
    }

    RunStreamLoop(&client, &cfg);

    rc = EndStream(&client);
    if (rc != 0) {
        printf("EndStream failed\n");
    }

    DeinitTexture(&client);
    DeinitRaylib();
    DeinitBroadcast(&client);
    DeinitSocket(&client);
    return 0;
}
