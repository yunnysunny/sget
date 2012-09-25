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
#ifndef ERROR_CODE_H_
#define ERROR_CODE_H_

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS									0x0
#endif

#define SOCKET_ERROR_BASE							0x10000000
#define ERROR_SOCKET_CREATE						SOCKET_ERROR_BASE + 1
#define ERROR_SOCKET_INIT							SOCKET_ERROR_BASE + 2
#define ERROR_SET_SEND_BUFF						SOCKET_ERROR_BASE + 3
#define ERROR_SET_RECV_BUFF						SOCKET_ERROR_BASE + 4
#define ERROR_SET_REUSE_ADDR					SOCKET_ERROR_BASE + 5
#define ERROR_SET_LINGER								SOCKET_ERROR_BASE + 6
#define ERROR_SET_TCP									SOCKET_ERROR_BASE + 7
#define ERROR_SOCKET_CONNECT					SOCKET_ERROR_BASE + 8
#define ERROR_GET_HOST_NAME					SOCKET_ERROR_BASE + 9

#endif