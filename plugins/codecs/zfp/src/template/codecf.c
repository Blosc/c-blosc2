#include <limits.h>
#include <math.h>

/* maximum number of bit planes to encode */
static uint
precision(int maxexp, uint maxprec, int minexp, int dims)
{
#if (ZFP_ROUNDING_MODE != ZFP_ROUND_NEVER) && defined(ZFP_WITH_TIGHT_ERROR)
  return MIN(maxprec, (uint)MAX(0, maxexp - minexp + 2 * dims + 1));
#else
  return MIN(maxprec, (uint)MAX(0, maxexp - minexp + 2 * dims + 2));
#endif
}

/* map integer x relative to exponent e to floating-point number */
static Scalar
_t1(dequantize, Scalar)(Int x, int e)
{
  return LDEXP((Scalar)x, e - ((int)(CHAR_BIT * sizeof(Scalar)) - 2));
}

/* inverse block-floating-point transform from signed integers */
static void
_t1(inv_cast, Scalar)(const Int* iblock, Scalar* fblock, uint n, int emax)
{
  /* compute power-of-two scale factor s */
  Scalar s = _t1(dequantize, Scalar)(1, emax);
  /* compute p-bit float x = s*y where |y| <= 2^(p-2) - 1 */
  do
    *fblock++ = (Scalar)(s * *iblock++);
  while (--n);
}
