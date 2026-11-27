# Initial native storage binding

`b2nd_exec_execute_arrays` binds compressed inputs to the existing typed
schedule. It does not prepare child graphs or introduce another map/reducer
loop. Current admission is serial, same-shape, native float64, in-memory
chunked arrays with built-in lossless codecs/filters and resident output.
Frame-backed arrays, postfilters, plugin codecs/filters, mixed inputs,
broadcast/partial axes, compressed sinks and parallel jobs are not admitted.

Each invocation owns one decoded physical chunk per operand, a private
single-thread decoder, and the executor's existing bounded logical gather/map
tiles. Padding never participates. No array cache or `current_nchunk` is
mutated. Headers are inspected as metadata during preflight; compressed
payloads are decoded only after bindings, output and budgets pass validation.

`decode_bytes` measures owned decoded cache allocation, separately from
gather/mapped scratch and fixed binding metadata. Codec-private memory is
excluded. `decoded_bytes` counts successful decode traffic including revisits;
cache hits count logical item lookups, and `read_spans` counts attempted spans.
The scalar result publishes only on success. Map failure can leave completed
prefix tiles committed. Neither failure path replays another executor.

Build/install C-Blosc2 against an artifact-off miniexpr installation, then:

```sh
cmake -S tests/exec-storage -B <build> -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="<blosc2-prefix>;<miniexpr-prefix>"
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```

The fixture compares map/sum/prod/min/max/any/all against resident execution of
the same schedule, with differing padded layouts and short final spans.
Signaling-NaN padding detects accidental physical participation. The fault
fixture injects owner allocation failures and early/late decode failures,
including scalar non-publication and committed map-prefix checks.

These fixtures compile the new owner with warnings as errors and can be built
with ASan/UBSan compiler/linker flags. The independent installed-miniexpr suite
also links this test against the actual installed C-Blosc2 static archive.
