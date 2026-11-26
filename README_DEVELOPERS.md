# Developer notes

## b2nd constructors with miniexpr

`BLOSC2_MINIEXPR_ROOT` enables SIMD acceleration for `b2nd_linspace()` and
`b2nd_arange()`. Point it to a built miniexpr source tree or installation
containing the core `miniexpr` library and headers (`miniexpr_kernel.h`,
`miniexpr_grid.h`, `miniexpr_reduce.h`).
Constructors and the optional `sum_linspace` benchmark use the core library
directly and do not need JSON support.
The 1.0 cleanup baseline is miniexpr
`41aad03c9146a273775c3d50cd28fcd9f099f257` (`next`).
Install `miniexpr_types.h` alongside the kernel/grid/reduction headers; new
constructor code does not depend on the callback-bearing compatibility API.

A parent CMake build can instead set `BLOSC2_USE_PARENT_MINIEXPR=ON` after
defining `miniexpr::core` and before adding C-Blosc2. Do not also set
`BLOSC2_MINIEXPR_ROOT`. This shares one core with other targets in the parent
build. To install static C-Blosc2 when miniexpr is also built from source in
the parent, configure miniexpr with `MINIEXPR_INSTALL_CORE=ON` so both packages
are exported for downstream consumers.

For sibling checkouts, run from the repository root:

```sh
cmake -S ../miniexpr -B ../miniexpr/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../miniexpr/build --target miniexpr -j8
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON \
  -DBLOSC2_MINIEXPR_ROOT=../miniexpr
cmake --build build --target sum_linspace test_b2nd_constructors -j8
./build/bench/sum_linspace
ctest --test-dir build -R '^test_b2nd_constructors$' --output-on-failure
```

The benchmark should report `b2nd_linspace (miniexpr)`.
To test native constructors, configure with `-DBLOSC2_MINIEXPR_ROOT=`;
add `-DBENCH_MINIEXPR_ROOT=../miniexpr` to retain the benchmark's reduction
dependency. Without either option, constructors use native C and the
`sum_linspace` target is omitted.
