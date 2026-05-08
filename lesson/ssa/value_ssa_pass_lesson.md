# Value SSA Pass Lesson（compiler_lab）

> 目标：专门讲 `value_ssa_pass` 这一层。这里不再重复 core representation，而是聚焦 SSA-side cleanup、canonicalization pipeline，以及这些 pass 的当前边界。

## 一句话定位

`value_ssa_pass` 是 SSA 世界里真正开始做“值流整理、规范化、局部优化”的变换层。

## 常见误解

- 误解一：有了 `value_ssa` core，pass 只是顺手加几条 helper。
  - 现在 pass 已经是独立 sibling 模块，不再是 core 的附属尾巴。
- 误解二：SSA pass 和 `ir_pass` 没本质区别。
  - 二者都在做整理，但这里吃的是 SSA representation 和 SSA analysis，而不是 canonical IR。

## 导学

现在 `value_ssa` 相关代码已经分成三层：

1. core：`value_ssa`
2. pass：`value_ssa_pass`
3. execution：`value_ssa_interp`

而 lower-ir one-shot 这一侧，现在还多了一层 memory-aware sibling：

4. memory-backed pass/bridge：`memory_ssa_pass`

这份讲义只讲第二层。

---

## 前置阅读

最推荐先读：

1. `lesson/ssa/value_ssa_lesson.md`
2. `lesson/core/lower_ir_lesson.md`

如果你还没先知道：

- SSA core 的基本表示和 shared analysis
- lower IR 为什么会进入 SSA

那这层的 cleanup / canonicalization / pipeline 很容易变成“知道有这些 pass，但不知道它们为什么放在这里”。

## 读完后接哪篇

最自然往下接：

1. `lesson/ssa/value_ssa_alloc_lesson.md`
2. `lesson/ssa/memory_ssa_pass_lesson.md`

最常见的两条后续路线是：

- 想继续看 SSA 值最后怎么进入资源分配
  - 先接 `value_ssa_alloc`
- 想看 memory-aware 变换怎么和 value-side pass 分层
  - 先接 `memory_ssa_pass`

---

## 最近同步

如果你之前还把 `value_ssa_pass` 记成“classic canonicalize + memory bridge wrapper”，现在要把它更新成：

1. **default helper 不再等于固定一个 mode**
   - `value_ssa_build_default_from_lower_ir(...)` 现在会先看 lower IR 形状，再决定是：
     - 走 indirect-memory direct fast-cleanup
     - 走 extreme straight-line hotspot fast-path
     - 还是回到常规 canonicalized build

2. **load/store cleanup 开始更明确地区分“安全可前推”和“地址已逃逸不能乱动”**
   - local load forwarding 现在会避开 address-taken local
   - global 侧现在多了一条 non-address-taken global forwarding
   - store cleanup 里还多了 “unread scalar local store” 这一条非常实用的窄优化

3. **pass 层开始承担第一批 very targeted perf work**
   - 有了 `value_ssa_optimize_perf_hotspots(...)`
   - 也有了 tiny internal helper inlining
   - 但它们仍然是保守、特定形状驱动的 pass，不是突然长成通用 inliner / superoptimizer

4. **有一条正式 timing trace 开关**
   - `VALUE_SSA_TRACE_TIMING`
   - 这意味着当前 pass 层除了“能变换”，也开始认真服务 perf 诊断

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
- tiny helper inline：`src/value_ssa_pass/value_ssa_inline_helper.inc`
- store cleanup：`src/value_ssa_pass/value_ssa_store_dce.inc`
- normalize：`src/value_ssa_pass/value_ssa_normalize.inc`
- identity：`src/value_ssa_pass/value_ssa_identity.inc`
- fold：`src/value_ssa_pass/value_ssa_fold.inc`
- redundant binary：`src/value_ssa_pass/value_ssa_cse.inc`
- perf hotspot：`src/value_ssa_pass/value_ssa_perf_hotspot.inc`
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
- `value_ssa_optimize_perf_hotspots(...)`
- `value_ssa_build_from_lower_ir_with_canonicalization(...)`
- `value_ssa_build_default_from_lower_ir(...)`
- `value_ssa_build_memory_value_canonicalized_from_lower_ir(...)`
- `value_ssa_build_memory_canonicalized_from_lower_ir(...)`
- `value_ssa_build_canonicalized_from_lower_ir(...)`

对应的 mode enum 现在也已经是公开 API：

```c
typedef enum {
    VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC = 0,
    VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE,
    VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
    VALUE_SSA_LOWER_IR_CANONICALIZE_DEFAULT = VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
} ValueSsaLowerIrCanonicalizeMode;
```

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

这两条 pass 现在都更明确地带着“地址逃逸边界”：

- local：同 block，或 single-idom straight chain；但 address-taken local 不再乱 forward
- global：同 block，或 single-idom straight chain，但 `call` 会 kill known globals
- global 侧还多了一条更窄、但更诚实的 internal helper：
  - **只对 non-address-taken global 做 forwarding**

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

如果要把最近这轮新边界一起记住，可以再补一句：

- `addr_local a.0` 一旦出现，local-forward 就不再把 `a.0` 当“纯 slot 值缓存”
- `addr_global g.0` 一旦出现，新的 non-address-taken global-forward 也会把 `g.0` 退出“安全可前推”集合

所以当前 authority 已经不是“见到 store/load 就尽量前推”，而是：

`只在地址没逃逸、call 没杀死、CFG 还保持 straight-chain 时才前推`

### 4.3 store cleanup

- `value_ssa_eliminate_redundant_stores(...)`
- `value_ssa_eliminate_dead_stores(...)`

另外，这一组内部现在还多了一条很实用的窄 cleanup：

- unread scalar local-store elimination
  - 如果某个 scalar local 从头到尾既没 `load_local`，也没 `addr_local`
  - 那么写给它的 `store_local` 可以整批视作“没人观察到”

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

而 unread scalar local-store cleanup 的最小例子更像：

```text
store_local a.0, 1
ret 0
```

如果 `a.0` 从来没被读，也没被取地址，那么这条 `store_local a.0, 1` 现在就可以直接删掉。

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
int value_ssa_build_memory_value_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int value_ssa_build_memory_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int value_ssa_build_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
```

### 5.1.1 现在 lower-ir one-shot bridge 已经分三档

这一点最近也变了，所以 lesson 里不该再只写一条旧入口。

当前 lower-ir one-shot 实际已经分成：

1. `value_ssa_build_canonicalized_from_lower_ir(...)`
   - classic Value-SSA canonicalize
2. `value_ssa_build_memory_value_canonicalized_from_lower_ir(...)`
   - 委托给 `memory_ssa_pass` 的 memory-value canonicalize
3. `value_ssa_build_memory_canonicalized_from_lower_ir(...)`
   - 委托给 `memory_ssa_pass` 的 full memory-aware pipeline

所以现在 `value_ssa_pass` 这一层不只是有自己 classic canonicalize，也开始承担“Value-SSA-facing memory-aware bridge facade”。

### 5.1.2 mode-based 入口现在是更完整的 caller surface

如果调用方不是“永远只走某一档”，而是想把策略显式做成参数，那么现在更完整的入口其实是：

```c
int value_ssa_build_from_lower_ir_with_canonicalization(const LowerIrProgram *program,
    ValueSsaLowerIrCanonicalizeMode mode,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
```

这条 API 的 lesson 重点不在“又多了个 wrapper”，而在：

- 策略选择现在有正式 enum
- caller 不用再手写 if/else 分发三条不同 build 函数
- `value_ssa_pass` 对外提供了一个稳定的高层入口

可以把它记成：

`build policy` 已经从“调用点习惯”提升成了 “API contract”

### 5.1.3 这三个 mode 到底差在哪

最容易讲清楚的例子，就是：

```text
store_local a.0, 7
ssa.0 = load_local a.0
ssa.1 = add ssa.0, 5
ret ssa.1
```

如果 lower IR 是这个形状，那么三档 one-shot 结果现在应该这样理解：

#### classic

classic 只走：

`lower_ir -> value_ssa -> classic value_ssa pipeline`

它能做的主要是原来那条 Value-SSA 侧的保守清理。

#### memory-value

memory-value 走：

`lower_ir -> value_ssa -> memory_ssa_pass_canonicalize_memory_values`

这时会先把 memory 相关机会吃掉，所以 `load_local a.0` 可以先塌成 `mov 7`。  
但它**不会继续**把 `add 7, 5` 折成 `12`。

所以代表结果会更像：

```text
func main() {
  bb.0:
    ssa.0 = add 7, 5
    ret ssa.0
}
```

#### memory-full

memory-full 则继续往后接 value-only cleanup tail，所以：

- 先 memory cleanup
- 再 normalize / fold / identity / DCE / CFG cleanup

最后可以继续收成：

```text
func main() {
  bb.0:
    ret 12
}
```

这也是当前三档最值得记的区别：

1. classic：只走 old Value-SSA canonicalize
2. memory-value：把 memory 形状先收稳，但不强行继续折 arithmetic
3. memory-full：当前最强的 canonicalized one-shot 结果

### 5.1.4 这条 mode dispatch 的伪代码

当前 `value_ssa_pass_bridge.inc` 的结构其实很直白，lesson 里可以直接记成：

```text
build_from_lower_ir_with_canonicalization(program, mode):
    switch mode:
        case CLASSIC:
            build raw value_ssa from lower_ir
            canonicalize once
            return classic result

        case MEMORY_VALUE:
            delegate to memory_ssa_pass_build_memory_canonicalized_from_lower_ir(...)

        case MEMORY_FULL:
            delegate to memory_ssa_pass_build_canonicalized_from_lower_ir(...)

        default:
            reject unknown mode
```

这里有两个课上很值得强调的小点：

1. classic 分支现在就是“一次 raw build + 一次 classic canonicalize”
2. memory 两个分支并不在 `value_ssa_pass` 里重写一遍逻辑，而是直接委托给 `memory_ssa_pass`

所以这层 bridge 的职责非常清楚：

`统一 Value-SSA-facing 入口，不重复发明 memory pipeline`

### 5.1.5 default 入口现在意味着什么

现在还有一条更高一层的 helper：

```c
int value_ssa_build_default_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
```

这条 helper 现在已经**不等于**“直接把 `DEFAULT` enum 喂给 mode-based API”。

它当前更接近：

```text
if lower_ir uses indirect memory:
    build raw value_ssa
    run one narrow direct fast-cleanup line
else if lower_ir looks like an extreme straight-line hotspot:
    build raw value_ssa only
else:
    fall back to build_from_lower_ir_with_canonicalization(DEFAULT)
```

也就是说，当前默认 helper 的 lesson 口径应该改成：

- **它不是单纯的 mode alias**
- **它是带形状判断的 policy entrypoint**
- `DEFAULT` enum 仍然重要
  - 但它主要服务 mode-based API / fallback canonicalized branch 这一支
- 真正的 default build 现在已经开始为：
  - indirect-memory 程序
  - 极端 straight-line compile-time hotspot

这两类 case 做专门分流。

### 5.1.6 这条 default policy 当前到底优化了什么

如果把这条 helper 拆开讲，当前最值得记的是：

1. **indirect-memory direct fast-cleanup**
   - 先 raw build
   - 再跑一条偏窄、偏性能导向的 direct cleanup：
     - local load forward
     - unread scalar local-store cleanup
     - edge repeated indirect-load forward
     - non-address-taken global forward
     - same-block repeated indirect-load forward
     - repeated pure internal-call reuse
     - tiny helper inline
     - address-root reuse
     - narrow redundant-binary cleanup

2. **extreme straight-line hotspot fast-path**
   - 如果 lower IR 已经是极端单块、超长、无间接内存热点
   - 当前 default helper 会宁可先 raw build
   - 目的不是追求“最干净 dump”，而是先把 compiler compile-time 压下来

3. **常规路径仍然回到 canonicalized build**
   - 也就是 lesson 过去讲的 classic / memory-value / memory-full 那套 mode dispatch

### 5.1.7 当前 bridge tier 的支持 / 不支持

**当前已经讲得稳的：**

- classic / memory-value / memory-full 三档都存在
- mode-based API 存在
- default helper 存在
- default helper 已经有有限的 shape-based fast-path
- invalid mode 会拒绝，而不是 silently fallback
- `memory-value` 与 `memory-full` 的差异已经有回归 authority

**当前不要讲成已有的：**

- 真正 profile-guided auto-tuning bridge
- 任意 per-function 粒度的自适应 mode 混跑
- alias-aware lower-ir bridge mode
- 通用 aggressive indirect-memory optimizer

### 5.2 classic canonicalization pipeline 现在怎么排

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

这里有个最近最容易讲错的点：

- **上面这条 16-step pipeline 是 classic / canonicalize 主线**
- **不是所有 default build 都必经这整条线**

因为前面已经提到：

- indirect-memory 程序现在会优先走 direct fast-cleanup
- extreme straight-line hotspot 甚至可能直接停在 raw build

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
