#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include "socket.h"
#include "error.h"
#include "protocol.h"

int main() {
    Error err;
    Socket s;
    Addr local, client;

    err = Socket_Init();
	if (err) {
        fprintf(stderr, "Socket_Init failed\n");
        return 1;
	}

	err = Socket_Open(&s);
    if (err) {
        fprintf(stderr, "Socket_Open failed\n");
        Socket_Deinit();
        return 1;
    }

    Addr_SetIp(&local, "0.0.0.0");
    Addr_SetPort(&local, 12345);
	err = Socket_Bind(&s, &local);
    if (err) {
        fprintf(stderr, "Bind failed\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    Addr_SetIp(&client, "127.0.0.1");
    Addr_SetPort(&client, 12346);
	err = Socket_Connect(&s, &client);
    if (err) {
        fprintf(stderr, "Connect to client failed\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    int width, height, channels;
    unsigned char *img_data = stbi_load("test.png", &width, &height, &channels, 0);
    if (!img_data) {
        fprintf(stderr, "Failed to load image test.png\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    printf("Loaded image: %dx%d, %d channels\n", width, height, channels);

    ImageTranferer img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data = img_data;
    img.dataCapacity = width * height * channels;

	while(1) {
		Packet p;
		Packet_Recv(&p, &s, -1);
		if (p.type!=PACKET_GET_IMAGE) continue;

		err = ImageTranferer_Send(&img, &s);
		if (err) {
			fprintf(stderr, "ImageTranferer_Send failed: %d\n", err);
			stbi_image_free(img_data);
			Socket_Close(&s);
			Socket_Deinit();
			return 1;
		}
	}

    stbi_image_free(img_data);
    Socket_Close(&s);
    Socket_Deinit();
    return 0;
}
