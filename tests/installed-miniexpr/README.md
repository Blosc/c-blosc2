# miniexpr integration fixtures

## Parent-provided core, artifact adapter disabled

Run from the C-Blosc2 repository root, adjusting the miniexpr source path:

```sh
cmake -S tests/installed-miniexpr/superbuild -B build-resident-pilot \
  -DMINIEXPR_SOURCE_DIR="$PWD/../miniexpr" \
  -DMINIEXPR_BUILD_ARTIFACT=OFF
cmake --build build-resident-pilot --target parent_kernel_grid -j8
ctest --test-dir build-resident-pilot --output-on-failure
```

The fixture covers grid and logical-partition execution. With core headers
providing `ME_EXTREMA_SPAN_API`, it also checks resident minimum and map/min
preparation, short final tiles, budget and dtype rejection, plan reuse and
integer wrapping. Older headers skip those resident cases.

Execution contracts are documented in [`b2nd-kernel.h`](../../blosc/b2nd-kernel.h).
