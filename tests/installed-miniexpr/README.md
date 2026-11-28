# miniexpr integration fixtures

## Parent-provided core, artifact adapter disabled

Run from the C-Blosc2 repository root, adjusting the miniexpr source path:

```sh
cmake -S tests/installed-miniexpr/superbuild -B build-exec-plan \
  -DMINIEXPR_SOURCE_DIR="$PWD/../miniexpr" \
  -DMINIEXPR_BUILD_ARTIFACT=OFF
cmake --build build-exec-plan --target parent_exec_plan -j8
ctest --test-dir build-exec-plan --output-on-failure
```

Execution contracts are documented in [`b2nd-exec.h`](../../blosc/b2nd-exec.h).
