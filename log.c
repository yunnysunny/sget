#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>

#if defined(WIN32) || defined(WIN64)
#include <io.h>
#include <stdlib.h> 
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <pthread.h>
#endif

#include "log.h"

unsigned int log_level     = LOG_ERROR;//Global

void LogMessage(char* sModule, int nLogLevel, char *sFile,int nLine,unsigned int unErrCode, char *sMessage)
{
#if defined(WIN32) || defined(WIN64)
	DWORD nThreadID;
#else
	unsigned int nThreadID;
#endif
	struct tm *newtime;
	time_t aclock;
	
	/*Get current time*/
  	time( &aclock );                 
	newtime = localtime( &aclock ); 
	
	/*Get current threadid*/
#if defined(WIN32) || defined(WIN64)
	nThreadID = GetCurrentProcessId();
	nThreadID = (nThreadID << 16) + GetCurrentThreadId();
#else
	nThreadID = getpid();
	nThreadID = (nThreadID << 16) + pthread_self();
#endif

	switch(nLogLevel)
	{
	case LOG_FAULT:			
			printf("\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%ud><Fault>[0x%08x]%s(%s:%d)",
				newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,
				newtime->tm_min,newtime->tm_sec,sModule,nThreadID,unErrCode, sMessage, sFile,nLine);
		break;
	case LOG_ERROR:
			printf("\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%ud><Error>[0x%08x]%s(%s:%d)",
				newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,
				newtime->tm_min,newtime->tm_sec,sModule,nThreadID,unErrCode, sMessage, sFile,nLine);
		break;
	case LOG_WARN:
		printf("\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%ud><Warning>%s<%d>(%s:%d)",
			newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,
			newtime->tm_min,newtime->tm_sec,sModule,nThreadID, sMessage, unErrCode, sFile,nLine);
		break;
	case LOG_INFO:
		printf("\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%ud><Info>%s(%d)(%s:%d)",
			newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,
			newtime->tm_min,newtime->tm_sec,sModule,nThreadID, sMessage,  unErrCode, sFile,nLine);
		break;
	case LOG_DEBUG:
		printf("\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%ud><Info>%s(%d)(%s:%d)",
			newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,
			newtime->tm_min,newtime->tm_sec,sModule,nThreadID, sMessage,  unErrCode, sFile,nLine);
		break;
	case LOG_TRACE:
		printf("\n<%4d-%02d-%02d %02d:%02d:%02d><%s><%ud><Trace>%s(%s:%d)",
			newtime->tm_year+1900,newtime->tm_mon+1,newtime->tm_mday,newtime->tm_hour,
			newtime->tm_min,newtime->tm_sec,sModule,nThreadID, sMessage, sFile,nLine);
		break;
	default:
		break;
	} 
}

int errorReturn(int errorCode,char *tag,char *msg)
{
	SIM_ERROR_TAG(tag,errorCode,msg);
	return errorCode;
}
