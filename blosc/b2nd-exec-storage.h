/* Experimental native storage binding for the general executor; not installed. */
#ifndef B2ND_EXEC_STORAGE_H
#define B2ND_EXEC_STORAGE_H
#include "b2nd-exec.h"
#include "b2nd.h"

typedef struct {
    b2nd_exec_report execution;
    size_t decode_bytes; /* Owned decoded-chunk caches; excludes codec internals. */
    size_t storage_metadata_bytes;
    size_t decodes; /* Successful chunk decodes, including repeated visits. */
    size_t decoded_bytes;
    size_t cache_hits; /* Logical item lookups served by the current chunk cache. */
    size_t read_spans; /* Attempted logical span reads, including a failing span. */
} b2nd_exec_storage_report;

/* In-memory native numeric chunked arrays (with right-aligned N-D broadcasting
 * and optional mixed resident views when `arrays[i]` is NULL and `views != NULL`).
 * Admits maps, whole-domain/partial-axis reductions, and elementwise compressed
 * sinks (`b2nd_exec_execute_to_array`). Frame-backed sources/sinks remain deferred.
 * Arrays/schunk metadata and contents must remain unchanged for the call.
 * Decode and gather/mapped budgets are independent; all preflight precedes
 * compressed reads or sink writes. Each worker owns single-thread codec contexts
 * and private decode caches without mutating shared array/context state. */
me_status b2nd_exec_execute_arrays(const b2nd_exec_schedule *schedule,
                                  const b2nd_array_t *const *inputs, size_t ninputs,
                                  me_mut_buffer output, unsigned raise_mask,
                                  size_t max_decode_bytes, size_t max_gather_bytes,
                                  b2nd_exec_storage_report *report, me_error *error);
me_status b2nd_exec_execute_arrays_ex(const b2nd_exec_schedule *schedule,
                                     const b2nd_array_t *const *arrays,
                                     const b2nd_exec_view *views, size_t ninputs,
                                     me_mut_buffer output, unsigned raise_mask,
                                     size_t max_decode_bytes, size_t max_gather_bytes,
                                     const b2nd_exec_jobs *jobs,
                                     b2nd_exec_storage_report *report, me_error *error);
/* Materialize an elementwise schedule directly into a compressed in-memory
 * b2nd_array_t destination using a block-level Blosc2 compression prefilter.
 * Validates all inputs/outputs/budgets before mutating `output`. */
me_status b2nd_exec_execute_to_array(const b2nd_exec_schedule *schedule,
                                     const b2nd_array_t *const *arrays,
                                     const b2nd_exec_view *views, size_t ninputs,
                                     b2nd_array_t *output, unsigned raise_mask,
                                     size_t max_decode_bytes, size_t max_gather_bytes,
                                     const b2nd_exec_jobs *jobs,
                                     b2nd_exec_storage_report *report, me_error *error);
#endif
