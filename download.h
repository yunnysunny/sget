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