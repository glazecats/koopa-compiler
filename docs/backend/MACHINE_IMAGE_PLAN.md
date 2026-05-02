# Machine Image / Load-Image Prep Plan

## Goal

The next backend-facing mainline after `machine_elf` is to turn the current
ELF object boundary into a **load-image preparation layer**.

This stage should answer:

- what loadable byte span and virtual base address does the current text image
  occupy?
- which symbol virtual addresses are already knowable without a full linker?
- which relocation sites are already resolved to in-image targets, and which
  still remain unresolved external obligations?

In short:

- `machine_elf` decides object-format structure and target-profile object policy
- `machine_image` decides first load-image view over that object

## Module Boundary

Create a new sibling module:

- `include/machine/image.h`
- `src/machine/runtime/machine_image/`
- `tests/machine/runtime/machine_image/`

Do not fold this into:

- `src/machine/object/machine_elf/`
- `src/machine/object/machine_container/`

`machine_elf` should remain the object-format authority.
`machine_image` should become the first load-image / entry-address authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineElfFile`
- project the current `.text` section into one first load segment
- assign a profile-owned base virtual address
- surface virtual addresses for image-defined text symbols
- surface relocation sites as resolved-in-image or unresolved-external
- select one entry symbol/address when a suitable global function exists

Do not start with:

- full linker symbol resolution across multiple objects
- real relocation patch application with ISA-accurate encodings
- final executable program headers
- process image / loader semantics

## First Public APIs

- `machine_image_build_from_machine_elf_file(...)`
- `machine_image_build_from_machine_ir_report(...)`
- `machine_image_verify_file(...)`
- `machine_image_dump_file(...)`

## Staging

### Stage MI1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MI2: First Lowering From `machine_elf`

- first load segment
- base virtual address policy
- entry-symbol selection
- symbol virtual-address surfacing
- relocation-site resolved/unresolved surfacing

### Stage MI3: First Upstream Bridge

- `machine_ir -> ... -> machine_elf -> machine_image`

## Current Position

- `MI1 representation skeleton`: now effectively **~100%**
  - the first `machine_image` sibling is landed with public structs,
    lifecycle, verifier, dump, query helpers, and isolated regression
    coverage
  - the first surface now also exposes owned flat image bytes plus direct
    segment/symbol/relocation lookup rather than only one dump path
  - the module now also has a first structured report artifact above the raw
    image file, with cached header / entry-symbol / segment / symbol /
    relocation summaries plus resolved-versus-unresolved relocation index
    sets, so later image-facing consumers no longer need to rescan the raw
    artifact every time
  - that same first slice now also has a basic artifact lifecycle of its own:
    image files can be cloned, image reports can be refreshed from their
    report-owned image file after local edits, and direct raw/report dump
    helpers now exist from image files, ELF files, ELF reports, and ELF bytes
- `MI2 first lowering from machine_elf`: now effectively **~99.95%**
  - one first lowering path from verifier-legal `machine_elf` is landed
  - current lowering projects `.text` into one load segment with a
    profile-owned base virtual address
  - image-defined text symbols now surface stable virtual addresses, and the
    current image layer chooses one entry symbol/address when a suitable
    global text function exists
  - relocation sites now also surface one first resolved-versus-unresolved
    load-image view: in-image text targets resolve to virtual addresses,
    while unresolved externals remain explicit unresolved image obligations
  - raw image-side navigation is now also more address-oriented than the
    first cut: callers can locate the segment covering one virtual address,
    find the first defined symbol at one virtual address, find the first
    relocation site at one site virtual address, and walk resolved versus
    unresolved relocation subsets directly instead of hand-filtering every
    relocation each time
  - the same lowering boundary now also exposes explicit target-policy
    summaries above both raw image files and structured image reports, so
    later consumers can query the current profile/base-address/alignment
    policy without reopening `machine_elf` policy helpers directly
  - raw/report-side segment-local consumer access is now also more explicit
    than the first all-table-scan cut: callers can ask one segment for its
    defined-symbol subset and relocation subset by count plus filtered index
    instead of rewalking the whole image symbol/relocation tables every time
  - that same segment-local view is now also stronger on the report side than
    the first query-only cut: structured image reports cache segment-local
    symbol/relocation index views, including resolved-versus-unresolved
    relocation subsets per segment, so later consumers do not need to
    repeatedly rebuild those filtered partitions from the full image tables
  - that same report-side segment view is now also promoted into a first
    reusable artifact shape rather than only a bag of parallel queries:
    callers can recover one segment artifact with its summary plus cached
    symbol/relocation/resolved/unresolved subviews and continue inspection
    from that boundary directly
  - that same report-side consumer graph is now also more object-like than
    the first segment-only artifact cut: callers can continue from the cached
    report into symbol artifacts and relocation artifacts, including
    ownership/target context such as source segment, target symbol, and
    resolved target segment when available
  - that same symbol-facing side is now also less one-directional than the
    first artifact graph cut: symbol artifacts and report-side symbol queries
    now expose incoming relocation subsets too, including resolved versus
    unresolved partitions, so later consumers can navigate both
    `relocation -> target symbol` and `symbol -> incoming relocations`
  - that same report layer now also has a first whole-image overview artifact
    plus cached symbol-class partitions such as defined/undefined,
    global/local, and defined-global/undefined-global, so downstream
    navigation no longer needs to reconstruct those common top-level slices
    outside the module before drilling down into per-symbol/per-segment views
  - that same whole-image overview layer now also reaches relocation-class
    partitions such as resolved/unresolved and call/control families, so
    top-level image-consumer navigation can start from the common relocation
    slices too rather than reconstructing them from the whole relocation
    table outside the module
  - the structured report is now also reflected in the dump surface rather
    than only in hidden query APIs: `machine_image_dump_report(...)` and the
    direct report-dump bridge entrypoints now emit report-only overview,
    symbol-filter, relocation-filter, and segment-cache sections on top of
    the raw image dump, so text-mode consumers can benefit from the cached
    report structure too
  - the same consumer boundary now also reaches ELF bytes directly rather
    than only in-memory ELF artifacts: callers can build raw images,
    structured image reports, and direct image dumps straight from ELF bytes
  - profile-aware rebuild/reprojection is now also a first-class part of the
    public lowering surface: callers can continue from existing image files,
    ELF files, ELF reports, or ELF bytes while explicitly choosing a target
    profile for the resulting load-image view
  - current authority is intentionally still conservative: relocation sites
  are surfaced structurally, not yet patched back into ISA-accurate final
  executable bytes
- `MI3 first upstream bridge`: now effectively **~99.99%**
  - a first direct bridge is landed from `MachineIrAllocateRewriteReport`
    through the current ELF pipeline into `machine_image`
  - that same bridge story now also reaches existing ELF artifacts more
    directly than the first raw-only cut: callers can continue from both
    `MachineElfFile` and `MachineElfReport` into either raw image files or
    structured image reports without reopening upstream internals by hand
  - that same bridge story now also reaches the serialized post-ELF boundary
    itself: callers can continue from ELF bytes into raw image files,
    structured image reports, or direct image dumps without staging an
    explicit intermediate `MachineElfFile`
  - that same direct-bridge story now also reaches one more artifact tier on
    both sides of the image boundary: callers can continue from raw image
    files, ELF files, ELF reports, or ELF bytes into either raw image dumps
    or report-owned image dumps without rebuilding that glue outside the
    module
  - that same bridge story now also reaches the profile-aware variants of
    those artifact surfaces, including direct raw/report build and
    raw/report-dump helpers from `MachineIrAllocateRewriteReport`, so an
    upstream machine-ir consumer can now drive the whole current image-prep
    layer while choosing either the repository-default or an explicit target
    profile at the bridge boundary
  - that same report-oriented bridge surface is now also more usable as a
    downstream image consumer boundary than the first summary-only cut:
    report-side callers can query entry-symbol summaries, segment coverage by
    virtual address, symbol lookup by virtual address, relocation lookup by
    site virtual address, and resolved-versus-unresolved relocation summaries
    by filtered index without dropping back to the raw image artifact first
  - that same downstream bridge boundary now also reaches one first explicit
    segment-artifact consumer tier: after bridging from machine-ir/ELF/bytes
    into a structured image report, callers can recover one cached segment
    artifact by segment index, name, or covered virtual address and continue
    inspection from that artifact without re-deriving the segment-local
    slices themselves
  - that same bridged report boundary now also reaches symbol-artifact and
    relocation-artifact consumer tiers, so a downstream caller can step from
    machine-ir/ELF/bytes into cached image report objects and then navigate
    directly through segment/symbol/relocation relationships without dropping
    back to raw whole-table scans
  - that same bridged object graph is now also navigable in both directions
    around symbol targets: callers can arrive at one bridged symbol artifact
    and inspect the incoming relocation set that targets it, again without
    reopening raw whole-table scans or rederiving filtered subsets outside
    the module
  - that same bridged report boundary now also has one higher-level entry
    point above the object graph itself: callers can build one overview
    artifact from machine-ir/ELF/bytes, inspect top-level counts plus cached
    symbol-class filters there, and then descend into segment/symbol/
    relocation artifacts only where needed
  - that same bridged overview layer now also includes relocation-class
    filters, so callers can start from machine-ir/ELF/bytes and land
    directly on resolved/unresolved or call/control relocation families
    without restaging separate classification logic outside the module
  - that same bridged report-dump surface is now also materially more useful
    than the old raw-dump passthrough, because report dumps built from
    machine-ir/ELF/bytes now carry the bridged overview/filter/artifact-cache
    picture directly in text form instead of forcing text-mode consumers back
    through raw whole-table inspection
  - the current bridge already proves that a post-ELF image-prep consumer can
    be exercised end-to-end without reopening earlier backend sibling layers

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_image/`
- keep all public declarations under `include/machine/image.h`
- keep tests under `tests/machine/runtime/machine_image/`

## Current Stop Condition

- Treat the current first machine-image slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-image`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into full linker /
  executable semantics once the first slice already covers:
  - direct `machine_elf -> machine_image`
  - profile-owned base address and entry selection
  - image-defined symbol virtual addresses
  - resolved/unresolved relocation-site surfacing
- Reopen active expansion for:
  - a concrete multi-object/linker consumer need
  - a confirmed image-address / relocation-site correctness bug
  - the next deliberately chosen executable / linker mainline
