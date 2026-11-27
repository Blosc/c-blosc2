/* Invocation-local storage bridge, not a public callback/source ABI. */
#ifndef B2ND_EXEC_PRIVATE_H
#define B2ND_EXEC_PRIVATE_H
#include "b2nd-exec.h"

typedef struct {
    void *context;
    me_status (*read)(void *context, size_t start, size_t count,
                      me_mut_buffer output, me_error *error);
} b2nd_exec_reader;

me_status b2nd_exec_schedule_input(const b2nd_exec_schedule *schedule, size_t slot,
                                  b2nd_exec_input_meta *out);
/* Readers are serial, same-shape logical C-order spans. NULL read selects the
 * corresponding resident view. No broadcast or partial-axis storage admission. */
me_status b2nd_exec_execute_readers(const b2nd_exec_schedule *schedule,
                                   const b2nd_exec_view *views,
                                   const b2nd_exec_reader *readers, size_t ninputs,
                                   me_mut_buffer output, unsigned raise_mask,
                                   size_t max_gather_bytes, b2nd_exec_report *report,
                                   me_error *error);
#endif
