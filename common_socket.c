/**
Open Source Initiative OSI - The MIT License (MIT):Licensing

The MIT License (MIT)
Copyright (c) <2012> <yunnysunny>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.

@author yunnysunny<yunnysunny@gmail.com>

*/
#include "common_socket.h"
#include "log.h"
#include "error_code.h"
#include <string.h>
#if defined(WIN32) || defined(WIN64)
#define USE_WIN_NOW
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#endif

#define SGET_TIMEOUT_SECONDS 30

unsigned int GetConnect(SOCKET *socketRt,const char   *sServerAddr, int  nPort) {
	struct sockaddr_in addr;
	SOCKET socketfd;
	struct addrinfo hints, *res = NULL;

#if defined(WIN32) || defined(WIN64)
	BOOL bNodelay = TRUE;
#else
	int bNodelay = 1;
#endif
	int on = 1;

	LOG(LOG_TRACE,0, "ConnectServer");
	*socketRt = INVALID_SOCKET;

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd == INVALID_SOCKET)
	{
		LOG(LOG_ERROR,ERROR_SOCKET_CREATE, "ConnectServer->socket");
		return ERROR_SOCKET_CREATE;
	}

	LOG(LOG_TRACE,0, "ConnectServer->setsockopt");

	if (setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on)))
	{
#if defined(WIN32) || defined(WIN64)
		LOG(LOG_ERROR,GetLastError(), "ConnectServer->setsockopt");
#else
		LOG(LOG_ERROR,errno, "ConnectServer->setsockopt");
#endif
		CloseSocket(socketfd);
		return ERROR_SET_REUSE_ADDR;
	}

	/* Socket buffers are left to the OS autotuning; a hard 20 KB cap throttles throughput. */

	if(setsockopt(socketfd,IPPROTO_TCP,TCP_NODELAY,(const char*)&bNodelay,sizeof(bNodelay)))
	{
		LOG(LOG_ERROR,ERROR_SET_TCP, "ConnectServer->setsockopt");
		CloseSocket(socketfd);
		return ERROR_SET_TCP;
	}

	addr.sin_family = AF_INET;

	/* Thread-safe DNS resolution via getaddrinfo */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(sServerAddr, NULL, &hints, &res) != 0 || res == NULL)
	{
#if defined(WIN32) || defined(WIN64)
		LOG(LOG_ERROR,WSAGetLastError(),"Unable to get the host name. ");
#else
		LOG(LOG_ERROR,errno, "Unable to get the host name. ");
#endif
		if (res) freeaddrinfo(res);
		CloseSocket(socketfd);
		return ERROR_GET_HOST_NAME;
	}

	addr.sin_port = htons((short)nPort);
	memcpy(&addr.sin_addr,
		&((struct sockaddr_in *)res->ai_addr)->sin_addr,
		sizeof(struct in_addr));
	freeaddrinfo(res);

	LOG(LOG_TRACE,0, "ConnectServer->connect");
	if (connect(socketfd,(struct sockaddr *)&addr,sizeof(addr)) < 0) 
	{
#if defined(WIN32) || defined(WIN64)
		LOG(LOG_ERROR,GetLastError(), "ConnectServer->connect");
#else
		LOG(LOG_ERROR,errno, "ConnectServer->connect");
#endif
		CloseSocket(socketfd);
		return ERROR_SOCKET_CONNECT;
	}	

	/* Set recv and send timeouts */
#if defined(WIN32) || defined(WIN64)
	{
		DWORD timeout_ms = SGET_TIMEOUT_SECONDS * 1000;
		setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
		setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
	}
#else
	{
		struct timeval tv;
		tv.tv_sec = SGET_TIMEOUT_SECONDS;
		tv.tv_usec = 0;
		setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	}
#endif

	LOG(LOG_TRACE,0, "ConnectServer->return");
	*socketRt = socketfd;
	return ERROR_SUCCESS;
}