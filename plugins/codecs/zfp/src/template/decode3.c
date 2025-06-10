/* private functions ------------------------------------------------------- */

/* scatter 4*4*4 block to strided array */
static void
_t2(scatter, Scalar, 3)(const Scalar* q, Scalar* p, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz)
{
  uint x, y, z;
  for (z = 0; z < 4; z++, p += sz - 4 * sy)
    for (y = 0; y < 4; y++, p += sy - 4 * sx)
      for (x = 0; x < 4; x++, p += sx)
        *p = *q++;
}

/* scatter nx*ny*nz block to strided array */
static void
_t2(scatter_partial, Scalar, 3)(const Scalar* q, Scalar* p, size_t nx, size_t ny, size_t nz, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz)
{
  size_t x, y, z;
  for (z = 0; z < nz; z++, p += sz - (ptrdiff_t)ny * sy, q += 4 * (4 - ny))
    for (y = 0; y < ny; y++, p += sy - (ptrdiff_t)nx * sx, q += 1 * (4 - nx))
      for (x = 0; x < nx; x++, p += sx, q++)
        *p = *q;
}

/* inverse decorrelating 3D transform */
static void
_t2(inv_xform, Int, 3)(Int* p)
{
  uint x, y, z;
  /* transform along z */
  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      _t1(inv_lift, Int)(p + 1 * x + 4 * y, 16);
  /* transform along y */
  for (x = 0; x < 4; x++)
    for (z = 0; z < 4; z++)
      _t1(inv_lift, Int)(p + 16 * z + 1 * x, 4);
  /* transform along x */
  for (z = 0; z < 4; z++)
    for (y = 0; y < 4; y++)
      _t1(inv_lift, Int)(p + 4 * y + 16 * z, 1);
}

/* public functions -------------------------------------------------------- */

/* decode 4*4*4 block and store at p using strides (sx, sy, sz) */
size_t
_t2(zfp_decode_block_strided, Scalar, 3)(zfp_stream* stream, Scalar* p, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz)
{
  /* decode contiguous block */
  cache_align_(Scalar block[64]);
  size_t bits = _t2(zfp_decode_block, Scalar, 3)(stream, block);
  /* scatter block to strided array */
  _t2(scatter, Scalar, 3)(block, p, sx, sy, sz);
  return bits;
}

/* decode nx*ny*nz block and store at p using strides (sx, sy, sz) */
size_t
_t2(zfp_decode_partial_block_strided, Scalar, 3)(zfp_stream* stream, Scalar* p, size_t nx, size_t ny, size_t nz, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz)
{
  /* decode contiguous block */
  cache_align_(Scalar block[64]);
  size_t bits = _t2(zfp_decode_block, Scalar, 3)(stream, block);
  /* scatter block to strided array */
  _t2(scatter_partial, Scalar, 3)(block, p, nx, ny, nz, sx, sy, sz);
  return bits;
}
