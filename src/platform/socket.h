#ifndef SOCKET_H
#define SOCKET_H

#include "error.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
#endif

typedef struct Addr {
    struct sockaddr_in sin;
} Addr;

Error Addr_SetPort(Addr *addr, int port);
Error Addr_SetIp(Addr *addr, const char *ip);

typedef struct Socket {
    int fd;
    int is_connected;
} Socket;

Error Socket_Init();
void Socket_Deinit();

Error Socket_Open(Socket *s);
void Socket_Close(Socket *s);
Error Socket_SetTimeout(Socket *s, int timeout);
Error Socket_Bind(Socket *s, const Addr *addr);
Error Socket_Connect(Socket *s, const Addr *addr);
Error Socket_Send(Socket *s, const char *buf, int len, int *sentLen);
Error Socket_Recv(Socket *s, char *buf, int len, int *receivedLen);

#endif
