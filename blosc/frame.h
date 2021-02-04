/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: The Blosc Developers <blosc@blosc.org>

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_FRAME_H
#define BLOSC_FRAME_H

#include <stdio.h>
#include <stdint.h>

// Constants for metadata placement in header
#define FRAME_HEADER_MAGIC 2
#define FRAME_HEADER_LEN (FRAME_HEADER_MAGIC + 8 + 1)  // 11
#define FRAME_LEN (FRAME_HEADER_LEN + 4 + 1)  // 16
#define FRAME_FLAGS (FRAME_LEN + 8 + 1)  // 25
#define FRAME_CODECS (FRAME_FLAGS + 2)  // 27
#define FRAME_NBYTES (FRAME_FLAGS + 4 + 1)  // 30
#define FRAME_CBYTES (FRAME_NBYTES + 8 + 1)  // 39
#define FRAME_TYPESIZE (FRAME_CBYTES + 8 + 1) // 48
#define FRAME_CHUNKSIZE (FRAME_TYPESIZE + 4 + 1)  // 53
#define FRAME_NTHREADS_C (FRAME_CHUNKSIZE + 4 + 1)  // 58
#define FRAME_NTHREADS_D (FRAME_NTHREADS_C + 2 + 1)  // 61
#define FRAME_HAS_USERMETA (FRAME_NTHREADS_D + 2)  // 63
#define FRAME_FILTER_PIPELINE (FRAME_HAS_USERMETA + 1 + 1) // 65
#define FRAME_HEADER_MINLEN (FRAME_FILTER_PIPELINE + 1 + 16)  // 82 <- minimum length
#define FRAME_METALAYERS (FRAME_HEADER_MINLEN)  // 82
#define FRAME_IDX_SIZE (FRAME_METALAYERS + 1 + 1)  // 84

#define FRAME_FILTER_PIPELINE_MAX (8)  // the maximum number of filters that can be stored in header

#define FRAME_TRAILER_VERSION_BETA2 (0U)  // for beta.2 and former
#define FRAME_TRAILER_VERSION (1U)        // can be up to 127

#define FRAME_TRAILER_USERMETA_LEN_OFFSET (3)  // offset to usermeta length
#define FRAME_TRAILER_USERMETA_OFFSET (7)  // offset to usermeta chunk
#define FRAME_TRAILER_MINLEN (30)  // minimum length for the trailer (msgpack overhead)
#define FRAME_TRAILER_LEN_OFFSET (22)  // offset to trailer length (counting from the end)


typedef struct {
  char* urlpath;            //!< The name of the file or directory if it's an eframe; if NULL, this is in-memory
  uint8_t* framebuf;        //!< The in-memory frame buffer
  bool avoid_framebuf_free; //!< Whether the framebuf can be freed (false) or not (true).
  uint8_t* coffsets;        //!< Pointers to the (compressed, on-disk) chunk offsets
  int64_t len;              //!< The current length of the frame in (compressed) bytes
  int64_t maxlen;           //!< The maximum length of the frame; if 0, there is no maximum
  uint32_t trailer_len;     //!< The current length of the trailer in (compressed) bytes
  bool eframe;              //!< Whether the frame is extended (sparse, on-disk)
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
blosc2_frame_s* frame_from_file(const char *urlpath);

/**
 * @brief Initialize a frame out of a frame buffer.
 *
 * @param buffer The buffer for the frame.
 * @param len The length of buffer for the frame.
 * @param copy Whether the frame buffer should be copied internally or not.
 *
 * @return The frame created from the frame buffer.
 */
blosc2_frame_s* frame_from_framebuf(uint8_t *framebuf, int64_t len, bool copy);

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
blosc2_schunk* frame_to_schunk(blosc2_frame_s* frame, bool copy);

blosc2_storage* get_new_storage(const blosc2_storage* storage, const blosc2_cparams* cdefaults,
                                const blosc2_dparams* ddefaults);

void* frame_append_chunk(blosc2_frame_s* frame, void* chunk, blosc2_schunk* schunk);
void* frame_insert_chunk(blosc2_frame_s* frame, int nchunk, void* chunk, blosc2_schunk* schunk);
void* frame_update_chunk(blosc2_frame_s* frame, int nchunk, void* chunk, blosc2_schunk* schunk);
int frame_reorder_offsets(blosc2_frame_s *frame, int *offsets_order, blosc2_schunk* schunk);

int frame_get_chunk(blosc2_frame_s* frame, int nchunk, uint8_t **chunk, bool *needs_free);
int frame_get_lazychunk(blosc2_frame_s* frame, int nchunk, uint8_t **chunk, bool *needs_free);
int frame_decompress_chunk(blosc2_context* dctx, blosc2_frame_s* frame, int nchunk,
                           void *dest, int32_t nbytes);

int frame_update_header(blosc2_frame_s* frame, blosc2_schunk* schunk, bool new);
int frame_update_trailer(blosc2_frame_s* frame, blosc2_schunk* schunk);

#endif //BLOSC_FRAME_H
