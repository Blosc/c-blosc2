/*
  Copyright (c) 2026  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program showing how to update many chunks of a contiguous frame
  between blosc2_schunk_begin_updates() and blosc2_schunk_end_updates().

  A contiguous frame (a cframe, on disk or in memory) stores its chunks back
  to back in one payload, followed by the offsets of the chunks and a trailer:

    | header | chunk A | chunk B | chunk C | chunk D | offsets | trailer |
             <--------------- payload ---------------->

  New data in a chunk usually compresses better or worse than the old data,
  so the chunk changes size.  To keep the payload compact, an update then
  moves everything stored behind the chunk.  Fine for one update, but
  overwriting all n chunks one by one moves ~n^2/2 chunks.  Here chunks of 4
  units grow to 5 units:

                     0    5    10   15   20
    as written       AAAABBBBCCCCDDDD
    update A         AAAAABBBBCCCCDDDD           B, C, D move
    update B         AAAAABBBBBCCCCDDDD          C, D move
    update C         AAAAABBBBBCCCCCDDDD         D moves
    update D         AAAAABBBBBCCCCCDDDDD        6 chunk moves in 4 updates

  Between blosc2_schunk_begin_updates() and blosc2_schunk_end_updates(), an
  update moves no other chunk.  The new chunk goes into its old place,
  together with any free space right before and after it, when it fits
  there; else it goes to the end of the payload.  Whatever space is left
  over stays free ('.') until blosc2_schunk_end_updates() closes all of it
  in one pass, in which every chunk moves at most once.  When the chunks are
  overwritten in order, the free space slides along the payload:

                     0    5    10   15   20
    as written       AAAABBBBCCCCDDDD
    update A         ....BBBBCCCCDDDDAAAAA       A does not fit: to the end
    update B         BBBBB...CCCCDDDDAAAAA       B fits where A and B were
    update C         BBBBBCCCCC..DDDDAAAAA       the free space slides on
    update D         BBBBBCCCCCDDDDD.AAAAA
    end_updates()    BBBBBCCCCCDDDDDAAAAA        A moves (by 1 unit), once

  Afterwards the payload has the same length as without the bracket; only the
  order of the chunks in it can differ (A now sits at the end, as an inserted
  chunk does).  That does not matter to readers: every access to a chunk goes
  through its offset.

  Things to know:

  - b2nd_set_slice_cbuffer(), b2nd_set_orthogonal_selection(), b2nd_resize()
    and blosc2_schunk_set_slice_buffer() bracket their own updates, so a
    single slice write gets all this without doing anything.  Bracket a
    sequence of such calls, or a loop over blosc2_schunk_update_chunk() or
    blosc2_schunk_update_buffer() as below.
  - The frame is valid at every point inside the bracket, just larger: it
    can be read, and `cbytes` includes the free space until the end.
  - Brackets nest; only the outermost blosc2_schunk_end_updates() closes the
    free space.  A frame whose bracket is never closed stays valid, its free
    space just stays (like the space of deleted chunks).
  - The bracket holds the exclusive frame lock, as blosc2_schunk_lock() does
    (a no-op unless file locking is enabled, see file-locking.c), so other
    locked handles never see the free space.
  - Only contiguous frames keep their chunks back to back.  For other
    super-chunks (sparse frames, plain in-memory super-chunks) the bracket
    is just the lock.

  To compile this program:

  $ gcc deferred_updates.c -o deferred_updates -lblosc2

  To run:

  $ ./deferred_updates
  Updating the chunks of a frame one by one:
    as written     AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH            (1 character: 1069 bytes)
    update A       AAAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH           moved 29932 bytes
    update B       AAAAABBBBBCCCCDDDDEEEEFFFFGGGGHHHH          moved 25656 bytes
    update C       AAAAABBBBBCCCCCDDDDEEEEFFFFGGGGHHHH         moved 21380 bytes
    update D       AAAAABBBBBCCCCCDDDDDEEEEFFFFGGGGHHHH        moved 17104 bytes
    update E       AAAAABBBBBCCCCCDDDDDEEEEEFFFFGGGGHHHH       moved 12828 bytes
    update F       AAAAABBBBBCCCCCDDDDDEEEEEFFFFFGGGGHHHH      moved 8552 bytes
    update G       AAAAABBBBBCCCCCDDDDDEEEEEFFFFFGGGGGHHHH     moved 4276 bytes
    update H       AAAAABBBBBCCCCCDDDDDEEEEEFFFFFGGGGGHHHHH    moved 0 bytes
    moved 119728 bytes in all, for a payload of 42432 bytes

  The same updates inside blosc2_schunk_begin_updates() / end_updates():
    as written     AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH            (1 character: 1069 bytes)
    update A       ....BBBBCCCCDDDDEEEEFFFFGGGGHHHHAAAAA       moved 0 bytes
    update B       BBBBB...CCCCDDDDEEEEFFFFGGGGHHHHAAAAA       moved 0 bytes
    update C       BBBBBCCCCC..DDDDEEEEFFFFGGGGHHHHAAAAA       moved 0 bytes
    update D       BBBBBCCCCCDDDDD.EEEEFFFFGGGGHHHHAAAAA       moved 0 bytes
    update E       BBBBBCCCCCDDDDDEEEEEFFFFGGGGHHHHAAAAA       moved 0 bytes
    update F       BBBBBCCCCCDDDDDEEEEE....GGGGHHHHAAAAAFFFFF  moved 0 bytes
    update G       BBBBBCCCCCDDDDDEEEEEGGGGG...HHHHAAAAAFFFFF  moved 0 bytes
    update H       BBBBBCCCCCDDDDDEEEEEGGGGGHHHHH..AAAAAFFFFF  moved 0 bytes
    end_updates()  BBBBBCCCCCDDDDDEEEEEGGGGGHHHHHAAAAAFFFFF    moved 10608 bytes
    (the payload shrank from 44816 to 42432 bytes)
    moved 10608 bytes in all, for a payload of 42432 bytes

  Both frames hold the same data in payloads of the same length.

  The drawings show where the chunks really lie in the frame after every
  step, one character per quarter of a chunk as first written; `moved`
  counts the bytes of the other chunks that changed place.  Without the
  bracket, the updates move 2.8 times the whole payload.  With it, F does not
  fit into the free space either and goes to the end, G and H slide into its
  old place, and blosc2_schunk_end_updates() moves just A and F, once each.
  (The exact sizes depend on the codec.)
*/

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blosc2.h"

#define URLPATH "deferred_updates.b2frame"
#define NCHUNKS 8
#define CHUNK_ITEMS 4096                    // int32 items: 16 KiB per chunk before compression
#define OLD_NOISE (CHUNK_ITEMS / 4)         // incompressible items in the chunks as first written...
#define NEW_NOISE (5 * CHUNK_ITEMS / 16)    // ... and in their new versions, which get ~25% larger
#define DRAW_WIDTH 44                       // width of the drawings, before the notes

typedef struct {
  int64_t offset;   // where the chunk starts in the payload
  int32_t cbytes;   // its compressed size
} place;

static int64_t bytes_per_char = 1;  // scale of the drawings

/* The data of a chunk: `noise` pseudo-random (incompressible) items, then a constant */
static void fill_chunk(int32_t *data, int nchunk, int noise) {
  uint32_t state = 2654435761u * (uint32_t)(nchunk + 1) + (uint32_t)noise;
  for (int i = 0; i < CHUNK_ITEMS; i++) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    data[i] = i < noise ? (int32_t)state : nchunk;
  }
}

/* Every chunk holds its new data */
static bool check_chunks(blosc2_schunk *schunk, const char *when) {
  int32_t expected[CHUNK_ITEMS];
  int32_t data[CHUNK_ITEMS];
  for (int i = 0; i < NCHUNKS; i++) {
    fill_chunk(expected, i, NEW_NOISE);
    if (blosc2_schunk_decompress_chunk(schunk, i, data, sizeof(data)) != (int)sizeof(data) ||
        memcmp(data, expected, sizeof(data)) != 0) {
      fprintf(stderr, "Chunk %c is wrong %s\n", 'A' + i, when);
      return false;
    }
  }
  return true;
}

/* Where every chunk lies in the payload: blosc2_frame_get_offsets() gives the offsets, and the
   header of each chunk its compressed size. */
static int get_layout(blosc2_schunk *schunk, place *layout) {
  int64_t *offsets = blosc2_frame_get_offsets(schunk);
  if (offsets == NULL) {
    return -1;
  }
  for (int i = 0; i < NCHUNKS; i++) {
    uint8_t *chunk;
    bool needs_free;
    int rc = blosc2_schunk_get_lazychunk(schunk, i, &chunk, &needs_free);
    if (rc >= 0) {
      layout[i].offset = offsets[i];
      rc = blosc2_cbuffer_sizes(chunk, NULL, &layout[i].cbytes, NULL);
      if (needs_free) {
        free(chunk);
      }
    }
    if (rc < 0) {
      free(offsets);
      return rc;
    }
  }
  free(offsets);
  return 0;
}

/* Draw `c` from column *col up to the column where payload byte `end` falls */
static void draw_run(char c, int64_t end, int *col) {
  int end_col = (int)((end + bytes_per_char / 2) / bytes_per_char);
  for (; *col < end_col; (*col)++) {
    putchar(c);
  }
}

/* Draw the payload, one character per bytes_per_char bytes: the letter of the chunk stored there, or '.'
   for free space.  Then the note. */
static void draw(const char *label, const place *layout, int64_t payload_len, const char *note) {
  printf("  %-14s ", label);
  int col = 0;
  int64_t pos = 0;  // payload drawn up to here
  while (true) {
    int next = -1;  // the next chunk in the payload
    for (int i = 0; i < NCHUNKS; i++) {
      if (layout[i].offset >= pos && (next < 0 || layout[i].offset < layout[next].offset)) {
        next = i;
      }
    }
    draw_run('.', next < 0 ? payload_len : layout[next].offset, &col);
    if (next < 0) {
      break;
    }
    pos = layout[next].offset + layout[next].cbytes;
    draw_run((char)('A' + next), pos, &col);
  }
  printf("%*s%s\n", col < DRAW_WIDTH ? DRAW_WIDTH - col : 1, "", note);
}

/* Bytes of the chunks other than `except` that changed place between two layouts */
static int64_t bytes_moved(const place *before, const place *after, int except) {
  int64_t moved = 0;
  for (int i = 0; i < NCHUNKS; i++) {
    if (i != except && after[i].offset != before[i].offset) {
      moved += after[i].cbytes;
    }
  }
  return moved;
}

/* Write a frame on disk, overwrite all its chunks with their new versions (inside a bracket or not),
   and check them.  Returns the final payload length, or -1 on errors. */
static int64_t overwrite_all(bool bracket) {
  int32_t data[CHUNK_ITEMS];
  place before[NCHUNKS];
  place after[NCHUNKS];
  char note[64];
  bool updating = false;
  int64_t total_moved = 0;
  int64_t result = -1;

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.typesize = sizeof(int32_t);
  cparams.compcode = BLOSC_LZ4;
  blosc2_storage storage = {.contiguous = true, .urlpath = URLPATH, .cparams = &cparams};
  blosc2_remove_urlpath(URLPATH);
  blosc2_schunk *schunk = blosc2_schunk_new(&storage);
  if (schunk == NULL) {
    fprintf(stderr, "Cannot create the super-chunk\n");
    return -1;
  }
  for (int i = 0; i < NCHUNKS; i++) {
    fill_chunk(data, i, OLD_NOISE);
    if (blosc2_schunk_append_buffer(schunk, data, sizeof(data)) < 0) {
      fprintf(stderr, "Cannot append chunk %c\n", 'A' + i);
      goto out;
    }
  }
  if (get_layout(schunk, before) < 0) {
    goto out;
  }
  bytes_per_char = before[0].cbytes / 4;
  snprintf(note, sizeof(note), "(1 character: %" PRId64 " bytes)", bytes_per_char);
  draw("as written", before, schunk->cbytes, note);

  // From here on, an update moves no other chunk; the free space it leaves stays until
  // blosc2_schunk_end_updates().  This also takes the exclusive frame lock.
  if (bracket) {
    if (blosc2_schunk_begin_updates(schunk) < 0) {
      fprintf(stderr, "Cannot begin the updates\n");
      goto out;
    }
    updating = true;
  }

  for (int i = 0; i < NCHUNKS; i++) {
    fill_chunk(data, i, NEW_NOISE);
    if (blosc2_schunk_update_buffer(schunk, i, data, sizeof(data)) < 0) {
      fprintf(stderr, "Cannot update chunk %c\n", 'A' + i);
      goto out;
    }
    if (get_layout(schunk, after) < 0) {
      goto out;
    }
    int64_t moved = bytes_moved(before, after, i);
    total_moved += moved;
    char label[16];
    snprintf(label, sizeof(label), "update %c", 'A' + i);
    snprintf(note, sizeof(note), "moved %" PRId64 " bytes", moved);
    draw(label, after, schunk->cbytes, note);
    memcpy(before, after, sizeof(after));
  }

  if (bracket) {
    // The frame is valid inside the bracket: every chunk reads back from wherever it lies now
    if (!check_chunks(schunk, "inside the bracket")) {
      goto out;
    }

    // Close the free space in one pass (every chunk moves at most once) and release the lock
    int64_t payload_inside = schunk->cbytes;
    updating = false;
    if (blosc2_schunk_end_updates(schunk) < 0) {
      fprintf(stderr, "Cannot end the updates\n");
      goto out;
    }
    if (get_layout(schunk, after) < 0) {
      goto out;
    }
    int64_t moved = bytes_moved(before, after, -1);
    total_moved += moved;
    snprintf(note, sizeof(note), "moved %" PRId64 " bytes", moved);
    draw("end_updates()", after, schunk->cbytes, note);
    printf("  (the payload shrank from %" PRId64 " to %" PRId64 " bytes)\n", payload_inside,
           schunk->cbytes);
  }
  printf("  moved %" PRId64 " bytes in all, for a payload of %" PRId64 " bytes\n", total_moved,
         schunk->cbytes);

  // The file holds the new data
  blosc2_schunk_free(schunk);
  schunk = blosc2_schunk_open(URLPATH);
  if (schunk == NULL) {
    fprintf(stderr, "Cannot reopen the frame\n");
    goto out;
  }
  if (check_chunks(schunk, "after reopening")) {
    result = schunk->cbytes;
  }

  out:
  if (updating) {
    blosc2_schunk_end_updates(schunk);  // releases the lock
  }
  if (schunk != NULL) {
    blosc2_schunk_free(schunk);
  }
  blosc2_remove_urlpath(URLPATH);
  return result;
}

int main(void) {
  blosc2_init();

  printf("Updating the chunks of a frame one by one:\n");
  int64_t plain_len = overwrite_all(false);

  printf("\nThe same updates inside blosc2_schunk_begin_updates() / end_updates():\n");
  int64_t bracket_len = overwrite_all(true);

  blosc2_destroy();
  if (plain_len < 0 || bracket_len < 0) {
    return 1;
  }
  if (plain_len != bracket_len) {
    fprintf(stderr, "The payloads differ: %" PRId64 " vs %" PRId64 " bytes\n", plain_len, bracket_len);
    return 1;
  }
  printf("\nBoth frames hold the same data in payloads of the same length.\n");
  return 0;
}
