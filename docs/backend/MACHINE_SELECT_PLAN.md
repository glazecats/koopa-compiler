# Machine Select / Target-Lowering Plan

## Goal

The next backend-facing mainline after the current `machine_ir` cleanup
checkpoint is to make `machine_ir` a **real consumed input layer**, not the
end of the story.

The first concrete consumer should be a small instruction-selection /
target-lowering skeleton that:

- consumes canonicalized `MachineIrProgram`
- keeps CFG/function structure explicit
- begins replacing generic machine-ir instructions with selected lower-level
  operations
- stays separate from `machine_ir` so cleanup and selection do not collapse
  into one oversized module

In short:

- `machine_ir` remains the stable machine-placed CFG/value IR
- the new layer becomes the first stage that says
  "given this machine-ir, what are the actual selected low-level ops?"

## Naming Choice

For the first version, prefer the name:

- `machine_select`

instead of adding more weight to `machine_ir` itself or jumping too early to
something broader like `codegen`.

Why this name:

- it describes the immediate job clearly: select/lower machine-ir operations
- it does not overclaim final encoding/emission responsibilities
- it stays neutral about whether the first target is toy, generic, or later
  replaced by a more concrete ISA

## File / Module Boundary

Create a new sibling module:

- `include/machine/select.h`
- `src/machine/lowering/machine_select/`
- `tests/machine/lowering/machine_select/`

Do **not** place this work under:

- `src/machine/lowering/machine_ir/`
- `src/value_ssa_machine/`
- `src/value_ssa_alloc/`

Those layers should remain:

- `value_ssa_alloc`: abstract color / spill decisions
- `value_ssa_machine`: machine-register interpretation/reporting
- `machine_ir`: machine-placed generic IR plus cleanup/canonicalization
- `machine_select`: first real consumer that lowers/selects machine-ir ops

## Recommended Internal File Layout

Start with the same split style the repository already uses:

- `src/machine/lowering/machine_select/machine_select.c`
- `src/machine/lowering/machine_select/machine_select_core.inc`
- `src/machine/lowering/machine_select/machine_select_verify.inc`
- `src/machine/lowering/machine_select/machine_select_dump.inc`
- `src/machine/lowering/machine_select/machine_select_lower.inc`

If the lowering logic grows quickly, split by concern rather than by vague
"misc" buckets:

- `machine_select_lower_value.inc`
- `machine_select_lower_memory.inc`
- `machine_select_lower_control.inc`
- `machine_select_lower_call.inc`

Prefer this over stuffing everything into one `lower.inc`.

Current implementation status:

- the first split is now real
- `machine_select_lower.inc` owns the top-level lowering loop / contract
- selected lowering detail is now split into:
  - `machine_select_lower_value.inc`
  - `machine_select_lower_memory.inc`
  - `machine_select_lower_control.inc`
  - `machine_select_lower_call.inc`

## Why A New Layer Is Better Than Extending `machine_ir`

`machine_ir` currently answers:

- where values live
- what CFG/phi/slot shape survives after allocation
- what cleanup/canonicalization can simplify safely

It should **not** also become the place that answers:

- which concrete low-level op implements one generic binary
- how call argument/result conventions are materialized
- how branch conditions become selected compare/test forms

If we keep growing `machine_ir` in that direction, we lose the boundary
between:

- a stable generic machine-facing IR
- the first target-lowering / instruction-selection consumer

So the new layer should be a **consumer of machine_ir**, not a deeper region
inside it.

## First Minimal Output Surface

The first selected form should still be conservative.

It does **not** need to be final assembly yet.

Recommended first model:

- program / function / block IR retained
- phi-free input expected from canonicalized machine-ir
- explicit low-level selected operations
- explicit operands that still reference:
  - machine registers
  - spill slots
  - immediates
  - local/global slots where lowering is not yet pushed further

That means the first output can still be a structured IR, but one level lower
than machine-ir in operation choice.

## First Minimal Selected Operation Families

Keep the first slice small and useful.

Suggested first selected op families:

- copy / move
- immediate materialization
- integer ALU ops
- compare/test + branch shape
- call
- return
- jump
- local/global load/store

Do not start with:

- full parallel-copy lowering
- full calling-convention lowering
- full frame-layout synthesis
- real encoding/emission

Those belong later.

## First Input Contract

The first lowering entrypoint should consume:

- `MachineIrProgram`

and assume callers can choose whether to feed:

- raw machine-ir
- or canonicalized machine-ir

But the preferred default should be:

- canonicalized, phi-free machine-ir

So the likely high-level flow becomes:

- `value_ssa -> machine_ir`
- `machine_ir canonicalize`
- `machine_select lower`

## First Public APIs

Start with a narrow public surface:

- `machine_select_lower_program_from_machine_ir(...)`
- `machine_select_verify_program(...)`
- `machine_select_dump_program(...)`

Optionally, once the layer is real:

- `machine_select_lower_dump_from_machine_ir(...)`

Avoid adding report/query surfaces too early unless a real consumer needs
them.

## Verification Scope

The verifier should initially lock:

- dense block/op ids if the representation uses them
- valid machine-register references
- valid spill-slot references
- valid slot references
- required block terminators
- CFG reachability
- selected-op operand/result shape invariants

The first verifier should be strict enough that selection bugs fail at the
boundary, not only later.

## Staging

### Stage MS1: Representation Skeleton

- public structs
- lifecycle
- builder helpers if needed
- verifier
- dump
- isolated tests

### Stage MS2: First Lowering From `machine_ir`

- lower one cleaned machine-ir program into selected form
- preserve function/block structure
- support representative:
  - `mov`
  - `binary`
  - `call`
  - `ret`
  - `jmp`
  - `br`
  - `load_*`
  - `store_*`

### Stage MS3: First Real Consumer Boundary

- make one real entrypoint that prefers:
  - `machine_ir canonicalize -> machine_select`
- add focused end-to-end tests on cleaned machine-ir fixtures

### Stage MS4: Early Legalization Pressure

Only after MS1-MS3 are stable:

- begin instruction-shape legalization
- begin simple fixed-operand / compare-branch shaping
- begin simple call-lowering shape rules

This is where the layer starts to feel like real instruction selection rather
than just "machine-ir with renamed opcodes".

## Current Position

- `MS1 representation skeleton`: now effectively **~85%**
  - public `machine_select` structs, lifecycle, verifier, dump, and isolated
    tests are landed as a real sibling module
  - the layer now also has first query/summary surfaces for whole-program and
    per-function inspection, so it is not only a dump-only artifact anymore
- `MS2 first lowering from machine_ir`: now effectively **~92%**
  - a first `machine_ir -> machine_select` lowering path is landed for
    representative `mov`, `binary`, `call`, `load_*`, `store_*`, `ret`,
    `jmp`, and `br`
  - current lowering now also enforces one explicit boundary condition rather
    than silently accepting every machine-ir shape: the first public contract
    requires phi-free machine-ir input and rejects remaining phi-bearing
    programs directly
- `MS3 first real consumer boundary`: now effectively **~100%**
  - the layer now also has a first explicit canonicalized bridge surface,
    so callers can ask for `machine_ir canonicalize -> machine_select lower`
    directly instead of restaging that contract outside the module
  - the layer now also has a first report-driven bridge surface, so
    canonicalized/rewritten machine-ir reports can feed machine_select
    directly without forcing callers to reopen report internals themselves
  - selected ops are now also beginning to differentiate their own policy
    surface instead of mirroring machine-ir opcodes one-to-one: compare-like
    binaries are lowered into a separate `cmp` family while arithmetic/logical
    binaries stay in `alu`
  - selected data movement now also has a first distinct immediate-materialize
    family rather than treating immediate `mov` the same as generic copies
  - that same selected-op shaping now also reaches the first selected control
    terminator family: a compare result that feeds the immediately following
    branch may now lower directly into a dedicated `cmpbr` terminator instead
    of always remaining in the more generic "compute compare result, then
    ordinary branch on that temporary" shape
  - call shaping is now also one step more explicit than raw machine-ir:
    value-returning calls and result-less calls are separated as `call` versus
    `call_void`, and the layer now has its own lower-report artifact above raw
    programs so callers can consume lowered output plus per-function summary
    facts directly
  - that lower-report artifact now also has a first real query/filter surface
    above raw struct-field access: callers can query report summary counts,
    recover the owned lowered program, look up one lowered function by index
    or by name, inspect one per-function summary artifact directly, and filter
    functions carrying calls, spills, memory ops, or branches without
    restaging those scans outside `machine_select`
  - that same lower-report artifact now also has a first real dump surface of
    its own instead of forcing callers to choose between "structured artifact
    only" and "raw selected program dump only": callers can dump the report
    artifact directly, or ask for a direct `machine_ir program/report ->
    machine_select report dump` path, and the textual report now includes
    program-level filter counts, filtered function index sets, per-function
    summaries, and the owned lowered program in one backend-facing artifact
  - the report artifact now also has first block-level summary/query coverage
    instead of stopping at function-level summaries: callers can query one
    lowered block summary directly from the report, and the report dump now
    includes per-block shape rows under each function summary, so later
    backend consumers can navigate selected call/memory/branch shapes at both
    function and block granularity without rescanning the raw selected program
  - on top of that, the per-function summary itself now also carries one
    first block-aggregation layer (`blocks_with_calls`,
    `blocks_with_memory_ops`, `branch/jump/return block counts`) instead of
    forcing every consumer to reconstruct those rollups from block rows only,
    so function-level and block-level report navigation are now materially
    more symmetric
  - the raw lowered program side now also has a more consumer-facing
    navigation surface instead of forcing every caller back through index-only
    table walks: whole-program function lookup by name, per-function block
    lookup by block id, and block summary queries are now public, which means
    both the report artifact and the owned selected program now expose stable
    navigation helpers rather than only dumps and direct field access
  - that same report-facing bridge now also reaches one more canonicalized and
    caller-facing tier instead of stopping at raw `machine_ir` programs:
    callers can build/dump a selected lower-report directly from a
    canonicalized `machine_ir` program, or build/dump the same selected report
    directly from the current single-block-spill canonicalized-flat
    `ValueSsa -> machine_ir` bridge, so report-oriented downstream consumers
    no longer need to hand-restage those bridge artifacts before entering
    `machine_select`
  - report-side navigation is now also slightly more symmetric with the raw
    selected-program side: callers can recover one function summary directly
    by function name and one block summary directly by `block_id`, rather than
    being forced to round-trip through function/block indices every time
  - the selected-program side now also has a first self-hosted report
    lifecycle instead of requiring every later consumer to bounce back up to
    `machine_ir` bridge entrypoints: callers can clone one existing
    `MachineSelectProgram`, build/dump a fresh selected report directly from
    it, and refresh an owned `MachineSelectLowerReport` after local mutations
    to recompute function/block/filter summaries in place
  - that report lifecycle now also exposes total block-summary count directly
    on the report summary query surface, so report-oriented consumers can size
    later block-summary walks without reopening report internals
  - that same selected-report surface now also carries one first explicit
    downstream-compatibility policy summary instead of leaving current preview
    lane expectations implicit: callers can query the current RISC-V preview
    logical-register cap plus whether selected lowering already supports early
    immediate legalization, compare-branch fusion, spill-operand preservation,
    and global-slot preservation for later address formation
  - that same selected-side compatibility surface now also has a first
    explicit verifier entrypoint for the current preview lane instead of
    deferring every incompatibility to `machine_bytes`: callers can now ask
    `machine_select` directly whether a selected program/report is compatible
    with the current `riscv32-preview` lane, and oversized logical register
    banks fail at the selected boundary rather than only downstream
  - that same selected-side compatibility surface now also has a first deeper
    downstream bytes-bridge check instead of only local shape screening:
    callers can now ask `machine_select` to validate current
    `riscv32-preview` compatibility by actually attempting the downstream
    `machine_select -> machine_layout -> machine_emit -> machine_encode ->
    machine_bytes` preview path, so bytes-side range failures surface at the
    earliest selected boundary
  - selected control flow now also has a first structured query surface above
    raw terminator unions and dump text: callers can ask one raw selected
    block or one report-owned selected block for a structured terminator
    summary (return value, branch condition, compare op, and target block ids),
    and report-side block/terminator lookup now also reaches direct
    `function_name + block_id` navigation instead of stopping at index-based
    access plus manual joins
  - that same block-navigation story now also reaches raw selected programs at
    the whole-program layer instead of stopping at "find function, then find
    block" two-step call sites: callers can now resolve one raw selected block
    directly by `function_name + block_id`, and the report side mirrors that
    same navigation path for raw block views too rather than only for summary
    artifacts
  - that same whole-program raw navigation now also reaches the structured
    block-inspection tier instead of stopping at raw block handles alone:
    callers can ask a selected program directly for one block's structured
    block-shape summary or terminator summary by `function_name + block_id`
    without reopening the intermediate function/block walk themselves
  - function-summary convenience is now also more symmetric across raw program
    and report/filter entrypoints instead of forcing later consumers to bounce
    back to raw function handles manually: a selected program can now produce
    one computed function summary directly by function index or function name,
    and report-side filter navigation can now advance from one filtered match
    directly to the corresponding function view plus cached function summary
  - that report-filter navigation now also reaches direct by-name lookup
    instead of forcing every consumer to first pull an index set and then scan
    it by hand: callers can ask each current filter family (`calls`, `spills`,
    `memory_ops`, `branches`) for the filtered entry matching one function
    name and recover both the filtered position and the corresponding cached
    function summary in one step
  - function-level consumer entrypoints are now also less split between
    "function view" and "function summary" on both raw and report sides:
    callers can now recover a function artifact in one step
    (`function + summary`) by function index or by function name, rather than
    stitching those two lookups together manually at every call site
  - block-level consumer entrypoints are now also less split between "block
    view", "block summary", and "terminator summary" on both raw and report
    sides: callers can now recover a block artifact in one step
    (`function + block + block-summary + terminator-summary`) by
    `function_name + block_id`, rather than chaining several lookups together
  - report-side block triage now also has a first direct per-function filter
    surface above raw block-summary scans: callers can now ask one function
    report for the count of call/memory/return/jump/branch/cmp-branch blocks
    and then recover the matching filtered block summaries directly, including
    the same lookup by function name on top of that filtered per-function view
  - that same filtered per-function block view now also reaches full block
    artifacts instead of stopping at block summaries alone: callers can now
    recover the filtered raw block, filtered cached block summary, and
    filtered terminator summary together from one filtered block query path
  - data-movement shaping is now also one step more explicit than raw
    machine-ir `mov`: immediate materialization is separated from generic
    copies, so the lowering surface now already distinguishes copy, imm,
    alu, cmp, cmp-branch, call, and call-void families
- `MS4 early legalization pressure`: now effectively **~99%**
  - immediate-bearing arithmetic / compare / compare-branch shapes now lower
    into separate `alui`, `cmpi`, and `cmpbri` families instead of remaining
    inside only the generic selected forms
  - memory-shape selection has now also started to split immediate stores
    away from generic memory stores, via `store_locali` / `store_globali`
  - simple constant-condition branches now fold to direct jumps inside
    `machine_select`, so the first control-legalization pressure is no longer
    only compare-result fusion
  - constant binary instructions with immediate/immediate inputs now lower
    straight to selected immediate materialization instead of surviving as
    selected `alu` / `cmp` families
  - control shaping now also eats one more local fact shape beyond raw
    compare fusion: both constant compare-branches and tail materialized
    boolean branches can collapse directly to `jmp`
  - the layer now also has a first explicit block-local cleanup stage after
    lowering, so selected output is no longer only whatever the direct
    lowering pass spelled literally
  - that cleanup stage can forward tail `copy` / `imm` facts into terminators
    and erase trivial temporary defs when they exist only to feed the block
    terminator
  - that same cleanup stage is now no longer terminator-only: it can also
    forward trivial tail `copy` / `imm` facts into the final real consumer
    op in the block, including selected `store`, `alu`, and `call` shapes
  - once those facts are forwarded, the selected op family itself may now
    tighten again locally, for example by upgrading a last-use `alu` into
    `alui` or a last-use `store` into its immediate-bearing family
  - the local cleanup logic is now no longer only "last consumer" flavored:
    it can walk the whole block for unique-consumer trivial defs, so multi-hop
    local copy/immediate chains can collapse into their eventual selected use
  - after local forwarding, the cleanup stage now also removes dead pure value
    ops that no longer feed any selected op or terminator, so the pass is
    beginning to behave like a real block-local selected cleanup pipeline
  - the cleanup policy is now also more structurally correct around register
    redefinitions and visible-use boundaries: propagation is no longer just a
    naive tail heuristic, and trivial defs may now feed multiple visible uses
    up to the next redefinition point
  - call shaping is now no longer only an incidental side effect of operand
    forwarding: the selected layer has started to split an explicit immediate-
    argument call family (`calli` / `call_voidi`) with verifier, dump, query,
    and regression coverage
  - call/result shaping is now also starting to express where the result lands
    as part of the selected family surface, for example separating
    register-result versus spill-result call families instead of treating all
    value-returning calls as one undifferentiated bucket
  - return shaping is now also split into explicit selected families, so the
    call/result line is no longer one-sided: immediate-return, spill-return,
    and register-return shapes now have their own verifier/dump/query surface
  - immediate-side legalization now also begins earlier than the old
    cleanup-only checkpoint: lowering itself canonicalizes commutative
    immediate operands onto the rhs, flips ordered compares like `4 < x`
    into rhs-immediate legal forms like `x > 4`, and keeps noncommutative
    lhs-immediate cases out of `_IMM` families instead of letting later
    consumers guess what the opcode spelling meant
  - verifier authority now also locks that convention explicitly:
    `alui`/`cmpi`/`cmpbri` families are only legal when the rhs is the
    immediate side, so downstream RISC-V-facing consumers can treat selected
    immediate families as structurally meaningful rather than as soft hints
  - lowering internals are now split by concern, which makes it much safer to
    keep widening selected families without turning one file into a sinkhole

In short:

- the module boundary is real
- the first lowering slice is real
- the first verifier/query contract is real
- the first canonicalized bridge is real
- the first report bridge is real
- the first selected data-movement split is real
- the first selected control-shape rewrite is real
- the first selected call-shape split is real
- the first machine_select-native report artifact is real
- the first early selected-form legalization pressure is real
- the historical next step was to keep widening selected families from the
  already-split lowering boundary, but that widening is no longer the default
  backend mainline now that downstream siblings through `machine_elf` exist

## Maintenance Stance

- `MS1`-`MS3` should now be treated as checkpoint-ready / historical by
  default.
- `MS4` should now be treated as checkpoint-close and maintenance-first:
  reopen it for confirmed selected-lowering/legalization regressions, a
  concrete downstream consumer need, or a deliberately chosen selected-form
  widening pass, not as the default backend mainline.
- For the current reopened RISC-V correctness line specifically, this stage
  is now best read as an upstream support sibling that is effectively
  **~99% of the currently chosen immediate/call-shaping slice**: the next
  likely work should stay downstream in `machine_bytes` / relocation / ELF
  unless a newly exposed selected-form bug reopens `machine_select` again.
- Default backend progress reporting should no longer fall back to `MS*`
  stage names once the active downstream checkpoint has moved to later sibling
  plans such as `machine_layout`, `machine_emit`, or `machine_elf`, unless
  work has actually reopened `machine_select` itself.

## Recommended First Implementation Order

1. Create the new module boundary only.
2. Land skeleton + verifier + dump.
3. Lower a tiny representative subset from machine-ir.
4. Add end-to-end tests using already-cleaned machine-ir fixtures.
5. Only then widen op coverage and legalization behavior.

## Testing Strategy

Prefer three levels:

1. Manual selected-form construction tests
2. `machine_ir -> machine_select` exact dump regressions
3. Small end-to-end integration tests from the existing higher-level bridge

Use new files under:

- `tests/machine/lowering/machine_select/`

Do not keep growing `tests/machine/lowering/machine_ir/` to cover selection behavior.

## File Management Rules For This Next Line

- Keep all new selection/lowering implementation under `src/machine/lowering/machine_select/`
- Keep all new public declarations under `include/machine/select.h`
- Keep tests under `tests/machine/lowering/machine_select/`
- Keep `machine_ir` changes limited to:
  - bug fixes
  - tiny consumer-enabling query helpers
  - no large new selection logic
- If target-specific rule tables appear later, add another split layer under
  `src/machine/lowering/machine_select/` first before considering a brand-new target directory

## Historical Hand-Off

The earlier recommendation to start the next implementation phase at
`machine_select` has already been carried out.

Treat this document as the historical hand-off from:

- `machine_ir` cleanup / canonicalization

into the later backend sibling chain:

- `machine_layout`
- `machine_emit`
- `machine_encode`
- `machine_bytes`
- `machine_object`
- `machine_reloc`
- `machine_container`
- `machine_elf`

Default next work should therefore prefer:

- bug-driven or consumer-driven reopening inside `machine_select`
- or downstream backend planning/reporting from the later sibling plans

rather than treating `machine_select` itself as the current default mainline.
