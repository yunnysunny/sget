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
