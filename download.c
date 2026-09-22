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
#include "common_socket.h"
#include "log.h"
#include "error_code.h"
#include "download.h"
#include "sget_thread.h"
#include "metadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

/* Bytes between metadata saves per chunk (64 KB) */
#define META_SAVE_INTERVAL (64 * 1024)

/* Maximum number of retries per chunk on transient errors */
#define CHUNK_MAX_RETRIES 3

/* Fixed-width ASCII progress bar: portable across Windows and POSIX terminals. */
#define PROGRESS_BAR_WIDTH 40

static void print_progress_bar(uint64_t downloaded, uint64_t total)
{
	int i;
	int filled;
	double ratio;

	if (total == 0) {
		int marker = (int)((downloaded / DOWNLOAD_BUF_SIZE) % PROGRESS_BAR_WIDTH);
		printf("\r[");
		for (i = 0; i < PROGRESS_BAR_WIDTH; i++)
			putchar(i == marker ? '>' : ' ');
		printf("]   --.-%% %" PRIu64 " bytes", downloaded);
		fflush(stdout);
		return;
	}

	if (downloaded > total) downloaded = total;
	ratio = (double)downloaded / (double)total;
	filled = (int)(ratio * PROGRESS_BAR_WIDTH);
	if (filled > PROGRESS_BAR_WIDTH) filled = PROGRESS_BAR_WIDTH;

	printf("\r[");
	for (i = 0; i < PROGRESS_BAR_WIDTH; i++) {
		if (i < filled)
			putchar('=');
		else if (i == filled && filled < PROGRESS_BAR_WIDTH)
			putchar('>');
		else
			putchar(' ');
	}
	printf("] %6.2f%% %" PRIu64 "/%" PRIu64 " bytes",
		ratio * 100.0, downloaded, total);
	fflush(stdout);
}

/* Last socket-level failure, so callers can inspect a failed download. */
static unsigned int g_sget_last_error = 0;

static unsigned int os_last_error(void)
{
#if defined(WIN32) || defined(WIN64)
	return (unsigned int)WSAGetLastError();
#else
	return (unsigned int)errno;
#endif
}

unsigned int getErrorCode()
{
	return g_sget_last_error;
}

static SOCKET ConnectHttpNonProxy(const char *strHostAddr,int nPort)
{
	SOCKET hSocket;
	unsigned int rv = GetConnect(&hSocket,strHostAddr,nPort);
	if(hSocket == INVALID_SOCKET)
	{
		g_sget_last_error = rv;
		return INVALID_SOCKET;
	}

	return hSocket;
}

static BOOL SocketSend(SOCKET sckDest,const char *szHttp)
{
	int sent = 0;
	int iLen = (int)strlen(szHttp);

	while (sent < iLen)
	{
		int rv = send(sckDest, szHttp + sent, iLen - sent, 0);
		if (rv == SOCKET_ERROR || rv == 0)
		{
			printf("send request failed\n");
			if (rv == SOCKET_ERROR) g_sget_last_error = os_last_error();
			return FALSE;
		}
		sent += rv;
	}

	return TRUE;
}

/**
* Read HTTP response headers until \r\n\r\n.
* The buffer size includes space for the terminating NUL.
*/
static DWORD GetHttpHeader(SOCKET sckDest,char *str,size_t str_size)
{
	size_t nIndex = 0;

	if (str == NULL || str_size == 0) return 0;

	while (nIndex + 1 < str_size)
	{
		char c;
		int rv = recv(sckDest, &c, 1, 0);
		if (rv == 0 || rv == SOCKET_ERROR)
		{
			if (rv == SOCKET_ERROR) g_sget_last_error = os_last_error();
			str[nIndex] = '\0';
			return 0;
		}
		str[nIndex++] = c;
		if (nIndex >= 4 &&
			str[nIndex - 4] == '\r' && str[nIndex - 3] == '\n' &&
			str[nIndex - 2] == '\r' && str[nIndex - 1] == '\n')
		{
			str[nIndex] = '\0';
			return (DWORD)nIndex;
		}
	}

	str[nIndex] = '\0';
	return 0;
}

static int ascii_header_name_equal(const char *left, const char *right, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		unsigned char a = (unsigned char)left[i];
		unsigned char b = (unsigned char)right[i];
		if (a >= 'A' && a <= 'Z') a = (unsigned char)(a + ('a' - 'A'));
		if (b >= 'A' && b <= 'Z') b = (unsigned char)(b + ('a' - 'A'));
		if (a != b) return 0;
	}
	return 1;
}

#define HTTP_SCHEME "http://"
#define HTTP_SCHEME_LEN 7
#define HTTPS_SCHEME "https://"
#define HTTPS_SCHEME_LEN 8

/* Schemes are case-insensitive per RFC 3986; tell the two apart for diagnostics. */
static int has_scheme(const char *url, const char *scheme, size_t scheme_len)
{
	return url != NULL && strlen(url) >= scheme_len &&
		ascii_header_name_equal(url, scheme, scheme_len);
}

static int get_header_value(const char *header, const char *name,
	const char **value, size_t *value_len)
{
	const char *line = strstr(header, "\r\n");
	size_t name_len = strlen(name);

	if (line == NULL) return 0;
	line += 2;
	while (line[0] != '\r' || line[1] != '\n') {
		const char *line_end = strstr(line, "\r\n");
		const char *begin;
		const char *end;
		if (line_end == NULL) return 0;
		if ((size_t)(line_end - line) > name_len && line[name_len] == ':' &&
			ascii_header_name_equal(line, name, name_len)) {
			begin = line + name_len + 1;
			while (begin < line_end && (*begin == ' ' || *begin == '\t')) begin++;
			end = line_end;
			while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) end--;
			*value = begin;
			*value_len = (size_t)(end - begin);
			return 1;
		}
		line = line_end + 2;
	}
	return 0;
}

static int parse_http_status_line(const char *header, int *status)
{
	const char *line_end = strstr(header, "\r\n");
	const char *p;
	int code;

	if (line_end == NULL || line_end - header < 13) return 0;
	if (memcmp(header, "HTTP/1.", 7) != 0 ||
		(header[7] != '0' && header[7] != '1') || header[8] != ' ' ||
		header[9] < '0' || header[9] > '9' ||
		header[10] < '0' || header[10] > '9' ||
		header[11] < '0' || header[11] > '9' || header[12] != ' ') {
		return 0;
	}
	for (p = header + 13; p < line_end; p++) {
		unsigned char c = (unsigned char)*p;
		if (c != '\t' && c < 0x20) return 0;
	}
	code = (header[9] - '0') * 100 + (header[10] - '0') * 10 +
		(header[11] - '0');
	*status = code;
	return 1;
}

static int parse_decimal_u64(const char **cursor, const char *end, uint64_t *number)
{
	uint64_t value = 0;
	const char *p = *cursor;
	if (p == end || *p < '0' || *p > '9') return 0;
	while (p < end && *p >= '0' && *p <= '9') {
		unsigned int digit = (unsigned int)(*p - '0');
		if (value > (UINT64_MAX - digit) / 10) return 0;
		value = value * 10 + digit;
		p++;
	}
	*number = value;
	*cursor = p;
	return 1;
}

static int parse_uint64_header(const char *header, const char *name, uint64_t *number)
{
	const char *value;
	const char *end;
	size_t value_len;
	if (!get_header_value(header, name, &value, &value_len)) return 0;
	end = value + value_len;
	return parse_decimal_u64(&value, end, number) && value == end;
}

static int parse_content_range(const char *header, uint64_t *start,
	uint64_t *end, uint64_t *total)
{
	const char *value;
	const char *limit;
	size_t value_len;

	if (!get_header_value(header, "Content-Range", &value, &value_len) ||
		value_len < 10 || memcmp(value, "bytes ", 6) != 0) return 0;
	value += 6;
	limit = value + value_len - 6;
	if (!parse_decimal_u64(&value, limit, start) || value == limit || *value++ != '-') return 0;
	if (!parse_decimal_u64(&value, limit, end) || value == limit || *value++ != '/') return 0;
	if (!parse_decimal_u64(&value, limit, total) || value != limit) return 0;
	return *start <= *end && *end < *total;
}

static const char *getDownFilename(const char *str)
{
	const char *filenameBegin = strstr(str, "filename=");
	return filenameBegin != NULL ? filenameBegin + strlen("filename=") : NULL;
}

/* Keep response-provided names inside the requested directory on every platform. */
static int copy_safe_filename(const char *value, char *output, size_t output_size)
{
	const char *start, *end, *p;
	size_t length;
	if (value == NULL || output_size == 0) return 0;
	while (*value == ' ' || *value == '\t') value++;
	if (*value == '"') {
		start = ++value;
		end = strchr(start, '"');
		if (end == NULL) return 0;
	} else {
		start = value;
		end = strchr(start, ';');
		if (end == NULL) end = start + strlen(start);
		while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
	}
	/* Extract the final component, even for Windows-style server paths. */
	for (p = start; p < end; p++) {
		if (*p == '/' || *p == '\\') start = p + 1;
	}
	length = (size_t)(end - start);
	if (length == 0 || length >= output_size ||
		(length == 1 && start[0] == '.') ||
		(length == 2 && start[0] == '.' && start[1] == '.') ||
		start[length - 1] == '.' || start[length - 1] == ' ') return 0;
	for (p = start; p < end; p++) {
		unsigned char c = (unsigned char)*p;
		if (c < 0x20 || c == 0x7f || strchr("<>:\"/\\|?*", c) != NULL) return 0;
	}
	/* Device names remain special on Windows even when an extension is present. */
	{
		size_t base_len = 0;
		while (base_len < length && start[base_len] != '.') base_len++;
		if ((base_len == 3 &&
			(ascii_header_name_equal(start, "con", 3) ||
			 ascii_header_name_equal(start, "prn", 3) ||
			 ascii_header_name_equal(start, "aux", 3) ||
			 ascii_header_name_equal(start, "nul", 3))) ||
			(base_len == 4 && start[3] >= '1' && start[3] <= '9' &&
			 (ascii_header_name_equal(start, "com", 3) ||
			  ascii_header_name_equal(start, "lpt", 3)))) return 0;
	}
	memcpy(output, start, length);
	output[length] = '\0';
	return 1;
}

/**
* Send HTTP request headers with optional Range support.
*
* @param range_request non-zero for an explicit chunk Range request
* @param range_start   requested chunk start
* @param range_end     requested chunk end (inclusive)
*/
static bool SendHttpHeaderEx(SOCKET hSocket,const char *strHostAddr,int nHttpPort,
			   const char *strHttpAddr,const char *strHttpFilename,
			   uint64_t *filesize,char *savedFilename,
			   int *accept_ranges, int range_request,
			   uint64_t range_start, uint64_t range_end, uint64_t expected_total,
			   char *validator)
{
	char sTemp[MAX_HTTP_HEADER_LINE_LEN] = {0};
	int temLenWrite = sizeof(sTemp) - 1;
	int httpBeginLen = HTTP_SCHEME_LEN;
	int status;
	uint64_t content_length = 0;
	uint64_t response_start = 0;
	uint64_t response_end = 0;
	uint64_t response_total = 0;
	int has_content_length;

	char requestPath[URL_STR_LEN];
	const char *httpPath;
	const char *fragment;
	size_t pathLength;
	DWORD headerLen = 0;
	int requestLen;

	if (!has_scheme(strHttpAddr, HTTP_SCHEME, HTTP_SCHEME_LEN)) return FALSE;
	httpPath = strpbrk(strHttpAddr + httpBeginLen, "/?#");
	if (httpPath == NULL || *httpPath == '#') {
		strcpy(requestPath, "/");
	} else {
		fragment = strchr(httpPath, '#');
		pathLength = fragment != NULL ? (size_t)(fragment - httpPath) : strlen(httpPath);
		if (pathLength + (*httpPath == '?' ? 1 : 0) >= sizeof(requestPath)) return FALSE;
		if (*httpPath == '?') {
			requestPath[0] = '/';
			memcpy(requestPath + 1, httpPath, pathLength);
			pathLength++;
		} else {
			memcpy(requestPath, httpPath, pathLength);
		}
		requestPath[pathLength] = '\0';
	}
	/* Line1: GET path HTTP/1.1 */
	memset(sTemp,0,sizeof(sTemp));
	requestLen = snprintf(sTemp,temLenWrite,"GET %s HTTP/1.1\r\n",requestPath);
	if (requestLen < 0 || requestLen >= temLenWrite) return FALSE;
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
	if (range_request) {
		snprintf(sTemp,temLenWrite,"Range: bytes=%" PRIu64 "-%" PRIu64 "\r\n",
			range_start, range_end);
	} else {
		snprintf(sTemp,temLenWrite,"Range: bytes=0-\r\n");
	}
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* If-Range header for validated resume */
	if (range_request && validator != NULL && validator[0] != '\0') {
		memset(sTemp,0,sizeof(sTemp));
		snprintf(sTemp,temLenWrite,"If-Range: %s\r\n", validator);
		sTemp[temLenWrite] = '\0';
		if(!SocketSend(hSocket,sTemp)) return FALSE;
	}

	/* Close after this response so length-less single-thread transfers can finish. */
	if (!SocketSend(hSocket, "Accept-Encoding: identity\r\n")) return FALSE;
	if (!SocketSend(hSocket, "Connection: close\r\n")) return FALSE;

	/* Last line: empty */
	memset(sTemp,0,sizeof(sTemp));
	snprintf(sTemp,temLenWrite,"\r\n");
	sTemp[temLenWrite] = '\0';
	if(!SocketSend(hSocket,sTemp)) return FALSE;

	/* Read response headers */
	memset(sTemp,0,sizeof(sTemp));
	headerLen=GetHttpHeader(hSocket,sTemp,sizeof(sTemp));
		
	if(!headerLen)
	{
		printf("Failed to get HTTP header\n");
		return FALSE;
	}

	if (!parse_http_status_line(sTemp, &status)) {
		printf("Invalid HTTP status line\n");
		return FALSE;
	}

	/* Check HTTP error status before encoding check */
	if (!range_request && status >= 300 && status < 400) {
		printf("HTTP %d redirect is not supported; use the final URL.\n", status);
		return FALSE;
	}
	if (!range_request && status != 200 && status != 206) {
		printf("HTTP error: %d\n", status);
		return FALSE;
	}

	{
		const char *value;
		size_t value_len;
		if ((get_header_value(sTemp, "Transfer-Encoding", &value, &value_len) &&
			!(value_len == 8 && ascii_header_name_equal(value, "identity", 8))) ||
			(get_header_value(sTemp, "Content-Encoding", &value, &value_len) &&
			!(value_len == 8 && ascii_header_name_equal(value, "identity", 8)))) {
			printf("Unsupported HTTP transfer or content encoding\n");
			return FALSE;
		}
	}

	has_content_length = parse_uint64_header(sTemp, "Content-Length", &content_length);
	if (range_request) {
		if (status != 206 ||
			!parse_content_range(sTemp, &response_start, &response_end, &response_total) ||
			response_start != range_start || response_end != range_end ||
			response_total != expected_total ||
			(has_content_length && content_length != range_end - range_start + 1)) {
			printf("Invalid HTTP range response\n");
			return FALSE;
		}
	} else if (status == 206) {
		if (!parse_content_range(sTemp, &response_start, &response_end, &response_total) ||
			response_start != 0 ||
			(has_content_length && content_length != response_end + 1)) {
			printf("Invalid HTTP probe response\n");
			return FALSE;
		}
	} else if (status != 200) {
		printf("Unexpected HTTP status: %d\n", status);
		return FALSE;
	}

	/* Extract file size. A 206 probe reports the full size in Content-Range. */
	if (filesize != NULL)
	{
		*filesize = (status == 206) ? response_total :
			(has_content_length ? content_length : 0);

		if (savedFilename != NULL) {
			const char *value;
			size_t value_len;
			if (get_header_value(sTemp, "Content-Disposition", &value, &value_len) &&
				value_len < MAX_HEADER_ELEMENT_LEN) {
				char disposition[MAX_HEADER_ELEMENT_LEN];
				const char *filename;
				memcpy(disposition, value, value_len);
				disposition[value_len] = '\0';
				filename = getDownFilename(disposition);
				if (filename != NULL)
					copy_safe_filename(filename, savedFilename, MAX_HEADER_ELEMENT_LEN);
			}
			if (savedFilename[0] == '\0') {
				const char *fallback = (strHttpFilename != NULL && strHttpFilename[0] != '\0')
					? strHttpFilename : "index.html";
				if (!copy_safe_filename(fallback, savedFilename, MAX_HEADER_ELEMENT_LEN))
					return FALSE;
			}
		}
	}

	/* Check Accept-Ranges */
	if (accept_ranges != NULL) {
		const char *value;
		size_t value_len;
		*accept_ranges = (status == 206) ||
			(get_header_value(sTemp, "Accept-Ranges", &value, &value_len) &&
			value_len == 5 && ascii_header_name_equal(value, "bytes", 5));
	}

	/* Extract strong ETag for probe requests */
	if (!range_request && validator != NULL) {
		const char *value;
		size_t value_len;
		validator[0] = '\0';
		if (get_header_value(sTemp, "ETag", &value, &value_len) &&
			value_len > 0 && value_len < 256 &&
			!(value_len >= 2 && value[0] == 'W' && value[1] == '/')) {
			memcpy(validator, value, value_len);
			validator[value_len] = '\0';
		}
	}

	return TRUE;
}

/* Legacy wrapper for single-thread fallback — compatible with old call sites */
static bool SendHttpHeader(SOCKET hSocket,const char *strHostAddr,int nHttpPort,
			   const char *strHttpAddr,const char *strHttpFilename,
			   uint64_t *filesize,char *savedFilename)
{
	return SendHttpHeaderEx(hSocket, strHostAddr, nHttpPort,
		strHttpAddr, strHttpFilename, filesize, savedFilename,
		NULL, 0, 0, 0, 0, NULL);
}

/* ========================================================================
 * Single-thread download (original logic, used as fallback)
 * ======================================================================== */

static UINT DownLoadProcess(const char *szHostAddr,int nHostPort,
					const char *strHttpAddr,
					const char *strHttpFilename,const char *savedName,uint64_t m_nFileLength)
{
	SOCKET hSocket;
	int nLen = 0;
	int failed = 0;
	uint64_t nSumLen = 0;
	uint64_t response_size = 0;
	char szBuffer[DOWNLOAD_BUF_SIZE];
	FILE *savefp;

	hSocket = ConnectHttpNonProxy(szHostAddr, nHostPort);
	if (hSocket == INVALID_SOCKET) return 1;

	if (!SendHttpHeader(hSocket, szHostAddr, nHostPort, strHttpAddr,
		strHttpFilename, &response_size, NULL) ||
		(m_nFileLength != 0 && response_size != 0 && response_size != m_nFileLength)) {
		CloseSocket(hSocket);
		return 1;
	}
	if ((savefp = fopen(savedName, "wb")) == NULL) {
		printf("Failed to create file\n");
		CloseSocket(hSocket);
		return 2;
	}

	while (!g_sget_should_stop) {
		int to_read = sizeof(szBuffer);
		if (response_size > 0 && response_size - nSumLen < (uint64_t)to_read)
			to_read = (int)(response_size - nSumLen);
		if (to_read == 0) break;
		nLen = recv(hSocket, szBuffer, to_read, 0);
		if (nLen == SOCKET_ERROR) {
			g_sget_last_error = os_last_error();
			printf("Read error!\n");
			failed = 1;
			break;
		}
		if (nLen == 0) break;
		if (fwrite(szBuffer, 1, (size_t)nLen, savefp) != (size_t)nLen) {
			g_sget_last_error = (unsigned int)errno;
			printf("Write error!\n");
			failed = 1;
			break;
		}
		nSumLen += (uint64_t)nLen;
		print_progress_bar(nSumLen, response_size);
	}
	if (fclose(savefp) != 0 || failed || g_sget_should_stop ||
		(response_size != 0 && nSumLen != response_size)) {
		CloseSocket(hSocket);
		return 1;
	}
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

static void chunk_set_status(ChunkDownloadArgs *args, int status)
{
	sget_mutex_lock(args->progress_lock);
	args->chunk->status = status;
	sget_mutex_unlock(args->progress_lock);
}

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
	int nLen;
	uint64_t write_offset;
	uint64_t bytes_since_save = 0;
	uint64_t actual_start;
	uint64_t actual_end;
	int retry;
	int attempt_succeeded = 0;

	/* Calculate the range we still need */
	actual_start = chunk->start + chunk->downloaded;
	actual_end = chunk->end;

	if (actual_start > actual_end) {
		/* Already complete */
		chunk_set_status(args, CHUNK_STATUS_DONE);
		goto done;
	}

	chunk_set_status(args, CHUNK_STATUS_RUNNING);

	for (retry = 0; retry <= CHUNK_MAX_RETRIES && !g_sget_should_stop; retry++) {
		int recv_error = 0;

		if (retry > 0) {
			printf("Chunk %d: retry %d/%d\n", chunk->index, retry, CHUNK_MAX_RETRIES);
#if defined(WIN32) || defined(WIN64)
			Sleep(retry * 1000);
#else
			/* usleep is XSI and absent from POSIX.1-2008; nanosleep is the portable option. */
			{
				struct timespec ts;
				ts.tv_sec = retry;
				ts.tv_nsec = 0;
				nanosleep(&ts, NULL);
			}
#endif
			/* Recalculate actual_start from current progress */
			actual_start = chunk->start + chunk->downloaded;
			if (actual_start > actual_end) { attempt_succeeded = 1; break; }
		}

		/* Each thread gets its own connection */
		hSocket = ConnectHttpNonProxy(args->host, args->port);
		if (hSocket == INVALID_SOCKET) {
			printf("Chunk %d: failed to connect\n", chunk->index);
			continue; /* retry */
		}

		/* Send request with precise Range header */
		if (!SendHttpHeaderEx(hSocket, args->host, args->port,
			args->url, args->filename, NULL, NULL,
			NULL, 1, actual_start, actual_end, args->meta->total_size,
			args->meta->validator))
		{
			printf("Chunk %d: failed to send HTTP header\n", chunk->index);
			CloseSocket(hSocket);
			continue; /* retry */
		}

		/* Read body and pwrite to shared file */
		write_offset = actual_start;
		bytes_since_save = 0;

		while (!g_sget_should_stop) {
			uint64_t remaining = actual_end - write_offset + 1;
			int to_read = (remaining < DOWNLOAD_BUF_SIZE) ? (int)remaining : DOWNLOAD_BUF_SIZE;

			nLen = recv(hSocket, buf, to_read, 0);

			if (nLen == SOCKET_ERROR) {
				g_sget_last_error = os_last_error();
				printf("Chunk %d: recv error\n", chunk->index);
				recv_error = 1;
				break;
			}
			if (nLen == 0) break;

			if (sget_pwrite(args->fd, buf, (unsigned int)nLen, write_offset) != nLen) {
				g_sget_last_error = (unsigned int)errno;
				printf("Chunk %d: write error\n", chunk->index);
				recv_error = 1;
				break;
			}

			write_offset += nLen;
			sget_mutex_lock(args->progress_lock);
			chunk->downloaded = write_offset - chunk->start;
			sget_mutex_unlock(args->progress_lock);
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

		if (recv_error) continue; /* retry */

		if (write_offset > actual_end) { attempt_succeeded = 1; break; }
		if (g_sget_should_stop) break;
		/* Short read but no error — also retry */
	}

	if (attempt_succeeded) {
		chunk_set_status(args, CHUNK_STATUS_DONE);
	} else if (g_sget_should_stop) {
		chunk_set_status(args, CHUNK_STATUS_PENDING);
	} else {
		chunk_set_status(args, CHUNK_STATUS_ERROR);
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
	const char *savePath, uint64_t filesize,
	int accept_ranges, int thread_count, const char *validator)
{
	SgetMetadata *meta = NULL;
	char *meta_path = NULL;
	sget_mutex_t progress_lock;
	sget_thread_t *threads = NULL;
	int *skipped = NULL;
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
		} else if (meta->validator[0] != '\0' && validator != NULL && validator[0] != '\0'
			&& strcmp(meta->validator, validator) != 0) {
			printf("ETag changed, starting fresh download\n");
			metadata_free(meta);
			meta = NULL;
		} else {
			printf("Resuming download from metadata (%d chunks)\n", meta->thread_count);
			thread_count = meta->thread_count;
		}
	}

	if (meta == NULL) {
		meta = metadata_create(strHttpAddr, savePath, filesize, thread_count, validator);
		if (meta == NULL) {
			printf("Failed to create download metadata\n");
			free(meta_path);
			return 1;
		}
		thread_count = meta->thread_count;
	}

	/* metadata_create may reduce the count for files smaller than the request. */
	thread_count = meta->thread_count;

	/* Open (or create) the target file for pwrite */
	fd = sget_open_rw(savePath);
	if (fd < 0) {
		printf("Failed to open file for writing: %s\n", savePath);
		metadata_free(meta);
		free(meta_path);
		return 1;
	}

	/* For a new download, pre-allocate the file to the correct size */
	{
		int is_resume = 0;
		int ci;
		for (ci = 0; ci < meta->thread_count; ci++) {
			if (meta->chunks[ci].downloaded > 0) { is_resume = 1; break; }
		}
		if (!is_resume) {
			sget_truncate(fd, filesize);
		}
	}

	/* Save initial metadata */
	metadata_save(meta, meta_path);

	/* Init synchronization */
	sget_mutex_init(&progress_lock);

	/* Allocate thread handles and argument structs */
	threads = (sget_thread_t *)calloc(thread_count, sizeof(sget_thread_t));
	skipped = (int *)calloc(thread_count, sizeof(int));
	args = (ChunkDownloadArgs *)calloc(thread_count, sizeof(ChunkDownloadArgs));
	if (threads == NULL || args == NULL || skipped == NULL) {
		printf("Memory allocation failed\n");
		sget_close(fd);
		sget_mutex_destroy(&progress_lock);
		metadata_free(meta);
		free(meta_path);
		free(threads);
		free(skipped);
		free(args);
		return 1;
	}

	printf("Starting multi-threaded download: %d threads, %" PRIu64 " bytes\n",
		thread_count, filesize);

	/* Launch worker threads */
	for (i = 0; i < thread_count; i++) {
		/* Skip already completed chunks (resume case) */
		if (meta->chunks[i].status == CHUNK_STATUS_DONE) {
			printf("  Chunk %d: already complete\n", i);
			skipped[i] = 1;
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
			chunk_set_status(&args[i], CHUNK_STATUS_ERROR);
		}
	}

	/* Progress reporting while threads are running */
	{
		int all_finished = 0;
		while (!all_finished && !g_sget_should_stop) {
			uint64_t total_downloaded = 0;
			all_finished = 1;

#if defined(WIN32) || defined(WIN64)
			Sleep(500);
#else
			{
				struct timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 500000000L;
				nanosleep(&ts, NULL);
			}
#endif

			sget_mutex_lock(&progress_lock);
			for (i = 0; i < thread_count; i++) {
				total_downloaded += meta->chunks[i].downloaded;
				if (meta->chunks[i].status != CHUNK_STATUS_DONE &&
					meta->chunks[i].status != CHUNK_STATUS_ERROR)
					all_finished = 0;
			}
			sget_mutex_unlock(&progress_lock);

			print_progress_bar(total_downloaded, filesize);
		}
		printf("\n");
	}

	/* Wait for all threads to finish */
	for (i = 0; i < thread_count; i++) {
		if (skipped[i]) {
			continue; /* was already done before we started */
		}
		sget_thread_join(threads[i]);
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
	free(skipped);
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
	uint64_t filesize = 0;
	int accept_ranges = 0;
	char saveFilename[MAX_HEADER_ELEMENT_LEN] = {0};
	char validator[256] = {0};

	memset(saveFilename,0,MAX_HEADER_ELEMENT_LEN);
	hSocket=ConnectHttpNonProxy(strHostAddr,nHttpPort);
	if(hSocket == INVALID_SOCKET){
		printf("can't connect to the server\n");
		return FALSE;
	}

	/* First request: get file size, filename, and check Accept-Ranges */
	if (!SendHttpHeaderEx(hSocket,strHostAddr,nHttpPort,strHttpAddr,strHttpFilename,
		&filesize,saveFilename,&accept_ranges,0,0,0,0,validator)) {
		CloseSocket(hSocket);
		return FALSE;
	}
	CloseSocket(hSocket);
	if (filesize > INT64_MAX) {
		printf("File is too large for this platform\n");
		return FALSE;
	}

	if (strlen(saveFilename) == 0)
	{
		printf("get the saved name failed.\n");
		return FALSE;
	}

	/* Build the path without truncating or treating the name as a path. */
	if (saveFolder != NULL) {
		char fullPath[MAX_HEADER_ELEMENT_LEN];
		size_t folderLen = strlen(saveFolder);
		size_t nameLen = strlen(saveFilename);
		int separator = folderLen > 0 && saveFolder[folderLen - 1] != '/' &&
			saveFolder[folderLen - 1] != '\\';
		if (folderLen >= sizeof(fullPath) ||
			nameLen >= sizeof(fullPath) - folderLen - (size_t)separator) {
			printf("the save folder path is too long.\n");
			return FALSE;
		}
		memcpy(fullPath, saveFolder, folderLen);
		if (separator) fullPath[folderLen++] = '/';
		memcpy(fullPath + folderLen, saveFilename, nameLen + 1);
		memcpy(saveFilename, fullPath, folderLen + nameLen + 1);
	}

	printf("File: %s (%" PRIu64 " bytes), Accept-Ranges: %s, Threads: %d\n",
		saveFilename, filesize,
		accept_ranges ? "yes" : "no",
		(accept_ranges && filesize > 0 && thread_count > 1) ? thread_count : 1);

	/* Dispatch to multi-threaded or single-threaded download */
	if (HttpDownLoadMT(strHostAddr, nHttpPort, strHttpAddr, strHttpFilename,
		saveFilename, filesize, accept_ranges, thread_count, validator))
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

/* Caller provides HOST_STR_LEN, HTTP_PORT_LEN, URL_STR_LEN and URL_FILENAME_LEN buffers. */
bool ParseURL(const char *URL, char *host, char *port, char *path, char *filename)
{
	const char *authority, *authority_end, *port_start, *target, *target_end;
	const char *last_slash, *name_end, *p;
	size_t host_len, port_len, path_len, name_len;
	unsigned long port_number;
	char *port_tail;

	if (URL == NULL || host == NULL || port == NULL || path == NULL || filename == NULL ||
		!has_scheme(URL, HTTP_SCHEME, HTTP_SCHEME_LEN) || strlen(URL) >= URL_STR_LEN) return false;
	authority = URL + HTTP_SCHEME_LEN;
	authority_end = strpbrk(authority, "/?#");
	if (authority_end == NULL) authority_end = authority + strlen(authority);
	port_start = memchr(authority, ':', (size_t)(authority_end - authority));
	host_len = (size_t)((port_start != NULL ? port_start : authority_end) - authority);
	if (host_len == 0 || host_len >= HOST_STR_LEN) return false;
	for (p = authority; p < authority_end; p++) {
		unsigned char c = (unsigned char)*p;
		if (c <= 0x20 || c == 0x7f || *p == '@' || *p == '\\' || *p == '[' || *p == ']')
			return false; /* IPv6 and userinfo are not supported by this IPv4 client. */
	}
	memcpy(host, authority, host_len);
	host[host_len] = '\0';

	if (port_start != NULL) {
		port_len = (size_t)(authority_end - port_start - 1);
		if (port_len == 0 || port_len >= HTTP_PORT_LEN) return false;
		for (p = port_start + 1; p < authority_end; p++) {
			if (*p < '0' || *p > '9') return false;
		}
		memcpy(port, port_start + 1, port_len);
		port[port_len] = '\0';
		port_number = strtoul(port, &port_tail, 10);
		if (*port_tail != '\0' || port_number == 0 || port_number > 65535) return false;
	} else {
		strcpy(port, "80");
	}

	target = authority_end;
	target_end = strchr(target, '#');
	if (target_end == NULL) target_end = target + strlen(target);
	if (*target == '/' || *target == '?') {
		path_len = (size_t)(target_end - target);
		if (path_len + (*target == '?' ? 1 : 0) >= URL_STR_LEN) return false;
		if (*target == '?') {
			path[0] = '/';
			memcpy(path + 1, target, path_len);
			path_len++;
		} else {
			memcpy(path, target, path_len);
		}
		path[path_len] = '\0';
	} else {
		strcpy(path, "/");
	}
	for (p = path; *p; p++) {
		if ((unsigned char)*p <= 0x20 || (unsigned char)*p == 0x7f || *p == '\\')
			return false;
	}

	name_end = strchr(path, '?');
	if (name_end == NULL) name_end = path + strlen(path);
	last_slash = path;
	for (p = path; p < name_end; p++) {
		if (*p == '/') last_slash = p + 1;
	}
	name_len = (size_t)(name_end - last_slash);
	if (name_len == 0) {
		strcpy(filename, "index.html");
	} else {
		if (name_len >= URL_FILENAME_LEN) return false;
		memcpy(filename, last_slash, name_len);
		filename[name_len] = '\0';
	}
	return true;
}

/* ========================================================================
 * Public entry points
 * ======================================================================== */

bool sget_download(const char*strUrl, const char *saveFolder)
{
	return sget_download_mt(strUrl, saveFolder, DEFAULT_THREAD_COUNT);
}

bool sget_download_mt(const char *strUrl, const char *saveFolder, int thread_count)
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
	if (thread_count > SGET_MAX_THREAD_COUNT) thread_count = SGET_MAX_THREAD_COUNT;

	g_sget_last_error = 0;

	/* Install signal handler for graceful interrupt. */
	g_sget_should_stop = 0;
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
#if defined(WIN32) || defined(WIN64)
		WSACleanup();
#endif
	}
	else
	{
		if (strUrl != NULL && has_scheme(strUrl, HTTPS_SCHEME, HTTPS_SCHEME_LEN)) {
			printf("HTTPS is not supported. Use an http:// URL.\n");
		} else {
			printf("Invalid or unsupported URL: %s\n", strUrl ? strUrl : "(null)");
		}
		downrv = false;
	}
	
	return downrv;
}
