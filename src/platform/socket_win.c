#include "socket.h"

static int ws_init_count = 0;

Error Socket_Init(void) {
    if (ws_init_count == 0) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return ERROR_INIT;
        }
    }
    ws_init_count++;
    return ERROR_OK;
}

void Socket_Deinit(void) {
    if (ws_init_count>0) {
        ws_init_count--;
        if (ws_init_count==0) {
            WSACleanup();
        }
    }
}

Error Socket_Open(Socket *s) {
    if (!s) return ERROR_ARG;
    s->is_connected = 0;
    s->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s->fd==(int)INVALID_SOCKET) {
        return ERROR_INIT;
    }
    return ERROR_OK;
}

void Socket_Close(Socket *s) {
    if (s && s->fd!=(int)INVALID_SOCKET) {
        closesocket(s->fd);
        s->fd = INVALID_SOCKET;
        s->is_connected = 0;
    }
}

Error Addr_SetPort(Addr *addr, int port) {
    if (!addr || port < 0 || port > 65535) return ERROR_ARG;
    addr->sin.sin_port = htons(port);
    return ERROR_OK;
}

Error Addr_SetIp(Addr *addr, const char *ip) {
    if (!addr || !ip) return ERROR_ARG;
    addr->sin.sin_family = AF_INET;
    if (InetPtonA(AF_INET, ip, &addr->sin.sin_addr)!=1) 
        return ERROR_ARG;
    return ERROR_OK;
}

Error Socket_Bind(Socket *s, const Addr *addr) {
    if (!s || !addr) return ERROR_ARG;
    if (bind(s->fd, (struct sockaddr*)&addr->sin, sizeof(addr->sin)) == SOCKET_ERROR)
        return ERROR_INIT;
    return ERROR_OK;
}

Error Socket_Connect(Socket *s, const Addr *addr) {
    if (!s || !addr) return ERROR_ARG;
    if (connect(s->fd, (struct sockaddr*)&addr->sin, sizeof(addr->sin)) == SOCKET_ERROR) 
        return ERROR_INIT;
    s->is_connected = 1;
    return ERROR_OK;
}

Error Socket_Send(Socket *s, const char *buf, int len, int *sentLen) {
    if (sentLen) *sentLen = 0;
    if (!s || !buf || len < 0) return ERROR_ARG;
    if (!s->is_connected) return ERROR_ARG;

    int sent = send(s->fd, buf, len, 0);
    if (sent == SOCKET_ERROR) return ERROR_WRITE;
    if (sentLen) *sentLen = sent;

    return ERROR_OK;
}

Error Socket_Recv(Socket *s, char *buf, int len, int *receivedLen) {
    if (receivedLen) *receivedLen = 0;
    if (!s || !buf || len < 0) return ERROR_ARG;
    if (!s->is_connected) return ERROR_ARG;

    int recvd = recv(s->fd, buf, len, 0);
    if (recvd == SOCKET_ERROR) return ERROR_READ;
    if (receivedLen) *receivedLen = recvd;

    return ERROR_OK;
}
