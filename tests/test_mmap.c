/*
  Copyright (c) 2021  Blosc Development Team <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)
*/

#include "test_common.h"
#include "cutest.h"


bool are_files_identical(const char* filename1, const char* filename2) {
  FILE *file1 = fopen(filename1, "rb");
  FILE *file2 = fopen(filename2, "rb");
  if (file1 == NULL || file2 == NULL) {
    return false;
  }

  /* Compare file sizes */
  fseek(file1, 0, SEEK_END);
  fseek(file2, 0, SEEK_END);
  size_t file_size1 = ftell(file1);
  size_t file_size2 = ftell(file2);
  if (file_size1 != file_size2) {
    fclose(file1);
    fclose(file2);
    return false;
  }

  /* Compare file contents */
  fseek(file1, 0, SEEK_SET);
  fseek(file2, 0, SEEK_SET);
  char* buffer1 = (char*)malloc(file_size1);
  char* buffer2 = (char*)malloc(file_size2);
  size_t count = fread(buffer1, 1, file_size1, file1);
  CUTEST_ASSERT("Could not read file 1", count == file_size1);
  count = fread(buffer2, 1, file_size2, file2);
  CUTEST_ASSERT("Could not read file 2", count == file_size2);
  fclose(file1);
  fclose(file2);

  bool are_identical = memcmp(buffer1, buffer2, file_size1) == 0;
  free(buffer1);
  free(buffer2);
  return are_identical;
}


CUTEST_TEST_DATA(mmap) {
  blosc2_cparams cparams;
};

CUTEST_TEST_SETUP(mmap) {
  blosc2_init();

  data->cparams = BLOSC2_CPARAMS_DEFAULTS;
  data->cparams.typesize = sizeof(float);
  data->cparams.compcode = BLOSC_BLOSCLZ;
  data->cparams.clevel = 9;
  data->cparams.nthreads = 1;
}

CUTEST_TEST_TEST(mmap) {
  char* urlpath_default = "test_udio_default.b2frame";
  char* urlpath_mmap = "test_udio_mmap.b2frame";
  blosc2_remove_urlpath(urlpath_default);
  blosc2_remove_urlpath(urlpath_mmap);

  /* New file using default I/O */
  blosc2_storage storage_default = {.cparams=&data->cparams, .contiguous=true, .urlpath = urlpath_default};
  blosc2_schunk *schunk_write_default = blosc2_schunk_new(&storage_default);

  float data_buffer[2] = {0.1, 0.2};
  int64_t cbytes = blosc2_schunk_append_buffer(schunk_write_default, data_buffer, sizeof(data_buffer));
  CUTEST_ASSERT("Could not write first chunk", cbytes > 0);

  float data_buffer2[2] = {0.3, 0.4};
  cbytes = blosc2_schunk_append_buffer(schunk_write_default, data_buffer2, sizeof(data_buffer2));
  CUTEST_ASSERT("Could not write second chunk", cbytes > 0);

  /* New file using memory-mapped I/O */
  blosc2_stdio_mmap mmap_file = {.addr=NULL, .size=-1, .offset=0, .file=NULL, .fd=-1, .mode="w+", .needs_free=false};
  blosc2_io io = {.id = BLOSC2_IO_FILESYSTEM_MMAP, .name = "filesystem_mmap", .params = &mmap_file};
  blosc2_storage storage_mmap = {.cparams=&data->cparams, .contiguous=true, .urlpath = urlpath_mmap, .io=&io};
  blosc2_schunk *schunk_write_mmap = blosc2_schunk_new(&storage_mmap);

  cbytes = blosc2_schunk_append_buffer(schunk_write_mmap, data_buffer, sizeof(data_buffer));
  CUTEST_ASSERT("Could not write first chunk", cbytes > 0);
  cbytes = blosc2_schunk_append_buffer(schunk_write_mmap, data_buffer2, sizeof(data_buffer2));
  CUTEST_ASSERT("Could not write second chunk", cbytes > 0);

  /* The compressed file content should not depend on the I/O which created it */
  CUTEST_ASSERT("Files are not identical", are_files_identical(urlpath_default, urlpath_mmap));

  /* Read the chunk data back again (using mmap) */
  blosc2_schunk* schunk_read = blosc2_schunk_open_offset_mmap(urlpath_mmap, 0, "r");
  CUTEST_ASSERT("Mismatch in number of chunks", schunk_read->nchunks == 2);

  float* chunk_data = (float*)malloc(schunk_read->chunksize);
  int dsize = blosc2_schunk_decompress_chunk(schunk_read, 0, chunk_data, schunk_read->chunksize);
  CUTEST_ASSERT("Size of decompressed chunk 1 does not match", dsize == sizeof(data_buffer));
  CUTEST_ASSERT("Value 1 of chunk 1 is wrong", fabs(chunk_data[0] - 0.1) < 1e-6);
  CUTEST_ASSERT("Value 2 of chunk 1 is wrong", fabs(chunk_data[1] - 0.2) < 1e-6);

  dsize = blosc2_schunk_decompress_chunk(schunk_read, 1, chunk_data, schunk_read->chunksize);
  CUTEST_ASSERT("Size of decompressed chunk 1 does not match", dsize == sizeof(data_buffer2));
  CUTEST_ASSERT("Value 1 of chunk 2 is wrong", fabs(chunk_data[0] - 0.3) < 1e-6);
  CUTEST_ASSERT("Value 2 of chunk 2 is wrong", fabs(chunk_data[1] - 0.4) < 1e-6);

  blosc2_schunk_free(schunk_write_default);
  blosc2_schunk_free(schunk_write_mmap);
  blosc2_schunk_free(schunk_read);
  free(chunk_data);

  return 0;
}

CUTEST_TEST_TEARDOWN(mmap) {
  BLOSC_UNUSED_PARAM(data);
  blosc2_destroy();
}


int main() {
  CUTEST_TEST_RUN(mmap);
}
