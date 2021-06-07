/*********************************************************************
  Blosc - Blocked Shuffling and Compression Library

  Copyright (C) 2021  The Blosc Developers <blosc@blosc.org>
  https://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  See LICENSE.txt for details about copyright and rights to use.
**********************************************************************/

#include "shuffle.h"
#include "blosc2/blosc2-common.h"
#include "shuffle-generic.h"
#include "bitshuffle-generic.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


#if !defined(__clang__) && defined(__GNUC__) && defined(__GNUC_MINOR__) && \
    __GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
#define HAVE_CPU_FEAT_INTRIN
#endif

/*  Include hardware-accelerated shuffle/unshuffle routines based on
    the target architecture. Note that a target architecture may support
    more than one type of acceleration!*/
#if defined(SHUFFLE_AVX2_ENABLED)
  #include "shuffle-avx2.h"
  #include "bitshuffle-avx2.h"
#endif  /* defined(SHUFFLE_AVX2_ENABLED) */

#if defined(SHUFFLE_SSE2_ENABLED)
  #include "shuffle-sse2.h"
  #include "bitshuffle-sse2.h"
#endif  /* defined(SHUFFLE_SSE2_ENABLED) */

#if defined(SHUFFLE_NEON_ENABLED)
  #if defined(__linux__)
    #include <sys/auxv.h>
    #ifdef ARM_ASM_HWCAP
      #include <asm/hwcap.h>
    #endif
  #endif
  #include "shuffle-neon.h"
  #include "bitshuffle-neon.h"
#endif  /* defined(SHUFFLE_NEON_ENABLED) */

#if defined(SHUFFLE_ALTIVEC_ENABLED)
  #include "shuffle-altivec.h"
  #include "bitshuffle-altivec.h"
#endif  /* defined(SHUFFLE_ALTIVEC_ENABLED) */


/*  Define function pointer types for shuffle/unshuffle routines. */
typedef void(* shuffle_func)(const int32_t, const int32_t, const uint8_t*, const uint8_t*);
typedef void(* unshuffle_func)(const int32_t, const int32_t, const uint8_t*, const uint8_t*);
// For bitshuffle, everything is done in terms of size_t and int64_t (return value)
// and although this is not strictly necessary for Blosc, it does not hurt either
typedef int64_t(* bitshuffle_func)(void*, void*, const size_t, const size_t, void*);
typedef int64_t(* bitunshuffle_func)(void*, void*, const size_t, const size_t, void*);

/* An implementation of shuffle/unshuffle routines. */
typedef struct shuffle_implementation {
  /* Name of this implementation. */
  const char* name;
  /* Function pointer to the shuffle routine for this implementation. */
  shuffle_func shuffle;
  /* Function pointer to the unshuffle routine for this implementation. */
  unshuffle_func unshuffle;
  /* Function pointer to the bitshuffle routine for this implementation. */
  bitshuffle_func bitshuffle;
  /* Function pointer to the bitunshuffle routine for this implementation. */
  bitunshuffle_func bitunshuffle;
} shuffle_implementation_t;

typedef enum {
  BLOSC_HAVE_NOTHING = 0,
  BLOSC_HAVE_SSE2 = 1,
  BLOSC_HAVE_AVX2 = 2,
  BLOSC_HAVE_NEON = 4,
  BLOSC_HAVE_ALTIVEC = 8
} blosc_cpu_features;

/* Detect hardware and set function pointers to the best shuffle/unshuffle
   implementations supported by the host processor. */
#if defined(SHUFFLE_AVX2_ENABLED) || defined(SHUFFLE_SSE2_ENABLED)    /* Intel/i686 */

/*  Disabled the __builtin_cpu_supports() call, as it has issues with
    new versions of gcc (like 5.3.1 in forthcoming ubuntu/xenial:
      "undefined symbol: __cpu_model"
    For a similar report, see:
    https://lists.fedoraproject.org/archives/list/devel@lists.fedoraproject.org/thread/ZM2L65WIZEEQHHLFERZYD5FAG7QY2OGB/
*/
#if defined(HAVE_CPU_FEAT_INTRIN) && 0
static blosc_cpu_features blosc_get_cpu_features(void) {
  blosc_cpu_features cpu_features = BLOSC_HAVE_NOTHING;
  if (__builtin_cpu_supports("sse2")) {
    cpu_features |= BLOSC_HAVE_SSE2;
  }
  if (__builtin_cpu_supports("avx2")) {
    cpu_features |= BLOSC_HAVE_AVX2;
  }
  return cpu_features;
}
#else

#if defined(_MSC_VER) && !defined(__clang__)
  #include <immintrin.h>  /* Needed for _xgetbv */
  #include <intrin.h>     /* Needed for __cpuid */
#else

/*  Implement the __cpuid and __cpuidex intrinsics for GCC, Clang,
    and others using inline assembly. */
__attribute__((always_inline))
static inline void
__cpuidex(int32_t cpuInfo[4], int32_t function_id, int32_t subfunction_id) {
  __asm__ __volatile__ (
# if defined(__i386__) && defined (__PIC__)
  /*  Can't clobber ebx with PIC running under 32-bit, so it needs to be manually restored.
      https://software.intel.com/en-us/articles/how-to-detect-new-instruction-support-in-the-4th-generation-intel-core-processor-family
  */
    "movl %%ebx, %%edi\n\t"
    "cpuid\n\t"
    "xchgl %%ebx, %%edi":
    "=D" (cpuInfo[1]),
#else
    "cpuid":
    "=b" (cpuInfo[1]),
#endif  /* defined(__i386) && defined(__PIC__) */
    "=a" (cpuInfo[0]),
    "=c" (cpuInfo[2]),
    "=d" (cpuInfo[3]) :
    "a" (function_id), "c" (subfunction_id)
    );
}

#define __cpuid(cpuInfo, function_id) __cpuidex(cpuInfo, function_id, 0)

#define _XCR_XFEATURE_ENABLED_MASK 0

// GCC folks added _xgetbv in immintrin.h starting in GCC 9
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=71659
#if !(defined(_IMMINTRIN_H_INCLUDED) && (BLOSC_GCC_VERSION >= 900))
/* Reads the content of an extended control register.
   https://software.intel.com/en-us/articles/how-to-detect-new-instruction-support-in-the-4th-generation-intel-core-processor-family
*/
static inline uint64_t
_xgetbv(uint32_t xcr) {
  uint32_t eax, edx;
  __asm__ __volatile__ (
    /* "xgetbv"
       This is specified as raw instruction bytes due to some older compilers
       having issues with the mnemonic form.
    */
    ".byte 0x0f, 0x01, 0xd0":
    "=a" (eax),
    "=d" (edx) :
    "c" (xcr)
    );
  return ((uint64_t)edx << 32) | eax;
}
#endif  // !(defined(_IMMINTRIN_H_INCLUDED) && (BLOSC_GCC_VERSION >= 900))
#endif /* defined(_MSC_VER) */

#ifndef _XCR_XFEATURE_ENABLED_MASK
#define _XCR_XFEATURE_ENABLED_MASK 0x0
#endif

static blosc_cpu_features blosc_get_cpu_features(void) {
  blosc_cpu_features result = BLOSC_HAVE_NOTHING;
  /* Holds the values of eax, ebx, ecx, edx set by the `cpuid` instruction */
  int32_t cpu_info[4];

  /* Get the number of basic functions available. */
  __cpuid(cpu_info, 0);
  int32_t max_basic_function_id = cpu_info[0];

  /* Check for SSE-based features and required OS support */
  __cpuid(cpu_info, 1);
  const bool sse2_available = (cpu_info[3] & (1 << 26)) != 0;
  const bool sse3_available = (cpu_info[2] & (1 << 0)) != 0;
  const bool ssse3_available = (cpu_info[2] & (1 << 9)) != 0;
  const bool sse41_available = (cpu_info[2] & (1 << 19)) != 0;
  const bool sse42_available = (cpu_info[2] & (1 << 20)) != 0;

  const bool xsave_available = (cpu_info[2] & (1 << 26)) != 0;
  const bool xsave_enabled_by_os = (cpu_info[2] & (1 << 27)) != 0;

  /* Check for AVX-based features, if the processor supports extended features. */
  bool avx2_available = false;
  bool avx512bw_available = false;
  if (max_basic_function_id >= 7) {
    __cpuid(cpu_info, 7);
    avx2_available = (cpu_info[1] & (1 << 5)) != 0;
    avx512bw_available = (cpu_info[1] & (1 << 30)) != 0;
  }

  /*  Even if certain features are supported by the CPU, they may not be supported
      by the OS (in which case using them would crash the process or system).
      If xsave is available and enabled by the OS, check the contents of the
      extended control register XCR0 to see if the CPU features are enabled. */
  bool xmm_state_enabled = false;
  bool ymm_state_enabled = false;
  //bool zmm_state_enabled = false;  // commented this out for avoiding an 'unused variable' warning

#if defined(_XCR_XFEATURE_ENABLED_MASK)
  if (xsave_available && xsave_enabled_by_os && (
      sse2_available || sse3_available || ssse3_available
      || sse41_available || sse42_available
      || avx2_available || avx512bw_available)) {
    /* Determine which register states can be restored by the OS. */
    uint64_t xcr0_contents = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);

    xmm_state_enabled = (xcr0_contents & (1UL << 1)) != 0;
    ymm_state_enabled = (xcr0_contents & (1UL << 2)) != 0;

    /*  Require support for both the upper 256-bits of zmm0-zmm15 to be
        restored as well as all of zmm16-zmm31 and the opmask registers. */
    //zmm_state_enabled = (xcr0_contents & 0x70) == 0x70;
  }
#endif /* defined(_XCR_XFEATURE_ENABLED_MASK) */

#if defined(BLOSC_DUMP_CPU_INFO)
  printf("Shuffle CPU Information:\n");
  printf("SSE2 available: %s\n", sse2_available ? "True" : "False");
  printf("SSE3 available: %s\n", sse3_available ? "True" : "False");
  printf("SSSE3 available: %s\n", ssse3_available ? "True" : "False");
  printf("SSE4.1 available: %s\n", sse41_available ? "True" : "False");
  printf("SSE4.2 available: %s\n", sse42_available ? "True" : "False");
  printf("AVX2 available: %s\n", avx2_available ? "True" : "False");
  printf("AVX512BW available: %s\n", avx512bw_available ? "True" : "False");
  printf("XSAVE available: %s\n", xsave_available ? "True" : "False");
  printf("XSAVE enabled: %s\n", xsave_enabled_by_os ? "True" : "False");
  printf("XMM state enabled: %s\n", xmm_state_enabled ? "True" : "False");
  printf("YMM state enabled: %s\n", ymm_state_enabled ? "True" : "False");
  //printf("ZMM state enabled: %s\n", zmm_state_enabled ? "True" : "False");
#endif /* defined(BLOSC_DUMP_CPU_INFO) */

  /* Using the gathered CPU information, determine which implementation to use. */
  /* technically could fail on sse2 cpu on os without xmm support, but that
   * shouldn't exist anymore */
  if (sse2_available) {
    result |= BLOSC_HAVE_SSE2;
  }
  if (xmm_state_enabled && ymm_state_enabled && avx2_available) {
    result |= BLOSC_HAVE_AVX2;
  }
  return result;
}
#endif /* HAVE_CPU_FEAT_INTRIN */

#elif defined(SHUFFLE_NEON_ENABLED) /* ARM-NEON */
static blosc_cpu_features blosc_get_cpu_features(void) {
  blosc_cpu_features cpu_features = BLOSC_HAVE_NOTHING;
#if defined(__aarch64__)
  /* aarch64 always has NEON */
  cpu_features |= BLOSC_HAVE_NEON;
#else
  if (getauxval(AT_HWCAP) & HWCAP_ARM_NEON) {
    cpu_features |= BLOSC_HAVE_NEON;
  }
#endif
  return cpu_features;
}
#elif defined(SHUFFLE_ALTIVEC_ENABLED) /* POWER9-ALTIVEC preliminary test*/
static blosc_cpu_features blosc_get_cpu_features(void) {
  blosc_cpu_features cpu_features = BLOSC_HAVE_NOTHING;
  cpu_features |= BLOSC_HAVE_ALTIVEC;
  return cpu_features;
}
#else   /* No hardware acceleration supported for the target architecture. */
  #if defined(_MSC_VER)
    #pragma message("Hardware-acceleration detection not implemented for the target architecture. Only the generic shuffle/unshuffle routines will be available.")
  #else
    #warning Hardware-acceleration detection not implemented for the target architecture. Only the generic shuffle/unshuffle routines will be available.
  #endif

static blosc_cpu_features blosc_get_cpu_features(void) {
return BLOSC_HAVE_NOTHING;
}

#endif /* defined(SHUFFLE_AVX2_ENABLED) || defined(SHUFFLE_SSE2_ENABLED) */

static shuffle_implementation_t get_shuffle_implementation(void) {
  blosc_cpu_features cpu_features = blosc_get_cpu_features();
#if defined(SHUFFLE_AVX2_ENABLED)
  if (cpu_features & BLOSC_HAVE_AVX2) {
    shuffle_implementation_t impl_avx2;
    impl_avx2.name = "avx2";
    impl_avx2.shuffle = (shuffle_func)shuffle_avx2;
    impl_avx2.unshuffle = (unshuffle_func)unshuffle_avx2;
    impl_avx2.bitshuffle = (bitshuffle_func)bshuf_trans_bit_elem_avx2;
    impl_avx2.bitunshuffle = (bitunshuffle_func)bshuf_untrans_bit_elem_avx2;
    return impl_avx2;
  }
#endif  /* defined(SHUFFLE_AVX2_ENABLED) */

#if defined(SHUFFLE_SSE2_ENABLED)
  if (cpu_features & BLOSC_HAVE_SSE2) {
    shuffle_implementation_t impl_sse2;
    impl_sse2.name = "sse2";
    impl_sse2.shuffle = (shuffle_func)shuffle_sse2;
    impl_sse2.unshuffle = (unshuffle_func)unshuffle_sse2;
    impl_sse2.bitshuffle = (bitshuffle_func)bshuf_trans_bit_elem_sse2;
    impl_sse2.bitunshuffle = (bitunshuffle_func)bshuf_untrans_bit_elem_sse2;
    return impl_sse2;
  }
#endif  /* defined(SHUFFLE_SSE2_ENABLED) */

#if defined(SHUFFLE_NEON_ENABLED)
  if (cpu_features & BLOSC_HAVE_NEON) {
    shuffle_implementation_t impl_neon;
    impl_neon.name = "neon";
    impl_neon.shuffle = (shuffle_func)shuffle_neon;
    impl_neon.unshuffle = (unshuffle_func)unshuffle_neon;
    //impl_neon.shuffle = (shuffle_func)shuffle_generic;
    //impl_neon.unshuffle = (unshuffle_func)unshuffle_generic;
    //impl_neon.bitshuffle = (bitshuffle_func)bitshuffle_neon;
    //impl_neon.bitunshuffle = (bitunshuffle_func)bitunshuffle_neon;
    // The current bitshuffle optimized for NEON is not any faster
    // (in fact, it is pretty much slower) than the scalar implementation.
    // Also, bitshuffle_neon (forward direction) is broken for 1, 2 and 4 bytes.
    // So, let's use the the scalar one, which is pretty fast, at least on a M1 CPU.
    impl_neon.bitshuffle = (bitshuffle_func)bshuf_trans_bit_elem_scal;
    impl_neon.bitunshuffle = (bitunshuffle_func)bshuf_untrans_bit_elem_scal;
    return impl_neon;
  }
#endif  /* defined(SHUFFLE_NEON_ENABLED) */

#if defined(SHUFFLE_ALTIVEC_ENABLED)
  if (cpu_features & BLOSC_HAVE_ALTIVEC) {
    shuffle_implementation_t impl_altivec;
    impl_altivec.name = "altivec";
    impl_altivec.shuffle = (shuffle_func)shuffle_altivec;
    impl_altivec.unshuffle = (unshuffle_func)unshuffle_altivec;
    impl_altivec.bitshuffle = (bitshuffle_func)bshuf_trans_bit_elem_altivec;
    impl_altivec.bitunshuffle = (bitunshuffle_func)bshuf_untrans_bit_elem_altivec;
    return impl_altivec;
  }
#endif  /* defined(SHUFFLE_ALTIVEC_ENABLED) */

  /* Processor doesn't support any of the hardware-accelerated implementations,
     so use the generic implementation. */
  shuffle_implementation_t impl_generic;
  impl_generic.name = "generic";
  impl_generic.shuffle = (shuffle_func)shuffle_generic;
  impl_generic.unshuffle = (unshuffle_func)unshuffle_generic;
  impl_generic.bitshuffle = (bitshuffle_func)bshuf_trans_bit_elem_scal;
  impl_generic.bitunshuffle = (bitunshuffle_func)bshuf_untrans_bit_elem_scal;
  return impl_generic;
}


/* Flag indicating whether the implementation has been initialized.
   Zero means it hasn't been initialized, non-zero means it has. */
static int32_t implementation_initialized;

/* The dynamically-chosen shuffle/unshuffle implementation.
   This is only safe to use once `implementation_initialized` is set. */
static shuffle_implementation_t host_implementation;

/* Initialize the shuffle implementation, if necessary. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
static
#if defined(_MSC_VER)
__forceinline
#else
inline
#endif
void init_shuffle_implementation(void) {
  /* Initialization could (in rare cases) take place concurrently on
     multiple threads, but it shouldn't matter because the
     initialization should return the same result on each thread (so
     the implementation will be the same). Since that's the case we
     can avoid complicated synchronization here and get a small
     performance benefit because we don't need to perform a volatile
     load on the initialization variable each time this function is
     called. */
#if defined(__GNUC__) || defined(__clang__)
  if (__builtin_expect(!implementation_initialized, 0)) {
#else
    if (!implementation_initialized) {
#endif
    /* Initialize the implementation. */
    host_implementation = get_shuffle_implementation();

    /* Set the flag indicating the implementation has been initialized. */
    implementation_initialized = 1;
  }
}

/* Shuffle a block by dynamically dispatching to the appropriate
   hardware-accelerated routine at run-time. */
void
shuffle(const int32_t bytesoftype, const int32_t blocksize,
        const uint8_t* _src, const uint8_t* _dest) {
  /* Initialize the shuffle implementation if necessary. */
  init_shuffle_implementation();

  /* The implementation is initialized.
     Dispatch to it's shuffle routine. */
  (host_implementation.shuffle)(bytesoftype, blocksize, _src, _dest);
}

/* Unshuffle a block by dynamically dispatching to the appropriate
   hardware-accelerated routine at run-time. */
void
unshuffle(const int32_t bytesoftype, const int32_t blocksize,
          const uint8_t* _src, const uint8_t* _dest) {
  /* Initialize the shuffle implementation if necessary. */
  init_shuffle_implementation();

  /* The implementation is initialized.
     Dispatch to it's unshuffle routine. */
  (host_implementation.unshuffle)(bytesoftype, blocksize, _src, _dest);
}

/*  Bit-shuffle a block by dynamically dispatching to the appropriate
    hardware-accelerated routine at run-time. */
int32_t
bitshuffle(const int32_t bytesoftype, const int32_t blocksize,
           const uint8_t *_src, const uint8_t *_dest,
           const uint8_t *_tmp) {
  /* Initialize the shuffle implementation if necessary. */
  init_shuffle_implementation();
  size_t size = blocksize / bytesoftype;
  /* bitshuffle only supports a number of elements that is a multiple of 8. */
  size -= size % 8;
  int ret = (int) (host_implementation.bitshuffle)((void *) _src, (void *) _dest,
                                             size, bytesoftype, (void *) _tmp);
  if (ret < 0) {
    // Some error in bitshuffle (should not happen)
    fprintf(stderr, "the impossible happened: the bitshuffle filter failed!");
    return ret;
  }

  // Copy the leftovers
  size_t offset = size * bytesoftype;
  memcpy((void *) (_dest + offset), (void *) (_src + offset), blocksize - offset);

  return blocksize;
}

/*  Bit-unshuffle a block by dynamically dispatching to the appropriate
    hardware-accelerated routine at run-time. */
int32_t bitunshuffle(const int32_t bytesoftype, const int32_t blocksize,
                     const uint8_t *_src, const uint8_t *_dest,
                     const uint8_t *_tmp, const uint8_t format_version) {
  /* Initialize the shuffle implementation if necessary. */
  init_shuffle_implementation();
  size_t size = blocksize / bytesoftype;

  if (format_version == 2) {
    /* Starting from version 3, bitshuffle() works differently */
    if ((size % 8) == 0) {
      /* The number of elems is a multiple of 8 which is supported by
         bitshuffle. */
      int ret = (int) (host_implementation.bitunshuffle)((void *) _src, (void *) _dest,
                                                   blocksize / bytesoftype,
                                                   bytesoftype, (void *) _tmp);
      if (ret < 0) {
        // Some error in bitshuffle (should not happen)
        fprintf(stderr, "the impossible happened: the bitunshuffle filter failed!");
        return ret;
      }
      /* Copy the leftovers (we do so starting from c-blosc 1.18 on) */
      size_t offset = size * bytesoftype;
      memcpy((void *) (_dest + offset), (void *) (_src + offset), blocksize - offset);
    }
    else {
      memcpy((void *) _dest, (void *) _src, blocksize);
    }
  }
  else {
    /* bitshuffle only supports a number of bytes that is a multiple of 8. */
    size -= size % 8;
    int ret = (int) (host_implementation.bitunshuffle)((void *) _src, (void *) _dest,
                                                 size, bytesoftype, (void *) _tmp);
    if (ret < 0) {
      fprintf(stderr, "the impossible happened: the bitunshuffle filter failed!");
      return ret;
    }

    /* Copy the leftovers */
    size_t offset = size * bytesoftype;
    memcpy((void *) (_dest + offset), (void *) (_src + offset), blocksize - offset);
  }

  return blocksize;
}
