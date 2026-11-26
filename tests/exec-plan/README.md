# Internal typed execution boundary

`blosc/b2nd-exec.h` is deliberately separate from the specialized grid pilot
header. It is not installed or a frozen public ABI. The initial descriptor
admits numeric inputs, exact typed/weak captures, add/subtract/multiply/negate
maps and an optional whole-domain minimum. Numerical inference and compilation
are owned by `me_prepare_menudet`; there is no JSON or artifact dependency.
Other operations/options must be added with explicit descriptor validation and
tests; they must not be silently ignored.

Preparation copies nodes and owns the compiled kernel. Specialization copies
same-shape metadata and borrows the immutable plan. Free schedules before plans.
Input values, callbacks and outputs are not part of preparation/specialization.
The initial boundary does not qualify broadcasting, strided inputs, axes,
masks, other reducers, aliases, storage adapters, jobs or stages.

Build/install miniexpr with `MINIEXPR_BUILD_ARTIFACT=OFF`, then:

```sh
cmake -S tests/exec-plan -B <build> -DCMAKE_PREFIX_PATH=<miniexpr-prefix>
cmake --build <build>
ctest --test-dir <build> --output-on-failure
```

Use a Debug build (the fixtures use assertions). The installed-miniexpr suite
also links this fixture against the actual C-Blosc2 static library.
`faults.c` injects failures in planner-owned allocations; core/compiler
allocation coverage stays in miniexpr. Sanitizers can be enabled with the
normal CMake compiler/linker flags.
