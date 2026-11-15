#!/usr/bin/env bash
# Build direct-kernel benchmarks without giving the SSE2 object AVX/SSSE3.
# Usage (macOS): bash bench/build_bitshuffle_simd.sh SOURCE_DIR OUTPUT_PREFIX [arm64|x86_64]
set -eo pipefail
src=$1
out=$2
arch=${3:-$(uname -m)}
mkdir -p "${out}.objects"
flags=(-O3 -I"$src/include" -I"$src/blosc")
if [[ $(uname -s) == Darwin ]]; then flags+=(-arch "$arch"); fi
for kind in generic neon sse2 avx2; do
    simd=()
    if [[ $arch == x86_64 ]]; then
        if [[ $kind == avx2 ]]; then simd=(-mavx2); else simd=(-msse2 -mno-avx -mno-ssse3); fi
    fi
    for operation in bitshuffle shuffle; do
        "${CC:-clang}" "${flags[@]}" "${simd[@]}" -c "$src/blosc/$operation-$kind.c" \
            -o "$out.objects/$operation-$kind.o"
    done
done
"${CC:-clang}" "${flags[@]}" "$(dirname "$0")/bench_bitshuffle_simd.c" \
    "$out.objects/"*.o -o "$out"
