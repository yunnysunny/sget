#include "win2linux.h"
#include "commom_socket.h"
#include "log.h"
#include "error_code.h"
#include "download.h"
#include <stdio.h>
#include <string.h>

static unsigned int errorCode = 0;

unsigned int getErrorCode()
{
	return errorCode;
}

SOCKET ConnectHttpNonProxy(const char *strHostAddr,int nPort)
{
	SOCKET hSocket;
	GetConnect(&hSocket,strHostAddr,nPort);
	if(hSocket == INVALID_SOCKET)
		return INVALID_SOCKET;

	return hSocket;
}
DWORD GetHeaderElement(const char *httpHeader,const char *key2find,STHttpHeadElement *element)
{
	int findLen = 0;
	char *endFind = END_OF_HTTP_HEADER_LINE;//行信息结束标志
	char *endStr;

	char *beginStr = strstr(httpHeader,key2find);//从关键字位置开始截取

	if (beginStr == NULL)
	{
		//printf("not found element:%s\n",key2find);
		return -1;
	}

	endStr = strstr(beginStr,endFind);//截取回车换行符
	if (endStr == NULL)
	{
		printf("not found end\n");
		return -2;
	}
	findLen = endStr - beginStr - strlen(key2find);//要拷贝的值的长度

	memset(element->strval,0,MAX_HEADER_ELEMENT_LEN);
	if (findLen >= MAX_HEADER_ELEMENT_LEN) {
		strncpy(element->strval,beginStr+strlen(key2find),MAX_HEADER_ELEMENT_LEN-1);
		element->strval[MAX_HEADER_ELEMENT_LEN-1] = 0;	
	} else {
		strncpy(element->strval,beginStr+strlen(key2find),findLen);
	}	

	//printf("find str is :%s\n",element->strval);
	if (element->type == HEADER_ELEMENT_NUM) {
		element->numval= atoi(element->strval);
	}
	return 0;
}
BOOL SocketSend(SOCKET sckDest,char *szHttp)
{
	int iLen=strlen(szHttp);
	if(send (sckDest,szHttp,iLen,0)==SOCKET_ERROR)
	{
		CloseSocket(sckDest);///////##############
		printf("发送请求失败\n");
		return FALSE;
	}

	return TRUE;
}
/**
* 获取http头部
*/
DWORD GetHttpHeader(SOCKET sckDest,char *str)
{
	BOOL bResponsed=FALSE;
	DWORD nResponseHeaderSize;

	if(!bResponsed)
	{
		char c = 0;
		int nIndex = 0;
		BOOL bEndResponse = FALSE;
		while(!bEndResponse && nIndex < 1024)//只获取前面1K的内容 
		{
			recv(sckDest,&c,1,0);
			str[nIndex++] = c;
			if(nIndex >= 4)
			{
				if( str[nIndex - 4] == '\r' && 
					str[nIndex - 3] == '\n' && 
					str[nIndex - 2] == '\r' && 
					str[nIndex - 1] == '\n')
					bEndResponse = TRUE;
			}
		}

		str[nIndex]=0;
		nResponseHeaderSize = nIndex;
		bResponsed = TRUE;
	}

	return nResponseHeaderSize;
}

static char *getDownFilename(const char *str) {
	char *filenameTag = "filename=";
	int filenameTagLen = strlen(filenameTag);
	char *filenameBegin = strstr(str,"filename=");
	if (filenameBegin != NULL && strlen(filenameBegin) > filenameTagLen) {		
		return filenameBegin+filenameTagLen;
	} else {
		return NULL;		
	}
}

bool SendHttpHeader(SOCKET hSocket,const char *strHostAddr,int nHttpPort,
			   const char *strHttpAddr,const char *strHttpFilename,unsigned long *filesize,char *savedFilename)
{
	// 
	char sTemp[MAX_HTTP_HEADER_LINE_LEN] = {0};
	int temLenWrite = sizeof(sTemp) - 1;	
	int httpBeginLen = strlen("http://");

	char *httpPath = strstr(strHttpAddr + httpBeginLen,"/");
	int headerLen = 0;

	// Line1: 请求的路径,版本.
	//sTemp.Format("GET %s%s HTTP/1.1\r\n",strHttpAddr,strHttpFilename);
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"GET %s HTTP/1.1\r\n",httpPath);
	sTemp[temLenWrite] = '\0';

	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// Line2:主机.
	memset(sTemp,0,sizeof(sTemp));
	if (nHttpPort == 80) {
		snprintf(sTemp,temLenWrite,"Host: %s\r\n",strHostAddr);
		sTemp[temLenWrite] = '\0';
	} else {
		snprintf(sTemp,temLenWrite,"Host: %s:%d\r\n",strHostAddr,nHttpPort);
		sTemp[temLenWrite] = '\0';
	}

	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// Line3:接收的数据类型.
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n");
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// Line4:参考地址http_refer.
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"Referer: %s\r\n",strHttpAddr); 
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// Line5:浏览器类型.
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,sizeof(DEFAULT_USER_AGENT),"%s",DEFAULT_USER_AGENT);
	strcat(sTemp,"\0");
	if(!SocketSend(hSocket,sTemp)) return FALSE;
	
	//Line6 可接受语言
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,sizeof(DEFAULT_ACCEPT_LANGUAGE),"%s",DEFAULT_ACCEPT_LANGUAGE);
	strcat(sTemp,"\0");
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// 续传. Range 是要下载的数据范围，对续传很重要.
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"Range: bytes=0-\r\n");

	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// LastLine: 空行.
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"%s","\r\n");
	strcat(sTemp,"\0");
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	// 取得http头.
	memset(sTemp,0,sizeof(sTemp));
	headerLen=GetHttpHeader(hSocket,sTemp);
		
	if(!headerLen)//响应信息为空
	{
		printf("鑾峰彇HTTP澶村嚭閿?\n");
		return FALSE;
	}

	//  如果取得的http头含有404等字样，则表示连接出问题.	
	if(strstr(sTemp,"404")!=NULL) return FALSE;

	// 得到待下载文件的大小.
	if (filesize != NULL)
	{	
		int rv;
		STHttpHeadElement element;
		element.type = HEADER_ELEMENT_NUM;
		memset(element.strval,0,MAX_HEADER_ELEMENT_LEN);

		rv = GetHeaderElement(sTemp,HTTP_HEADER_CONTENT_LENGTH,&element);
		if (rv == 0) {
			*filesize = element.numval;
		} else {
			*filesize = 0;
		}	

		if (savedFilename != NULL) {
			element.type = HEADER_ELEMENT_STRING;
			rv = GetHeaderElement(sTemp,HTTP_HEADER_CONTENT_DISPOSITION,&element);
			if (rv == 0) {
				char *filename = getDownFilename(element.strval);
				if (filename != NULL) {
					strcpy(savedFilename,filename);
				}
			} else if (strHttpFilename != NULL) {
				//strHttpFilename缓冲区长度URL_FILENAME_LEN
				//savedFilename缓冲区长度MAX_HEADER_ELEMENT_LEN
				strcpy(savedFilename,strHttpFilename);
			}
		}
	}	
	return TRUE;
}

UINT DownLoadProcess(const char *szHostAddr,int nHostPort, 
					const char *strHttpAddr,
					const char *strHttpFilename,const char *savedName,unsigned long m_nFileLength)
{
	SOCKET hSocket;
	DWORD nLen; 
	unsigned long nSumLen=0; 
	char szBuffer[1024] = {0};//------------每次读取1KB的内容--------------------
	FILE *savefp = NULL;

	if ((savefp = fopen(savedName,"wb")) == NULL)
	{
		printf("生成保存文件失败\n");
		return 2;
	}

	hSocket=ConnectHttpNonProxy(szHostAddr,nHostPort);

	if(hSocket == INVALID_SOCKET) return 1;

	SendHttpHeader(hSocket,szHostAddr, nHostPort,strHttpAddr,strHttpFilename,NULL,NULL);	

	while(1)
	{		
		nLen=recv(hSocket,szBuffer,sizeof(szBuffer),0);	

		if (nLen == SOCKET_ERROR){
			printf("Read error!\n");
			return 1;
		}
		if(nLen==0) break;

		fwrite(szBuffer,sizeof(char),nLen,savefp);
		
		nSumLen +=nLen;
		//printf("has downloaded:%d\n",nSumLen);
		printf("\r %4d%c%4d", nSumLen, '/', m_nFileLength);
		if(m_nFileLength>0 && nSumLen>=m_nFileLength){ 			
			break;
		}	
	}

	CloseSocket(hSocket); // 断开连接
	return 0;
}

bool HttpDownLoad(
					 const char *strHostAddr,
					 int nHttpPort,
					 const char *strHttpAddr,
					 const char *strHttpFilename,
					 const char *saveFolder )
{
	SOCKET hSocket;
	unsigned long filesize = 0;
	char saveFilename[MAX_HEADER_ELEMENT_LEN] = {0};

	memset(saveFilename,0,MAX_HEADER_ELEMENT_LEN);
	hSocket=ConnectHttpNonProxy(strHostAddr,nHttpPort);//
	if(hSocket == INVALID_SOCKET){
		printf("服务器无法连接\n");
		return 1;
	}
	// 发送文件头，计算文件大小.
	SendHttpHeader(hSocket,strHostAddr,nHttpPort,strHttpAddr,strHttpFilename,&filesize,saveFilename);
	CloseSocket(hSocket);	

	if (saveFilename == NULL)
	{
		printf("获取保存文件名失败\n");
		return FALSE;
	}
	if (saveFolder != NULL)
	{
		int nameLen = strlen(saveFilename);
		int folderLen = strlen(saveFolder);
		char *endChar = strrchr(saveFolder,"/");
		bool endFolder = false;
		if (nameLen + folderLen +2 > MAX_HEADER_ELEMENT_LEN)
		{
			printf("保存文件夹路径名过长\n");
			return FALSE;
		}
		if (endChar != NULL && strlen(endChar) == 1)
		{
			sprintf(saveFilename,"%s%s",saveFolder,saveFilename);
			endFolder = true;
		}
		else
		{
			endChar = strrchr(saveFolder,"\\");
			if (endChar != NULL && strlen(endChar) == 1)
			{
				sprintf(saveFilename,"%s%s",saveFolder,saveFilename);
				endFolder = true;
			}
		}
		if (endFolder == false)
		{
			sprintf(saveFilename,"%s/%s",saveFolder,saveFilename);
		}
	}
	if(DownLoadProcess(strHostAddr,nHttpPort,strHttpAddr,strHttpFilename,saveFilename,filesize))
		return FALSE;

	return TRUE;
}

/**
* [in] const char* URL url地址
* [out] char *host 主机名
* [out] char *port 端口号
* [out] char *path 路径名
* [out] char *filename 文件名
*/
bool ParseURL(const char*URL,char *host,char *port, char *path,char *filename)
{
	char *httpBegin = "http://";
	char *portStart = NULL;

	char *hostEnd = NULL;
	char *fileStart = NULL;
	int hostLen = 0;
	int portLen = 0;
	int pathLen = 0;

	portStart = strstr(URL + strlen(httpBegin),":");
	hostEnd = strstr(URL + strlen(httpBegin),"/");
	if (portStart == NULL) {
		port = "80";		
		hostLen = hostEnd - URL - strlen(httpBegin);
	} else {		
		portLen = hostEnd - portStart -1;
		hostLen = portStart - URL  - strlen(httpBegin);
		if (portLen > HTTP_PORT_LEN) {
			return false;
		}
		memset(port,0,HTTP_PORT_LEN);
		strncpy(port,portStart+1,portLen);
	}

	if (hostLen > HOST_STR_LEN)
	{
		return false;
	}
	memset(host,0,HOST_STR_LEN);
	strncpy(host,URL + strlen(httpBegin),hostLen);

	pathLen = strlen(hostEnd);
	if (pathLen > URL_STR_LEN)
	{
		return false;
	}
	memset(path,0,URL_STR_LEN);
	strcpy(path,hostEnd);

	fileStart = strrchr(path,'/');
	if (fileStart != NULL)
	{
		int fileLen = strlen(fileStart);
		memset(filename,0,URL_FILENAME_LEN);
		if (fileLen > 1 && fileLen < URL_FILENAME_LEN)
		{
			fileStart += 1;			
			strcpy(filename,fileStart);
		}		
	}	
	return true;
}

// 下载入口函数
bool WHY_Download(const char*strUrl,
				  const char *saveFolder )
{
	char  strHostAddr[HOST_STR_LEN] = {0};
	char  strHttpAddr[URL_STR_LEN] = {0};
	char  strHttpFilename[URL_FILENAME_LEN] = {0};
	char  strHttpPort[HTTP_PORT_LEN] = {0};
	bool downrv = true;	
#if defined(WIN32) || defined(WIN64)
	WSADATA WsaData;
#endif

	if (ParseURL(strUrl,strHostAddr,strHttpPort,strHttpAddr,strHttpFilename))
	{
		int nHostPort = atoi((const char*)strHttpPort);
		if (nHostPort == 0)
		{
			nHostPort = 80;
		}
#if defined(WIN32) || defined(WIN64)
		if (( WSAStartup(MAKEWORD(1,1),&WsaData)) != 0)
		{
			LOG(LOG_ERROR,ERROR_SOCKET_INIT, "CheckServer->WSAStartup error");
			return false;
		}
#endif
		if(!HttpDownLoad(strHostAddr,nHostPort,strUrl,strHttpFilename,saveFolder)) {			
			downrv = false;
		} else {

		}
	}
	else
	{
		downrv = false;
	}
	
	return downrv;
}
