Here there are stored testing complementary files that are available for
all users to test their plugins.

The folder files are multidimensional arrays created by b2nd using
the 'example_frame_generator.c' example.
(https://github.com/Blosc/c-blosc2/blob/main/examples/b2nd/example_frame_generator.c).
Moreover, they have the next parameters:
- nthreads = 1;
- splitmode = BLOSC_ALWAYS_SPLIT;
- compcodec = BLOSC_ZSTD;
- complevel = 9;

Moreover, each dataset has different types and shapes:

**example_rand.b2nd**:

- ndim = 3;
- type = int;
- typesize = 4;
- shape[8] = {32, 18, 32};
- chunkshape[8] = {17, 16, 24};
- blockshape[8] = {8, 9, 8};

**example_same_cells.b2nd**:

- ndim = 2;
- type = int;
- typesize = 4;
- shape[8] = {128, 111};
- chunkshape[8] = {32, 11};
- blockshape[8] = {16, 7};

**example_some_matches.b2nd**:

- ndim = 2;
- type = long;
- typesize = 8;
- shape[8] = {128, 111};
- chunkshape[8] = {48, 32};
- blockshape[8] = {14, 18};

**example_float_cyclic.b2nd**:

- int8_t ndim = 3;
- type = float;
- typesize = 4;
- int64_t shape[] = {40, 60, 20};
- int32_t chunkshape[] = {20, 30, 16};
- int32_t blockshape[] = {11, 14, 7};

**example_double_same_cells.b2nd**:

- int8_t ndim = 2;
- type = double;
- typesize = 8;
- int64_t shape[] = {40, 60};
- int32_t chunkshape[] = {20, 30};
- int32_t blockshape[] = {16, 16};

**example_day_month_temp.b2nd**:

- int ndim = 2;
- type = float;
- typesize = 4;
- int64_t shape[] = {400, 3};
- int32_t chunkshape[] = {110, 3};
- int32_t blockshape[] = {57, 3};

This frame simulates values for: 
- Day: between 1 and 31 (column 0)
- Month: between 1 and 12 (column 1)
- Temperature: between -20 and 40 (column 2)

These fields are grouped by columns, so the frame has 400 rows that 
represent different days with a value for each of the three columns.

**example_item_prices.b2nd**:

- int ndim = 3;
- type = float;
- typesize = 4;
- int64_t shape[] = {12, 25, 250};
- int32_t chunkshape[] = {8, 10, 50};
- int32_t blockshape[] = {4, 5, 10};
    
This frame simulates item prices based on 3 dimensions:
- Month (dim 0): items are more expensive depending on the month
- Store ID (dim 1): items are more expensive depending on the store type
- Item ID (dim 2): each item has a different base price

Depending on each dimension index, the final price is calculated for 
each situation.
