# Developer notes

The `b2nd-constructors` branch in the Codeberg repository is the current home
for work on `b2nd_linspace()` and `b2nd_arange()`.

## Optional miniexpr acceleration

The floating-point b2nd constructors work without additional dependencies.
By default they use native C kernels. Configure with
`-DBLOSC2_MINIEXPR_ROOT=/path/to/miniexpr` to use miniexpr's ahead-of-time SIMD
ramp kernels instead. No runtime JIT compiler is needed for these constructors,
and miniexpr types are not exposed in the public c-blosc2 API.

The path must point to an already built miniexpr source tree (headers in `src/`,
libraries in `build/`) or an installation (headers in `include/`, libraries in
`lib/`). Both `miniexpr` and `miniexpr_artifact` are required. For sibling
checkouts, run these commands from the c-blosc2 repository root:

```sh
cmake -S ../miniexpr -B ../miniexpr/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../miniexpr/build --target miniexpr miniexpr_artifact -j8

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON \
  -DBLOSC2_MINIEXPR_ROOT=../miniexpr
cmake --build build --target sum_linspace test_b2nd_constructors -j8
```

CMake should report `b2nd ramp constructors: miniexpr backend`.
Rebuild miniexpr after changing its sources, then rebuild c-blosc2. If linking
against shared miniexpr, its library must remain available at runtime.

## Benchmark and tests

```sh
./build/bench/sum_linspace
ctest --test-dir build -R '^test_b2nd_constructors$' --output-on-failure
```

The benchmark creates a real float64 b2nd array with `b2nd_linspace()` and sums
its stored, compressed values using miniexpr's pairwise reduction API. Defaults
are one billion elements, eight threads, and seven repeats of ten sum calls
after a warmup. Creation includes allocation, generation, and compression.

Optional positional arguments are:

```text
sum_linspace [nitems [nthreads [repeats [loops [chunk_items [block_items]]]]]]
```

The creation banner should say `b2nd_linspace (miniexpr)`. On an Apple M2,
representative results are about 196 ms for creation and 140 ms for sum, close
to Python's 199 ms and 138 ms on the same machine. Compare identical thread
counts, geometry, compression settings, and timing methods on the same host.

The constructor tests cover float32/float64, logical C-order multidimensional
generation, padded blocks/chunks, byte order, persistent storage, empty/scalar
arrays, and invalid parameters.

## Testing the native C fallback

The benchmark inherits `BLOSC2_MINIEXPR_ROOT`; a separate benchmark flag is
normally unnecessary. To disable library acceleration but keep miniexpr for
the benchmark's reduction, explicitly clear the cached library option and set
the benchmark-only path:

```sh
cmake -S . -B build -DBLOSC2_MINIEXPR_ROOT= \
  -DBENCH_MINIEXPR_ROOT=../miniexpr
cmake --build build --target sum_linspace test_b2nd_constructors -j8
./build/bench/sum_linspace
ctest --test-dir build -R '^test_b2nd_constructors$' --output-on-failure
```

The banner will say `b2nd_linspace (native C)`. The benchmark-only option does
**not** accelerate library constructors. Clear both options for a build with no
miniexpr dependency; in that configuration the `sum_linspace` target is omitted.
