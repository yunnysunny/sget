#ifndef COMMON_SOCKET_H_
#define COMMON_SOCKET_H_
#if defined(WIN32) || defined(WIN64)
#include <winsock2.h>
#include <windows.h>
#pragma comment (lib,"ws2_32")
#else
#include <sys/socket.h>
#define INVALID_SOCKET				(-1)
#define SOCKET_ERROR				(-1)
typedef  int SOCKET;
#endif

#if defined(WIN32) || defined(WIN64)
#define CloseSocket(fd) do{ closesocket(fd); shutdown(fd, 2); }while(0)
#else
#define CloseSocket(fd) do{ close(fd); shutdown(fd,2); }while(0)
#endif 

#define SO_SNDBUF_SIZE (20480)
#define SO_RCVBUF_SIZE (20480)

unsigned int GetConnect(SOCKET *socket,const char   *sServerAddr, int  nPort);
#endif

