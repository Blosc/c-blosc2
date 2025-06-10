/* private functions ------------------------------------------------------- */

/* gather 4*4*4*4 block from strided array */
static void
_t2(gather, Scalar, 4)(Scalar* q, const Scalar* p, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz, ptrdiff_t sw)
{
  uint x, y, z, w;
  for (w = 0; w < 4; w++, p += sw - 4 * sz)
    for (z = 0; z < 4; z++, p += sz - 4 * sy)
      for (y = 0; y < 4; y++, p += sy - 4 * sx)
        for (x = 0; x < 4; x++, p += sx)
          *q++ = *p;
}

/* gather nx*ny*nz*nw block from strided array */
static void
_t2(gather_partial, Scalar, 4)(Scalar* q, const Scalar* p, size_t nx, size_t ny, size_t nz, size_t nw, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz, ptrdiff_t sw)
{
  size_t x, y, z, w;
  for (w = 0; w < nw; w++, p += sw - (ptrdiff_t)nz * sz) {
    for (z = 0; z < nz; z++, p += sz - (ptrdiff_t)ny * sy) {
      for (y = 0; y < ny; y++, p += sy - (ptrdiff_t)nx * sx) {
        for (x = 0; x < nx; x++, p += sx)
          q[64 * w + 16 * z + 4 * y + x] = *p; 
        _t1(pad_block, Scalar)(q + 64 * w + 16 * z + 4 * y, nx, 1);
      }
      for (x = 0; x < 4; x++)
        _t1(pad_block, Scalar)(q + 64 * w + 16 * z + x, ny, 4);
    }
    for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++)
        _t1(pad_block, Scalar)(q + 64 * w + 4 * y + x, nz, 16);
  }
  for (z = 0; z < 4; z++)
    for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++)
        _t1(pad_block, Scalar)(q + 16 * z + 4 * y + x, nw, 64);
}

/* forward decorrelating 4D transform */
static void
_t2(fwd_xform, Int, 4)(Int* p)
{
  uint x, y, z, w;
  /* transform along x */
  for (w = 0; w < 4; w++)
    for (z = 0; z < 4; z++)
      for (y = 0; y < 4; y++)
        _t1(fwd_lift, Int)(p + 4 * y + 16 * z + 64 * w, 1);
  /* transform along y */
  for (x = 0; x < 4; x++)
    for (w = 0; w < 4; w++)
      for (z = 0; z < 4; z++)
        _t1(fwd_lift, Int)(p + 16 * z + 64 * w + 1 * x, 4);
  /* transform along z */
  for (y = 0; y < 4; y++)
    for (x = 0; x < 4; x++)
      for (w = 0; w < 4; w++)
        _t1(fwd_lift, Int)(p + 64 * w + 1 * x + 4 * y, 16);
  /* transform along w */
  for (z = 0; z < 4; z++)
    for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++)
        _t1(fwd_lift, Int)(p + 1 * x + 4 * y + 16 * z, 64);
}

/* public functions -------------------------------------------------------- */

/* encode 4*4*4*4 block stored at p using strides (sx, sy, sz, sw) */
size_t
_t2(zfp_encode_block_strided, Scalar, 4)(zfp_stream* stream, const Scalar* p, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz, ptrdiff_t sw)
{
  /* gather block from strided array */
  cache_align_(Scalar block[256]);
  _t2(gather, Scalar, 4)(block, p, sx, sy, sz, sw);
  /* encode block */
  return _t2(zfp_encode_block, Scalar, 4)(stream, block);
}

/* encode nx*ny*nz*nw block stored at p using strides (sx, sy, sz, sw) */
size_t
_t2(zfp_encode_partial_block_strided, Scalar, 4)(zfp_stream* stream, const Scalar* p, size_t nx, size_t ny, size_t nz, size_t nw, ptrdiff_t sx, ptrdiff_t sy, ptrdiff_t sz, ptrdiff_t sw)
{
  /* gather block from strided array */
  cache_align_(Scalar block[256]);
  _t2(gather_partial, Scalar, 4)(block, p, nx, ny, nz, nw, sx, sy, sz, sw);
  /* encode block */
  return _t2(zfp_encode_block, Scalar, 4)(stream, block);
}
