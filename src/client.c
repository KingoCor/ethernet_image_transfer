#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include "socket.h"
#include "error.h"
#include "protocol.h"

int main() {
    Socket s;
    Addr local, server;

    if (Socket_Init() != ERROR_OK) {
        fprintf(stderr, "Socket_Init failed\n");
        return 1;
    }

    if (Socket_Open(&s) != ERROR_OK) {
        fprintf(stderr, "Socket_Open failed\n");
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

	uint8_t data[1024*1024*3];

    ImageTranferer img;
    img.data = data;
    img.dataCapacity = 1024*1024*3;

    ImageTranferer_Recv(&img, &s, 5000);

    printf("Received image: %dx%d, %d channels\n", img.width, img.height, img.channels);

	FILE *f = fopen("raw_received.bin", "wb");
	int total_size = img.width * img.height * img.channels;
	fwrite(img.data, 1, total_size, f);
	fclose(f);

    // Save as PNG
    int success = stbi_write_png("received.png", img.width, img.height, img.channels, img.data, 0);
    if (!success) {
        fprintf(stderr, "Failed to save image received.png\n");
        free(img.data);
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    printf("Image saved as received.png\n");

    Socket_Close(&s);
    Socket_Deinit();
    return 0;
}
