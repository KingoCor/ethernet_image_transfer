#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

#include "error.h"
#include "socket.h"
#include "protocol.h"

#define DEFAULT_BIND_IP     "0.0.0.0"
#define DEFAULT_BIND_PORT   12345
#define DEFAULT_CLIENT_IP   "127.0.0.1"
#define DEFAULT_CLIENT_PORT 12346
#define DEFAULT_TIMEOUT_MS  200
#define DEFAULT_POLL_MS     1
#define DEFAULT_WIDTH       640
#define DEFAULT_HEIGHT      480
#define DEFAULT_CHANNELS    3
#define DEFAULT_FRAMES      0

typedef struct {
    const char *bind_ip;
    int         bind_port;
    const char *client_ip;
    int         client_port;
    int         timeout_ms;
    int         poll_ms;
    uint16_t    width;
    uint16_t    height;
    uint8_t     channels;
    int         total_frames;
} Config;

typedef struct {
    Socket    sock;
    Addr      client;
    int       connected;
    Broadcast bcast;
} Server;

static volatile sig_atomic_t g_stop = 0;

static void OnSigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static void PrintUsage(const char *prog) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -b, --bind-ip <IP>       Bind IP                  (default: %s)\n"
        "  -p, --bind-port <PORT>   Bind port                (default: %d)\n"
        "  -s, --client-ip <IP>     Client IP                (default: %s)\n"
        "  -q, --client-port <PORT> Client port              (default: %d)\n"
        "  -t, --timeout <MS>       Handshake timeout (ms)   (default: %d)\n"
        "      --poll <MS>          Streaming poll timeout   (default: %d)\n"
        "  -W, --width <PX>         Frame width              (default: %d)\n"
        "  -H, --height <PX>        Frame height             (default: %d)\n"
        "  -c, --channels <N>       Channels 1..4            (default: %d)\n"
        "  -n, --frames <N>         Stop after N frames; 0 = infinite (default: %d)\n"
        "  -h, --help               Show this help\n",
        prog,
        DEFAULT_BIND_IP, DEFAULT_BIND_PORT,
        DEFAULT_CLIENT_IP, DEFAULT_CLIENT_PORT,
        DEFAULT_TIMEOUT_MS, DEFAULT_POLL_MS,
        DEFAULT_WIDTH, DEFAULT_HEIGHT,
        DEFAULT_CHANNELS, DEFAULT_FRAMES);
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
    cfg->bind_ip      = DEFAULT_BIND_IP;
    cfg->bind_port    = DEFAULT_BIND_PORT;
    cfg->client_ip    = DEFAULT_CLIENT_IP;
    cfg->client_port  = DEFAULT_CLIENT_PORT;
    cfg->timeout_ms   = DEFAULT_TIMEOUT_MS;
    cfg->poll_ms      = DEFAULT_POLL_MS;
    cfg->width        = DEFAULT_WIDTH;
    cfg->height       = DEFAULT_HEIGHT;
    cfg->channels     = DEFAULT_CHANNELS;
    cfg->total_frames = DEFAULT_FRAMES;

    int w, h, c;
    int rc;

    enum { OPT_POLL = 1000 };

    static struct option long_opts[] = {
        {"bind-ip",     required_argument, 0, 'b'},
        {"bind-port",   required_argument, 0, 'p'},
        {"client-ip",   required_argument, 0, 's'},
        {"client-port", required_argument, 0, 'q'},
        {"timeout",     required_argument, 0, 't'},
        {"poll",        required_argument, 0, OPT_POLL},
        {"width",       required_argument, 0, 'W'},
        {"height",      required_argument, 0, 'H'},
        {"channels",    required_argument, 0, 'c'},
        {"frames",      required_argument, 0, 'n'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "b:p:s:q:t:W:H:c:n:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'b':
            cfg->bind_ip = optarg;
            break;
        case 's':
            cfg->client_ip = optarg;
            break;
        case 'p':
            rc = ParseInt(optarg, &cfg->bind_port, 0, 65535);
            if (rc != 0) {
                printf("Invalid --bind-port: %s\n", optarg);
                return -1;
            }
            break;
        case 'q':
            rc = ParseInt(optarg, &cfg->client_port, 0, 65535);
            if (rc != 0) {
                printf("Invalid --client-port: %s\n", optarg);
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
        case OPT_POLL:
            rc = ParseInt(optarg, &cfg->poll_ms, 0, 60000);
            if (rc != 0) {
                printf("Invalid --poll: %s\n", optarg);
                return -1;
            }
            break;
        case 'W':
            rc = ParseInt(optarg, &w, 1, 16384);
            if (rc != 0) {
                printf("Invalid --width: %s\n", optarg);
                return -1;
            }
            cfg->width = (uint16_t)w;
            break;
        case 'H':
            rc = ParseInt(optarg, &h, 1, 16384);
            if (rc != 0) {
                printf("Invalid --height: %s\n", optarg);
                return -1;
            }
            cfg->height = (uint16_t)h;
            break;
        case 'c':
            rc = ParseInt(optarg, &c, 1, 4);
            if (rc != 0) {
                printf("Invalid --channels: %s\n", optarg);
                return -1;
            }
            cfg->channels = (uint8_t)c;
            break;
        case 'n':
            rc = ParseInt(optarg, &cfg->total_frames, 0, 1 << 30);
            if (rc != 0) {
                printf("Invalid --frames: %s\n", optarg);
                return -1;
            }
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

static int InitSocketBlock(Server *s, const Config *cfg) {
    Addr bindAddr;
    Error err;

    err = Socket_Init();
    if (err != ERROR_OK) {
        printf("Socket_Init failed: %d\n", err);
        return -1;
    }

    err = Socket_Open(&s->sock);
    if (err != ERROR_OK) {
        printf("Socket_Open failed: %d\n", err);
        Socket_Deinit();
        return -1;
    }

    err = Socket_SetTimeout(&s->sock, cfg->timeout_ms);
    if (err != ERROR_OK) {
        printf("Socket_SetTimeout failed: %d\n", err);
        Socket_Close(&s->sock);
        Socket_Deinit();
        return -1;
    }

    Addr_SetIp(&bindAddr, cfg->bind_ip);
    Addr_SetPort(&bindAddr, (unsigned short)cfg->bind_port);

    err = Socket_Bind(&s->sock, &bindAddr);
    if (err != ERROR_OK) {
        printf("Bind %s:%d failed: %d\n", cfg->bind_ip, cfg->bind_port, err);
        Socket_Close(&s->sock);
        Socket_Deinit();
        return -1;
    }

    Addr_SetIp(&s->client, cfg->client_ip);
    Addr_SetPort(&s->client, (unsigned short)cfg->client_port);

    err = Socket_Connect(&s->sock, &s->client);
    if (err != ERROR_OK) {
        printf("Socket_Connect(%s:%d) failed: %d\n",
               cfg->client_ip, cfg->client_port, err);
        Socket_Close(&s->sock);
        Socket_Deinit();
        return -1;
    }
    s->connected = 1;

    printf("UDP server bound on %s:%d, connected to %s:%d\n",
           cfg->bind_ip, cfg->bind_port, cfg->client_ip, cfg->client_port);
    return 0;
}

static void DeinitSocketBlock(Server *s) {
    Socket_Close(&s->sock);
    Socket_Deinit();
    s->connected = 0;
}

static int InitBroadcastBlock(Server *s, const Config *cfg) {
    memset(&s->bcast, 0, sizeof(s->bcast));

    s->bcast.width    = cfg->width;
    s->bcast.height   = cfg->height;
    s->bcast.channels = cfg->channels;

    uint32_t frameSize = (uint32_t)cfg->width
                       * (uint32_t)cfg->height
                       * (uint32_t)cfg->channels;

    if (frameSize == 0) {
        printf("Invalid frame size\n");
        return -1;
    }

    uint8_t *buf = (uint8_t *)calloc(1, frameSize);
    if (buf == NULL) {
        printf("Out of memory for frame buffer\n");
        return -1;
    }
    s->bcast.data = buf;

    printf("Frame buffer: %ux%u x%u = %u bytes\n",
           cfg->width, cfg->height, cfg->channels, frameSize);
    return 0;
}

static void DeinitBroadcastBlock(Server *s) {
    if (s->bcast.data != NULL) {
        free(s->bcast.data);
        s->bcast.data = NULL;
    }
}

static void GenerateFrame(Broadcast *b, uint32_t frameNum) {
    uint32_t W = b->width;
    uint32_t H = b->height;
    uint32_t C = b->channels;
    uint8_t *p = b->data;

    if (W == 0 || H == 0 || C == 0 || p == NULL) return;

    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint8_t *px = p + ((size_t)y * W + x) * C;

            uint8_t r  = (uint8_t)((x * 255) / W);
            uint8_t g  = (uint8_t)((y * 255) / H);
            uint8_t bl = (uint8_t)((frameNum * 3) & 0xFF);

            if (C >= 1) px[0] = r;
            if (C >= 2) px[1] = g;
            if (C >= 3) px[2] = bl;
            if (C >= 4) px[3] = 255;
        }
    }
}

static Error SendPacket(Server *s, const Packet *p) {
    uint8_t  buf[64 + MAX_PACKET_DATA_SIZE];
    uint32_t used = 0;

    Error err = Packet_Serialize(p, buf, sizeof(buf), &used);
    if (err != ERROR_OK) return err;

    err = Socket_Send(&s->sock, buf, used, NULL);
    return err;
}

static Error RecvPacket(Server *s, Packet *resp, uint8_t *scratch, uint32_t scratchSize) {
    uint32_t got = 0;

    Error err = Socket_Recv(&s->sock, scratch, scratchSize, &got);
    if (err != ERROR_OK) return err;

    err = Packet_Deserialize(scratch, got, resp);
    return err;
}

static int HandshakeBlock(Server *s, uint8_t *scratch, uint32_t scratchSize) {
    Packet req;
    Packet res;
    Error  err;

    memset(&req, 0, sizeof(req));
    memset(&res, 0, sizeof(res));

    err = RecvPacket(s, &req, scratch, scratchSize);
    if (err != ERROR_OK) {
        printf("RecvPacket(GET_STREAM_INFO) failed: %d\n", err);
        return -1;
    }
    if (req.type != PACKET_GET_STREAM_INFO) {
        printf("Expected GET_STREAM_INFO, got %d\n", (int)req.type);
        return -1;
    }

    err = Broadcast_Responde(&s->bcast, s->client, &req, &res);
    if (err != ERROR_OK) {
        printf("Broadcast_Responde(GET_STREAM_INFO) failed: %d\n", err);
        return -1;
    }

    err = SendPacket(s, &res);
    if (err != ERROR_OK) {
        printf("SendPacket(SET_STREAM_INFO) failed: %d\n", err);
        return -1;
    }

    memset(&req, 0, sizeof(req));

    err = RecvPacket(s, &req, scratch, scratchSize);
    if (err != ERROR_OK) {
        printf("RecvPacket(START_STREAM) failed: %d\n", err);
        return -1;
    }
    if (req.type != PACKET_START_STREAM) {
        printf("Expected START_STREAM, got %d\n", (int)req.type);
        return -1;
    }

    err = Broadcast_Responde(&s->bcast, s->client, &req, NULL);
    if (err != ERROR_OK) {
        printf("Broadcast_Responde(START_STREAM) failed: %d\n", err);
        return -1;
    }

    printf("Client connected, starting stream\n");
    return 0;
}

static int PollClientRequest(Server *s, uint8_t *scratch, uint32_t scratchSize) {
    Packet req;
    Error  err;

    err = RecvPacket(s, &req, scratch, scratchSize);
    if (err != ERROR_OK) {
        return 0;
    }

    if (req.type == PACKET_END_STREAM) {
        err = Broadcast_Responde(&s->bcast, s->client, &req, NULL);
        if (err != ERROR_OK) {
            printf("Broadcast_Responde(END_STREAM) failed: %d\n", err);
        }
        printf("Client sent END_STREAM\n");
        return 1;
    }

    return 0;
}

static int StreamBlock(Server *s, const Config *cfg, uint8_t *scratch, uint32_t scratchSize) {
    Error err;
    int   rc;

    uint32_t frameSize = (uint32_t)s->bcast.width
                       * (uint32_t)s->bcast.height
                       * (uint32_t)s->bcast.channels;

    if (frameSize == 0) {
        printf("Frame size is zero\n");
        return -1;
    }

    err = Socket_SetTimeout(&s->sock, cfg->poll_ms);
    if (err != ERROR_OK) {
        printf("Socket_SetTimeout(stream) failed: %d\n", err);
        return -1;
    }

    s->bcast.frame  = 0;
    s->bcast.offset = 0;
    GenerateFrame(&s->bcast, 0);

    int framesSent = 0;

    while (g_stop == 0) {
		if ((s->bcast.offset/MAX_PACKET_DATA_SIZE)%10==0) {
			rc = PollClientRequest(s, scratch, scratchSize);
			if (rc > 0) {
				printf("Streaming finished by END_STREAM\n");
				break;
			}
		}

        if (s->bcast.streaming == 0) {
            printf("Streaming disabled\n");
            break;
        }

        if (s->bcast.offset >= frameSize) {
            s->bcast.frame++;
            s->bcast.offset = 0;
            GenerateFrame(&s->bcast, s->bcast.frame);
            framesSent++;

            if (cfg->total_frames > 0 && framesSent >= cfg->total_frames) {
                printf("Sent %d frame(s), stopping\n", framesSent);
                break;
            }
        }

        Packet chunk;
        err = Broadcast_Stream(&s->bcast, &chunk);
        if (err != ERROR_OK) {
            continue;
        }

        err = SendPacket(s, &chunk);
        if (err != ERROR_OK) {
            printf("SendPacket(STREAM_DATA) failed: %d\n", err);
            break;
        }
    }

    printf("Streaming finished: %d complete frame(s)\n", framesSent);
    return 0;
}

int main(int argc, char **argv) {
    Config cfg;
    int    rc;

    rc = ParseArgs(argc, argv, &cfg);
    if (rc != 0) return 2;

    signal(SIGINT, OnSigint);

    Server server;
    memset(&server, 0, sizeof(server));

    rc = InitSocketBlock(&server, &cfg);
    if (rc != 0) return 1;

    rc = InitBroadcastBlock(&server, &cfg);
    if (rc != 0) {
        DeinitSocketBlock(&server);
        return 1;
    }

    uint8_t scratch[64 + MAX_PACKET_DATA_SIZE];

    rc = HandshakeBlock(&server, scratch, sizeof(scratch));
    if (rc != 0) {
        printf("Handshake failed\n");
        DeinitBroadcastBlock(&server);
        DeinitSocketBlock(&server);
        return 1;
    }

    rc = StreamBlock(&server, &cfg, scratch, sizeof(scratch));
    if (rc != 0) {
        printf("Streaming failed\n");
        DeinitBroadcastBlock(&server);
        DeinitSocketBlock(&server);
        return 1;
    }

    DeinitBroadcastBlock(&server);
    DeinitSocketBlock(&server);
    printf("Server stopped\n");
    return 0;
}
