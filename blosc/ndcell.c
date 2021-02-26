/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Author: Oscar Griñón <oscar@blosc.org>
  Author: Aleix Alcacer <aleix@blosc.org>
  Creation date: 2020-06-12

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*********************************************************************
  This codec is meant to leverage multidimensionality for getting
  better compression ratios.  The idea is to look for similarities
  in places that are closer in a euclidean metric, not the typical
  linear one.
**********************************************************************/

#define XXH_NAMESPACE ndcell

#define XXH_INLINE_ALL
#include <stdio.h>
#include <ndcell.h>
#include <math.h>
#include <blosc2.h>
#include <blosc-private.h>

/*
 * Give hints to the compiler for branch prediction optimization.
 */
#if defined(__GNUC__) && (__GNUC__ > 2)
#define NDCELL_EXPECT_CONDITIONAL(c)    (__builtin_expect((c), 1))
#define NDCELL_UNEXPECT_CONDITIONAL(c)  (__builtin_expect((c), 0))
#else
#define NDCELL_EXPECT_CONDITIONAL(c)    (c)
#define NDCELL_UNEXPECT_CONDITIONAL(c)  (c)
#endif

/*
 * Use inlined functions for supported systems.
 */
#if defined(_MSC_VER) && !defined(__cplusplus)   /* Visual Studio */
#define inline __inline  /* Visual C is not C99, but supports some kind of inline */
#endif

#define MAX_COPY 32U
#define MAX_DISTANCE 65535
#define CATERVA_MAX_DIM 8


#ifdef BLOSC_STRICT_ALIGN
  #define NDCELL_READU16(p) ((p)[0] | (p)[1]<<8)
  #define NDCELL_READU32(p) ((p)[0] | (p)[1]<<8 | (p)[2]<<16 | (p)[3]<<24)
#else
  #define NDCELL_READU16(p) *((const uint16_t*)(p))
  #define NDCELL_READU32(p) *((const uint32_t*)(p))
#endif

#define HASH_LOG (12)


static void index_unidim_to_multidim(int8_t ndim, int64_t *shape, int64_t i, int64_t *index) {
  int64_t strides[CATERVA_MAX_DIM];
  strides[ndim - 1] = 1;
  for (int j = ndim - 2; j >= 0; --j) {
    strides[j] = shape[j + 1] * strides[j + 1];
  }

  index[0] = i / strides[0];
  for (int j = 1; j < ndim; ++j) {
    index[j] = (i % strides[j - 1]) / strides[j];
  }
}
static void swap_store(void *dest, const void *pa, int size) {
  uint8_t *pa_ = (uint8_t *) pa;
  uint8_t *pa2_ = malloc((size_t) size);
  int i = 1; /* for big/little endian detection */
  char *p = (char *) &i;

  if (p[0] == 1) {
    /* little endian */
    switch (size) {
      case 8:
        pa2_[0] = pa_[7];
        pa2_[1] = pa_[6];
        pa2_[2] = pa_[5];
        pa2_[3] = pa_[4];
        pa2_[4] = pa_[3];
        pa2_[5] = pa_[2];
        pa2_[6] = pa_[1];
        pa2_[7] = pa_[0];
        break;
      case 4:
        pa2_[0] = pa_[3];
        pa2_[1] = pa_[2];
        pa2_[2] = pa_[1];
        pa2_[3] = pa_[0];
        break;
      case 2:
        pa2_[0] = pa_[1];
        pa2_[1] = pa_[0];
        break;
      case 1:
        pa2_[0] = pa_[0];
        break;
      default:
        fprintf(stderr, "Unhandled nitems: %d\n", size);
    }
  }
  memcpy(dest, pa2_, size);
  free(pa2_);
}


static int32_t deserialize_meta(uint8_t *smeta, uint32_t smeta_len, int8_t *ndim, int64_t *shape,
                                int32_t *chunkshape, int32_t *blockshape) {
  uint8_t *pmeta = smeta;

  // Check that we have an array with 5 entries (version, ndim, shape, chunkshape, blockshape)
  pmeta += 1;

  // version entry
  int8_t version = pmeta[0];  // positive fixnum (7-bit positive integer)
  pmeta += 1;

  // ndim entry
  *ndim = pmeta[0];
  int8_t ndim_aux = *ndim;  // positive fixnum (7-bit positive integer)
  pmeta += 1;

  // shape entry
  // Initialize to ones, as required by Caterva
  for (int i = 0; i < CATERVA_MAX_DIM; i++) shape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(shape + i, pmeta, sizeof(int64_t));
    pmeta += sizeof(int64_t);
  }

  // chunkshape entry
  // Initialize to ones, as required by Caterva
  for (int i = 0; i < CATERVA_MAX_DIM; i++) chunkshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(chunkshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }

  // blockshape entry
  // Initialize to ones, as required by Caterva
  for (int i = 0; i < CATERVA_MAX_DIM; i++) blockshape[i] = 1;
  pmeta += 1;
  for (int8_t i = 0; i < ndim_aux; i++) {
    pmeta += 1;
    swap_store(blockshape + i, pmeta, sizeof(int32_t));
    pmeta += sizeof(int32_t);
  }
  uint32_t slen = (uint32_t)(pmeta - smeta);
  return 0;
}


int ndcell_encoder(blosc2_context* context, const void* input, int length, void* output) {

  uint8_t* content;
  uint32_t content_len;
  int nmetalayer = blosc2_meta_get(context->schunk, "caterva", &content, &content_len);
  if (nmetalayer < 0) {
    BLOSC_TRACE_ERROR("Metalayer \"caterva\" not found.");
    return nmetalayer;
  }
  int8_t ndim;
  int64_t shape[CATERVA_MAX_DIM];
  int32_t chunkshape[CATERVA_MAX_DIM];
  int32_t blockshape[CATERVA_MAX_DIM];
  deserialize_meta(content, content_len, &ndim, shape, chunkshape, blockshape);

  int filter_ind;
  for (filter_ind = 0; filter_ind < BLOSC2_MAX_FILTERS; filter_ind++) {
    if (context->filters[filter_ind] == BLOSC_NDCELL) {
      break;
    }
  }
  uint8_t cell_shape = context->filters_meta[filter_ind];
  const int cell_size = (int) pow(cell_shape, ndim);
  int32_t typesize = context->typesize;

  if (NDCELL_UNEXPECT_CONDITIONAL(length != (blockshape[0] * blockshape[1] * typesize))) {
    printf("Length not equal to blocksize %d %d %d \n", length, blockshape[0], blockshape[1]);
    return -1;
  }

  uint8_t* ip = (uint8_t *) input;
  uint8_t* op = (uint8_t *) output;
  uint8_t* op_limit = op + length;

  /* input and output buffer cannot be less than cell size */
  if (NDCELL_UNEXPECT_CONDITIONAL(length < cell_size * typesize)) {
    printf("Incorrect length");
    return 0;
  }

  uint8_t* obase = op;

  int64_t i_shape[ndim];
  for (int i = 0; i < ndim; ++i) {
    i_shape[i] = (blockshape[i] + cell_shape - 1) / cell_shape;
  }

  int64_t ncells = 1;
  for (int i = 0; i < ndim; ++i) {
    ncells *= i_shape[i];
  }

  /* main loop */
  int64_t pad_shape[ndim];
  int64_t ii[ndim];
  for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell
    index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
    uint32_t orig = 0;
    int64_t nd_aux = cell_shape;
    for (int i = ndim - 1; i >= 0; i--) {
      orig += ii[i] * nd_aux;
      nd_aux *= blockshape[i];
    }

    for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
      if ((blockshape[dim_ind] % cell_shape != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
        pad_shape[dim_ind] = blockshape[dim_ind] % cell_shape;
      } else {
        pad_shape[dim_ind] = cell_shape;
      }
    }
    int64_t ncopies = 1;
    for (int i = 0; i < ndim - 1; ++i) {
      ncopies *= pad_shape[i];
    }
    int64_t kk[ndim];
    for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
      index_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
      nd_aux = blockshape[ndim - 1];
      int64_t ind = orig;
      for (int i = ndim - 2; i >= 0; i--) {
        ind += kk[i] * nd_aux;
        nd_aux *= blockshape[i];
      }
      memcpy(op, &ip[ind * typesize], pad_shape[ndim - 1] * typesize);
      op += pad_shape[ndim - 1] * typesize;
    }

    if (NDCELL_UNEXPECT_CONDITIONAL(op > op_limit)) {
      printf("Output too big");
      return 0;
    }
  }

  if((op - obase) != length) {
    printf("Output size must be equal to input size \n");
    return 0;
  }

  free(content);
  return (int)(op - obase);
}


// See https://habr.com/en/company/yandex/blog/457612/
#ifdef __AVX2__

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#endif
#endif
#define ALIGNED_TYPE_(t, x) t ALIGNED_(x)

static unsigned char* copy_match_16(unsigned char *op, const unsigned char *match, int32_t len)
{
  size_t offset = op - match;
  while (len >= 16) {

    static const ALIGNED_TYPE_(uint8_t, 16) masks[] =
      {
                0,  1,  2,  1,  4,  1,  4,  2,  8,  7,  6,  5,  4,  3,  2,  1, // offset = 0, not used as mask, but for shift
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // offset = 1
                0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,  0,  1,
                0,  1,  2,  0,  1,  2,  0,  1,  2,  0,  1,  2,  0,  1,  2,  0,
                0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,  1,  2,  3,  4,  0,
                0,  1,  2,  3,  4,  5,  0,  1,  2,  3,  4,  5,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  5,  6,  0,  1,  2,  3,  4,  5,  6,  0,  1,
                0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  0,  1,  2,  3,  4,  5,  6,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  1,  2,  3,  4,  5,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,  0,  1,  2,  3,  4,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12,  0,  1,  2,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13,  0,  1,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  0,
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  15, // offset = 16
      };

    _mm_storeu_si128((__m128i *)(op),
                     _mm_shuffle_epi8(_mm_loadu_si128((const __m128i *)(match)),
                                      _mm_load_si128((const __m128i *)(masks) + offset)));

    match += masks[offset];

    op += 16;
    len -= 16;
  }
  // Deal with remainders
  for (; len > 0; len--) {
    *op++ = *match++;
  }
  return op;
}
#endif


int ndcell_decoder(blosc2_context* context, const void* input, int length, void* output) {

  uint8_t* content;
  uint32_t content_len;
  int nmetalayer = blosc2_meta_get(context->schunk, "caterva", &content, &content_len);
  if (nmetalayer < 0) {
    BLOSC_TRACE_ERROR("Metalayer \"caterva\" not found.");
    return nmetalayer;
  }
  int8_t ndim;
  int64_t shape[CATERVA_MAX_DIM];
  int32_t chunkshape[CATERVA_MAX_DIM];
  int32_t blockshape[CATERVA_MAX_DIM];
  deserialize_meta(content, content_len, &ndim, shape, chunkshape, blockshape);

  int filter_ind;
  for (filter_ind = 0; filter_ind < BLOSC2_MAX_FILTERS; filter_ind++) {
    if (context->filters[filter_ind] == BLOSC_NDCELL) {
      break;
    }
  }
  uint8_t cell_shape = context->filters_meta[filter_ind];
  const int cell_size = (int) pow(cell_shape, ndim);
  int32_t typesize = context->typesize;

  uint8_t* ip = (uint8_t*)input;
  uint8_t* ip_limit = ip + length;
  uint8_t* op = (uint8_t*)output;

  if (NDCELL_UNEXPECT_CONDITIONAL(length != (blockshape[0] * blockshape[1] * typesize))) {
    printf("Length not equal to blocksize \n");
    return -1;
  }

  /* input and output buffer cannot be less than cell size */
  if (NDCELL_UNEXPECT_CONDITIONAL(length < cell_size * typesize)) {
    printf("Incorrect length");
    return 0;
  }

  int64_t i_shape[ndim];
  for (int i = 0; i < ndim; ++i) {
    i_shape[i] = (blockshape[i] + cell_shape - 1) / cell_shape;
  }

  int64_t ncells = 1;
  for (int i = 0; i < ndim; ++i) {
    ncells *= i_shape[i];
  }

  /* main loop */
  int64_t pad_shape[ndim];
  int64_t ii[ndim];
  int32_t ind;
  for (int cell_ind = 0; cell_ind < ncells; cell_ind++) {      // for each cell

    if (NDCELL_UNEXPECT_CONDITIONAL(ip > ip_limit)) {
      printf("Literal copy \n");
      return 0;
    }
    index_unidim_to_multidim(ndim, i_shape, cell_ind, ii);
    uint32_t orig = 0;
    int64_t nd_aux = cell_shape;
    for (int i = ndim - 1; i >= 0; i--) {
      orig += ii[i] * nd_aux;
      nd_aux *= blockshape[i];
    }

    for (int dim_ind = 0; dim_ind < ndim; dim_ind++) {
      if ((blockshape[dim_ind] % cell_shape != 0) && (ii[dim_ind] == i_shape[dim_ind] - 1)) {
        pad_shape[dim_ind] = blockshape[dim_ind] % cell_shape;
      } else {
        pad_shape[dim_ind] = cell_shape;
      }
    }

    int64_t ncopies = 1;
    for (int i = 0; i < ndim - 1; ++i) {
      ncopies *= pad_shape[i];
    }
    int64_t kk[ndim];
    for (int copy_ind = 0; copy_ind < ncopies; ++copy_ind) {
      index_unidim_to_multidim(ndim - 1, pad_shape, copy_ind, kk);
      nd_aux = blockshape[ndim - 1];
      ind = orig;
      for (int i = ndim - 2; i >= 0; i--) {
        ind += kk[i] * nd_aux;
        nd_aux *= blockshape[i];
      }
      memcpy(&op[ind * typesize], ip, pad_shape[ndim - 1] * typesize);
      ip += pad_shape[ndim - 1] * typesize;
    }
  }
  ind += pad_shape[ndim - 1];


  if (ind != (int32_t) (blockshape[0] * blockshape[1])) {
    printf("Output size is not compatible with embeded blockshape ind %d, %d \n", ind, (blockshape[0] * blockshape[1] * typesize));
    return 0;
  }

  free(content);
  return ind;
}
