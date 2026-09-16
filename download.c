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
#include "win2linux.h"
#include "commom_socket.h"
#include "log.h"
#include "error_code.h"
#include "download.h"
#include "sget_thread.h"
#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bytes between metadata saves per chunk (64 KB) */
#define META_SAVE_INTERVAL (64 * 1024)

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
	char *endFind = END_OF_HTTP_HEADER_LINE;
	char *endStr;

	char *beginStr = strstr(httpHeader,key2find);

	if (beginStr == NULL)
	{
		return -1;
	}

	endStr = strstr(beginStr,endFind);
	if (endStr == NULL)
	{
		printf("not found end\n");
		return -2;
	}
	findLen = endStr - beginStr - strlen(key2find);

	memset(element->strval,0,MAX_HEADER_ELEMENT_LEN);
	if (findLen >= MAX_HEADER_ELEMENT_LEN) {
		strncpy(element->strval,beginStr+strlen(key2find),MAX_HEADER_ELEMENT_LEN-1);
		element->strval[MAX_HEADER_ELEMENT_LEN-1] = 0;	
	} else {
		strncpy(element->strval,beginStr+strlen(key2find),findLen);
	}	

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
		CloseSocket(sckDest);
		printf("send request failed\n");
		return FALSE;
	}

	return TRUE;
}

/**
* Read HTTP response headers until \r\n\r\n
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
		while(!bEndResponse && nIndex < 1024)
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

/**
* Send HTTP request headers with optional Range support.
*
* @param accept_ranges  [out] set to 1 if server returns Accept-Ranges: bytes (may be NULL)
* @param range_start    if range_end > 0, send Range: bytes=start-end header
* @param range_end      0 means no range header
*/
bool SendHttpHeaderEx(SOCKET hSocket,const char *strHostAddr,int nHttpPort,
			   const char *strHttpAddr,const char *strHttpFilename,
			   unsigned long *filesize,char *savedFilename,
			   int *accept_ranges,
			   unsigned long range_start, unsigned long range_end)
{
	char sTemp[MAX_HTTP_HEADER_LINE_LEN] = {0};
	int temLenWrite = sizeof(sTemp) - 1;	
	int httpBeginLen = strlen("http://");

	char *httpPath = strstr(strHttpAddr + httpBeginLen,"/");
	int headerLen = 0;

	/* Line1: GET path HTTP/1.1 */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"GET %s HTTP/1.1\r\n",httpPath);
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Line2: Host */
	memset(sTemp,0,sizeof(sTemp));
	if (nHttpPort == 80) {
		snprintf(sTemp,temLenWrite,"Host: %s\r\n",strHostAddr);
	} else {
		snprintf(sTemp,temLenWrite,"Host: %s:%d\r\n",strHostAddr,nHttpPort);
	}
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Line3: Accept */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n");
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Line4: Referer */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"Referer: %s\r\n",strHttpAddr); 
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Line5: User-Agent */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"%s",DEFAULT_USER_AGENT);
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;
	
	/* Line6: Accept-Language */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"%s",DEFAULT_ACCEPT_LANGUAGE);
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Range header */
	memset(sTemp,0,sizeof(sTemp));
	if (range_end > 0) {
		snprintf(sTemp,temLenWrite,"Range: bytes=%lu-%lu\r\n", range_start, range_end);
	} else {
		snprintf(sTemp,temLenWrite,"Range: bytes=0-\r\n");
	}
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Last line: empty */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"\r\n");
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Read response headers */
	memset(sTemp,0,sizeof(sTemp));
	headerLen=GetHttpHeader(hSocket,sTemp);
		
	if(!headerLen)
	{
		printf("Failed to get HTTP header\n");
		return FALSE;
	}

	if(strstr(sTemp,"404")!=NULL) return FALSE;

	/* Extract Content-Length */
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
				strcpy(savedFilename,strHttpFilename);
			}
		}
	}

	/* Check Accept-Ranges */
	if (accept_ranges != NULL) {
		STHttpHeadElement arElement;
		arElement.type = HEADER_ELEMENT_STRING;
		memset(arElement.strval,0,MAX_HEADER_ELEMENT_LEN);
		if (GetHeaderElement(sTemp, HTTP_HEADER_ACCEPT_RANGES, &arElement) == 0) {
			if (strstr(arElement.strval, "bytes") != NULL) {
				*accept_ranges = 1;
			} else {
				*accept_ranges = 0;
			}
		} else {
			*accept_ranges = 0;
		}
	}

	return TRUE;
}

/* Legacy wrapper for single-thread fallback — compatible with old call sites */
bool SendHttpHeader(SOCKET hSocket,const char *strHostAddr,int nHttpPort,
			   const char *strHttpAddr,const char *strHttpFilename,
			   unsigned long *filesize,char *savedFilename)
{
	return SendHttpHeaderEx(hSocket, strHostAddr, nHttpPort,
		strHttpAddr, strHttpFilename, filesize, savedFilename,
		NULL, 0, 0);
}

/* ========================================================================
 * Single-thread download (original logic, used as fallback)
 * ======================================================================== */

UINT DownLoadProcess(const char *szHostAddr,int nHostPort, 
					const char *strHttpAddr,
					const char *strHttpFilename,const char *savedName,unsigned long m_nFileLength)
{
	SOCKET hSocket;
	DWORD nLen; 
	unsigned long nSumLen=0; 
	char szBuffer[1024] = {0};
	FILE *savefp = NULL;

	if ((savefp = fopen(savedName,"wb")) == NULL)
	{
		printf("Failed to create file\n");
		return 2;
	}

	hSocket=ConnectHttpNonProxy(szHostAddr,nHostPort);

	if(hSocket == INVALID_SOCKET) { fclose(savefp); return 1; }

	SendHttpHeader(hSocket,szHostAddr, nHostPort,strHttpAddr,strHttpFilename,NULL,NULL);	

	while(1)
	{		
		nLen=recv(hSocket,szBuffer,sizeof(szBuffer),0);	

		if (nLen == SOCKET_ERROR){
			printf("Read error!\n");
			fclose(savefp);
			CloseSocket(hSocket);
			return 1;
		}
		if(nLen==0) break;

		fwrite(szBuffer,sizeof(char),nLen,savefp);
		
		nSumLen +=nLen;
		printf("\r %lu/%lu", nSumLen, m_nFileLength);
		fflush(stdout);
		if(m_nFileLength>0 && nSumLen>=m_nFileLength){ 			
			break;
		}	
	}

	fclose(savefp);
	CloseSocket(hSocket);
	return 0;
}

/* ========================================================================
 * Multi-threaded chunk download
 * ======================================================================== */

typedef struct {
	char host[HOST_STR_LEN];
	int port;
	char url[URL_STR_LEN];
	char filename[URL_FILENAME_LEN];
	int fd;                      /* shared file descriptor for pwrite */
	SgetChunk *chunk;            /* pointer to this thread's chunk in metadata */
	SgetMetadata *meta;          /* shared metadata (for save) */
	sget_mutex_t *progress_lock; /* mutex protecting metadata save & progress */
	char *meta_path;             /* path to .sget.meta file */
} ChunkDownloadArgs;

#if defined(WIN32) || defined(WIN64)
static DWORD WINAPI ChunkDownloadWorker(void *arg)
#else
static void *ChunkDownloadWorker(void *arg)
#endif
{
	ChunkDownloadArgs *args = (ChunkDownloadArgs *)arg;
	SgetChunk *chunk = args->chunk;
	SOCKET hSocket;
	char buf[DOWNLOAD_BUF_SIZE];
	DWORD nLen;
	unsigned long write_offset;
	unsigned long bytes_since_save = 0;
	unsigned long actual_start;
	unsigned long actual_end;

	/* Calculate the range we still need */
	actual_start = chunk->start + chunk->downloaded;
	actual_end = chunk->end;

	if (actual_start > actual_end) {
		/* Already complete */
		chunk->status = CHUNK_STATUS_DONE;
		goto done;
	}

	chunk->status = CHUNK_STATUS_RUNNING;

	/* Each thread gets its own connection */
	hSocket = ConnectHttpNonProxy(args->host, args->port);
	if (hSocket == INVALID_SOCKET) {
		printf("Chunk %d: failed to connect\n", chunk->index);
		chunk->status = CHUNK_STATUS_ERROR;
		goto done;
	}

	/* Send request with precise Range header */
	if (!SendHttpHeaderEx(hSocket, args->host, args->port,
		args->url, args->filename, NULL, NULL,
		NULL, actual_start, actual_end))
	{
		printf("Chunk %d: failed to send HTTP header\n", chunk->index);
		CloseSocket(hSocket);
		chunk->status = CHUNK_STATUS_ERROR;
		goto done;
	}

	/* Read body and pwrite to shared file */
	write_offset = actual_start;

	while (!g_sget_should_stop) {
		unsigned long remaining = actual_end - write_offset + 1;
		int to_read = (remaining < DOWNLOAD_BUF_SIZE) ? (int)remaining : DOWNLOAD_BUF_SIZE;

		nLen = recv(hSocket, buf, to_read, 0);

		if (nLen == SOCKET_ERROR) {
			printf("Chunk %d: recv error\n", chunk->index);
			chunk->status = CHUNK_STATUS_ERROR;
			CloseSocket(hSocket);
			goto done;
		}
		if (nLen == 0) break;

		if (sget_pwrite(args->fd, buf, nLen, write_offset) < 0) {
			printf("Chunk %d: write error\n", chunk->index);
			chunk->status = CHUNK_STATUS_ERROR;
			CloseSocket(hSocket);
			goto done;
		}

		write_offset += nLen;
		chunk->downloaded = write_offset - chunk->start;
		bytes_since_save += nLen;

		/* Periodically save metadata */
		if (bytes_since_save >= META_SAVE_INTERVAL) {
			sget_mutex_lock(args->progress_lock);
			metadata_save(args->meta, args->meta_path);
			sget_mutex_unlock(args->progress_lock);
			bytes_since_save = 0;
		}

		if (write_offset > actual_end) break;
	}

	CloseSocket(hSocket);

	if (write_offset > actual_end) {
		chunk->status = CHUNK_STATUS_DONE;
	} else if (g_sget_should_stop) {
		/* Interrupted — leave status as RUNNING so resume knows */
		chunk->status = CHUNK_STATUS_PENDING;
	} else {
		chunk->status = CHUNK_STATUS_ERROR;
	}

done:
	/* Final save of progress for this chunk */
	sget_mutex_lock(args->progress_lock);
	metadata_save(args->meta, args->meta_path);
	sget_mutex_unlock(args->progress_lock);

#if defined(WIN32) || defined(WIN64)
	return (chunk->status == CHUNK_STATUS_DONE) ? 0 : 1;
#else
	return (void *)(long)(chunk->status == CHUNK_STATUS_DONE ? 0 : 1);
#endif
}

/**
* Multi-threaded download orchestrator.
* Returns 0 on success, non-zero on failure.
*/
static UINT HttpDownLoadMT(const char *strHostAddr, int nHttpPort,
	const char *strHttpAddr, const char *strHttpFilename,
	const char *savePath, unsigned long filesize,
	int accept_ranges, int thread_count)
{
	SgetMetadata *meta = NULL;
	char *meta_path = NULL;
	sget_mutex_t progress_lock;
	sget_thread_t *threads = NULL;
	ChunkDownloadArgs *args = NULL;
	int fd = -1;
	int i;
	int all_done = 1;

	/* If server doesn't support ranges or file is small, fallback */
	if (!accept_ranges || filesize == 0 || thread_count <= 1) {
		return DownLoadProcess(strHostAddr, nHttpPort, strHttpAddr,
			strHttpFilename, savePath, filesize);
	}

	/* Build metadata path */
	meta_path = metadata_build_path(savePath);
	if (meta_path == NULL) return 1;

	/* Try to resume from existing metadata */
	meta = metadata_load(meta_path);
	if (meta != NULL) {
		/* Validate URL and size match */
		if (strcmp(meta->url, strHttpAddr) != 0 || meta->total_size != filesize) {
			printf("Metadata mismatch, starting fresh download\n");
			metadata_free(meta);
			meta = NULL;
		} else {
			printf("Resuming download from metadata (%d chunks)\n", meta->thread_count);
			thread_count = meta->thread_count;
		}
	}

	if (meta == NULL) {
		meta = metadata_create(strHttpAddr, savePath, filesize, thread_count);
		if (meta == NULL) {
			printf("Failed to create download metadata\n");
			free(meta_path);
			return 1;
		}
	}

	/* Open (or create) the target file for pwrite */
	fd = sget_open_rw(savePath);
	if (fd < 0) {
		printf("Failed to open file for writing: %s\n", savePath);
		metadata_free(meta);
		free(meta_path);
		return 1;
	}

	/* Save initial metadata */
	metadata_save(meta, meta_path);

	/* Init synchronization */
	sget_mutex_init(&progress_lock);

	/* Allocate thread handles and argument structs */
	threads = (sget_thread_t *)calloc(thread_count, sizeof(sget_thread_t));
	args = (ChunkDownloadArgs *)calloc(thread_count, sizeof(ChunkDownloadArgs));
	if (threads == NULL || args == NULL) {
		printf("Memory allocation failed\n");
		sget_close(fd);
		sget_mutex_destroy(&progress_lock);
		metadata_free(meta);
		free(meta_path);
		free(threads);
		free(args);
		return 1;
	}

	printf("Starting multi-threaded download: %d threads, %lu bytes\n",
		thread_count, filesize);

	/* Launch worker threads */
	for (i = 0; i < thread_count; i++) {
		/* Skip already completed chunks (resume case) */
		if (meta->chunks[i].status == CHUNK_STATUS_DONE) {
			printf("  Chunk %d: already complete\n", i);
			threads[i] = (sget_thread_t)0;
			continue;
		}

		strncpy(args[i].host, strHostAddr, HOST_STR_LEN - 1);
		args[i].host[HOST_STR_LEN - 1] = '\0';
		args[i].port = nHttpPort;
		strncpy(args[i].url, strHttpAddr, URL_STR_LEN - 1);
		args[i].url[URL_STR_LEN - 1] = '\0';
		strncpy(args[i].filename, strHttpFilename, URL_FILENAME_LEN - 1);
		args[i].filename[URL_FILENAME_LEN - 1] = '\0';
		args[i].fd = fd;
		args[i].chunk = &meta->chunks[i];
		args[i].meta = meta;
		args[i].progress_lock = &progress_lock;
		args[i].meta_path = meta_path;

		if (sget_thread_create(&threads[i], ChunkDownloadWorker, &args[i]) != 0) {
			printf("Failed to create thread for chunk %d\n", i);
			meta->chunks[i].status = CHUNK_STATUS_ERROR;
		}
	}

	/* Wait for all threads to finish */
	for (i = 0; i < thread_count; i++) {
		if (meta->chunks[i].status == CHUNK_STATUS_DONE && threads[i] == (sget_thread_t)0) {
			continue; /* was already done before we started */
		}
		if (threads[i] != (sget_thread_t)0) {
			sget_thread_join(threads[i]);
		}
	}

	/* Print final progress */
	{
		unsigned long total_downloaded = 0;
		for (i = 0; i < thread_count; i++) {
			total_downloaded += meta->chunks[i].downloaded;
		}
		printf("\rDownloaded: %lu/%lu bytes\n", total_downloaded, filesize);
	}

	/* Check if all chunks completed */
	all_done = 1;
	for (i = 0; i < thread_count; i++) {
		if (meta->chunks[i].status != CHUNK_STATUS_DONE) {
			all_done = 0;
			break;
		}
	}

	/* Cleanup */
	sget_close(fd);
	sget_mutex_destroy(&progress_lock);

	if (all_done) {
		metadata_delete(meta_path);
		printf("Download complete: %s\n", savePath);
	} else {
		/* Save final state for resume */
		metadata_save(meta, meta_path);
		if (g_sget_should_stop) {
			printf("Download interrupted. Resume by running the same command.\n");
		} else {
			printf("Download incomplete. Some chunks failed. Resume by running the same command.\n");
		}
	}

	metadata_free(meta);
	free(meta_path);
	free(threads);
	free(args);

	return all_done ? 0 : 1;
}

/* ========================================================================
 * Top-level HTTP download — resolves filename, then dispatches
 * ======================================================================== */

static bool HttpDownLoadEx(
	const char *strHostAddr,
	int nHttpPort,
	const char *strHttpAddr,
	const char *strHttpFilename,
	const char *saveFolder,
	int thread_count)
{
	SOCKET hSocket;
	unsigned long filesize = 0;
	int accept_ranges = 0;
	char saveFilename[MAX_HEADER_ELEMENT_LEN] = {0};

	memset(saveFilename,0,MAX_HEADER_ELEMENT_LEN);
	hSocket=ConnectHttpNonProxy(strHostAddr,nHttpPort);
	if(hSocket == INVALID_SOCKET){
		printf("can't connect to the server\n");
		return FALSE;
	}

	/* First request: get file size, filename, and check Accept-Ranges */
	SendHttpHeaderEx(hSocket,strHostAddr,nHttpPort,strHttpAddr,strHttpFilename,
		&filesize,saveFilename,&accept_ranges,0,0);
	CloseSocket(hSocket);	

	if (strlen(saveFilename) == 0)
	{
		printf("get the saved name failed.\n");
		return FALSE;
	}

	/* Build full save path */
	if (saveFolder != NULL)
	{
		int nameLen = strlen(saveFilename);
		int folderLen = strlen(saveFolder);
		char *endChar;
		bool endFolder = false;

		if (nameLen + folderLen + 2 > MAX_HEADER_ELEMENT_LEN)
		{
			printf("the save folder path is too long.\n");
			return FALSE;
		}

		endChar = strrchr(saveFolder,"/");
		if (endChar != NULL && strlen(endChar) == 1)
		{
			char tmp[MAX_HEADER_ELEMENT_LEN] = {0};
			snprintf(tmp, MAX_HEADER_ELEMENT_LEN-1, "%s%s", saveFolder, saveFilename);
			strncpy(saveFilename, tmp, MAX_HEADER_ELEMENT_LEN-1);
			endFolder = true;
		}
		else
		{
			endChar = strrchr(saveFolder,"\\");
			if (endChar != NULL && strlen(endChar) == 1)
			{
				char tmp[MAX_HEADER_ELEMENT_LEN] = {0};
				snprintf(tmp, MAX_HEADER_ELEMENT_LEN-1, "%s%s", saveFolder, saveFilename);
				strncpy(saveFilename, tmp, MAX_HEADER_ELEMENT_LEN-1);
				endFolder = true;
			}
		}
		if (endFolder == false)
		{
			char tmp[MAX_HEADER_ELEMENT_LEN] = {0};
			snprintf(tmp, MAX_HEADER_ELEMENT_LEN-1, "%s/%s", saveFolder, saveFilename);
			strncpy(saveFilename, tmp, MAX_HEADER_ELEMENT_LEN-1);
		}
	}

	printf("File: %s (%lu bytes), Accept-Ranges: %s, Threads: %d\n",
		saveFilename, filesize,
		accept_ranges ? "yes" : "no",
		(accept_ranges && filesize > 0 && thread_count > 1) ? thread_count : 1);

	/* Dispatch to multi-threaded or single-threaded download */
	if (HttpDownLoadMT(strHostAddr, nHttpPort, strHttpAddr, strHttpFilename,
		saveFilename, filesize, accept_ranges, thread_count))
		return FALSE;

	return TRUE;
}

/* Legacy single-thread entry (keeps old API) */
bool HttpDownLoad(
	const char *strHostAddr,
	int nHttpPort,
	const char *strHttpAddr,
	const char *strHttpFilename,
	const char *saveFolder)
{
	return HttpDownLoadEx(strHostAddr, nHttpPort, strHttpAddr,
		strHttpFilename, saveFolder, 1);
}

/**
* URL parser (unchanged)
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

/* ========================================================================
 * Public entry points
 * ======================================================================== */

bool WHY_Download(const char*strUrl, const char *saveFolder)
{
	return WHY_DownloadMT(strUrl, saveFolder, DEFAULT_THREAD_COUNT);
}

bool WHY_DownloadMT(const char *strUrl, const char *saveFolder, int thread_count)
{
	char  strHostAddr[HOST_STR_LEN] = {0};
	char  strHttpAddr[URL_STR_LEN] = {0};
	char  strHttpFilename[URL_FILENAME_LEN] = {0};
	char  strHttpPort[HTTP_PORT_LEN] = {0};
	bool downrv = true;	
#if defined(WIN32) || defined(WIN64)
	WSADATA WsaData;
#endif

	if (thread_count <= 0) thread_count = DEFAULT_THREAD_COUNT;

	/* Install signal handler for graceful interrupt */
	sget_install_signal_handler();

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
		if(!HttpDownLoadEx(strHostAddr,nHostPort,strUrl,strHttpFilename,
			saveFolder, thread_count)) {			
			downrv = false;
		}
	}
	else
	{
		downrv = false;
	}
	
	return downrv;
}
