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
#include <stdio.h>
#include <stdlib.h>

#if defined(WIN32) || defined(WIN64)
#include <io.h>
#include <direct.h>
#include <locale.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

#include "win2linux.h"

int main( int argc, char *argv[ ] , char *envp[ ]  )
{
	const char*url;
	const char *saveFolder = NULL;
	int rv;

#if defined(WIN32) || defined(WIN64)
	//system("chcp 65001");
	setlocale(LC_ALL,"chs");
#endif

	if (argc == 1)
	{
		printf("you haven't given the download url.\n");
		return 1;
	}
	url = argv[1];//一个url地址例如：http://127.0.0.1:8880/qloa_v1.2.7z
	printf("the url you wanna download now:%s\n",url);
	if (argc > 2)
	{
		saveFolder = argv[2];
		if( (access( saveFolder, 0 )) == -1 ) 
		{
			mkdir(saveFolder);
		}
	}
	
	rv = WHY_Download(url,saveFolder);
	if (rv)
	{
		printf("下载完成\n");		
	}
	else
	{
		printf("下载失败\n");
	}
}
