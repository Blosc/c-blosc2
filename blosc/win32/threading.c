/*
 * Code for simulating pthreads API on Windows.  This is Git-specific,
 * but it is enough for Numexpr needs too.
 *
 * Copyright (C) 2009 Andrzej K. Haczewski <ahaczewski@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * DISCLAIMER: The implementation is Git-specific, it is subset of original
 * Pthreads API, without lots of other features that Git doesn't use.
 * Git also makes sure that the passed arguments are valid, so there's
 * no need for double-checking.
 */

#include "../threading.h"

#include "stdio.h"
#include "stdlib.h"
#include "process.h"
#include "errno.h"
#include "limits.h"


#define PTHREAD_UNUSED_PARAM(x) ((void)(x))

// The following typedefs are used to make this file as similar as possible to the original code,
// replacing `blosc2_pthread_{xyz}` structs with `pthread_{xyz}`, so that future bumps of this file
// will be easier.
// For function names we explicitly add the `blosc2_` prefix, without any typedefs or macros, and
// therefore we will have to update the code manually next time we bump this file's code.
typedef blosc2_pthread_t pthread_t;
typedef blosc2_pthread_cond_t pthread_cond_t;


void die(const char *err, ...)
{
	printf("%s", err);
	exit(-1);
}

static unsigned __stdcall win32_start_routine(void *arg)
{
	pthread_t *thread = (pthread_t*)arg;
	thread->arg = thread->start_routine(thread->arg);
	return 0;
}

int blosc2_pthread_create(pthread_t *thread, const void *unused,
		           void *(*start_routine)(void*), void *arg)
{
	PTHREAD_UNUSED_PARAM(unused);
	thread->arg = arg;
	thread->start_routine = start_routine;
	thread->handle = (HANDLE)
		_beginthreadex(NULL, 0, win32_start_routine, thread, 0, NULL);

	if (!thread->handle)
		return errno;
	else
		return 0;
}

int blosc2_pthread_join_impl(pthread_t *thread, void **value_ptr)
{
	DWORD result = WaitForSingleObject(thread->handle, INFINITE);
	switch (result) {
		case WAIT_OBJECT_0:
			if (value_ptr)
				*value_ptr = thread->arg;
			CloseHandle(thread->handle);
			return 0;
		case WAIT_ABANDONED:
			CloseHandle(thread->handle);
			return EINVAL;
		default:
			return GetLastError();
	}
}
