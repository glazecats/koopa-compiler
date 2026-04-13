# Value SSA Pass Lesson（compiler_lab）

> 目标：专门讲 `value_ssa_pass` 这一层。这里不再重复 core representation，而是聚焦 SSA-side cleanup、canonicalization pipeline，以及这些 pass 的当前边界。

## 导学

现在 `value_ssa` 相关代码已经分成三层：

1. core：`value_ssa`
2. pass：`value_ssa_pass`
3. execution：`value_ssa_interp`

这份讲义只讲第二层。

---

## 1. 这层为什么单独存在

`value_ssa_pass` 的出现，是为了把这些东西从 core 拿出来：

- cleanup
- canonicalization
- SSA-native optimization

也就是说：

- `src/value_ssa/` 负责表示、verifier、analysis、conversion
- `src/value_ssa_pass/` 负责变换

这个拆分很重要，因为它让后续新增 pass 时不会继续把 `value_ssa.c` 撑成杂物间。

---

## 2. 文件定位

- 接口：`include/value_ssa_pass.h`
- 聚合入口：`src/value_ssa_pass/value_ssa_pass.c`
- pipeline：`src/value_ssa_pass/value_ssa_pass_pipeline.inc`
- trivial simplify：`src/value_ssa_pass/value_ssa_simplify.inc`
- load forwarding：`src/value_ssa_pass/value_ssa_load_forward.inc`
- store cleanup：`src/value_ssa_pass/value_ssa_store_dce.inc`
- normalize：`src/value_ssa_pass/value_ssa_normalize.inc`
- identity：`src/value_ssa_pass/value_ssa_identity.inc`
- fold：`src/value_ssa_pass/value_ssa_fold.inc`
- redundant binary：`src/value_ssa_pass/value_ssa_cse.inc`
- CFG simplify：`src/value_ssa_pass/value_ssa_simplify_cfg.inc`
- dead-value DCE：`src/value_ssa_pass/value_ssa_dce.inc`

对外 API 当前包括：

- `value_ssa_simplify_trivial_values(...)`
- `value_ssa_forward_local_loads(...)`
- `value_ssa_forward_global_loads(...)`
- `value_ssa_eliminate_redundant_stores(...)`
- `value_ssa_eliminate_dead_stores(...)`
- `value_ssa_normalize_binary_operands(...)`
- `value_ssa_simplify_algebraic_identities(...)`
- `value_ssa_fold_constants(...)`
- `value_ssa_eliminate_redundant_binaries(...)`
- `value_ssa_simplify_cfg(...)`
- `value_ssa_eliminate_dead_value_defs(...)`
- `value_ssa_canonicalize_program(...)`
- `value_ssa_build_canonicalized_from_lower_ir(...)`

---

## 3. 当前 pass 层的风格

这层现在有几个很明确的工程边界：

1. 小步
2. 保守
3. value-only / local-memory-only 优先
4. verifier-safe
5. 不假装已经进入 memory SSA

所以不要把它理解成“已经有完整优化器”。

更准确地说，它现在是一条：

`小而真实的 SSA-side pass 线`

### 3.1 当前支持 / 不支持什么

如果把 pass layer 单独列成对照表，当前最合适的口径是：

**已经支持：**

- trivial value rewrite
- local/global load forwarding
- redundant/dead store cleanup
- normalize / identity / fold / redundant-binary
- local CFG simplify
- pure value DCE
- verifier-safe canonicalization pipeline

**还没有：**

- memory SSA optimization
- alias-sensitive rewrite
- stronger loop optimization
- full GVN / PRE
- spill / regalloc-aware transform
- target/backend-aware rewrite

---

## 4. 各类 pass 在做什么

### 4.1 trivial values

`value_ssa_simplify_trivial_values(...)`

主要做：

- trivial `mov` use rewrite
- same-value `phi` use rewrite

它不会：

- 直接删原定义
- 做完整 DCE
- 改 CFG

所以它更像“use-site simplification”。

一个最小例子是：

```text
ssa.0 = mov 1
ssa.1 = mov ssa.0
ret ssa.1
```

跑完之后，更关键的变化不是删定义，而是 use-site 先被改成：

```text
ssa.0 = mov 1
ssa.1 = mov ssa.0
ret 1
```

真正把 `ssa.0/ssa.1` 清掉，要交给后续 DCE。

### 4.2 load forwarding

- `value_ssa_forward_local_loads(...)`
- `value_ssa_forward_global_loads(...)`

这两条 pass 都很保守：

- local：同 block，或 single-idom straight chain
- global：同 block，或 single-idom straight chain，但 `call` 会 kill known globals

它们的定位是：

`narrow slot forwarding`

不是：

`memory SSA`

最容易看懂的 local-forward 例子是：

```text
store_local a.0, 7
ssa.0 = load_local a.0
ret ssa.0
```

当前会被改写成更接近：

```text
store_local a.0, 7
ssa.0 = mov 7
ret ssa.0
```

如果再经过 trivial simplify + DCE，才可能继续塌成：

```text
ret 7
```

### 4.3 store cleanup

- `value_ssa_eliminate_redundant_stores(...)`
- `value_ssa_eliminate_dead_stores(...)`

两者区别：

- redundant store：
  - 当前已知 slot value 本来就等于要写入的值
- dead store：
  - 较早 store 在被观察前就被后续 store 覆盖了

这两条 pass 都只在很窄的 straight-chain 范围里传播已知 slot 状态。

一个很典型的 dead-store 例子是：

```text
store_global g.0, 1
store_global g.0, 2
ret 0
```

在没有中间 `load_global` / `call` 观察点时，前一条 store 可以被删掉，留下：

```text
store_global g.0, 2
ret 0
```

而 redundant-store 的最小例子则更像：

```text
store_local a.0, 7
store_local a.0, 7
ssa.0 = load_local a.0
ret ssa.0
```

当前当 straight-chain knowledge 还成立时，后一个“重复写相同值”的 store 就会被视作冗余写。

### 4.4 binary cleanup

当前 binary 这一串小链条是：

1. `value_ssa_normalize_binary_operands(...)`
2. `value_ssa_simplify_algebraic_identities(...)`
3. `value_ssa_fold_constants(...)`
4. `value_ssa_eliminate_redundant_binaries(...)`

各自定位：

- normalize：把 shape 先收稳
- identity：做安全 identity rewrite
- fold：做 immediate/immediate fold
- redundant-binary：做 very narrow dominator-aware binary elimination

这些 pass 现在都故意避开：

- memory-sensitive fold
- aggressive div/mod/shift rewrite
- 完整 GVN

最典型的 preserve case 是：

```text
ssa.0 = div 1, 0
ssa.1 = div 1, 0
ret ssa.1
```

当前 redundant-binary cleanup 不会把第二条直接抹成 `mov ssa.0`，因为这会偷掉潜在 trap/validation 语义。

### 4.5 CFG simplify

`value_ssa_simplify_cfg(...)`

当前 authority 已包括：

- same-target branch folding
- immediate-branch folding
- empty jump threading
- `jmp -> empty ret`
- same-return diamond folding
- `jmp -> non-phi successor` 且 successor 只有一个 predecessor 的局部 merge
- unreachable block compaction
- dense value-id compaction

所以它是：

`safe local CFG rewrites`

不是完整 CFG optimizer。

一个最直观的 same-return diamond 例子是：

```text
bb.0:
  ssa.0 = load_local a.0
  br ssa.0, bb.1, bb.2
bb.1:
  ret ssa.0
bb.2:
  ret ssa.0
```

当前可以直接塌成：

```text
bb.0:
  ssa.0 = load_local a.0
  ret ssa.0
```

而 single-predecessor non-phi merge 则更像把：

```text
bb.0:
  ssa.0 = load_local a.0
  jmp bb.1
bb.1:
  ssa.1 = add ssa.0, 1
  ret ssa.1
```

收成：

```text
bb.0:
  ssa.0 = load_local a.0
  ssa.1 = add ssa.0, 1
  ret ssa.1
```

### 4.6 dead-value DCE

`value_ssa_eliminate_dead_value_defs(...)`

当前会删：

- dead `phi`
- dead `mov`
- dead `binary`
- dead `load_local`
- dead `load_global`

不会删：

- `call`
- `store_local`
- `store_global`
- dangerous dead `div/mod/shift`

所以它仍然是：

`保守的 pure value DCE`

---

## 5. Canonicalization Pipeline

### 5.1 入口

当前最重要的入口是：

```c
int value_ssa_canonicalize_program(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_build_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
```

### 5.2 pipeline 现在怎么排

当前 canonicalization pipeline 会串起：

1. trivial-value simplify
2. local-load forwarding
3. global-load forwarding
4. trivial-value simplify 再跑一遍
5. redundant-store cleanup
6. dead-store cleanup
7. binary normalize
8. algebraic identity simplify
9. constant fold
10. trivial-value simplify 再跑一遍
11. redundant-binary cleanup
12. trivial-value simplify 再跑一遍
13. dead-value DCE
14. CFG simplify
15. dead-value DCE 再跑一遍
16. alpha-renaming

这说明它不是“一次 pass 就变好”，而是：

- 某些 pass 先制造更简单的形状
- 后面的 pass 再继续压
- 最后再用 alpha-rename 收稳定输出

### 5.2.1 为什么要多次 rerun trivial simplify / DCE

这条 pipeline 如果只看名字，很容易显得“怎么又跑一遍”。

但当前多次 rerun 的原因其实很具体：

- load forwarding 会制造 `mov`
- identity / fold / redundant-binary 也会制造 `mov`
- DCE 可能把 CFG 清空到适合继续做 CFG simplify
- CFG simplify 之后又可能新出现 dead value defs

所以这些 rerun 不是噪音，而是为了让：

`前一条 pass 暴露出的机会，能被后一条 pass 真正吃掉`

### 5.3 一个 canonicalize before/after 例子

比如这个很典型的 local-chain case：

```text
store_local a.0, 7
ssa.0 = load_local a.0
ssa.1 = add ssa.0, 0
ret ssa.1
```

当前 canonicalization 管线会分阶段暴露机会：

1. load-forward:
   - `load_local a.0 -> mov 7`
2. identity simplify:
   - `add 7, 0 -> mov 7`
3. trivial simplify:
   - `ret ssa.1 -> ret 7`
4. DCE:
   - 把 dead `mov` / dead `load` 清掉

最后可能收成：

```text
ret 7
```

这个例子很适合记住 pass 层的核心价值：

`不是每个 pass 单独很强，而是它们按顺序把机会一层层暴露出来`

### 5.4 再看一个“必须保留副作用”的 authority 例子

当前 canonicalization 最重要的安全边界之一，是 dead-result `call` 必须保留。

也就是：

```text
ssa.0 = call id()
ret 0
```

即使 `ssa.0` 没人再用，canonicalize 后也仍然必须保留这条 `call`。

同样，dangerous dead binary 也不能被当纯值乱清：

```text
ssa.0 = div 1, 0
ret 0
```

这两条 together 很适合在课上压成一句：

`当前 pass 层可以清 pure value，但不能偷掉 side-effect / trap boundary`

### 5.5 一个 global call barrier 的 authority dump

当前 canonicalization 里，一个非常适合讲 “为什么 global forwarding / store cleanup 不能太激进” 的例子是：

```text
global g.0

declare touch()

func main() {
  bb.0:
    store_global g.0, 9
    call touch()
    ssa.0 = load_global g.0
    ret ssa.0
}
```

这个形状现在被 authority 锁成：

- `call touch()` 不能被越过去
- `load_global g.0` 不能因为前面有 `store_global g.0, 9` 就直接改成 `mov 9`

也就是说，当前 pass 层在 global 侧明确坚持：

`call 是 global knowledge 的 kill / observation barrier`

### 5.6 一个 canonicalize fixed-point authority 例子

fixed-point 不是抽象口号，当前是有 exact dump authority 的。

例如 scrambled diamond canonicalize 后当前要稳定成：

```text
func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    br ssa.0, bb.1, bb.2
  bb.1:
    jmp bb.3
  bb.2:
    jmp bb.3
  bb.3:
    ssa.1 = phi [bb.1: 1], [bb.2: 2]
    ret ssa.1
}
```

然后再跑第二次 canonicalize，dump 不该继续变化。

同理，current authority 里像这种 global call chain：

```text
global g.0

declare touch()

func main() {
  bb.0:
    store_global g.0, 1
    call touch()
    store_global g.0, 2
    ret 2
}
```

也已经被固定点回归锁住了：

- 既要保持 `call` / `store_global` 的行为边界
- 也要保证第二次 canonicalize 不再继续改形

所以 fixed-point 现在不是“理想目标”，而是 pass 层真实的 regression contract。

### 5.3 当前 contract

这条 pipeline 现在有几个很强的 contract：

1. 入口先 verify 输入
2. 成功返回保证输出 verifier-legal
3. 结果要求固定点
4. dead-result `call` 不能被错误抹掉
5. dangerous dead `div/mod/shift` 不能被错误抹掉

所以它现在已经不是“方便调用的小 helper”，而是：

`稳定输出边界`

---

## 6. 当前还没做什么

这层现在还没有：

1. memory SSA pass
2. alias-sensitive optimization
3. 真正的 regalloc-aware coalescing pass
4. spill insertion
5. target-aware backend optimization

所以不要把现在这条 pass 线理解成“已经开始后端优化”。

---

## 7. 测试怎么看

这份 lesson 最相关的是：

- `tests/value_ssa/value_ssa_regression_test.c`
- `tests/value_ssa/value_ssa_oracle_test.c`

当前 regression authority 已明确锁住：

- trivial simplify
- local/global load-forward
- redundant/dead store cleanup
- normalize / identity / fold / redundant-binary
- CFG simplify
- canonicalization fixed-point
- dead-result call / dangerous binary preservation

其中 oracle test 更像组合边界：

- interpreter 可直接执行 SSA
- pass/canonicalization 后的行为可以和执行结果对照

---

## 8. 一句话总结

`value_ssa_pass` 现在是一条独立的、保守的 SSA-side cleanup / canonicalization layer。它已经能稳定整理 SSA，但还不是完整 optimizer，更不是 backend allocator。
