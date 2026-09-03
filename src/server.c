#include <stdio.h>
#include <string.h>
#include "socket.h"
#include "error.h"

int main() {
    Error err;
    Socket s;
    Addr local, client;

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
    Addr_SetPort(&local, 12345);
    if (Socket_Bind(&s, &local) != ERROR_OK) {
        fprintf(stderr, "Bind failed\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    Addr_SetIp(&client, "127.0.0.1");
    Addr_SetPort(&client, 12346);
    if (Socket_Connect(&s, &client) != ERROR_OK) {
        fprintf(stderr, "Connect to client failed\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }

    printf("Server waiting for message from client...\n");
    char buf[1024];
    int recvd;
    err = Socket_Recv(&s, buf, sizeof(buf)-1, &recvd);
    if (err != ERROR_OK) {
        fprintf(stderr, "Recv error\n");
        Socket_Close(&s);
        Socket_Deinit();
        return 1;
    }
    buf[recvd] = '\0';
    printf("Received: %s\n", buf);

    char *reply = "Hello from server";
    int sent;
    err = Socket_Send(&s, reply, strlen(reply), &sent);
    if (err != ERROR_OK) {
        fprintf(stderr, "Send error\n");
    } else {
        printf("Sent reply (%d bytes)\n", sent);
    }

    Socket_Close(&s);
    Socket_Deinit();
    return 0;
}
