/* Lightweight internal portable dispatcher declaration; no codec context API. */
#ifndef BLOSC_PARALLEL_H
#define BLOSC_PARALLEL_H
#include <stddef.h>
#include <stdint.h>

/* Honors the synchronous threads callback, otherwise dispatches via C-Blosc2's
 * persistent shared pool on POSIX (portable per-call threads on Windows). */
int blosc2_run_parallel(int16_t nthreads, void (*dojob)(void *),
                        size_t jobdata_elsize, void *jobdata);
#endif
