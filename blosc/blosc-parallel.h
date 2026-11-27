/* Lightweight internal portable dispatcher declaration; no codec context API. */
#ifndef BLOSC_PARALLEL_H
#define BLOSC_PARALLEL_H
#include <stddef.h>
#include <stdint.h>

/* Honors the synchronous threads callback, otherwise creates portable threads.
 * This is NOT currently a persistent-pool dispatcher. */
int blosc2_run_parallel(int16_t nthreads, void (*dojob)(void *),
                        size_t jobdata_elsize, void *jobdata);
#endif
