/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FRAME_H
#define BLOSC_FRAME_H

#include "blosc2.h"

#include <stdbool.h>
#include <stdint.h>

// Different types of frames
#define FRAME_CONTIGUOUS_TYPE 0
#define FRAME_DIRECTORY_TYPE 1


// Constants for metadata placement in header
#define FRAME_HEADER_MAGIC 2
#define FRAME_HEADER_LEN (FRAME_HEADER_MAGIC + 8 + 1)  // 11
#define FRAME_LEN (FRAME_HEADER_LEN + 4 + 1)  // 16
#define FRAME_FLAGS (FRAME_LEN + 8 + 1)  // 25
#define FRAME_TYPE (FRAME_FLAGS + 1)  // 26
#define FRAME_CODECS (FRAME_FLAGS + 2)  // 27
#define FRAME_OTHER_FLAGS (FRAME_FLAGS + 3)  // 28
#define FRAME_NBYTES (FRAME_FLAGS + 4 + 1)  // 30
#define FRAME_CBYTES (FRAME_NBYTES + 8 + 1)  // 39
#define FRAME_TYPESIZE (FRAME_CBYTES + 8 + 1) // 48
#define FRAME_BLOCKSIZE (FRAME_TYPESIZE + 4 + 1)  // 53
#define FRAME_CHUNKSIZE (FRAME_BLOCKSIZE + 4 + 1)  // 58
#define FRAME_NTHREADS_C (FRAME_CHUNKSIZE + 4 + 1)  // 63
#define FRAME_NTHREADS_D (FRAME_NTHREADS_C + 2 + 1)  // 66
#define FRAME_HAS_VLMETALAYERS (FRAME_NTHREADS_D + 2)  // 68
#define FRAME_FILTER_PIPELINE (FRAME_HAS_VLMETALAYERS + 1 + 1) // 70
#define FRAME_UDCODEC (FRAME_FILTER_PIPELINE + 1 + 6) // 77
#define FRAME_CODEC_META (FRAME_FILTER_PIPELINE + 1 + 7) // 78
#define FRAME_HEADER_MINLEN (FRAME_FILTER_PIPELINE + 1 + 16)  // 87 <- minimum length
#define FRAME_METALAYERS (FRAME_HEADER_MINLEN)  // 87
#define FRAME_IDX_SIZE (FRAME_METALAYERS + 1 + 1)  // 89

#define FRAME_FILTER_PIPELINE_MAX (8)  // the maximum number of filters that can be stored in header

#define FRAME_TRAILER_VERSION_BETA2 (0U)  // for beta.2 and former
#define FRAME_TRAILER_VERSION (1U)        // can be up to 127

#define FRAME_TRAILER_MINLEN (25)  // minimum length for the trailer (msgpack overhead)
#define FRAME_TRAILER_LEN_OFFSET (22)  // offset to trailer length (counting from the end)
#define FRAME_TRAILER_VLMETALAYERS (2)


typedef struct {
  char* urlpath;            //!< The name of the file or directory if it's an sframe; if NULL, this is in-memory
  uint8_t* cframe;          //!< The in-memory, contiguous frame buffer
  bool avoid_cframe_free;   //!< Whether the cframe can be freed (false) or not (true).
  uint8_t* coffsets;        //!< Pointers to the (compressed, on-disk) chunk offsets
  bool coffsets_needs_free; //!< Whether the coffsets memory need to be freed or not.
  int64_t len;              //!< The current length of the frame in (compressed) bytes
  int64_t maxlen;           //!< The maximum length of the frame; if 0, there is no maximum
  uint32_t trailer_len;     //!< The current length of the trailer in (compressed) bytes
  bool sframe;              //!< Whether the frame is sparse (true) or not
  blosc2_schunk *schunk;    //!< The schunk associated
  int64_t file_offset;      //!< The offset where the frame starts inside the file
} blosc2_frame_s;


/*********************************************************************
  Frame struct related functions.
  These are rather low-level and the blosc2_schunk interface is
  recommended instead.
*********************************************************************/

/**
 * @brief Create a new frame.
 *
 * @param urlpath The filename of the frame.  If not persistent, pass NULL.
 *
 * @return The new frame.
 */
blosc2_frame_s* frame_new(const char* urlpath);

/**
 * @brief Create a frame from a super-chunk.
 *
 * @param schunk The super-chunk from where the frame will be created.
 * @param frame The pointer for the frame that will be populated.
 *
 * @note If frame->urlpath is NULL, a frame is created in-memory; else it is created
 * on-disk.
 *
 * @return The size in bytes of the frame. If an error occurs it returns a negative value.
 */
int64_t frame_from_schunk(blosc2_schunk* schunk, blosc2_frame_s* frame);

/**
 * @brief Set `avoid_cframe_free` from @param frame to @param avoid_cframe_free.
 *
 * @param frame The frame to set the property to.
 * @param avoid_cframe_free The value to set in @param frame.
 * @warning If you set it to `true` you will be responsible of freeing it.
 */
void frame_avoid_cframe_free(blosc2_frame_s* frame, bool avoid_cframe_free);

/**
 * @brief Free all memory from a frame.
 *
 * @param frame The frame to be freed.
 *
 * @return 0 if succeeds.
 */
int frame_free(blosc2_frame_s *frame);

/**
 * @brief Initialize a frame out of a file.
 *
 * @param urlpath The file name.
 *
 * @return The frame created from the file.
 */
blosc2_frame_s* frame_from_file_offset(const char *urlpath, const blosc2_io *io_cb, int64_t offset);

/**
 * @brief Initialize a frame out of a contiguous frame buffer.
 *
 * @param cframe The buffer for the frame.
 * @param len The length of buffer for the frame.
 * @param copy Whether the frame buffer should be copied internally or not.
 *
 * @return The frame created from the contiguous frame buffer.
 *
 * @note The user is responsible to `free` the returned frame.
 */
blosc2_frame_s* frame_from_cframe(uint8_t *cframe, int64_t len, bool copy);

/**
 * @brief Create a super-chunk from a frame.
 *
 * @param frame The frame from which the super-chunk will be created.
 * @param copy If true, a new frame buffer is created
 * internally to serve as storage for the super-chunk. Else, the
 * super-chunk will be backed by @p frame (i.e. no copies are made).
 *
 * @return The super-chunk corresponding to the frame.
 */
blosc2_schunk* frame_to_schunk(blosc2_frame_s* frame, bool copy, const blosc2_io *udio);

blosc2_storage *
get_new_storage(const blosc2_storage *storage, const blosc2_cparams *cdefaults, const blosc2_dparams *ddefaults,
                const blosc2_io *iodefaults);

void* frame_append_chunk(blosc2_frame_s* frame, void* chunk, blosc2_schunk* schunk);
void* frame_insert_chunk(blosc2_frame_s* frame, int64_t nchunk, void* chunk, blosc2_schunk* schunk);
void* frame_update_chunk(blosc2_frame_s* frame, int64_t nchunk, void* chunk, blosc2_schunk* schunk);
void* frame_delete_chunk(blosc2_frame_s* frame, int64_t nchunk, blosc2_schunk* schunk);
int frame_reorder_offsets(blosc2_frame_s *frame, const int64_t *offsets_order, blosc2_schunk* schunk);

int frame_get_chunk(blosc2_frame_s* frame, int64_t nchunk, uint8_t **chunk, bool *needs_free);
int frame_get_lazychunk(blosc2_frame_s* frame, int64_t nchunk, uint8_t **chunk, bool *needs_free);
int frame_decompress_chunk(blosc2_context* dctx, blosc2_frame_s* frame, int64_t nchunk,
                           void *dest, int32_t nbytes);

int frame_update_header(blosc2_frame_s* frame, blosc2_schunk* schunk, bool new);
int frame_update_trailer(blosc2_frame_s* frame, blosc2_schunk* schunk);

int64_t frame_fill_special(blosc2_frame_s* frame, int64_t nitems, int special_value,
                       int32_t chunksize, blosc2_schunk* schunk);

#endif /* BLOSC_FRAME_H */
