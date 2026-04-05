- Canonical IR is non-SSA, block-based, and verifier-backed; pass outputs must preserve entry reachability plus dense block/temp IDs.
- CFG cleanup is currently limited to safe local rewrites: constant/same-target branch folding, empty-jump threading, `jmp -> ret`, and single-predecessor jump-block merge.
- Shared internal pass facts now include temp analysis (use counts, mov-copy facts, recursive temp constants), CFG analysis (reachable blocks, predecessor counts), and instruction side-effect classification.
- Safe fold extensions currently include immediate/immediate folding, single-immediate identity reductions, and same-operand simplifications; avoid folds that erase UB checks such as division/modulo by a possibly zero operand or invalid shift validation.
- Treat `call` as effectful even when its result temp is dead; direct DCE and the default pipeline must preserve those calls while removing surrounding pure work.
- Temp copy/constant facts are only sound for uniquely defined temps; verifier-legal cross-block join-temp materialization means shared temp analysis must invalidate those facts once a temp is defined on multiple mutually exclusive paths.
- Shared temp analysis now has explicit def/use-oriented facts too: definition counts and unique def-sites are first-class analysis outputs, and copy/constant propagation should key off those facts instead of hidden overwrite-prone state.
- Shared temp analysis now also records explicit unique use-site facts (including operand kind and site location), and temp-copy propagation is allowed to consume those facts for direct single-use rewrites before falling back to broader scans.

- Shared `IrValueRef` equality is now a reusable pass utility; CFG cleanup may use it for local value-preserving rewrites such as folding a branch whose two instruction-free successors both `ret` the same non-temp value.
- Same-return diamond folding is sound for equal temp returns too when both successor blocks are instruction-free: verifier-valid empty successors that `ret tmp.N` imply the temp is already available at the branch block exit, so the branch can collapse to `ret tmp.N` directly.
- CFG same-return diamond folding should compare returns by shared temp-analysis equivalence, not only by literal value-ref identity: safe constant resolution and safe copy resolution can expose equal returns even when the two empty successors use different temps.
- The default pass pipeline may need a post-DCE CFG revisit: temp propagation can canonicalize successor returns, DCE can then empty those successor blocks, and a final `simplify-cfg` + dead-temp cleanup pass can safely collapse the newly exposed branch diamond.
- Pipeline-level multi-def join-temp regressions should lock semantic path distinction, not a specific pre-cleanup join-block shape: full-pipeline CFG cleanup may legitimately sink the join `ret tmp.N` into the two successor return blocks as long as the branch and the distinct per-path return values remain preserved.
- Pipeline-level side-effect regressions should include CFG cleanup interaction, not only straight-line DCE: a dead-result `call` inside a branch that later becomes constant-folded must still survive after branch simplification, dead-block cleanup, and the post-DCE CFG revisit.





