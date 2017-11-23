/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Author: Francesc Alted <francesc@blosc.org>
  Creation date: 2017-08-02

  See LICENSES/BLOSC.txt for details about copyright and rights to use.
**********************************************************************/

#ifndef BLOSC_TRUNC_PREC_H
#define BLOSC_TRUNC_PREC_H

void truncate_precision(uint8_t prec_bits, size_t typesize,
                        const int32_t nbytes, const uint8_t* src,
                        uint8_t* dest);

#endif //BLOSC_TRUNC_PREC_H
