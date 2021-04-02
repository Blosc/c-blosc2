# Corpus files

Here are the files for the fuzzer to check.  These can be generated with:

```
$ ./tests/generate_inputs_corpus
Blosc version info: 2.0.0.beta.6.dev ($Date:: 2020-04-21 #$)

*** Creating simple frame for blosclz
Compression ratio: 1953.1 KB -> 32.1 KB (60.8x)
Compression time: 0.00403 s, 472.9 MB/s
Successfully created frame_simple-blosclz.b2frame

*** Creating simple frame for lz4
Compression ratio: 1953.1 KB -> 40.1 KB (48.7x)
Compression time: 0.00335 s, 569.1 MB/s
Successfully created frame_simple-lz4.b2frame

*** Creating simple frame for lz4hc
Compression ratio: 1953.1 KB -> 23.2 KB (84.1x)
Compression time: 0.00582 s, 327.9 MB/s
Successfully created frame_simple-lz4hc.b2frame

*** Creating simple frame for zlib
Compression ratio: 1953.1 KB -> 20.8 KB (94.1x)
Compression time: 0.0082 s, 232.6 MB/s
Successfully created frame_simple-zlib.b2frame

*** Creating simple frame for zstd
Compression ratio: 1953.1 KB -> 10.8 KB (180.2x)
Compression time: 0.00916 s, 208.3 MB/s
Successfully created frame_simple-zstd.b2frame
```

If you want to copy the new files to the corpus directory, do:

```
$ cp *.b2frame ../tests/fuzz/corpus
```
