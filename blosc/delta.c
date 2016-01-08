/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2015-12-18

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <string.h>
#include "blosc.h"
#include "delta.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))


/* Apply the delta filters to src.  This can never fail. */
void delta_encoder8(uint8_t* filters_chunk, int32_t offset, int32_t nbytes,
                    uint8_t* src, uint8_t* dest) {
  int i;
  int32_t rbytes = *(int32_t*)(filters_chunk + 4);
  int32_t mbytes;
  uint8_t* dref = (uint8_t*)filters_chunk + BLOSC_MAX_OVERHEAD;

  mbytes = MIN(nbytes, rbytes - offset);

  /* Encode delta */
  for (i = 0; i < mbytes; i++) {
    dest[i] = src[i] - dref[i + offset];
  }

  /* Copy the leftovers */
  if (nbytes > mbytes) {
    mbytes = MAX(0, mbytes); 	/* negative mbytes are not considered */
    memcpy(dest + mbytes, src + mbytes, nbytes - mbytes);
  }
}


/* Undo the delta filter in dest.  This can never fail. */
void delta_decoder8(uint8_t* filters_chunk, int32_t offset, int32_t nbytes, uint8_t* dest) {
  int i;
  uint8_t* dref = (uint8_t*)filters_chunk + BLOSC_MAX_OVERHEAD;
  int32_t rbytes = *(int32_t*)(filters_chunk + 4);
  int32_t mbytes;

  mbytes = MIN(nbytes, rbytes - offset);

  /* Decode delta */
  for (i = 0; i < mbytes; i++) {
    dest[i] += dref[i + offset];
  }

  /* The leftovers are in-place already */

}
