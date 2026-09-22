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
#ifndef WIN_TO_LINUX_H_
#define WIN_TO_LINUX_H_
#include <stdint.h>
#include <stdbool.h>
#ifndef TRUE
#define TRUE								1
#endif
#ifndef FALSE
#define FALSE							0
#endif

#if !defined(WIN32) && !defined(WIN64)
typedef unsigned int DWORD;
typedef int BOOL;
typedef unsigned int UINT;
#endif

#if defined(WIN32) || defined(WIN64)
#define access		_access
#define mkdir			_mkdir
#endif

/* --- 64-bit file offset type --- */
#if defined(WIN32) || defined(WIN64)
#ifndef _WINSOCKAPI_
#include <winsock2.h>
#endif
#include <windows.h>
typedef __int64 sget_off_t;
#else
#include <sys/types.h>
typedef off_t sget_off_t;
#endif

/* --- Portable pwrite (write at offset without changing file position) --- */
#if defined(WIN32) || defined(WIN64)
#include <io.h>
/* Implemented in win2linux.c */
int sget_pwrite(int fd, const void *buf, unsigned int count, uint64_t offset);
int sget_open_rw(const char *path);
int sget_truncate(int fd, uint64_t size);
int sget_close(int fd);
#else
#include <unistd.h>
#include <fcntl.h>
#define sget_pwrite(fd, buf, count, offset) pwrite(fd, buf, count, offset)
#define sget_open_rw(path) open(path, O_CREAT | O_WRONLY, 0644)
#define sget_truncate(fd, size) ftruncate(fd, (off_t)(size))
#define sget_close(fd) close(fd)
#endif

#endif