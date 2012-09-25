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

