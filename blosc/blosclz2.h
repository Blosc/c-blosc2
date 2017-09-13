#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BLOSCLZ2_VERSION_MAJOR    1   /* for major interface/format changes  */
#define BLOSCLZ2_VERSION_MINOR    0   /* for minor interface/format changes  */
#define BLOSCLZ2_VERSION_RELEASE  0   /* for tweaks, bug-fixes, or development */


/* Returns the minimum size for workdict passed to blosclz2_compress */
extern uint32_t blosclz2_get_workdict_size();

/* Compress `in` buffer with `length` bytes into `out` with `maxout` bytes.
 *
 * Returns the size of the compressed data or -1 if not compressible */
extern int32_t blosclz2_compress(const uint8_t* in, int32_t length,
                                 uint8_t* out, int32_t maxout,
                                 uint8_t* workdict);

/* Decompress `in` into `out`.
 *
 * Returns the number of decompressed bytes.  0 if decompressed buffer is
 * larger than maxout. */
extern int32_t blosclz2_decompress(const uint8_t* in, uint8_t* out,
                                   int32_t maxout);


/*! Example Usage
	uint8_t* workdict = ( uint8_t* )malloc( blosclz2_get_workdict_size() );
	int32_t compressed_size = blosclz2_compress( decompressed, decompressed_size,
                                               compressed, maxout, workdict);

	....

	blosclz2_decompress( compressed, decompressed, maxout );
*/


#ifdef __cplusplus
}
#endif
