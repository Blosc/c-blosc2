# Native storage binding

`b2nd_exec_execute_arrays` binds compressed inputs to the existing typed
schedule. It does not prepare child graphs or introduce another map/reducer
loop. Numeric inputs support frame-backed arrays, mixed resident bindings,
right-aligned broadcasting, partial-axis reductions and admitted parallel jobs.
Elementwise compressed sinks use a separate compression-prefilter binding.
Postfilters and plugin codecs/filters are not admitted by native storage.

Each resident-output worker owns a private single-thread decoder and a bounded
LRU of up to eight decoded physical chunks per operand. Task admission first
reserves one slot per operand/worker, then extra slots grow round-robin within
the remaining aggregate `max_decode_bytes` budget, capped by physical chunk
count. A one-slot budget retains the original footprint. Cache growth never
changes worker geometry, logical traversal or serial reduction order. Compressed
sinks and logical partitions separately use their existing four-block caches.
Padding never participates. No array cache or `current_nchunk` is mutated.
Headers are inspected as metadata during preflight; compressed payloads are
decoded only after bindings, output and budgets pass validation.

`decode_bytes` measures owned decoded cache allocation, separately from
gather/mapped scratch and fixed binding metadata. Codec-private memory is
excluded. `decoded_bytes` counts successful decode traffic including revisits;
cache hits count logical item lookups, and `read_spans` counts attempted spans.
Default cache allocations can now be larger within the same budget; reports
show actual allocation, not the minimum one-slot requirement.
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
including scalar non-publication and committed map-prefix checks. Frame-mutex
initialization failure is injected in both resident-output and compressed-sink
paths, with exact cleanup and unchanged sink contents. The LRU fixture checks
one/two/four/eight-slot budgets, cancellation-sensitive serial sums, poisoned
padding, decode traffic and cache-allocation OOM.

`b2nd_exec_admit_partitions` is a separate metadata/header-only capability check:
it performs no payload decoding, source callbacks or numerical execution.
Only a successful `admitted=false` result permits an old wrapper route.
Execution failures, including reader `NotImplementedError`, never authorize
fallback or replay.

These fixtures compile the new owner with warnings as errors and can be built
with ASan/UBSan compiler/linker flags. The independent installed-miniexpr suite
also links this test against the actual installed C-Blosc2 static archive.
