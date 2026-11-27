# Typed reduction-to-map stage slice

`b2nd_exec_staged_prepare` splits a typed graph containing exactly one reduction
feeding an arithmetic map. It remaps node IDs and input slots into two ordinary
plans directly, without artifact handles, interchange serialization, or another
numerical traversal loop. It rejects unreachable nodes and multiple/nested
reductions. This is the first native M7 slice, not the complete staged planner.

Specialization infers the producer dtype/shape through the existing executor,
including accumulator promotion. It calculates the producer's bytes and last
consumer before execution. A budget one byte below that result is rejected.
Schedules borrow plans and retain no operand values.

Execution currently accepts resident views and synchronous generic sources,
serially, with resident contiguous output. Both stages' source metadata, output
bindings and gather budgets pass preflight before the first reader callback.
The common binding validator explicitly supports planned stage-owned contiguous
allocations without an address; preflight does not fabricate pointers or
allocate the producer result. Binding arrays are bounded heap metadata sized by
actual child input counts, not two maximum-sized arrays on the worker stack.
The only full intermediate is the explicitly budgeted producer result; it is
invocation-local and released after the consumer, or on any failure. Final map
failure can leave prefix output writes. FP flags are unioned in the report, with
`raise_mask` applied after each stage's traversal, matching the adapter: a
flagged producer must not enter its consumer. Callbacks are never replayed.
`fp_observed` distinguishes absent observation from unsupported FP; staged support
is an AND over observing stages, not an OR. Fault injection checks both merge
orders and an absent observation.

Compressed bindings/sinks, multiple stages/reductions, masks, imported stage
bindings, Python facade integration and default dispatch are not implemented.
The native tests do not establish those gates or cross-platform qualification.

Build against an artifact-off C-Blosc2 and miniexpr installation:

```sh
cmake -S tests/exec-stages -B <build> -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="<blosc2-prefix>;<miniexpr-prefix>"
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```

ASan/UBSan flags can be supplied in `CMAKE_C_FLAGS` / `CMAKE_EXE_LINKER_FLAGS`.
The fixture compiles the staged owner with warnings as errors. Fault injection
covers all staged-owner preparation allocations, schedule/intermediate OOM,
source failures in both stages, no replay and recovery.
The shared-input `x - sum(x)` fixture checks source coverage across both regions,
consumer failure, resident parity and reuse. Empty non-NULL buffer origins are
checked separately from nonempty overlapping allocations. Failed metadata
preflight reports zero producer allocation, while the peak byte counter remains
truthful for buffers actually allocated.

For optional differential qualification, additionally set
`EXEC_STAGES_ADAPTER_LIBRARY` to a matching `libminiexpr_artifact.a` and
`EXEC_STAGES_ADAPTER_INCLUDE_DIR` to its graph-header directory. This adds a
separate adapter comparison target for bitwise values, dtype/rank, stage count
and FP flags/support across tile widths and a late signaling NaN. The default
fixture and installed C consumer still require no artifact library.
