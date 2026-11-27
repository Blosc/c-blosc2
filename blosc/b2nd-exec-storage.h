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

/* Serial, same-shape native float64 in-memory chunked inputs. Arrays/schunk
 * metadata and contents must remain unchanged for the synchronous call.
 * Uses the existing schedule/traversal/reducers, not a storage reducer loop.
 * No postfilters, frame-backed sources, broadcasting, partial-axis reductions
 * or compressed sinks are admitted yet. Output follows the resident publication
 * contract. Decode and gather budgets are independent; all preflight precedes
 * compressed reads. One codec thread, no shared array/context cache mutation. */
me_status b2nd_exec_execute_arrays(const b2nd_exec_schedule *schedule,
                                  const b2nd_array_t *const *inputs, size_t ninputs,
                                  me_mut_buffer output, unsigned raise_mask,
                                  size_t max_decode_bytes, size_t max_gather_bytes,
                                  b2nd_exec_storage_report *report, me_error *error);
#endif
