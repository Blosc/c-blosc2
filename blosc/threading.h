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


#ifndef BLOSC_THREADING_H
#define BLOSC_THREADING_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "windows.h"

/*
 * Defines that adapt Windows API threads to pthreads API
 */
#define blosc2_pthread_mutex_t CRITICAL_SECTION

#define blosc2_pthread_mutex_init(a,b) InitializeCriticalSection((a))
#define blosc2_pthread_mutex_destroy(a) DeleteCriticalSection((a))
#define blosc2_pthread_mutex_lock EnterCriticalSection
#define blosc2_pthread_mutex_unlock LeaveCriticalSection

/*
 * Implement simple condition variable for Windows threads, based on ACE
 * implementation.
 */
typedef struct {
	LONG waiters;
	int was_broadcast;
	CRITICAL_SECTION waiters_lock;
	HANDLE sema;
	HANDLE continue_broadcast;
} blosc2_pthread_cond_t;

int blosc2_pthread_cond_init(blosc2_pthread_cond_t *cond, const void *unused);
int blosc2_pthread_cond_destroy(blosc2_pthread_cond_t *cond);
int blosc2_pthread_cond_wait(blosc2_pthread_cond_t *cond, CRITICAL_SECTION *mutex);
int blosc2_pthread_cond_signal(blosc2_pthread_cond_t *cond);
int blosc2_pthread_cond_broadcast(blosc2_pthread_cond_t *cond);

/*
 * Simple thread creation implementation using pthread API
 */
typedef struct {
	HANDLE handle;
	void *(*start_routine)(void*);
	void *arg;
} blosc2_pthread_t;

int blosc2_pthread_create(blosc2_pthread_t *thread, const void *unused,
			  void *(*start_routine)(void*), void *arg);

/*
 * To avoid the need of copying a struct, we use small macro wrapper to pass
 * pointer to win32_pthread_join instead.
 */
#define blosc2_pthread_join(a, b) blosc2_pthread_join_impl(&(a), (b))

int blosc2_pthread_join_impl(blosc2_pthread_t *thread, void **value_ptr);

#else /* not _WIN32 */

#include <pthread.h>

#define blosc2_pthread_mutex_t pthread_mutex_t
#define blosc2_pthread_mutex_init(a, b) pthread_mutex_init((a), (b))
#define blosc2_pthread_mutex_destroy(a) pthread_mutex_destroy((a))
#define blosc2_pthread_mutex_lock(a) pthread_mutex_lock((a))
#define blosc2_pthread_mutex_unlock(a) pthread_mutex_unlock((a))

#define blosc2_pthread_cond_t pthread_cond_t
#define blosc2_pthread_cond_init(a, b) pthread_cond_init((a), (b))
#define blosc2_pthread_cond_destroy(a) pthread_cond_destroy((a))
#define blosc2_pthread_cond_wait(a, b) pthread_cond_wait((a), (b))
#define blosc2_pthread_cond_signal(a) pthread_cond_signal((a))
#define blosc2_pthread_cond_broadcast(a) pthread_cond_broadcast((a))

#define blosc2_pthread_t pthread_t
#define blosc2_pthread_create(a, b, c, d) pthread_create((a), (b), (c), (d))
#define blosc2_pthread_join(a, b) pthread_join((a), (b))

#endif

#endif /* BLOSC_THREADING_H */
