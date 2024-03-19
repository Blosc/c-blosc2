/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

/*
 * Benchmark to measure the retrieval time of a number of elements in random positions
 * in b2nd arrays. To get the necessary arrays (air1.cat, precip1.cat, snow1.cat...)
 * you can use the following script, changing the dataset by your preference (see
 * https://docs.digitalearthafrica.org/fr/latest/sandbox/notebooks/Datasets/Climate_Data_ERA5_AWS.html):

8<---snip---- "fetch_data.py"
#!/usr/bin/env python
import numpy as np
import s3fs
import xarray as xr
import blosc2

def open_zarr(year, month, datestart, dateend):
    fs = s3fs.S3FileSystem(anon=True)
    datestring = "era5-pds/zarr/{year}/{month:02d}/data/".format(year=year, month=month)
    s3map = s3fs.S3Map(datestring + "precipitation_amount_1hour_Accumulation.zarr/", s3=fs)
    precip_zarr = xr.open_dataset(s3map, engine="zarr")
    precip_zarr = precip_zarr.sel(time1=slice(np.datetime64(datestart), np.datetime64(dateend)))
    return precip_zarr.precipitation_amount_1hour_Accumulation

print("Fetching data from S3 (era5-pds)...")
precip_m0 = open_zarr(1987, 10, "1987-10-01", "1987-10-30 23:59")
precip0 = blosc2.empty(shape=precip_m0.shape, dtype=precip_m0.dtype, urlpath="precip1.b2nd")
print("Fetching and storing 1st month...")
values = precip_m0.values
precip0[:] = values
8<---snip----

 * To call this script, you can run the following commands:
 * $ pip install blosc2
 * $ python fetch_data.py
 *
 */

#include "../plugins/codecs/zfp/zfp-private.h"
#include "../../plugins/codecs/zfp/blosc2-zfp.h"
#include "context.h"
#include "blosc2/codecs-registry.h"
#include "b2nd.h"
#include "blosc2.h"

int comp(const char *urlpath) {
  blosc2_init();

  blosc2_schunk *schunk = blosc2_schunk_open(urlpath);

  if (schunk->typesize != 4) {
    printf("Error: This test is only for floats.\n");
    return -1;
  }

  blosc2_remove_urlpath("schunk_rate.cat");
  blosc2_remove_urlpath("schunk.cat");

  // Get multidimensional parameters and configure Blosc2 NDim array
  int8_t ndim;
  int64_t shape[4];
  int64_t shape_aux[4];
  int32_t chunkshape[4];
  int32_t blockshape[4];
  uint8_t *smeta;
  int32_t smeta_len;
  if (blosc2_meta_get(schunk, "b2nd", &smeta, &smeta_len) < 0) {
    printf("This benchmark only supports b2nd arrays");
    return -1;
  }
  char *dtype;
  int8_t dtype_format;
  b2nd_deserialize_meta(smeta, smeta_len, &ndim, shape_aux, chunkshape, blockshape, &dtype, &dtype_format);
  free(smeta);
  free(dtype);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 6;

  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};
  b2_storage.urlpath = "schunk_rate.cat";

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, ndim, shape_aux, chunkshape, blockshape, NULL, 0,
                                        NULL, 0);

  b2nd_array_t *arr;
  b2nd_from_schunk(schunk, &arr);
  int copied;
  printf("LZ4 comp ratio: %f \n", (float) arr->sc->nbytes / (float) arr->sc->cbytes);

  /* Use BLOSC_CODEC_ZFP_FIXED_RATE */
  b2nd_array_t *arr_rate;
  blosc2_context *ctx_zfp = blosc2_create_cctx(cparams);
  ctx_zfp->compcode = BLOSC_CODEC_ZFP_FIXED_RATE;
  ctx_zfp->splitmode = BLOSC_NEVER_SPLIT;
  ctx_zfp->compcode_meta = (uint8_t) (100.0 * (float) arr->sc->cbytes / (float) arr->sc->nbytes);
  ctx_zfp->filters[BLOSC2_MAX_FILTERS - 1] = 0;
  ctx_zfp->filters_meta[BLOSC2_MAX_FILTERS - 1] = 0;
  copied = b2nd_copy(ctx, arr, &arr_rate);
  if (copied != 0) {
    printf("Error BLOSC_CODEC_ZFP_FIXED_RATE \n");
    b2nd_free(arr);
    return -1;
  }
  printf("ZFP_FIXED_RATE comp ratio: %f \n", (float) arr_rate->sc->nbytes / (float) arr_rate->sc->cbytes);

  int64_t nelems = arr_rate->nitems;
  int dsize_zfp, dsize_blosc;
  int64_t index;
  float item_zfp, item_blosc;
  blosc_timestamp_t t0, t1;
  double zfp_time, blosc_time;
  zfp_time = blosc_time = 0;
  int64_t index_ndim[ZFP_MAX_DIM];
  int64_t index_chunk_ndim[ZFP_MAX_DIM];
  int64_t ind_ndim[ZFP_MAX_DIM];
  int32_t stride_chunk, ind_chunk;
  int64_t nchunk;
  bool needs_free_blosc, needs_free_zfp;
  uint8_t *chunk_blosc, *chunk_zfp;
  int32_t chunk_nbytes_zfp, chunk_cbytes_zfp, chunk_nbytes_lossy, chunk_cbytes_lossy;
  double ntests = 500.0;
  for (int i = 0; i < ntests; ++i) {
    srand(i);
    index = rand() % nelems;
    blosc2_unidim_to_multidim(ndim, shape, index, index_ndim);
    for (int j = 0; j < ndim; ++j) {
      index_chunk_ndim[j] = index_ndim[j] / chunkshape[j];
      ind_ndim[j] = index_ndim[j] % chunkshape[j];
    }
    stride_chunk = (int32_t)(shape[1] - 1) / chunkshape[1] + 1;
    nchunk = index_chunk_ndim[0] * stride_chunk + index_chunk_ndim[1];
    ind_chunk = (int32_t) (ind_ndim[0] * chunkshape[1] + ind_ndim[1]);
    blosc2_schunk_get_lazychunk(arr->sc, nchunk, &chunk_blosc, &needs_free_blosc);
    blosc2_cbuffer_sizes(chunk_blosc, &chunk_nbytes_lossy, &chunk_cbytes_lossy, NULL);
    blosc_set_timestamp(&t0);
    dsize_blosc = blosc2_getitem_ctx(arr->sc->dctx, chunk_blosc, chunk_cbytes_lossy,
                                     ind_chunk, 1, &item_blosc, sizeof(item_blosc));
    blosc_set_timestamp(&t1);
    blosc_time += blosc_elapsed_secs(t0, t1);
    blosc2_schunk_get_lazychunk(arr_rate->sc, nchunk, &chunk_zfp, &needs_free_zfp);
    blosc2_cbuffer_sizes(chunk_zfp, &chunk_nbytes_zfp, &chunk_cbytes_zfp, NULL);
    blosc_set_timestamp(&t0);
    dsize_zfp = blosc2_getitem_ctx(arr_rate->sc->dctx, chunk_zfp, chunk_cbytes_zfp,
                                   ind_chunk, 1, &item_zfp, sizeof(item_zfp));
    blosc_set_timestamp(&t1);
    zfp_time += blosc_elapsed_secs(t0, t1);
    if (dsize_blosc != dsize_zfp) {
      printf("Different amount of items gotten");
      return -1;
    }
  }
  printf("ZFP_FIXED_RATE time: %.5f microseconds\n", (zfp_time * 1000000.0 / ntests));
  printf("Blosc2 time: %.5f microseconds\n", (blosc_time * 1000000.0 / ntests));

  b2nd_free(arr);
  b2nd_free(arr_rate);
  BLOSC_ERROR(b2nd_free_ctx(ctx));
  blosc2_free_ctx(ctx_zfp);
  if (needs_free_blosc) {
    free(chunk_blosc);
  }
  if (needs_free_zfp) {
    free(chunk_zfp);
  }
  blosc2_destroy();

  return BLOSC2_ERROR_SUCCESS;
}

int solar1(void) {
  const char *urlpath = "../../bench/solar1.cat";

  int result = comp(urlpath);
  return result;
}

int air1(void) {
  const char *urlpath = "../../bench/air1.cat";

  int result = comp(urlpath);
  return result;
}

int snow1(void) {
  const char *urlpath = "../../bench/snow1.cat";

  int result = comp(urlpath);
  return result;
}

int wind1(void) {
  const char *urlpath = "../../bench/wind1.cat";

  int result = comp(urlpath);
  return result;
}

int precip1(void) {
  const char *urlpath = "../../bench/precip1.cat";

  int result = comp(urlpath);
  return result;
}

int precip2(void) {
  const char *urlpath = "../../bench/precip2.cat";

  int result = comp(urlpath);
  return result;
}

int precip3(void) {
  const char *urlpath = "../../bench/precip3.cat";

  int result = comp(urlpath);
  return result;
}

int precip3m(void) {
  const char *urlpath = "../../bench/precip-3m.cat";

  int result = comp(urlpath);
  return result;
}


int main() {

  printf("wind1 \n");
  BLOSC_ERROR(wind1());
  printf("air1 \n");
  BLOSC_ERROR(air1());
  printf("solar1 \n");
  BLOSC_ERROR(solar1());
  printf("snow1 \n");
  BLOSC_ERROR(snow1());
  printf("precip1 \n");
  BLOSC_ERROR(precip1());
  printf("precip2 \n");
  BLOSC_ERROR(precip2());
  printf("precip3 \n");
  BLOSC_ERROR(precip3());
//    printf("precip3m \n");
  //  BLOSC_ERROR(precip3m());
  return BLOSC2_ERROR_SUCCESS;

}
