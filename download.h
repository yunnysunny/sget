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
#ifndef DOWNLOAD_H_
#define DOWNLOAD_H_
#include "win2linux.h"

#define HOST_STR_LEN							64
#define URL_STR_LEN							2048
#define URL_FILENAME_LEN					64
#define HTTP_PORT_LEN						8
#define MAX_HTTP_HEADER_LINE_LEN		2048

#define END_OF_HTTP_HEADER_LINE		"\r\n"
#define HTTP_HEADER_CONTENT_LENGTH			"Content-Length:"
#define HTTP_HEADER_CONTENT_DISPOSITION			"Content-Disposition:"

#define DEFAULT_USER_AGENT				"User-Agent: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET4.0C; .NET4.0E)\r\n"
#define DEFAULT_ACCEPT_LANGUAGE	"Accept-Language: zh-cn\r\n"

#define MAX_HEADER_ELEMENT_LEN							512
#define HEADER_ELEMENT_STRING							0
#define HEADER_ELEMENT_NUM							1

typedef struct HttpHeadElement 
{
	char type;	
	char strval[MAX_HEADER_ELEMENT_LEN];
	unsigned int numval;	
}STHttpHeadElement;

bool WHY_Download(const char*strUrl,
				  const char *saveFolder );
unsigned int getErrorCode();
#endif