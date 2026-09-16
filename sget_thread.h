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
#ifndef SGET_THREAD_H_
#define SGET_THREAD_H_
#include "win2linux.h"

/* --- Thread --- */
#if defined(WIN32) || defined(WIN64)
#include <windows.h>
typedef HANDLE sget_thread_t;
typedef DWORD (WINAPI *sget_thread_func_t)(void *arg);
#else
#include <pthread.h>
typedef pthread_t sget_thread_t;
typedef void *(*sget_thread_func_t)(void *arg);
#endif

int sget_thread_create(sget_thread_t *thread, sget_thread_func_t func, void *arg);
int sget_thread_join(sget_thread_t thread);

/* --- Mutex --- */
#if defined(WIN32) || defined(WIN64)
typedef CRITICAL_SECTION sget_mutex_t;
#else
typedef pthread_mutex_t sget_mutex_t;
#endif

int sget_mutex_init(sget_mutex_t *mutex);
int sget_mutex_lock(sget_mutex_t *mutex);
int sget_mutex_unlock(sget_mutex_t *mutex);
int sget_mutex_destroy(sget_mutex_t *mutex);

/* --- Atomic flag for signal handling --- */
/* Use volatile int as portable atomic flag */
extern volatile int g_sget_should_stop;
void sget_install_signal_handler(void);

#endif
