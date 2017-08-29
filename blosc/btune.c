/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2017-08-29

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdio.h>
#include "btune.h"


/* Whether a codec is meant for High Compression Ratios */
/* Includes LZ4 + BITSHUFFLE here, but not BloscLZ + BITSHUFFLE because,
   for some reason, the latter does not work too well */
int HCR(blosc2_context *context) {
  switch (context->compcode) {
    case BLOSC_BLOSCLZ :
      return 0;
    case BLOSC_LZ4 :
      return (context->filter_flags & BLOSC_DOBITSHUFFLE) ? 1 : 0;
    case BLOSC_LZ4HC :
      return 1;
    case BLOSC_LIZARD :
      return 1;
    case BLOSC_ZLIB :
      return 1;
    case BLOSC_ZSTD :
      return 1;
    default :
      fprintf(stderr, "Error in HCR: codec %d not handled\n",
              context->compcode);
  }
  return 0;
}


/* Tune some compression parameters based in the context */
void btune_cparams(blosc2_context* context) {
  int32_t clevel = context->clevel;
  size_t typesize = context->typesize;
  size_t nbytes = context->sourcesize;
  size_t user_blocksize = context->blocksize;
  size_t blocksize = nbytes;

  /* Protection against very small buffers */
  if (nbytes < typesize) {
    context->blocksize = 1;
    return;
  }

  if (user_blocksize) {
    blocksize = user_blocksize;
    /* Check that forced blocksize is not too small */
    if (blocksize < BLOSC_MIN_BUFFERSIZE) {
      blocksize = BLOSC_MIN_BUFFERSIZE;
    }
  }
  else if (nbytes >= L1) {
    blocksize = L1;

    /* For HCR codecs, increase the block sizes by a factor of 2 because they
       are meant for compressing large blocks (i.e. they show a big overhead
       when compressing small ones). */
    if (HCR(context)) {
      blocksize *= 2;
    }

    /* Choose a different blocksize depending on the compression level */
    switch (clevel) {
      case 0:
        /* Case of plain copy */
        blocksize /= 4;
        break;
      case 1:
      case 2:
      case 3:
      case 4:
        blocksize *= 1;
        break;
      case 5:
        blocksize *= 2;
        break;
      case 6:
        blocksize *= 4;
        break;
      case 7:
      case 8:
        blocksize *= 8;
        break;
      case 9:
        /* Do not exceed 256 KB for non HCR codecs */
        blocksize *= 8;
        if (HCR(context)) {
          blocksize *= 2;
        }
        break;
      default:
        break;
    }
  }

  /* Check that blocksize is not too large */
  if (blocksize > (int32_t)nbytes) {
    blocksize = nbytes;
  }

  /* blocksize *must absolutely* be a multiple of the typesize */
  if (blocksize > typesize) {
    blocksize = blocksize / typesize * typesize;
  }

  context->blocksize = blocksize;
}
