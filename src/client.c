#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "error.h"
#include "socket.h"
#include "protocol.h"

#define DEFAULT_SERVER_IP   "127.0.0.1"
#define DEFAULT_SERVER_PORT 12345
#define DEFAULT_LOCAL_IP    "0.0.0.0"
#define DEFAULT_LOCAL_PORT  12346
#define DEFAULT_TIMEOUT_MS  50
#define DEFAULT_FRAMES      30
#define DEFAULT_OUTPUT      "received.png"

typedef struct {
    const char *server_ip;
    int         server_port;
    const char *local_ip;
    int         local_port;
    int         timeout_ms;
    int         frames;
    const char *output;
} Config;

static void PrintUsage(const char *prog) {
    printf(
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Options:\n"
        "  -s, --server-ip <IP>     Server IP address (default: %s)\n"
        "  -p, --server-port <PORT> Server port       (default: %d)\n"
        "  -l, --local-ip <IP>      Local bind IP     (default: %s)\n"
        "  -b, --local-port <PORT>  Local bind port   (default: %d)\n"
        "  -t, --timeout <MS>       Socket timeout ms (default: %d)\n"
        "  -n, --frames <N>         Number of frames  (default: %d)\n"
        "  -o, --output <FILE>      Output PNG file   (default: %s)\n"
        "  -h, --help               Show this help\n",
        prog,
        DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT,
        DEFAULT_LOCAL_IP,  DEFAULT_LOCAL_PORT,
        DEFAULT_TIMEOUT_MS, DEFAULT_FRAMES, DEFAULT_OUTPUT
	);
}

static int ParseInt(const char *s, int *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0' || v < 0 || v > 65535)
        return -1;
    *out = (int)v;
    return 0;
}

static int ParseArgs(int argc, char **argv, Config *cfg) {
    cfg->server_ip  = DEFAULT_SERVER_IP;
    cfg->server_port = DEFAULT_SERVER_PORT;
    cfg->local_ip   = DEFAULT_LOCAL_IP;
    cfg->local_port = DEFAULT_LOCAL_PORT;
    cfg->timeout_ms = DEFAULT_TIMEOUT_MS;
    cfg->frames     = DEFAULT_FRAMES;
    cfg->output     = DEFAULT_OUTPUT;

    static struct option long_opts[] = {
        {"server-ip",   required_argument, 0, 's'},
        {"server-port", required_argument, 0, 'p'},
        {"local-ip",    required_argument, 0, 'l'},
        {"local-port",  required_argument, 0, 'b'},
        {"timeout",     required_argument, 0, 't'},
        {"frames",      required_argument, 0, 'n'},
        {"output",      required_argument, 0, 'o'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "s:p:l:b:t:n:o:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 's': cfg->server_ip = optarg; break;
            case 'l': cfg->local_ip  = optarg; break;
            case 'o': cfg->output    = optarg; break;

            case 'p':
                if (ParseInt(optarg, &cfg->server_port) != 0) {
                    printf("Invalid --server-port: %s\n", optarg);
                    return -1;
                }
                break;
            case 'b':
                if (ParseInt(optarg, &cfg->local_port) != 0) {
                    printf("Invalid --local-port: %s\n", optarg);
                    return -1;
                }
                break;
            case 't':
                if (ParseInt(optarg, &cfg->timeout_ms) != 0) {
                    printf("Invalid --timeout: %s\n", optarg);
                    return -1;
                }
                break;
            case 'n':
                if (ParseInt(optarg, &cfg->frames) != 0 || cfg->frames <= 0) {
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

static void Exit(Socket *s, ImageTranferer *img, int code) {
    if (img) if (img->data) free(img->data);
    Socket_Close(s);
	Socket_Deinit();
    exit(code);
}

int main(int argc, char **argv) {
    Config cfg;
    if (ParseArgs(argc, argv, &cfg) != 0)
        return 2;

	/*
    printf("Config: server=%s:%d local=%s:%d timeout=%dms frames=%d output=%s\n",
    	cfg.server_ip, cfg.server_port, cfg.local_ip,  cfg.local_port, cfg.timeout_ms, cfg.frames, cfg.output);
	*/

    Socket s;
    Addr local, server;

    if (Socket_Init() != ERROR_OK) {
        printf("Socket_Init failed\n");
        return 1;
    }
    if (Socket_Open(&s) != ERROR_OK) {
        printf("Socket_Open failed\n");
        Exit(&s, NULL, 1);
    }
    if (Socket_SetTimeout(&s, cfg.timeout_ms)) {
        printf("Socket_SetTimeout failed\n");
        Exit(&s, NULL, 1);
    }

    Addr_SetIp(&local, cfg.local_ip);
    Addr_SetPort(&local, (unsigned short)cfg.local_port);
    if (Socket_Bind(&s, &local) != ERROR_OK) {
        printf("Bind %s:%d failed\n", cfg.local_ip, cfg.local_port);
        Exit(&s, NULL, 1);
    }

    Addr_SetIp(&server, cfg.server_ip);
    Addr_SetPort(&server, (unsigned short)cfg.server_port);
    if (Socket_Connect(&s, &server) != ERROR_OK) {
        printf("Connect to %s:%d failed\n", cfg.server_ip, cfg.server_port);
        Exit(&s, NULL, 1);
    }

    ImageTranferer img;
    img.data = malloc(1024 * 1024 * 3);
    img.dataCapacity = 1024 * 1024 * 3;
    if (!img.data) {
        printf("Out of memory\n");
        Exit(&s, NULL, 1);
    }

    int count = 0;
    printf("starting transfer\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < cfg.frames; ++i) {
        Error err = ImageTranferer_Req(&img, &s, -1);
        if (!err) count += 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    printf("transfer ended\n");

    double time = (end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
    printf("Received %d/%d in %fs\n", count, cfg.frames, time);

    int success = stbi_write_png(cfg.output, img.width, img.height, img.channels, img.data, 0);
    if (!success) {
        printf("Failed to save image %s\n", cfg.output);
        Exit(&s, &img, 1);
    }
    printf("Image saved as %s\n", cfg.output);

    Exit(&s, &img, 0);
}
