# Developer notes

## b2nd constructors with miniexpr

`BLOSC2_MINIEXPR_ROOT` enables SIMD acceleration for `b2nd_linspace()` and
`b2nd_arange()`. Point it to a built miniexpr source tree or installation
containing both `miniexpr` and `miniexpr_artifact`.

For sibling checkouts, run from the repository root:

```sh
cmake -S ../miniexpr -B ../miniexpr/build -DCMAKE_BUILD_TYPE=Release
cmake --build ../miniexpr/build --target miniexpr miniexpr_artifact -j8
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
