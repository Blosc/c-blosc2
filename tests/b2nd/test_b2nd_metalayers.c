/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "test_common.h"


CUTEST_TEST_SETUP(metalayers) {
  blosc2_init();

  // Add parametrizations
  CUTEST_PARAMETRIZE(typesize, uint8_t, CUTEST_DATA(1, 2, 4, 8));
  CUTEST_PARAMETRIZE(shapes, _test_shapes, CUTEST_DATA(
      {0, {0}, {0}, {0}}, // 0-dim
      {1, {10}, {7}, {2}}, // 1-idim
      {2, {100, 100}, {20, 20}, {10, 10}},
  ));
  CUTEST_PARAMETRIZE(backend, _test_backend, CUTEST_DATA(
      {true, true},
      {false, true},
  ));
}


CUTEST_TEST_TEST(metalayers) {
  CUTEST_GET_PARAMETER(shapes, _test_shapes);
  CUTEST_GET_PARAMETER(typesize, uint8_t);
  CUTEST_GET_PARAMETER(backend, _test_backend);

  char *urlpath = "test_metalayers.b2nd";
  blosc2_remove_urlpath(urlpath);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 2;
  cparams.typesize = typesize;
  blosc2_storage b2_storage = {.cparams=&cparams};
  if (backend.persistent) {
    b2_storage.urlpath = urlpath;
  }
  b2_storage.contiguous = backend.contiguous;

  int8_t nmetalayers = 1;
  blosc2_metalayer metalayers[B2ND_MAX_METALAYERS];
  blosc2_metalayer *meta0 = &metalayers[0];
  meta0->name = "test_meta";
  meta0->content_len = 3;
  double sdata0 = 5.789;
  meta0->content = (uint8_t *) &sdata0;
  meta0->content_len = sizeof(sdata0);

  b2nd_context_t *ctx = b2nd_create_ctx(&b2_storage, shapes.ndim, shapes.shape,
                                        shapes.chunkshape, shapes.blockshape, NULL, 0, metalayers, nmetalayers);

  /* Create original data */
  size_t buffersize = typesize;
  for (int i = 0; i < ctx->ndim; ++i) {
    buffersize *= (size_t) ctx->shape[i];
  }

  uint8_t *buffer = malloc(buffersize);
  CUTEST_ASSERT("Buffer filled incorrectly", fill_buf(buffer, typesize, buffersize / typesize));

  /* Create b2nd_array_t with original data */
  b2nd_array_t *src;
  B2ND_TEST_ASSERT(b2nd_from_cbuffer(ctx, &src, buffer, buffersize));

  blosc2_metalayer vlmeta1;

  uint64_t sdata1 = 56;
  vlmeta1.name = "vlmeta1";
  vlmeta1.content = (uint8_t *) &sdata1;
  vlmeta1.content_len = sizeof(sdata1);

  B2ND_TEST_ASSERT(blosc2_vlmeta_add(src->sc, vlmeta1.name, vlmeta1.content, vlmeta1.content_len,
                                     src->sc->storage->cparams));

  int rc = blosc2_vlmeta_exists(src->sc, "vlmeta2");
  CUTEST_ASSERT("", rc < 0);
  rc = blosc2_vlmeta_exists(src->sc, vlmeta1.name);
  CUTEST_ASSERT("", rc == 0);

  uint8_t *content;
  int32_t content_len;
  B2ND_TEST_ASSERT(blosc2_vlmeta_get(src->sc, vlmeta1.name, &content, &content_len));
  CUTEST_ASSERT("Contents are not equal",
                *((uint64_t *) vlmeta1.content) == *((uint64_t *) content));
  CUTEST_ASSERT("Sizes are not equal", vlmeta1.content_len == content_len);
  free(content);

  float sdata11 = 4.5f;
  vlmeta1.content = (uint8_t *) &sdata11;
  vlmeta1.content_len = sizeof(sdata11);

  B2ND_TEST_ASSERT(blosc2_vlmeta_update(src->sc, vlmeta1.name, vlmeta1.content, vlmeta1.content_len,
                                        src->sc->storage->cparams));

  B2ND_TEST_ASSERT(blosc2_vlmeta_get(src->sc, vlmeta1.name, &content, &content_len));
  CUTEST_ASSERT("Contents are not equal", *((float *) vlmeta1.content) == *((float *) content));
  CUTEST_ASSERT("Sizes are not equal", vlmeta1.content_len == content_len);
  free(content);

  blosc2_metalayer vlmeta2;
  vlmeta2.name = "vlmeta2";
  vlmeta2.content = (uint8_t *) &sdata1;
  vlmeta2.content_len = sizeof(sdata1);
  B2ND_TEST_ASSERT(blosc2_vlmeta_add(src->sc, vlmeta2.name, vlmeta2.content, vlmeta2.content_len,
                                     src->sc->storage->cparams));
  B2ND_TEST_ASSERT(b2nd_free(src));

  b2nd_array_t *src2;
  b2nd_open(urlpath, &src2);

  B2ND_TEST_ASSERT(blosc2_vlmeta_get(src2->sc, vlmeta2.name, &content, &content_len));
  CUTEST_ASSERT("Contents are not equal", *((uint64_t *) vlmeta2.content) == *((uint64_t *) content));
  CUTEST_ASSERT("Sizes are not equal", vlmeta2.content_len == content_len);
  free(content);

  sdata0 = 1e-10;
  blosc2_metalayer meta1;
  meta1.name = meta0->name;
  meta1.content = (uint8_t *) &sdata0;
  meta1.content_len = sizeof(sdata0);

  rc = blosc2_meta_exists(src2->sc, meta0->name);
  CUTEST_ASSERT("", rc == 1);
  B2ND_TEST_ASSERT(blosc2_meta_update(src2->sc, meta1.name, meta1.content, meta1.content_len));

  blosc2_metalayer meta2;
  B2ND_TEST_ASSERT(blosc2_meta_get(src2->sc, meta1.name, &meta2.content, &meta2.content_len));

  CUTEST_ASSERT("Contents are not equal", *((double *) meta2.content) == *((double *) meta1.content));
  CUTEST_ASSERT("Sizes are not equal", meta2.content_len == meta1.content_len);
  free(meta2.content);

  /* Free mallocs */
  free(buffer);
  B2ND_TEST_ASSERT(b2nd_free(src2));
  B2ND_TEST_ASSERT(b2nd_free_ctx(ctx));

  blosc2_remove_urlpath(urlpath);
  return 0;
}


CUTEST_TEST_TEARDOWN(metalayers) {
  blosc2_destroy();
}

int main() {
  CUTEST_TEST_RUN(metalayers);
}
