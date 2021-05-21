Here there are stored testing complementary files that are avaiable for
all users to test their plugins.

The folder files are multidimensional arrays created in Caterva with 
the next parameters:
- cfg.nthreads = 1;
- cfg.splitmode = BLOSC_ALWAYS_SPLIT;
- cfg.compcodec = BLOSC_ZSTD;
- cfg.complevel = 9;

Moreover, each dataset has different types and shapes:

**example_rand.caterva**:

- ndim = 3;
- typesize = 4;
- shape[8] = {32, 18, 32};
- chunkshape[8] = {17, 16, 24};
- blockshape[8] = {8, 9, 8};

**example_same_cells.caterva**:

- ndim = 2;
- typesize = 4;
- shape[8] = {128, 111};
- chunkshape[8] = {32, 11};
- blockshape[8] = {16, 7};

**example_some_matches.caterva**:

- ndim = 2;
- typesize = 8;
- shape[8] = {128, 111};
- chunkshape[8] = {48, 32};
- blockshape[8] = {14, 18};

