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
#include "sget_thread.h"

#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#endif

volatile int g_sget_should_stop = 0;

/* --- Thread --- */

int sget_thread_create(sget_thread_t *thread, sget_thread_func_t func, void *arg)
{
#if defined(WIN32) || defined(WIN64)
	HANDLE h = CreateThread(NULL, 0, func, arg, 0, NULL);
	if (h == NULL) return -1;
	*thread = h;
	return 0;
#else
	return pthread_create(thread, NULL, func, arg);
#endif
}

int sget_thread_join(sget_thread_t thread)
{
#if defined(WIN32) || defined(WIN64)
	DWORD ret = WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	return (ret == WAIT_OBJECT_0) ? 0 : -1;
#else
	return pthread_join(thread, NULL);
#endif
}

/* --- Mutex --- */

int sget_mutex_init(sget_mutex_t *mutex)
{
#if defined(WIN32) || defined(WIN64)
	InitializeCriticalSection(mutex);
	return 0;
#else
	return pthread_mutex_init(mutex, NULL);
#endif
}

int sget_mutex_lock(sget_mutex_t *mutex)
{
#if defined(WIN32) || defined(WIN64)
	EnterCriticalSection(mutex);
	return 0;
#else
	return pthread_mutex_lock(mutex);
#endif
}

int sget_mutex_unlock(sget_mutex_t *mutex)
{
#if defined(WIN32) || defined(WIN64)
	LeaveCriticalSection(mutex);
	return 0;
#else
	return pthread_mutex_unlock(mutex);
#endif
}

int sget_mutex_destroy(sget_mutex_t *mutex)
{
#if defined(WIN32) || defined(WIN64)
	DeleteCriticalSection(mutex);
	return 0;
#else
	return pthread_mutex_destroy(mutex);
#endif
}

/* --- Signal handler --- */

#if defined(WIN32) || defined(WIN64)
static BOOL WINAPI sget_console_handler(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
		g_sget_should_stop = 1;
		return TRUE;
	}
	return FALSE;
}
#else
static void sget_signal_handler(int sig)
{
	(void)sig;
	g_sget_should_stop = 1;
}
#endif

void sget_install_signal_handler(void)
{
#if defined(WIN32) || defined(WIN64)
	SetConsoleCtrlHandler(sget_console_handler, TRUE);
#else
	signal(SIGINT, sget_signal_handler);
	signal(SIGTERM, sget_signal_handler);
#endif
}
