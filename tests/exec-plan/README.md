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

The executor binds native aligned contiguous buffers in slot order.
Same-shape metadata includes rank-zero scalars and ranked empty map domains;
empty minimum remains an error. No broadcasting or strided gathering is implied.
Default whole-domain sum/product/min/max use shared core primitives and
accumulator typing. Serial sums/products carry accumulator state across tiles;
tiles are not independently reduced. Narrow integer sums/products return the
core-resolved 64-bit accumulator dtype while mapped scratch retains map width.
Empty sum/product identities come from the core. Any/all, explicit accumulator
conversions, axes, masks, initial values and pairwise sums remain unadmitted.
Computed maps feeding a reducer use one bounded mapped tile; disjoint pure maps
evaluate directly into the destination with zero mapped scratch. Proven identity maps read their
declared input slot directly without a kernel call. Minimum results publish
only after success. Map results may contain completed tiles after a later
failure, and a failed map call may partially write its tile. Buffer capacities, pointer overflow and output disjointness are
validated before allocation/evaluation. Reports distinguish traversal tiles,
map calls, identity tiles, mapped scratch and reducer state; compiler/core
allocation is not included in the mapped-tile bound.
Host reducer arithmetic uses miniexpr's shared strict FP scope, including
nearest rounding, gradual-underflow validation and caller-state restoration.
Raise masks check the final map/reducer flag union after traversal, matching
the comparison graph executor rather than changing failure timing per tile.

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
Hosts may request partitioned jobs through `b2nd_exec_jobs`. C-Blosc2 owns the
partition boundaries, the aggregate mapped-scratch budget and the ordered
combination; hosts only supply a count. Ordered sums/products reject
partitioning so strict serial leaves cannot be redefined. Order-independent
reductions, identity reductions and disjoint map tiles partition. Partials
combine by logical task ID, never completion order, and every started thread is
joined before return, including on a start failure. Windows currently uses the
serial path only.
