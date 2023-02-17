Here there are stored testing complementary files that are available for
all users to test their plugins.

The folder files are multidimensional arrays created by b2nd using
the 'example_frame_generator.c' example. 
(https://github.com/Blosc/c-blosc2/blob/main/examples/b2nd/example_frame_generator.c).
There is one exception: teapot.ppm is an image used for testing J2K.

Moreover, they have the next parameters:
- nthreads = 1;
- splitmode = BLOSC_ALWAYS_SPLIT;
- compcodec = BLOSC_ZSTD;
- complevel = 9;

Moreover, each dataset has different types and shapes:

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
- int32_t chunkshape[] = {6, 10, 50};
- int32_t blockshape[] = {3, 5, 10};
    
This frame simulates item prices based on 3 dimensions:
- Month (dim 0): items are more expensive depending on the month
- Store ID (dim 1): items are more expensive depending on the store type
- Item ID (dim 2): each item has a different base price

Depending on each dimension index, the final price is calculated for 
each situation.
