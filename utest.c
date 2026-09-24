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
#include "download.h"
#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WIN32) || defined(WIN64)
#include <io.h>
#include <direct.h>
#include <locale.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "win2linux.h"

static void print_usage(const char *prog)
{
	printf("Usage: %s <url> [saveFolder] [-t thread_count]\n", prog);
	printf("  url           HTTP URL to download\n");
	printf("  saveFolder    Directory to save the file (optional)\n");
	printf("  -t N          Number of download threads (default: %d)\n", DEFAULT_THREAD_COUNT);
}

int main(int argc, char *argv[])
{
	const char *url = NULL;
	const char *saveFolder = NULL;
	int thread_count = 0; /* 0 = use default */
	int rv;
	int i;

#if defined(WIN32) || defined(WIN64)
	setlocale(LC_ALL,"chs");
#endif

	if (argc < 2)
	{
		print_usage(argv[0]);
		return 1;
	}

	/* Parse arguments */
	url = argv[1];

	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0) {
			char *end;
			long requested;
			if (i + 1 >= argc) {
				fprintf(stderr, "Missing thread count after -t\n");
				return 1;
			}
			requested = strtol(argv[++i], &end, 10);
			if (*end != '\0' || requested < 1 || requested > SGET_MAX_THREAD_COUNT) {
				fprintf(stderr, "Thread count must be between 1 and %d\n", SGET_MAX_THREAD_COUNT);
				return 1;
			}
			thread_count = (int)requested;
		} else if (saveFolder == NULL) {
			saveFolder = argv[i];
		}
	}

	printf("URL: %s\n", url);
	if (saveFolder != NULL) {
		if ((access(saveFolder, 0)) == -1) {
#if defined(WIN32) || defined(WIN64)
			if (mkdir(saveFolder) != 0) {
#else
			if (mkdir(saveFolder, 0755) != 0) {
#endif
				perror("Failed to create save folder");
				return 1;
			}
		}
	}
	if (thread_count > 0) {
		printf("Thread count: %d\n", thread_count);
	}

	rv = sget_download_mt(url, saveFolder, thread_count);
	if (rv)
	{
		printf("\nDownload succeeded\n");
	}
	else
	{
		printf("\nDownload failed (error 0x%08x)\n", getErrorCode());
	}

	return rv ? 0 : 1;
}
