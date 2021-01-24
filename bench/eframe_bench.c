/*********************************************************************
  Benchmark for testing eframe vs frame.

  You can select different degrees of 'randomness' in input buffer, as
  well as external datafiles (uncomment the lines after "For data
  coming from a file" comment).

  For usage instructions of this benchmark, please see:

    http://blosc.org/synthetic-benchmarks.html

  I'm collecting speeds for different machines, so the output of your
  benchmarks and your processor specifications are welcome!

  Author: The Blosc Developers <blosc@blosc.org>

  Note: Compiling this with VS2008 does not work well with cmake.  Here
  it is a way to compile the benchmark (with added support for LZ4):

  > cl /DHAVE_LZ4 /arch:SSE2 /Ox /Febench.exe /Iblosc /Iinternal-complibs\lz4-1.7.0 bench\bench.c blosc\blosc.c blosc\blosclz.c blosc\shuffle.c blosc\shuffle-sse2.c blosc\shuffle-generic.c blosc\bitshuffle-generic.c blosc\bitshuffle-sse2.c internal-complibs\lz4-1.7.0\*.c

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "blosc2.h"
#include <sys/time.h>

#define KB  1024
#define MB  (1024*KB)
#define GB  (1024*MB)

#define NCHUNKS 1000       /* number of chunks */
#define CHUNKSIZE (200 * 1000)

int nchunks = NCHUNKS;

#if defined(_WIN32)
#include <malloc.h>

#endif  /* defined(_WIN32) && !defined(__MINGW32__) */



int main(int argc, char* argv[]) {

  blosc_timestamp_t last, current;
  double frame_append_time, eframe_append_time, frame_decompress_time, eframe_decompress_time;


  if (argc == 2) {
    nchunks = (int)strtol(argv[1], NULL, 10);
  }
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int32_t* data = malloc(isize);
  int32_t* data_dest = malloc(isize);

  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;

  blosc2_schunk* schunk_eframe;
  blosc2_schunk* schunk_frame;


  /* Initialize the Blosc compressor */
  blosc_init();

  /* Create a frame container */
  cparams.typesize = sizeof(int32_t);

  cparams.nthreads = 1;
  dparams.nthreads = 1;

  blosc2_storage storage = {.sequential=false, .urlpath="dir.b2eframe", .cparams=&cparams, .dparams=&dparams};
  schunk_eframe = blosc2_schunk_new(storage);

  blosc2_storage storage2 = {.sequential=true, .urlpath="test_frame.b2frame", .cparams=&cparams, .dparams=&dparams};
  schunk_frame = blosc2_schunk_new(storage2);

  // Set random seed
  srand(time(0));
  // Feed it with data
  eframe_append_time=0.0;
  frame_append_time=0.0;
  blosc_set_timestamp(&current);
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    for (int i = 0; i < CHUNKSIZE; i++) {
      data[i] = rand();
    }
    blosc_set_timestamp(&current);
    blosc2_schunk_append_buffer(schunk_eframe, data, isize);
    blosc_set_timestamp(&last);
    eframe_append_time += blosc_elapsed_secs(current, last);

    blosc_set_timestamp(&current);
    blosc2_schunk_append_buffer(schunk_frame, data, isize);
    blosc_set_timestamp(&last);
    frame_append_time += blosc_elapsed_secs(current, last);
  }
  blosc_set_timestamp(&last);

  // Decompress the data
  // eframe
  blosc_set_timestamp(&current);
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    blosc2_schunk_decompress_chunk(schunk_eframe, nchunk, (void *) data_dest, isize);
  }
  blosc_set_timestamp(&last);
  eframe_decompress_time = blosc_elapsed_secs(current, last);
  // frame
  blosc_set_timestamp(&current);
  for (int nchunk = 0; nchunk < nchunks; nchunk++) {
    blosc2_schunk_decompress_chunk(schunk_frame, nchunk, (void *) data_dest, isize);
  }
  blosc_set_timestamp(&last);
  frame_decompress_time = blosc_elapsed_secs(current, last);


  /* Remove directory */
  blosc2_remove_dir(storage.urlpath);
  /* Free blosc resources */
  free(data_dest);
  free(data);
  blosc2_schunk_free(schunk_eframe);
  blosc2_schunk_free(schunk_frame);
  /* Destroy the Blosc environment */
  blosc_destroy();
  return 0;
}
