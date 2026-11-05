/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Read-ahead for frames on disk (see blosc2_set_prefetch()).
*/

#ifndef BLOSC_PREFETCH_H
#define BLOSC_PREFETCH_H

#include "frame.h"
#include "blosc2.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct prefetch_batch prefetch_batch;

/* Whether read-ahead is on and applies to @p frame (a contiguous frame on disk; not on Windows): callers can skip
   working out what to read ahead otherwise */
bool prefetch_applies(const blosc2_frame_s *frame);

/* Ask the kernel to start reading, in the background, what opening the frame at @p offset of the file behind @p fp (a
   handle of the default filesystem or the mmap I/O; @p file_size bytes) reads next: its header at the start and its
   trailer and chunk index at the end, also with read-ahead off; with read-ahead on, all of it up to 1 MiB */
void prefetch_open_hint(const blosc2_io *io, void *fp, int64_t offset, int64_t file_size);

/* Start reading the parts of chunks nchunks[0..n) of a contiguous frame on disk that are not in the page cache, with
   the read-ahead threads, in this order.  @p maskouts is NULL (whole chunks), or one pointer per chunk: NULL to read
   chunk nchunks[i] whole, or @p nblocks flags as for blosc2_set_maskout(), set for the blocks not needed; of a large
   enough chunk only the blocks needed are read then (and its header).  Takes the frame lock for the planning
   (re-entrant).  NULL if there is nothing to do: read-ahead is off or does not apply (see prefetch_applies()), or
   everything is cached.  Free it with prefetch_batch_free(). */
prefetch_batch* prefetch_batch_new(blosc2_frame_s *frame, const int64_t *nchunks, int64_t n,
                                   const bool *const *maskouts, int64_t nblocks);

/* Block until chunk @p nchunk of @p batch has been read, and let the threads read further ahead.  No-op for a NULL
   batch or a chunk that is not in it. */
void prefetch_batch_wait(prefetch_batch *batch, int64_t nchunk);

/* Drop the reads not started yet, wait for those in flight and free @p batch.  No-op for NULL. */
void prefetch_batch_free(prefetch_batch *batch);

/* Stop and join the read-ahead threads (blosc2_destroy()) */
void prefetch_destroy(void);

#endif /* BLOSC_PREFETCH_H */
