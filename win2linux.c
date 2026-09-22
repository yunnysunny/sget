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
#include <string.h>
#include "win2linux.h"

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

int sget_pwrite(int fd, const void *buf, unsigned int count, uint64_t offset) {
	HANDLE h;
	OVERLAPPED ov;
	DWORD written = 0;
	BOOL ok;

	h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) return -1;

	memset(&ov, 0, sizeof(ov));
	ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
	ov.OffsetHigh = (DWORD)(offset >> 32);

	ok = WriteFile(h, buf, count, &written, &ov);
	if (!ok) return -1;
	return (int)written;
}

int sget_open_rw(const char *path) {
	return _open(path, _O_CREAT | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
}

int sget_truncate(int fd, uint64_t size) {
	return _chsize_s(fd, size) == 0 ? 0 : -1;
}

int sget_close(int fd) {
	return _close(fd);
}
#endif
