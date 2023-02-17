// Copyright (c) 2019 - 2021, Osamu Watanabe
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
//    modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "image_class.hpp"
#include <cmath>
#include <cfloat>
#include <string>

int main(int argc, char *argv[]) {
  if (argc != 3 && argc != 5) {
    printf("\nusage: imgcmp file1 file2 [PAE MSE]\n");
    printf("  (only accepts pnm or pgx files)\n");
    printf("  - PAE and MSE are threshold for conformance testing.\n\n");
    return EXIT_FAILURE;
  }
  image img0, img1;
  img0.read_pnmpgx(argv[1]);
  img1.read_pnmpgx(argv[2]);
  uint_fast32_t w, h;
  if (((w = img0.get_width()) != img1.get_width()) || (h = img0.get_height()) != img1.get_height()) {
    printf("width and height shall be the same\n");
    return EXIT_FAILURE;
  }
  int_fast32_t *sp0, *sp1;
  sp0 = img0.access_pixels();
  sp1 = img1.access_pixels();

  uint_fast64_t PAE = 0, tmp, sum = 0;
  int_fast64_t d;
  const uint_fast32_t length = w * h * img0.get_num_components();
  for (uint_fast32_t i = 0; i < length; ++i) {
    d   = (int_fast64_t)sp0[i] - (int_fast64_t)sp1[i];
    tmp = (d < 0) ? static_cast<uint_fast64_t>(-d) : static_cast<uint_fast64_t>(d);
    PAE = (tmp > PAE) ? tmp : PAE;
    sum += static_cast<uint_fast64_t>(d * d);
  }
  auto mse    = static_cast<double>(sum) / static_cast<double>(length);
  auto maxval = static_cast<double>(img0.get_maxval());
  auto psnr   = 10 * log10((maxval * maxval) / mse);
  if (mse < DBL_EPSILON) {
    psnr = INFINITY;
  }

  printf("%4llu, %12.6f, %12.6f\n", PAE, mse, psnr);

  if (argc == 5) {
    uint_fast64_t thPAE = static_cast<uint_fast64_t>(std::stoi(argv[3]));
    double thMSE        = std::stof(argv[4]);
    if (PAE > thPAE || mse > thMSE) {
      printf("conformance test failure.\n");
      exit(EXIT_FAILURE);
    }
  }
  return EXIT_SUCCESS;
}
