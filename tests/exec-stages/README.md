# Typed staged graph planner and executor

`b2nd_exec_staged_prepare` / `b2nd_exec_staged_prepare_ex` splits a typed graph
into up to `B2ND_EXEC_MAX_STAGES` (32) regions at reductions and strong-dependent
shared computed nodes. It remaps node IDs and input/producer slots directly into
ordinary plans without artifact handles, interchange serialization, or another
numerical traversal loop. Explicit composition via `b2nd_exec_staged_assemble` /
`b2nd_exec_staged_assemble_ex` borrows prepared child plans (including authored
Menudet kernels and resident masked reductions) without copying or re-lowering.
Automatic conditional/masked multi-stage graphs and shared scalar-only
materialization retain the adapter's prepare-time rejections.

Specialization infers each producer dtype/shape through the existing executor,
including accumulator promotion. It calculates cumulative intermediate bytes,
per-stage `last_consumer`, and stage/node diagnostics before execution. A budget
one byte below that requirement is rejected during specialization. Schedules
borrow plans and retain no operand values.

Execution currently accepts resident views and synchronous generic sources,
serially, with resident contiguous output. Reader-backed masked stages and
compressed staged operands reject before any reader callback. All stages' source
metadata, output bindings and gather budgets pass preflight before the first
reader callback. Binding arrays are bounded heap metadata sized by actual child
input counts. Intermediates are invocation-local, released as soon as their
last consumer completes, and cleaned up on every failure. Final map failure can
leave prefix output writes; FP flags are unioned across entered stages,
`fp.supported` and plan `has_jit` combine with AND across observing/component
stages, and `raise_mask` applies after each stage's traversal. Callbacks are
never replayed.

Build against an artifact-off C-Blosc2 and miniexpr installation:

```sh
cmake -S tests/exec-stages -B <build> -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="<blosc2-prefix>;<miniexpr-prefix>"
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```

ASan/UBSan flags can be supplied in `CMAKE_C_FLAGS` / `CMAKE_EXE_LINKER_FLAGS`.
The fixture compiles the staged owner with warnings as errors. Fault injection
covers staged-owner preparation allocations, schedule/intermediate OOM, source
failures across stages, no replay and recovery. The installed consumer fixture
`tests/installed-miniexpr/exec-stages.c` runs the same core staged suite against
the installed artifact-off static archive.

For optional differential qualification, additionally set
`EXEC_STAGES_ADAPTER_LIBRARY` to a matching `libminiexpr_artifact.a` and
`EXEC_STAGES_ADAPTER_INCLUDE_DIR` to its graph-header directory. This adds a
separate adapter comparison target for multi-stage, nested integer, and masked
explicit stage parity. The default fixture and installed C consumer still require
no artifact library.
