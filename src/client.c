#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "error.h"
#include "socket.h"
#include "protocol.h"

int main() {
    Socket s;
    Addr local, server;

	struct timespec start, end;

    if (Socket_Init() != ERROR_OK) {
        fprintf(stderr, "Socket_Init failed\n");
        return 1;
    }

    if (Socket_Open(&s) != ERROR_OK) {
        fprintf(stderr, "Socket_Open failed\n");
        Socket_Deinit();
        return 1;
    }
	
	if (Socket_SetTimeout(&s, 50)) {
        fprintf(stderr, "Socket_SetTimeout failed\n");
        Socket_Deinit();
        return 1;
	}

    Addr_SetIp(&local, "0.0.0.0");
    Addr_SetPort(&local, 12346);
    if (Socket_Bind(&s, &local) != ERROR_OK) {
        fprintf(stderr, "Bind failed\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    Addr_SetIp(&server, "127.0.0.1");
    Addr_SetPort(&server, 12345);
    if (Socket_Connect(&s, &server) != ERROR_OK) {
        fprintf(stderr, "Connect to server failed\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }


    ImageTranferer img;
    img.data = malloc(1024*1024*3);
    img.dataCapacity = 1024*1024*3;
	
	int framesCount = 120;
	int count = 0;

	printf("starting transfer\n");
	clock_gettime(CLOCK_MONOTONIC, &start);

	for (int i=0; i<framesCount; ++i) {
		Error err = ImageTranferer_Req(&img, &s, -1);
		if (!err) count+=1;
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("transfer ended\n");

	double time = (end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
    printf("Received %d/%d in %fs\n", count, framesCount, time);

    int success = stbi_write_png("received.png", img.width, img.height, img.channels, img.data, 0);
    if (!success) {
        fprintf(stderr, "Failed to save image received.png\n");
        free(img.data);
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    printf("Image saved as received.png\n");

	free(img.data);

    Socket_Close(&s);
    Socket_Deinit();
    return 0;
}
