# Machine ELF / Target-Specific Object Format Plan

## Goal

The next backend-facing mainline after `machine_container` is to turn the
current final serialized state into a **minimal ELF-compatible object
artifact**.

This stage should answer:

- how is the current container/relocation state projected into a real
  ELF relocatable object skeleton?
- how are `.text`, `.symtab`, `.strtab`, `.rel.text`, and `.shstrtab`
  surfaced inside one format-specific byte image?
- how can later target-specific object work build from a real object-format
  boundary instead of only the repository's custom container?

In short:

- `machine_container` decides first stable custom serialized object bytes
- `machine_elf` decides first ELF relocatable object skeleton

## Module Boundary

Create a new sibling module:

- `include/machine/elf.h`
- `src/machine/object/machine_elf/`
- `tests/machine/object/machine_elf/`

Do not fold this into:

- `src/machine/object/machine_container/`
- `src/machine/object/machine_reloc/`

`machine_container` should remain the custom container authority.
`machine_elf` should become the first target-specific post-container consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineContainerFile`
- serialize one ELF32 little-endian relocatable object skeleton
- preserve the current `.text` bytes
- preserve current symbols in `.symtab`/`.strtab`
- preserve current relocations in `.rel.text`
- dump ELF section layout as one format-specific snapshot

Do not start with:

- full platform-ABI relocation semantics
- program headers / executable images
- COFF or Mach-O support

## First Public APIs

- `machine_elf_build_from_machine_container_file(...)`
- `machine_elf_build_from_machine_ir_report(...)`
- `machine_elf_verify_file(...)`
- `machine_elf_dump_file(...)`

## Staging

### Stage MELF1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MELF2: First ELF Serialization From `machine_container`

- ELF header
- section headers
- `.text`, `.strtab`, `.symtab`, `.rel.text`, `.shstrtab`
- byte-image/query helpers

### Stage MELF3: First Upstream Bridge

- `machine_ir -> ... -> machine_container -> machine_elf`

### Stage MELF4: Target Profile / Relocation Policy Plumbing

- explicit target-profile selection at the `machine_elf` boundary
- profile-owned `e_machine` / `e_flags` policy instead of one hardcoded header
- profile-owned relocation-type mapping instead of one global placeholder map
- focused regression coverage for both generic and preview target profiles

### Stage MELF5: ELF Parse / Round-Trip Surface

- parse one owned ELF byte image back into `MachineElfFile`
- recover section/symbol/relocation tables from the byte image
- preserve target-profile selection across build -> bytes -> parse -> verify
- refresh one edited `MachineElfFile` back into canonical ELF bytes
- normalize one accepted ELF byte image directly into canonical bytes
- focused round-trip and malformed-header regression coverage

## Current Position

- `MELF1 representation skeleton`: now effectively **~100%**
  - the first `machine_elf` sibling is landed with public structs, lifecycle,
    verifier, dump, and isolated regression coverage
- `MELF2 first ELF serialization from machine_container`: now effectively
  **~100%**
  - the first ELF byte image can now be built directly from
    `MachineContainerFile`
  - current serialization preserves one real ELF header, section headers,
    `.text`, `.strtab`, `.symtab`, `.rel.text`, and `.shstrtab`
  - section/symbol/relocation query helpers and owned byte-copy helpers are
    landed
  - the ELF symbol/relocation side is now also one notch less placeholder-like
    than the earliest skeleton: `.symtab` now keeps local block symbols ahead
    of globals, `sh_info` now tracks the first global symbol, and relocation
    entries now resolve against that real ELF symbol ordering instead of a raw
    object-symbol + 1 shortcut
  - the ELF-facing query surface is now also more direct than plain
    name-based lookup: callers can ask for `.text`, `.strtab`, `.symtab`,
    `.rel.text`, and `.shstrtab` through dedicated helpers, and can query the
    first global symbol index directly from the ELF view instead of restaging
    that from raw section-header fields
  - the ELF verifier is now also one notch more real than metadata-only
    checking: it re-reads the generated ELF header, section headers, symbol
    table entries, and relocation entries from the owned byte image and
    cross-checks those bytes against the structured in-memory ELF view
  - the ELF-facing summary surface now also includes direct header-level
    introspection, so callers can query class/data/object-type/header-table
    facts without reparsing the raw byte image themselves
- `MELF3 first upstream bridge`: now effectively **~100%**
  - a first direct bridge is landed from `MachineIrAllocateRewriteReport`
    through the current container pipeline into `machine_elf`
  - the module is wired into `Makefile`, included in `make test`, and covered
    by focused regression tests
- `MELF4 target profile / relocation policy plumbing`: now effectively
  **~100%**
  - `machine_elf` no longer hardcodes one global `EM_NONE + fixed reloc-id`
    policy internally; callers can now build through an explicit
    `MachineElfTargetProfile`
  - the default public builds still stay conservative on
    `generic-elf32`, so the repository does not need to pretend a committed
    backend ISA already exists
- the target-policy surface is now real enough to exercise a second profile:
  `riscv32-preview` drives `e_machine=EM_RISCV` plus focused relocation-type
  mapping for current call / primary-control / secondary-control fixup kinds
  - that same profile surface is now also exercised by a third concrete
    preview lane rather than only one “real machine” example:
    `i386-preview` now drives `e_machine=EM_386` plus distinct preview
    relocation-type mapping for current call / primary-control / secondary-
    control fixup kinds
  - the profile surface is now also no longer only “choose profile at build
    time”: accepted input ELF bytes can now be normalized directly into
    canonical bytes under a requested preview profile, so the same public
    boundary can parse, canonicalize, and reprofile in one step
  - the consumer-facing profile helpers are now also a little more complete
    than the earlier build-only cut: callers can clone one ELF artifact for
    isolated local edits, and can ask raw bytes for a directly reprofiled
    canonical dump or canonical structured file without staging the
    parse/refresh sequence themselves
  - that same consumer surface now also has a first structured report artifact
    above the raw ELF file, so profile-aware import/normalize flows can hand
    later consumers one stable summary object instead of forcing every caller
    back through ad hoc header/section/symbol/relocation scans
  - that same consumer-facing summary layer now also includes one first
    relocation-family summary above raw relocation-table iteration: callers
    can ask raw files and report artifacts for direct counts of
    `call` / `primary-control` / `secondary-control` relocation families, so
    consumers can distinguish direct preview artifacts from
    imported/reprofiled artifacts without rescanning `.rel.text` manually
  - preview direct-versus-imported relocation semantics are now also verifier-
    backed at the ELF boundary itself instead of staying only as dump/query
    decoration: canonical direct preview ELF artifacts are no longer allowed
    to keep secondary control relocations under `direct-patch-spans`, while
    generic-origin reprofiled helpers still remain legal because that
    provenance now stays explicit as `origin_profile != target_profile`
  - that same target-policy surface is now also explicit as public structured
    data rather than remaining an internal helper only: callers can ask for a
    direct target-policy summary from one profile, from one owned ELF file,
    or from one ELF report artifact, and recover `e_machine`, `e_flags`, plus
    the current call / primary-control / secondary-control relocation opcode
    mapping without reparsing dumps or mirroring the internal policy table
  - verifier, header-summary, relocation-summary, and dump output now all
    preserve the chosen target profile instead of treating header/relocation
    policy as invisible implementation detail
  - focused tests now cover both the conservative generic profile and the
    preview RISC-V and i386 profiles, including direct checks for
    profile-selected `e_machine` and relocation opcode values across both
    control-flow and external-call shapes, plus direct canonicalize-with-
    profile re-emission from generic input bytes
  - repository target-direction note: treat `riscv32-preview` as aligned with
    the intended final target direction, while `generic-elf32` and
    `i386-preview` remain staging/compatibility profiles rather than signals
    that the backend has switched away from RISC-V
  - the direct `machine_ir -> ... -> machine_elf` profile bridge is now also
    one step less header-only than before: `machine_elf_build_from_machine_ir_report_with_profile(...)`
    threads a profile-aware bytes/object/container chain so `riscv32-preview`
    can change `.text` bytes for the landed subset rather than only changing
    `e_machine` and relocation opcode policy
  - that same direct preview bridge is now also more honest about
    fallthrough-shaped control relocations: when the direct
    `riscv32-preview` bytes/object/reloc chain knows a secondary edge had no
    concrete patch span, the resulting ELF artifact keeps only the one real
    primary control relocation instead of reviving a second fake
    fallthrough-edge relocation
  - this substage should now be read as complete for the current preview ABI
    tier: any further reopening is no longer about making target-policy
    plumbing itself visible or structured, but about deliberately deepening
    profile-specific ABI semantics beyond the current preview relocation
    policy
- Reopened direct-build global-object honesty slice: now effectively
  **~100% (checkpoint-ready / maintenance-first)**
  - direct `machine_object -> machine_container -> machine_elf` builds may now
    preserve optional `.sbss` / `.sdata` sections and `STT_OBJECT` globals
    when those sections already exist upstream, instead of flattening every
    direct-build object back to a strict `.text`-only ELF skeleton
  - verifier-side canonical direct-build checks now also accept that specific
    optional section family (`.sbss` as `SHT_NOBITS`, `.sdata` as writable
    `SHT_PROGBITS`) without weakening the existing `.text/.strtab/.symtab/
    .rel.text/.shstrtab` core invariants
  - that same reopened import side now also accepts and round-trips the same
    optional data-section family: parse/normalize/refresh may now preserve
    `.sbss` / `.sdata` plus their `STT_OBJECT` globals instead of forcing
    those direct-build bytes back through a strict core-section-only shape
  - that same reopened preview line now also reaches one first data-side
    global-access relocation surface: when direct preview lowering preserves
    small global load/store fixups downstream, direct preview ELF builds and
    repository-side parse/refresh round-trips may now carry the matching
    paired `R_RISCV_HI20 + R_RISCV_LO12_I/S` relocation kinds instead of
    collapsing those accesses back to relocation-free code bytes only
  - that same data-side relocation slice is now also visible through
    consumer-facing query helpers instead of only raw ELF relocation rows:
    target-policy summaries may expose the preview/global data relocation
    opcodes directly, and relocation-family summaries may count data-addr /
    data-load / data-store relocations alongside call/control families
  - current stop boundary is still intentionally conservative: this slice is
    scoped to the repository's own canonical direct-build optional data
    sections, not to arbitrary broader ELF data-section import semantics
- `MELF5 ELF parse / round-trip surface`: now effectively **~100% (checkpoint-ready / maintenance-first)**  
  - `machine_elf` can now parse one generated ELF byte image back into a fresh
    `MachineElfFile` instead of only building forward from `machine_container`
  - current parse coverage rebuilds section tables, symbol tables, relocation
    tables, owned bytes, and target-profile selection from the byte image, and
    then reuses the existing verifier to confirm the reconstructed artifact is
    still self-consistent
  - the same surface is now no longer read-only after parse: callers can edit
    the structured `MachineElfFile` view, then ask `machine_elf` to refresh
    that view back into one canonical ELF byte image with rebuilt string
    tables, symbol-table entries, relocation opcodes, section-header layout,
    and profile-owned header fields
  - parse/import is now also less tied to one exact emitted section-header
    order than the first cut: the current importer can accept the repository's
    same ELF subset even when the section headers appear in a different order,
    provided the section-link/name semantics stay self-consistent, and then
    normalize that input back into the repository's canonical internal section
    ordering plus canonical rebuilt bytes
  - the importer is now also one notch less coupled to “exactly 6 sections”:
    it can accept the repository's same core ELF subset even when extra
    non-core sections are present in the input, as long as the required
    `.text/.strtab/.symtab/.rel.text/.shstrtab` family is still self-
    consistent, and will normalize that input back into the canonical 6-
    section internal artifact
  - the same importer is now also one notch less coupled to “every canonical
    section must already be present in the input”: a core-subset input may now
    omit `.rel.text` entirely, and `machine_elf` will synthesize the empty
    canonical relocation section during normalization rather than rejecting the
    artifact outright
  - importer/refresh is now also one notch less coupled to a fully
    repository-authored `.symtab`: accepted input may carry extra local
    section-style symbols, including unnamed local section references and
    `STT_SECTION` entries that point at non-core sections, and
    `machine_elf` will drop them during canonicalization as long as no kept
    relocation still depends on them
  - that same import-hardening slice now also treats one common external-ELF
    local-file artifact as droppable instead of malformed: local `STT_FILE`
    symbols with absolute/special section indices may be accepted and
    normalized away, and raw parsed symbol-section handling is now guarded
    against out-of-range/special section indices before any canonical
    section-index remap is consulted
  - importer now also tolerates one broader family of local-only symbol noise
    on ignored sections: a local named symbol defined in an extra non-core
    section may now be accepted and normalized away together with that ignored
    section, while relocations that still target one of those dropped local
    symbols remain parse-fatal
  - the normalizer is now also one notch less coupled to “every undefined
    global in the imported `.symtab` matters”: unused undefined globals may
    now be accepted as external-noise input and normalized away, while any
    undefined global still referenced by a relocation remains preserved in the
    canonical output
  - that same symbol-noise trimming now also reaches unneeded defined globals
    on unsupported sections: unused globals defined on ignored extra sections
    or special/absolute section indices may now be accepted and normalized
    away, while relocations that target those unsupported defined globals
    remain parse-fatal
  - importer canonicalization is now also one notch less coupled to the exact
    incoming `.symtab[0]` convention: when an accepted input omits the
    canonical null symbol entry at symbol index 0, `machine_elf` may now
    synthesize that null symbol back into the canonical output while
    remapping relocation symbol indices accordingly
  - importer canonicalization is now also one notch less coupled to the exact
    incoming null-section placement convention: accepted input may now carry
    the canonical null section header in a nonzero header slot, or omit it and
    rely on `machine_elf` to synthesize the canonical null section back into
    the normalized 6-section output
  - one intentional preview import asymmetry is now part of the current
    contract: a direct `riscv32-preview` build may carry only one control
    relocation for a fallthrough-shaped branch, while a generic ELF imported
    and then normalized under `riscv32-preview` may still keep both control
    relocations because the raw imported relocation table does not record
    which original edge had `patch_byte_count == 0`
  - that distinction is now also surfaced as first-class artifact metadata
    instead of only being latent in relocation counts: `machine_elf` files
    and reports now carry a small provenance/relocation-semantics summary so
    downstream consumers can tell whether the current relocation table still
    comes from direct patch-span-aware lowering or from imported ELF
    relocation-table semantics
  - that same provenance signal is now also locked through the module's
    consumer-facing helper entrypoints rather than only the raw file/report
    core: focused coverage now exercises the expected semantics class across
    direct build helpers, bytes-to-file helpers, file-to-report helpers, and
    dump/report-dump helpers
  - that provenance summary now also records `origin_profile` alongside the
    current target profile, so later consumers can distinguish a direct
    profile-native artifact from one that only became preview-profiled at a
    later in-memory helper boundary; after a byte round-trip, the importer
    still only guarantees the origin that the ELF header itself can express
  - helper-side authority is now slightly sharper than that one sentence
    alone: in-memory `...with_profile(...)` helpers may preserve the source
    artifact's origin while changing the current target profile, but once that
    rebuilt byte image is parsed again the importer only recovers the origin
    that the ELF header itself now states
  - symbol-table import is now also less coupled to the repository's own
    canonical local/global ordering: a self-consistent input may now carry
    non-canonical local/global `symtab` ordering, and `machine_elf` will
    reorder symbols plus remap relocation symbol indices back into the
    canonical local-then-global layout during normalization
  - that same import surface is now also explicitly a canonicalizer rather
    than only a parser: callers can hand `machine_elf` one accepted byte image
    directly and ask for the normalized canonical ELF bytes back, without
    staging intermediate container-side rebuild logic
  - the same consumer surface is now also a little more ergonomic around local
    experimentation: parsed artifacts can be deep-cloned before mutation, so
    normalization/edit flows do not need to destructively reuse the only copy
    of one imported ELF object
  - the structured import helpers are now also less glue-heavy than the first
    parse-only cut: callers can now ask raw bytes for a canonicalized
    `MachineElfFile` directly, not only for a dump string or a normalized byte
    buffer
  - the same line now also has a first report-oriented consumer surface over
    canonicalized ELF artifacts, with cached header/section/symbol/relocation
    summaries and direct report build entrypoints from raw bytes,
    `machine_container`, and `machine_ir`
  - the parse-side consumer surface is now also one notch less manual than the
    first round-trip cut: callers can now ask `machine_elf` to dump directly
    from raw ELF bytes without staging an explicit parse object themselves
  - verifier-side authority is now also stricter about the repository's
    current generated ELF subset: the canonical section family
    (`NULL/.text/.strtab/.symtab/.rel.text/.shstrtab`) must now keep the
    expected names, types, link/info relationships, alignment, and entry-size
    semantics rather than only preserving byte/table self-consistency
  - focused tests now cover generic-profile and `riscv32-preview` round-trips,
    parsed-view `refresh -> re-emit` after target-profile/symbol-name edits,
    reordered-section-header import normalization, extra-non-core-section
    import normalization, missing-`.rel.text` import normalization,
    droppable-local-section-symbol import normalization plus relocation-side
    rejection when a relocation still targets one of those dropped symbols,
    droppable-local-file-symbol import normalization across `STT_FILE` /
    `SHN_ABS`-style input,
    droppable-local-extra-section-symbol import normalization plus relocation-
    side rejection when a relocation still targets one of those ignored-
    section locals,
    unused-undefined-global import normalization plus preservation of
    relocation-referenced undefined globals,
    unused-defined-global-on-unsupported-section normalization plus
    relocation-side rejection when a relocation still targets one of those
    unsupported defined globals,
    missing-null-symbol import normalization with relocation symbol-index
    remap back to canonical `.symtab[0]`,
    reordered-null-section import normalization and missing-null-section
    synthesis back to canonical `shdr[0]`,
    non-canonical-`symtab` import normalization,
    canonical-byte normalization equivalence, and malformed-header,
    malformed-section-link, malformed-relocation-opcode, plus out-of-range
    relocation-symbol rejection cases
  - this substage should now be treated as checkpoint-ready rather than the
    default continuing implementation mainline: current parse/import/refresh
    authority is broad enough for the repository's deliberately narrow ELF
    subset, and any further broadening should now be reopened only for a
    concrete external-ELF consumer need or a confirmed import correctness gap

## File Management Rules

- keep all implementation under `src/machine/object/machine_elf/`
- keep all public declarations under `include/machine/elf.h`
- keep tests under `tests/machine/object/machine_elf/`

## Current Stop Condition

- Treat the current first machine-elf slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-elf`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied for the initial
  machine-elf skeleton after direct ELF serialization, query helpers,
  upstream bridge, and repository integration landed together.
- The earlier `MELF1`-`MELF3` slice should still be read as
  **checkpoint-ready**: ELF header/section serialization, verifier-backed ELF
  checks, dump/query helpers, and the first upstream `machine_ir` bridge are
  all landed strongly enough that current default work can stay focused on
  “make the ELF boundary more target-policy-real” rather than reopening the
  first skeleton itself.
- The active line is now no longer a default-expanding parse/import workstream.
  Treat `machine_elf` as **checkpoint-ready / maintenance-first** by default,
  and reopen it only for:
  - a concrete external-ELF compatibility bug
  - a deliberate decision to deepen preview target-profile ABI semantics
  - a real downstream consumer that needs a broader accepted ELF subset
- Do not keep expanding this line by default into full platform ABI
  compatibility once the first slice already covers:
  - direct `machine_container -> machine_elf`
  - stable ELF header and section-header serialization
  - `.text` / `.strtab` / `.symtab` / `.rel.text` / `.shstrtab`
  - verifier-backed ELF artifact checks
