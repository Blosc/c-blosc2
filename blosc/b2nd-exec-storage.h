/* Experimental native storage binding for the general executor; not installed. */
#ifndef B2ND_EXEC_STORAGE_H
#define B2ND_EXEC_STORAGE_H
#include "b2nd-exec.h"
#include "b2nd.h"

typedef struct {
    b2nd_exec_report execution;
    size_t decode_bytes; /* Aggregate owned chunk/block caches; excludes codec internals. */
    size_t storage_metadata_bytes;
    size_t decodes; /* Successful chunk decodes, including repeated visits. */
    size_t decoded_bytes;
    size_t cache_hits; /* Logical item lookups served by an owned cache slot. */
    size_t read_spans; /* Attempted logical span reads, including a failing span. */
    size_t evaluated_groups;
    size_t max_group_items;
} b2nd_exec_storage_report;

typedef me_status (*b2nd_exec_source_read_fn)(void *context, const size_t *indices,
                                              size_t count, me_mut_buffer dst,
                                              me_error *error);

typedef struct {
    size_t struct_size;
    me_dtype dtype;
    size_t itemsize;
    size_t ndim;
    int64_t shape[B2ND_EXEC_MAX_RANK];
    void *context;
    b2nd_exec_source_read_fn read; /* NULL selects resident views[slot]. */
} b2nd_exec_source;

typedef me_status (*b2nd_exec_sink_write_fn)(void *context, size_t offset_items,
                                              size_t count, me_buffer src,
                                              me_error *error);

typedef struct {
    size_t struct_size;
    me_dtype dtype;
    size_t itemsize;
    size_t ndim;
    int64_t shape[B2ND_EXEC_MAX_RANK];
    void *context;
    b2nd_exec_sink_write_fn write;
} b2nd_exec_sink;

typedef struct {
    b2nd_exec_report execution;
    size_t read_calls;
    size_t read_items;
    size_t max_read_items;
    size_t read_bytes;
    size_t write_calls;
    size_t write_items;
    size_t max_write_items;
    size_t written_bytes;
    size_t index_buffer_bytes;
    size_t output_buffer_bytes;
} b2nd_exec_callback_report;

/* In-memory or frame-backed native numeric chunked arrays (with right-aligned N-D
 * broadcasting and optional mixed resident views when `arrays[i]` is NULL and
 * `views != NULL`). Admits maps, whole-domain/partial-axis reductions (including
 * moments and arg-extrema), and elementwise compressed sinks
 * (`b2nd_exec_execute_to_array`). Arrays/schunk metadata and contents must remain
 * unchanged for the call. Decode and gather/mapped budgets are independent; all
 * preflight precedes compressed reads or sink writes. Each worker owns single-thread
 * codec contexts and private decode caches without mutating shared `current_nchunk`.
 * Resident-output readers grow up to eight LRU chunk slots per operand/worker,
 * using only decode budget left after task admission. A one-slot budget preserves
 * the original reader footprint. Cache policy never changes logical reduction order. */
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
/* Execute a schedule over synchronous metadata-only source and/or sink span
 * callbacks mixed with resident views. Validates all metadata and bounded
 * workspaces before the first callback; never mutates output budgets past
 * max_gather_bytes or max_output_bytes. */
me_status b2nd_exec_execute_sources(const b2nd_exec_schedule *schedule,
                                    const b2nd_exec_source *sources,
                                    const b2nd_exec_view *views, size_t ninputs,
                                    me_mut_buffer output,
                                    const b2nd_exec_sink *sink,
                                    unsigned raise_mask,
                                    size_t max_gather_bytes,
                                    size_t max_output_bytes,
                                    b2nd_exec_callback_report *report,
                                    me_error *error);
/* Execute a specialized logical-partition schedule over compressed arrays,
 * resident views, and/or generic source callbacks without substituting physical
 * storage blocks for logical groups. */
/* Metadata/header-only capability admission; never decodes payloads, invokes
 * callbacks, allocates output or executes a kernel. Success with admitted=false
 * permits a wrapper's old route. Execution errors must never use this fallback. */
me_status b2nd_exec_admit_partitions(const b2nd_exec_schedule *schedule,
                                     const b2nd_array_t *const *arrays,
                                     const b2nd_exec_view *views,
                                     const b2nd_exec_source *sources, size_t ninputs,
                                     bool *admitted, me_error *error);
me_status b2nd_exec_execute_partitions(const b2nd_exec_schedule *schedule,
                                       const b2nd_array_t *const *arrays,
                                       const b2nd_exec_view *views,
                                       const b2nd_exec_source *sources,
                                       size_t ninputs,
                                       me_mut_buffer output,
                                       unsigned raise_mask,
                                       size_t max_decode_bytes,
                                       size_t max_gather_bytes,
                                       b2nd_exec_storage_report *report,
                                       me_error *error);
#endif
