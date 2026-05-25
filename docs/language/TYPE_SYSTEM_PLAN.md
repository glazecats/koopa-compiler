# Type System Plan

## Goal

- Unify the current conservative composite-type work into one shared type/compatibility layer.
- Keep the next feature step compatible with the existing `-extension` pipeline.
- Use that shared layer as the bridge toward `float` and richer type checking later.

## Why Now

- `pair` and `struct` are already both landed, but they still live as mostly separate semantic slices.
- The next useful feature work is no longer “another shape”, but the shared machinery that makes future shapes cheaper and safer.
- A unified type layer will also make later higher-order / function-valued work less ad hoc.

## Target Scope

- shared type descriptors for:
  - scalar `int`
  - `void`
  - function values / callable signatures
  - builtin aggregates such as `pair`
  - named aggregates such as `struct`
- shared compatibility checks for:
  - declaration
  - initialization
  - assignment
  - return
  - parameter passing
  - field/member lookup
- conservative extension-only lowering, with no new ABI object model yet

## First Landing Shape

- keep the first slice mostly semantic/front-end facing
- factor out aggregate compatibility and field lookup where possible
- keep lowering conservative and explicit
- reject anything that would require a real closure/object ABI too early

## Current Implementation Direction

- treat `pair` and named `struct` as two aggregate kinds over one shared semantic helper layer
- keep the shared layer conservative:
  - same-kind assignment only
  - field lookup only for known local aggregates
  - initializer-list bounds checked against the declared field count
  - invalid field names rejected consistently across builtin and named
    aggregates
- keep aggregate member lookup in one shared helper path rather than
  separate pair/struct validators, so the later scalar-type broadening only
  has one place to extend
- keep shared member lookup covered by both positive and negative regressions
  so builtin and named aggregates stay aligned
- keep the aggregate-value identifier lookup helper shared too, so member and
  value-position checks draw from the same declaration-kind/type-name facts
- keep local aggregate member lookup and whole-value assignment checks on one
  shared scope-side aggregate descriptor path, so pair/struct handling does
  not drift back into separate ad hoc branches
- keep helper naming/type flow aggregate-oriented rather than pair-specific,
  so later scalar-type broadening does not inherit misleading special-case APIs
- keep aggregate parameters/returns explicitly out of scope for now, with
  honest parser/driver diagnostics instead of misleading top-level-declaration
  failures
- keep the remaining aggregate diagnostics and regression strings aligned on
  the shared aggregate vocabulary, so `pair` and `struct` stay covered by one
  semantic surface
- do not introduce a new runtime aggregate ABI just to make the type layer feel cleaner

## Current Checkpoint

- the shared descriptor/compatibility layer is now broad enough to carry a
  first conservative `float` slice under `-extension`
- current landed `float` boundary:
  - declarations / parameters / returns may use `float`
  - direct identifier/call transport of float values is allowed
  - scalar call-argument checking now distinguishes `int` vs `float`
- current lowering follow-up:
  - visible float metadata now survives through canonical IR, lower IR,
    ValueSSA, machine IR, and the conservative machine-select path
- current literal follow-up:
  - `1.25`-style literals now participate in the same conservative float
    transport story for locals, call arguments, returns, and top-level
    globals, with top-level globals flowing through the existing runtime-init
    helper path rather than opening a separate static-float object model
- current report-surface follow-up:
  - the same transport-only float metadata now also survives through the
    translation-only / program-only machine-IR report path and the matching
    conservative machine-select report path, so report dumps stay aligned with
    the already-landed lowering surfaces
- latest top-level-init follow-up:
  - top-level same-type float runtime initialization now also has explicit
    coverage beyond the literal-only seed case: `float h = g;` and
    `float h = id(g);` are both accepted and regression-locked through the
    conservative runtime-init transport path, including the expected legal
    downstream folds where default optimized `ValueSSA`, `machine_ir`, and
    `machine_select` layers collapse them to immediate global stores
- current query-surface follow-up:
  - focused regression coverage now also exercises report-side query access on
    the conservative float path, so machine-IR report program/function/shape
    queries, the downstream machine-select report bridge, and the first
    machine-select report query surface no longer rely only on textual dumps
    to keep float metadata visible
- latest assignment follow-up:
  - plain scalar assignment now participates in the same conservative float
    transport story as declarations/returns/calls: same-type direct transport
    such as `float y; y = id(g);` is accepted, while `int x; x = g;` now
    rejects explicitly as `SEMA-TYPE-006` instead of falling through to the
    broader derived-float-expression rejection path
  - the default optimized `ValueSSA -> machine_ir -> machine_select` path is
    now also better understood on a live assignment witness:
    `float mainf(){ float y; y = id(g); return y; }` may be legally folded
    down to a direct `load_global g` return, and that optimized transport
    shape is now regression-locked rather than being mistaken for a dropped
    float path
- latest return/global-flow follow-up:
  - direct float return transport sourced from a global (`float get(){ return g; }`)
    and from a same-type float call on that global (`float get(){ return id(g); }`)
    are now regression-locked too, and the default optimized downstream
    reports were confirmed to legally fold the latter to the same direct
    `load_global g` return shape when the call remains transport-only
  - that same return/global-flow slice is now also locked one stage deeper on
    the default optimized machine-report surfaces, so the current conservative
    transport story is no longer only a front-end/translation-only fact
- latest parameter-forward follow-up:
  - same-type float parameter forwarding is now also confirmed on the default
    optimized `ValueSSA` path for both direct forwarding
    (`float forward(float x){ return id(x); }`) and one-hop local forwarding
    (`float bounce(float x){ float y; y = x; return id(y); }`), and both may
    legally collapse to a direct `load_local x` return shape
  - that same parameter-forward slice is now also covered at the user-facing
    compiler-driver surface, not only in semantic and default-ValueSSA probes
- latest call-chain follow-up:
  - one more transport-only family is now regression-locked on the
    semantic / IR / lower-IR / translation-only ValueSSA path:
    a global-fed chain (`float getg(){ return wrap(g); }`) and a local-fed
    one-hop chain (`float bounce(float x){ float y; y = x; return wrap(y); }`)
    both preserve the expected explicit call-chain transport before any later
    default-pipeline folding
  - the global-fed chain now also has one first default-ValueSSA lock:
    `float getg(){ return wrap(g); }` may legally fold to a direct
    `load_global g` return shape after optimization
  - downstream machine-report probes now also agree with that same fold on the
    global-fed chain, so the current open question is coverage breadth rather
    than uncertainty about the optimized shape
  - the same family is now also covered at the compiler-driver surface for
    both the global-fed and local-fed source shapes
- current kept restriction:
  - float operators and other derived float expressions are still rejected
    explicitly; this is not yet a true floating-point arithmetic slice
  - the next preferred broadening after this checkpoint is now documented
    separately as an explicit-conversion step rather than as more implicit
    type drift:
    [docs/language/FLOAT_EXPLICIT_CONVERSION_PLAN.md](/workspaces/compiler_lab/docs/language/FLOAT_EXPLICIT_CONVERSION_PLAN.md)
  - latest explicit-conversion follow-up now landed:
    explicit scalar conversion is now available in both deliberate directions
    under `-extension`: `int(float_expr)` lowers through `__builtin_f2i32`
    and `float(int_expr)` lowers through `__builtin_i2f32`
  - current kept boundary after that follow-up:
    this still does not open implicit conversion, promotion lattices, or
    redundant same-type conversions; `int(3)` / `float(g)` continue to reject
    through `SEMA-EXT-038`
  - latest value-producing follow-up now landed:
    same-type float ternary values are now also accepted on the focused
    front-half surface for return / assignment / initializer families
  - latest float-callarg follow-up now landed:
    that same same-type ternary-value slice now also extends one narrow value
    context further: `float get(){ return wrap(g ? h : h); }` and the unary
    call-root sibling `float get(){ return wrap(-id(1.0) ? 1.0 : 2.0); }`
    are now regression-locked through semantic / compiler / IR / lower-IR
    plus the downstream default `ValueSSA` / `machine_ir` /
    `machine_select` surfaces
  - current optimized-shape note after that follow-up:
    the downstream default path does not promise to preserve an explicit
    float call-argument transport shape forever. The global-fed witness may
    still keep ternary structure while legally collapsing the `wrap(...)`
    call itself away, and the unary-call-root witness may fold all the way to
    a direct float-literal return. Those folds are now treated as the current
    expected checkpoint, not as regressions
  - latest boundary-repair follow-up:
    the neighboring still-closed family is now explicitly back under control
    too: same-type float ternary values may feed a same-type float call
    argument directly, but they still may not first pass through a later
    float arithmetic/comparison family such as
    `wrap((g ? h : h) + h)` or `(g ? h : h) == h`
  - latest gate-hardening note:
    that repair required tightening the shared semantic float-usage gate on
    two fronts: ternary-valued float operands are now rejected again when
    nested under later float arithmetic/comparison operators, and float call
    expressions now recurse into their arguments for float-usage validation
    instead of treating “call returns float” as blanket permission for all
    float subexpressions in the argument list
  - current kept boundary after that follow-up:
    mixed ternary branches such as `g ? h : 0`, and ternary-float values
    immediately feeding later float arithmetic families, still remain closed
- latest post-transport boundary cleanup:
  - arrays of `float` are now also explicitly rejected in the current
    extension slice for local declarations, top-level declarations, and
    function parameters via `SEMA-EXT-037`, so the implementation no longer
    silently drifts into an undesigned float-array surface
  - that float-array boundary is now covered at least on the semantic and
    user-facing compiler surfaces, which is enough to keep the current slice
    honest while later post-transport planning decides whether float arrays
    deserve a real design instead of an accidental lowering path
- first post-transport feature now checkpoint-complete:
  - direct float values may now be used as control-flow conditions in
    `if`, `while`, and `for`, but only for direct transport shapes
    (identifier / direct call / float literal). Lowering currently treats the
    float bit-pattern as truthy exactly when `(bits & 0x7fffffff) != 0`,
    so both `+0.0` and `-0.0` are false and all other direct float values are
    true. Derived float expressions such as `g + 1` remain rejected.
  - that direct-condition slice is now regression-locked end-to-end across
    semantic, compiler-driver, canonical IR, lower IR, translation-only
    ValueSSA, and the default optimized `machine_ir` / `machine_select`
    report surfaces, so it should be treated as a closed checkpoint rather
    than an in-progress semantic broadening experiment
- current next incremental slice:
  - the conservative transport checkpoint plus its first two
    post-transport control-flow slices are now in place
  - latest follow-up now landed:
    direct-float condition support now also extends through the
    condition-composition family for logical condition structure
    (`!g`, `g && h`, `g || h`, and ternary-condition positions such as
    `g ? 1 : 0`)
  - latest deeper regression follow-up:
    that condition-composition slice is now also locked on the default
    optimized downstream path: focused `ValueSSA`, `machine_ir`, and
    `machine_select` regressions all accept the same source-driven witness and
    record the current legal optimized shapes instead of assuming the earlier
    translation-only truthiness bridge must stay textually visible forever
  - current kept boundary after that follow-up:
    float values still may not participate in derived value-producing
    operator families such as `g + 1` or float-valued ternary branches such as
    `g ? h : 0`; this step broadens condition composition, not general float
    arithmetic or generic float-valued expression support
  - latest comparison follow-up now landed:
    explicit float comparisons now also work under `-extension` for same-type
    scalar operands, including equality/inequality and the relational family
    (`float ==/!=/< /<=/>/>= float`)
  - latest signed-literal follow-up now landed:
    negative float literals are now treated as part of the same conservative
    direct transport story for the first version, so shapes such as `-1.25`
    and direct condition positions such as `if(-0.0)` no longer depend on
    accidentally falling into unsupported general float arithmetic
  - latest unary-sign follow-up now landed:
    that same conservative transport boundary now also extends through direct
    unary sign transport on float roots, so shapes such as `-g`, `+g`, and
    `-id(g)` are supported without opening generic float arithmetic
  - latest arithmetic follow-up now landed:
    the first true float-expression slice is now open in a narrow same-type
    form: `float + float` and `float - float` on direct float roots are
    accepted under `-extension`
  - current arithmetic-lowering note:
    this first arithmetic slice is intentionally lowered through explicit
    helper calls (`__builtin_fadd32` / `__builtin_fsub32`) rather than
    pretending the current IR/backends already have a native float ALU model
  - latest arithmetic-composition follow-up:
    that helper-lowered arithmetic slice now also composes with the earlier
    unary-sign transport path on focused witnesses such as `-g + y` and
    `y - -g`, so add and sub are no longer checked only on the easiest
    positive-parameter forms
  - latest deeper arithmetic follow-up:
    focused default-path `ValueSSA` coverage plus downstream `machine_ir` /
    `machine_select` report witnesses now also accept the helper-lowered
    `float + float` / `float - float` slice on both the plain two-parameter
    and unary-sign-composition witnesses
  - latest arithmetic-boundary follow-up:
    the same first arithmetic slice now also has an explicit reject split:
    mixed scalar arithmetic such as `float + int` rejects as `SEMA-TYPE-008`,
    while unsupported derived float-expression families such as `(x + y) + z`
    and the already-kept top-level `g + 1` family still reject through
    `SEMA-EXT-035`
  - latest mixed-root arithmetic follow-up:
    that same `SEMA-TYPE-008` split now also applies across more than one
    direct-root family on the live implementation: local direct roots remain
    covered, float-literal roots such as `1.25 + 1` now follow the same rule,
    direct call roots such as `id(x) + 1` now also resolve to the same
    arithmetic type-mismatch boundary, and unary-sign call-root shapes such as
    `-id(x) * 1` now match it too in direct semantic/compiler probes, while
    the deliberately kept top-level-global `g + 1` family still stays on
    `SEMA-EXT-035`
  - latest reject-matrix follow-up:
    that broader mixed-root split is now also locked on more than just direct
    probes: focused compiler / default-`ValueSSA` / `machine_ir` /
    `machine_select` regressions now cover the local-id, float-literal,
    direct-call, and unary-sign-call mixed-arithmetic families together
  - latest helper-backed arithmetic broadening:
    the next narrow operator pair is now also open under the same
    conservative contract: same-type `float * float` and `float / float` on
    direct float roots are accepted, and current lowering keeps the same
    explicit helper-call honesty boundary through `__builtin_fmul32` and
    `__builtin_fdiv32`
  - latest multiplication/division composition follow-up:
    that same helper-backed `*` / `/` slice already composes with unary-sign
    transport on focused source shapes such as `-g * y` and `y / -g`, so the
    new operators are not limited only to the simplest positive parameter pair
  - latest multiplication/division regression follow-up:
    focused semantic / compiler / IR / lower-IR coverage is now in place for
    the plain and unary-sign-composition `*` / `/` witnesses, and the same
    helper-call shape is also locked on the focused default-`ValueSSA` path
    plus the downstream `machine_ir` / `machine_select` report path
  - latest recursive helper-backed arithmetic follow-up:
    the same same-type helper-backed arithmetic line is now also known to
    compose across pure float expression trees in direct probes, for example
    `(x + y) + z` and `-a * (b / c)`. Current authority is intentionally
    narrower than the earlier direct-root checkpoint, though: semantic-focused
    regressions plus direct compiler / IR / lower-IR / default-`ValueSSA`
    probes now agree on those shapes, and the downstream default
    `machine_ir` / `machine_select` report surfaces now lock the same helper
    call tree too. Full compiler/IR/lower-IR aggregate-runner cleanliness is
    still pending, so the remaining work is breadth/runner cleanup rather
    than core lowering uncertainty
  - current recursive-surface note:
    the recursive helper-backed arithmetic line should now be treated as
    implementation-real rather than hypothetical. The remaining work is to
    convert that direct-probe confidence into broader stable regression locks,
    not to re-prove that the core semantic/lowering path can form the helper
    call tree
  - current comparison-lowering note:
    lowering now normalizes signed zero before comparison so `+0.0` and
    `-0.0` compare equal, and relational comparisons use a conservative
    integer order-key mapping on the normalized bit pattern rather than
    pretending this repository already has a full native float arithmetic IR
  - latest deeper regression follow-up on that comparison slice:
    focused semantic / compiler / IR / lower-IR coverage is now in place, and
    focused default-path locks now also cover `ValueSSA`, `machine_ir`, and
    `machine_select` on source-driven float equality and relational witnesses
  - current kept boundary after that comparison follow-up:
    mixed scalar comparisons such as `float == int` still reject explicitly,
    and this is still a conservative comparison slice rather than a claim that
    the language now has full float arithmetic or a complete IEEE feature set
  - next nearby investigation:
    continue broadening expression support without weakening the current
    same-type and direct-root boundaries, or open another narrowly helper-
    backed float feature only if the current transport/arithmetic slice stays
    regression-closed

## Non-Goals For The First Slice

- full C type equivalence
- true float arithmetic / general conversions / derived float expressions
- pointers
- arrays of aggregates as a general ABI feature
- capturing closures
- generic user-defined structs beyond the current conservative surface

## Follow-Up Direction

1. broaden the scalar type system
2. add `float`
3. then revisit richer callable / higher-order support
