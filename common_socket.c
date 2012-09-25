#include "commom_socket.h"
#include "log.h"
#include "error_code.h"
#if defined(WIN32) || defined(WIN64)
#define USE_WIN_NOW
#else
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

unsigned int GetConnect(SOCKET *socketRt,const char   *sServerAddr, int  nPort) {
	struct sockaddr_in addr;
	int socketfd;
	struct hostent *phostent = NULL;

#if defined(WIN32) || defined(WIN64)
	BOOL bNodelay = TRUE;
#else
	int bNodelay = 1;
#endif
	int on = 1;
	int so_buf_size;
	struct linger so_linger;

	LOG(LOG_TRACE,0, "ConnectServer");
	*socketRt = INVALID_SOCKET;

	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		LOG(LOG_ERROR,ERROR_SOCKET_CREATE, "ConnectServer->socket");
		return ERROR_SOCKET_CREATE;
	}

	LOG(LOG_TRACE,0, "ConnectServer->setsockopt");

	if (setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on)))
	{
#if defined(WIN32) || defined(WIN64)
		LOG(LOG_ERROR,GetLastError(), "ConnectServer->setsockopt");
#else
		LOG(LOG_ERROR,errno, "ConnectServer->setsockopt");
#endif
		return ERROR_SET_REUSE_ADDR;
	}

	so_linger.l_onoff = 1;
	so_linger.l_linger = 0;
	if (setsockopt(socketfd,SOL_SOCKET,SO_LINGER,(char *)&so_linger,sizeof(struct linger)))
	{
		LOG(LOG_ERROR,ERROR_SET_LINGER, "ConnectServer->setsockopt");
		return ERROR_SET_LINGER;
	}

	so_buf_size = SO_RCVBUF_SIZE;
	if(setsockopt(socketfd,SOL_SOCKET,SO_RCVBUF,(const char *)&so_buf_size,sizeof(so_buf_size)))
	{
		LOG(LOG_ERROR,ERROR_SET_RECV_BUFF, "ConnectServer->setsockopt");
		CloseSocket(socketfd);
		return ERROR_SET_RECV_BUFF;
	}
	so_buf_size = SO_SNDBUF_SIZE;
	if(setsockopt(socketfd,SOL_SOCKET,SO_SNDBUF,(const char *)&so_buf_size,sizeof(so_buf_size)))
	{
		LOG(LOG_ERROR,ERROR_SET_SEND_BUFF, "ConnectServer->setsockopt");
		CloseSocket(socketfd);
		return ERROR_SET_SEND_BUFF;
	}

	if(setsockopt(socketfd,IPPROTO_TCP,TCP_NODELAY,(const char*)&bNodelay,sizeof(bNodelay)))
	{
		LOG(LOG_ERROR,ERROR_SET_TCP, "ConnectServer->setsockopt");
		CloseSocket(socketfd);
		return ERROR_SET_TCP;
	}

	addr.sin_family = AF_INET;

	// 获取与主机相关的信息.
	if ((phostent = gethostbyname (sServerAddr)) == NULL) 
	{
		LOG(LOG_ERROR,WSAGetLastError (),"Unable to get the host name. ");
		closesocket (socketfd);
		return ERROR_GET_HOST_NAME;
	}

	addr.sin_port = htons((short)nPort);
	//addr.sin_addr.s_addr = inet_addr(sServerAddr); 
	// 给套接字IP地址赋值.
	memcpy ((char *)&(addr.sin_addr), 
		phostent->h_addr, 
		phostent->h_length);

	LOG(LOG_TRACE,0, "ConnectServer->connect");
	if (connect(socketfd,(struct sockaddr *)&addr,sizeof(addr)) < 0) 
	{
#if defined(WIN32) || defined(WIN64)
		LOG(LOG_ERROR,GetLastError(), "ConnectServer->connect");
#else
		LOG(LOG_ERROR,errno, "ConnectServer->connect");
#endif
		CloseSocket(socketfd);
		return ERROR_SOCKET_CONNECT;
	}	

	LOG(LOG_TRACE,0, "ConnectServer->return");
	*socketRt = socketfd;
	return ERROR_SUCCESS;
}