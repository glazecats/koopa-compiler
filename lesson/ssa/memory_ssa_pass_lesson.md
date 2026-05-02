# Memory SSA Pass Lesson（compiler_lab）

> 目标：专门讲 `memory_ssa_pass`。这里聚焦它为什么要独立成模块、当前有哪些真正可用的 pass / pipeline / bridge，以及它和 `value_ssa_pass` 的边界。

## 一句话定位

`memory_ssa_pass` 是把 memory-SSA facts 真正转成 Value-SSA program 变换的那一层。

## 常见误解

- 误解一：memory-aware 优化只是 `value_ssa_pass` 里再多写几个 heuristic。
  - 现在这层已经独立成模块，明确区分了 value-only pass 和 memory-aware pass。
- 误解二：它是在优化另一份独立 IR。
  - 当前它利用的是 memory-SSA facts，但改写对象仍然是 `ValueSsaProgram`。

## 导学

这层最好理解成：

`memory-SSA-backed transforms on Value-SSA programs`

也就是说：

- 它吃的是 `memory_ssa` facts
- 但改写对象还是 `ValueSsaProgram`

所以它既不是 old `value_ssa_pass` 里的局部 heuristic，也不是另一套独立的 machine/backend optimizer。

---

## 前置阅读

最推荐先读：

1. `lesson/ssa/memory_ssa_lesson.md`
2. `lesson/ssa/value_ssa_pass_lesson.md`

因为这层最容易读顺的方式，就是先知道：

- memory SSA 到底补了哪些事实
- value-side pass 目前又已经做到什么程度

这样你才能看清这里为什么必须单独成模块。

## 读完后接哪篇

最自然往下接：

1. `lesson/ssa/value_ssa_machine_lesson.md`
2. `lesson/machine/README.md`

因为 memory-aware cleanup 之后，最常见的后续问题就是：

- 这些更干净的 SSA / slot state 后面怎么继续进入 machine-facing 世界

---

## 1. 为什么单独成模块

如果把 memory-SSA 相关优化继续塞进 `value_ssa_pass`，会很容易把这几层混掉：

- value-only cleanup
- memory-aware cleanup
- future memory-specific pipeline
- lower-ir one-shot memory bridge

所以现在单独拆成：

- `include/memory_ssa_pass.h`
- `src/memory_ssa_pass/`

是合理的。

这说明项目现在已经明确接受一个事实：

`memory-aware optimization 不再只是 Value-SSA 层的附带 heuristic`

---

## 2. 文件定位

- 接口：`include/memory_ssa_pass.h`
- 聚合入口：`src/memory_ssa_pass/memory_ssa_pass.c`
- core helper：`src/memory_ssa_pass/memory_ssa_pass_core.inc`
- load forward：`src/memory_ssa_pass/memory_ssa_pass_load_forward.inc`
- slot promotion：`src/memory_ssa_pass/memory_ssa_pass_slot_promotion.inc`
- store cleanup：`src/memory_ssa_pass/memory_ssa_pass_store_cleanup.inc`
- local scalar replace：`src/memory_ssa_pass/memory_ssa_pass_local_scalar_replace.inc`
- global scalar replace：`src/memory_ssa_pass/memory_ssa_pass_global_scalar_replace.inc`
- combined scalar replace：`src/memory_ssa_pass/memory_ssa_pass_scalar_replace.inc`
- memory-only canonicalize：`src/memory_ssa_pass/memory_ssa_pass_memory_value.inc`
- lower-ir bridge：`src/memory_ssa_pass/memory_ssa_pass_bridge.inc`
- full pipeline：`src/memory_ssa_pass/memory_ssa_pass_pipeline.inc`

当前 build 也已经显式跟踪：

- `MEMORY_SSA_PASS_SPLIT_INCLUDES`

所以这层已经不再是“一个越写越大的实验大文件”，而是正式 split 成：

- core
- load
- slot promotion
- store
- local/global scalar replace
- memory-only canonicalize
- bridge
- pipeline

---

## 3. 当前 API

当前对外入口已经包括：

- `memory_ssa_pass_forward_local_loads(...)`
- `memory_ssa_pass_forward_global_loads(...)`
- `memory_ssa_pass_promote_local_slots(...)`
- `memory_ssa_pass_promote_global_slots(...)`
- `memory_ssa_pass_eliminate_redundant_local_stores(...)`
- `memory_ssa_pass_eliminate_redundant_global_stores(...)`
- `memory_ssa_pass_eliminate_redundant_stores(...)`
- `memory_ssa_pass_eliminate_dead_stores(...)`
- `memory_ssa_pass_eliminate_dead_local_stores(...)`
- `memory_ssa_pass_eliminate_dead_global_stores(...)`
- `memory_ssa_pass_scalar_replace_local_slots(...)`
- `memory_ssa_pass_scalar_replace_global_slots(...)`
- `memory_ssa_pass_scalar_replace_slots(...)`
- `memory_ssa_pass_eliminate_redundant_memory_binaries(...)`
- `memory_ssa_pass_canonicalize_memory_values(...)`
- `memory_ssa_pass_build_memory_canonicalized_from_lower_ir(...)`
- `memory_ssa_pass_run_pipeline(...)`
- `memory_ssa_pass_build_canonicalized_from_lower_ir(...)`

这说明它当前已经不只是四条孤立 cleanup，而是形成了几层明确的 consumer：

1. 单条 pass
2. slot promotion
3. scalar replacement consumer
4. memory-only canonicalizer
5. narrow memory-aware CSE consumer
6. full memory-aware mini-pipeline
7. lower-ir one-shot bridge

### 3.1 现在这层最好按四条子线来理解

如果还只把 `memory_ssa_pass` 理解成：

- forwarding
- store cleanup

其实已经有点过期了。

现在更贴近事实的讲法是：

1. `memory-aware forwarding / reuse`
   - 直接解析到值
   - 复用 dominating load
   - 复用 dominating equivalent value
2. `narrow memory-aware PRE`
   - join 上部分冗余 load
   - 缺失 predecessor 补值再合流
3. `slot promotion`
   - `memory phi -> value phi`
   - later load 直接改成 `mov promoted_value`
4. `scalar replacement consumer`
   - promotion + forwarding + store cleanup + SSA cleanup 的打包入口
5. `memory-aware CSE / join-local GVN`
   - memory-derived SSA values 上的 redundant binary cleanup
   - join-local predecessor projection / materialization

也就是说，当前 `memory_ssa_pass` 已经开始同时碰到：

- memory-aware GVN/CSE
- memory-aware PRE
- slot promotion
- scalar replacement

只是每一条都还刻意收得很窄。

---

## 4. 当前几类真正有用的变换

### 4.1 `forward_local_loads`

它会把：

```text
ssa.0 = load_local a.0
```

改成：

```text
ssa.0 = mov value
```

前提是 memory-SSA 能证明这个 load 的 reaching memory version 对应一个唯一值。

当前已经不只认：

- direct reaching store

也能穿过：

- same-value memory phi
- same-value loop-carried memory phi

### 4.2 `forward_global_loads`

同理，但对象是 global。

这里当前最重要的保守边界仍然是：

- `call` barrier 不穿
- differing-value phi 不乱推
- entry seed 不瞎猜

这里最近已经不该再只理解成“遇到 call 就全杀光”。

当前 bridge / analysis 线已经能把 call 分成更细的情况：

- declaration-only / unknown callee：保守全局 barrier
- internal pure / readonly-on-other-global callee：只 kill 它真正会写的 tracked global

所以现在 global forwarding 已经不只是：

`比 local 多一个 call barrier`

而更像：

`基于 memory_ssa call effect 摘要的保守 global forwarding`

### 4.3 `eliminate_redundant_stores`

这条 pass 看的不是“后面有没有人读”，而是：

`这次 store 覆盖掉的旧值是不是和新值相同`

它的关键依赖是：

- `memory_ssa_resolve_store_previous_value(...)`

所以像：

```text
store_local a.0, 7
store_local a.0, 7
```

后者可以因为“写回同值”而消失。

更强的是，现在即使这个“旧值”来自：

- same-value join phi
- same-value loop phi
- dominating `load_*` 结果

也仍然能继续识别 no-op store。

### 4.4 `eliminate_dead_stores`

这条 pass 看的则是：

`这次 store 产出的 memory version 后面到底有没有任何 observing use`

它的关键依赖是：

- `memory_ssa_is_trivially_dead_store(...)`

所以它现在已经能删：

- zero-use dead store
- overwritten-store deadness
- phi-overwrite deadness
- loop-overwrite deadness

也就是说，`phi` 的存在本身已经不再会把一条旧 store 人为保活。

### 4.5 `promote_local_slots` / `promote_global_slots`

这是最近最值得单独拎出来讲的地方。

它们做的不是：

- “再补一个 predecessor load”

而是：

- 当某个 later `load_local` / `load_global` 读到的是一个 `memory phi`
- 并且这个 memory phi 所在块支配当前 load
- 就尝试在那个 dominating block 上直接合成一个 `value phi`
- 然后把 later load 改成 `mov value_phi`

这已经不是单纯的 forwarding 小招了，而是开始摸到：

`slot promotion / scalar replacement`

最重要的直觉是：

- 以前：值还挂在 slot / memory version 上
- 现在：值开始挂在 SSA value / value phi 上

也就是：

`memory-carried value -> SSA-carried value`

### 4.6 `scalar_replace_local_slots` / `scalar_replace_global_slots` / `scalar_replace_slots`

这三条 API 是最近 lesson 必须跟上的另一件事。

它们不是新 IR，也不是完整框架，而是：

`higher-level narrow consumers`

当前组合关系大致是：

- `scalar_replace_local_slots`
  - `promote_local_slots`
  - `forward_local_loads`
  - `eliminate_redundant_local_stores`
  - `eliminate_dead_local_stores`
  - 小段 SSA cleanup tail
- `scalar_replace_global_slots`
  - `promote_global_slots`
  - `forward_global_loads`
  - `eliminate_redundant_global_stores`
  - `eliminate_dead_global_stores`
  - 小段 SSA cleanup tail
- `scalar_replace_slots`
  - local consumer
  - global consumer
  - 再做一个小 fixed point

所以当前这条线已经不只是“给研究代码拼点单条 pass”，而是正式长出：

`把简单 slot-carrier family 收成 SSA value flow`

的公开入口了。

### 4.7 `eliminate_redundant_memory_binaries`

这条是最近新长出来、很值得单独讲的一条线。

它不是重新发明一套独立的 memory equivalence engine，而是：

1. 先做一轮很窄的 memory-derived value exposure
   - promotion
   - load forwarding
   - store cleanup
   - trivial simplify
2. 然后把得到的 arithmetic graph 交给：
   - join-local rewrite
   - 现有 `value_ssa` binary normalize / fold / identity / redundant-binary cleanup

所以它的定位更像：

`narrow memory-aware CSE consumer`

而不是：

`完整 memory-aware GVN framework`

### 4.8 `memory-aware GVN/CSE`、`PRE`、`slot promotion`、`scalar replacement` 各自是什么意思

这几个词最近很容易混，所以 lesson 里最好把它们摆正：

#### memory-aware GVN/CSE

问的是：

`这两次 load / 这两个 memory-sensitive value，是不是其实同值？`

当前更像这条线的能力包括：

- reuse dominating load
- reuse dominating equivalent value
- same slot + same memory version -> 直接复用已有 SSA value
- joined binary over memory-derived values 的 per-edge 结果复用

#### PRE

问的是：

`这次 load 是不是部分冗余？`

当前更像这条线的能力包括：

- 某些 predecessor 已有值
- 某些 predecessor 还没有
- 先补缺失边上的值
- 再在 join 合成 phi
- predecessor-side binary materialization

#### slot promotion

问的是：

`这个 slot 当前值，能不能不再通过 load 来表达，而是直接变成 value phi / SSA value？`

当前更像这条线的能力包括：

- `memory phi -> value phi`
- later load -> `mov promoted_value`

#### scalar replacement

问的是：

`这一段 slot-carried memory flow，能不能整体收成 SSA value flow？`

当前公开 consumer：

- `memory_ssa_pass_scalar_replace_local_slots(...)`
- `memory_ssa_pass_scalar_replace_global_slots(...)`
- `memory_ssa_pass_scalar_replace_slots(...)`

所以你可以把它们的关系记成：

- GVN/CSE：识别并复用同值
- PRE：消部分冗余
- promotion：把 slot 值提升成 SSA 值
- scalar replacement：把这些小能力打包成一个更像“局部 mem2reg” 的 consumer

### 4.9 这几条主线的核心伪代码

如果把这层 lesson 写得更像“实现怎么想”，最有用的是把新的主线一起写进伪代码。

#### load forwarding

```text
for each load(slot) at instruction I:
    reaching_version = memory access use-version on I

    if resolve_load_store_value(reaching_version) gives concrete value V:
        rewrite load to mov V
        continue

    if some dominating scope already materializes
       an equivalent SSA value for (slot, reaching_version):
        rewrite load to mov dominating_equivalent_value
        continue

    if reaching_version is a dominating memory phi and
       all incoming versions are SSA-expressible:
        synthesize value phi at the memory-phi block
        rewrite later load to mov promoted_value_phi
        continue

    if current load is at a join and every predecessor already has
       some equivalent value:
        synthesize join value phi
        rewrite load to mov phi_result
        continue

    if current load is at an acyclic join and only some predecessors
       are missing values:
        try ancestor-value reuse first
        otherwise insert missing predecessor-edge loads
        rebuild analyses if structure changed
        retry
        continue

    otherwise keep the load
```

这段伪代码正好对应现在最值钱的几档能力：

1. 直接值解析
2. 重用 dominating equivalent value
3. dominating memory-phi promotion
4. join 处直接合成 value phi
5. acyclic partial PRE

#### redundant store cleanup

```text
for each store(slot, new_value) at instruction I:
    previous_version = memory access use-version on I

    if resolve_store_previous_value(previous_version) gives old_value
       and old_value == new_value:
        remove this store
        continue

    if new_value itself is equivalent to
       "slot at previous_version":
        remove this store
```

第二个分支很关键，因为它解释了为什么这种形状现在也能消：

```text
ssa.0 = load_local a.0
store_local a.0, ssa.0
```

即使 `a.0` 的具体值并不知道，只要能证明：

- `ssa.0` 就是“写之前那个版本的 a.0”

这个 store 仍然是 no-op。

#### dead store cleanup

```text
for each store-produced version:
    if memory_ssa_is_trivially_dead_store(version):
        remove its defining store
```

这里关键是：pass 层不自己瞎写一套死活推理，而是直接消费 analysis 层的 version-use 结论。

#### slot promotion

```text
for each load(slot) at instruction I:
    reaching_version = memory access use-version on I

    if reaching_version is not defined by memory phi:
        keep load
        continue

    phi_block = block that defines that memory phi
    if phi_block does not dominate I:
        keep load
        continue

    for each incoming version of the memory phi:
        try resolve_version_value(...)
        else try find predecessor equivalent value
        else try find dominating ancestor equivalent value
        if still missing:
            promotion fails

    if all incoming values are equal:
        rewrite load to mov common_value
    else:
        materialize value phi in the phi block
        rewrite current load to mov promoted_value_phi
```

#### scalar replacement consumer

```text
repeat until dump stable:
    promote slots
    forward loads
    eliminate redundant stores
    eliminate dead stores
    simplify trivial values
    eliminate dead value defs
    simplify cfg
```

所以这条 consumer 的本质不是“单次很强的超大 pass”，而是：

`promotion + forwarding + cleanup + small SSA cleanup`

#### memory-aware binary CSE

```text
repeat until dump stable:
    expose memory-derived SSA values
        (promotion + forwarding + store cleanup + trivial simplify)

    for each safe join-local binary:
        project its operands/result to each predecessor edge
        if predecessor already has that binary result:
            reuse it
        else if edge operands are immediate/immediate:
            fold per-edge
        else if edge is simple enough and materialization is allowed:
            synthesize missing predecessor-side binary
            restart

        if every incoming edge now has a result:
            if all edge results equal:
                rewrite joined binary to mov common_value
            else:
                rewrite joined binary to phi of edge results

    run value-side normalize / fold / identity / redundant-binary cleanup
```

这条线的重点不在“又多了一个 binary pass”，而在：

- 它开始对 memory-derived SSA expressions 做 join-local 同值复用
- 也开始有一小块 predecessor-materialization 的 PRE 味道

---

## 5. 典型收益例子

### 5.1 join preserved local load forwarding

```text
bb.0:
  store_local a.0, 7
  br 1, bb.1, bb.2
bb.1:
  jmp bb.3
bb.2:
  jmp bb.3
bb.3:
  ssa.0 = load_local a.0
  ret ssa.0
```

这里虽然经过了 join，但两边没有改 `a.0`。

所以当前 pass 可以把它改成：

```text
bb.3:
  ssa.0 = mov 7
  ret ssa.0
```

这说明它已经比 old straight-chain heuristic 更强。

### 5.2 same-value phi join forwarding

```text
bb.1:
  store_local a.0, 7
  jmp bb.3
bb.2:
  store_local a.0, 7
  jmp bb.3
bb.3:
  ssa.0 = load_local a.0
  ret ssa.0
```

join 处虽然有 memory phi，但 incoming 值相同，所以 load 仍然能 forward。

### 5.3 differing-value phi 保守停下

```text
bb.1:
  store_local a.0, 1
  jmp bb.3
bb.2:
  store_local a.0, 2
  jmp bb.3
bb.3:
  ssa.0 = load_local a.0
  ret ssa.0
```

这里就必须停，因为 incoming 值不同。

### 5.4 parameter-local join 也能 forward，但方式不同

最近一个很值得补进 lesson 的 authority，是 parameter-local join。

例如：

```text
func main(a.0) {
  bb.0:
    br 1, bb.1, bb.2
  bb.1:
    ssa.0 = load_local a.0
    jmp bb.3
  bb.2:
    ssa.1 = load_local a.0
    jmp bb.3
  bb.3:
    ssa.2 = load_local a.0
    ret ssa.2
}
```

这里 join 处第三个 load 当前可以被改成：

```text
func main(a.0) {
  bb.0:
    br 1, bb.1, bb.2
  bb.1:
    ssa.0 = load_local a.0
    jmp bb.3
  bb.2:
    ssa.1 = load_local a.0
    jmp bb.3
  bb.3:
    ssa.3 = phi [bb.1: ssa.0], [bb.2: ssa.1]
    ssa.2 = mov ssa.3
    ret ssa.2
}
```

这个例子非常关键，因为它说明当前 forward 并不要求：

- 必须知道具体 immediate 值

只要能证明：

- 每条 incoming edge 都已经有“等于这个 slot/version 的 SSA value”

join load 仍然可以消掉。

### 5.5 global call barrier 现在也不是“一刀切”

另一个适合写进 lesson 的例子，是 global + internal call。

```text
global g.0

func helper() {
  bb.0:
    ret 0
}

func main() {
  bb.0:
    store_global g.0, 9
    call helper()
    ssa.0 = load_global g.0
    ret ssa.0
}
```

如果 `helper()` 不触碰 `g.0`，当前 authority 下：

```text
func main() {
  bb.0:
    store_global g.0, 9
    call helper()
    ssa.0 = mov 9
    ret ssa.0
}
```

这说明现在的 global pass 已经不是旧 heuristic 那种：

`看见 call 就永远不 forward`

而是：

`看见 call，就看这次 call 对这个 global 到底是不是 barrier`

### 5.6 现在已经有一小块真正的 acyclic PRE

这类例子是最近最值得写进 lesson 的。

```text
func main(a.0) {
  bb.0:
    br 1, bb.1, bb.2
  bb.1:
    ssa.0 = load_local a.0
    jmp bb.3
  bb.2:
    jmp bb.3
  bb.3:
    ssa.1 = load_local a.0
    ret ssa.1
}
```

以前更容易停在：

- `bb.1` 已经有值
- `bb.2` 还没有
- 所以 `bb.3` 那个 load 不能直接合流

现在这条 acyclic PRE slice 会保守地：

1. 在 `bb.2` 补一个 `load_local a.0`
2. 在 `bb.3` 合成 `phi`
3. 把 `bb.3` 原来的 `load` 改成 `mov phi`

结果更像：

```text
func main(a.0) {
  bb.0:
    br 1, bb.1, bb.2
  bb.1:
    ssa.0 = load_local a.0
    jmp bb.3
  bb.2:
    ssa.2 = load_local a.0
    jmp bb.3
  bb.3:
    ssa.3 = phi [bb.1: ssa.0], [bb.2: ssa.2]
    ssa.1 = mov ssa.3
    ret ssa.1
}
```

这就是非常标准的：

`部分冗余 join-load -> predecessor 补值 -> join phi`

### 5.7 这条 PRE 现在已经不靠 block id 运气了

scrambled join 现在也是 authority。

例如 join block 编号比 predecessor 还小的时候，当前也能做：

```text
bb.0:
  br 1, bb.2, bb.3
bb.1:
  ssa.3 = phi [bb.2: ssa.0], [bb.3: ssa.2]
  ssa.1 = mov ssa.3
  ret ssa.1
bb.2:
  ssa.0 = load_local a.0
  jmp bb.1
bb.3:
  ssa.2 = load_local a.0
  jmp bb.1
```

它能成立，不是因为扫描顺序刚好合适，而是因为：

- forward/load 这条线现在发生结构性插入后会重建 analysis
- 所以 scrambled CFG 也能吃到这一小块 PRE

### 5.8 ancestor-value preferred 说明它越来越像 memory-aware GVN

另一个很好的例子是：

```text
func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    br 1, bb.1, bb.2
  bb.1:
    ssa.1 = mov ssa.0
    jmp bb.3
  bb.2:
    jmp bb.3
  bb.3:
    ssa.2 = load_local a.0
    ret ssa.2
}
```

这里当前更强的结果不是“在 `bb.2` 再补一个 load”，而是：

```text
bb.3:
  ssa.2 = mov ssa.0
  ret ssa.2
```

也就是：

- 祖先块里已经有等价值
- 那就优先直接复用祖先值
- 不重新制造新的 load

这就是为什么这条线现在已经很有：

`memory-aware GVN/CSE`

的味道了。

### 5.9 现在也开始碰到 slot promotion 的第一刀

这轮最值得单独讲清楚的，不是 PRE，而是 promotion。

看这个形状：

```text
bb.1:
  ssa.1 = load_global g.0
  jmp bb.3
bb.2:
  ssa.2 = load_global g.0
  jmp bb.3
bb.3:
  jmp bb.4
bb.4:
  ssa.4 = load_global g.0
  ret ssa.4
```

如果 `bb.3` 处的 memory state 已经是一个 `memory phi`，那么这轮的新 promotion 路径会尝试：

1. 在 `bb.3` 直接造一个 `value phi`
2. 把 `bb.4` 的 later load 改成 `mov value_phi`

结果更像：

```text
bb.3:
  ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]
  jmp bb.4
bb.4:
  ssa.4 = mov ssa.3
  ret ssa.4
```

这和 PRE 的区别很关键：

- PRE：还是围着那次 join/load 本身打转
- promotion：开始把 slot 当前值直接提升成 value phi

这就是：

`memory-carried value -> SSA-carried value`

### 5.10 scalar replacement consumer 已经能把 mixed join 收成很像 mem2reg 的结果

现在更高层的 `memory_ssa_pass_scalar_replace_slots(...)` 已经不只是“少几条 load”，而是能把 mixed local/global join family 直接收成：

```text
global g.0

func main(c.0) {
  bb.0:
    ssa.0 = load_local c.0
    br ssa.0, bb.1, bb.2
  bb.1:
    jmp bb.3
  bb.2:
    jmp bb.3
  bb.3:
    ssa.1 = phi [bb.1: 1], [bb.2: 2]
    ssa.2 = phi [bb.1: 10], [bb.2: 20]
    ssa.3 = add ssa.1, ssa.2
    ret ssa.3
}
```

这时候 lesson 里最值得强调的是：

- local/global slot 本身已经不是主要载体
- 真正流动的是 `ssa.1` / `ssa.2`
- `add` 直接吃的是 promoted values

这已经非常像：

`narrow scalar replacement`

而不只是 load forwarding。

### 5.11 但全局 barrier 仍然保守保留

比如 mixed global barrier case，当前结果仍然会保留：

- `call touch()`
- call 后的 `load_global g.0`

只把确实能 promotion 的另一半值流继续提升成 phi。

这说明现在这条线虽然已经切进 scalar replacement，但仍然保留了正确的边界：

- 不跨未知 global barrier 乱 promotion
- loop / call / differing-value family 该停就停

### 5.12 现在也有一小块真正的 memory-aware CSE / join-local GVN

最近这条线又往前走了一步，不再只是在 slot 值上做 forwarding/promotion，而是开始在：

`memory-derived SSA arithmetic`

上做去冗余。

最直观的家族是这种：

```text
bb.1:
  a1 = ...
  g1 = ...
  sum1 = add a1, g1
  jmp bb.3
bb.2:
  a2 = ...
  g2 = ...
  sum2 = add a2, g2
  jmp bb.3
bb.3:
  a = phi [bb.1: a1], [bb.2: a2]
  g = phi [bb.1: g1], [bb.2: g2]
  sum = add a, g
  ret sum
```

以前更容易停在：

```text
sum = add(phi(...), phi(...))
```

因为虽然 join 上这个 `add` 看起来和前驱算过的 `sum1/sum2` 有关系，但系统还没有把它当成一条明确的 “per-edge same value” 规则。

现在这条 memory-aware CSE 会更进一步：

```text
bb.3:
  sum = phi [bb.1: sum1], [bb.2: sum2]
  ret sum
```

这已经比“普通 dominated redundant binary”更强了，因为它不是只复用同一块里的老 binary，而是在做：

- join-local per-edge 投影
- predecessor-side binary reuse

所以它确实已经开始带一点：

`join-local GVN`

的味道。

### 5.13 这条 memory-aware CSE 现在还能混合几种 edge source

更强的一点是，当前它不再要求所有边都用同一种复用策略。

例如三路 join：

- 一条边已经有 predecessor-side binary
- 一条边可以直接 immediate folding
- 另一条边是 unknown global after call，只能保留一个 predecessor-side expression

当前也可以收成：

```text
phi [11], [22], [add(load_global g.0, 3)]
```

而不是被迫退回：

```text
add(phi(...), phi(...))
```

这很重要，因为它说明这条线现在已经不是“要么全复用、要么全失败”，而是：

`per-edge mixed reuse`

### 5.14 现在连缺失 predecessor binary 也能保守 materialize

再往前一点，当前这条 join-local memory-aware CSE 还多了一块很窄的 predecessor materialization。

如果某个 join-local safe binary：

- 在某条 predecessor edge 上，操作数已经都可得
- 但那个 predecessor 里还没有现成 binary

那么它现在可以：

1. 把那个 binary 直接补到 predecessor 末尾
2. 下一轮 fixed point 里复用它
3. 最后把 join 上的 binary 收成 phi

所以这条线已经不只是 CSE，也开始带一点：

`binary-side PRE`

的味道。

### 5.15 这条 binary projection 现在也能递归穿过 join block 自己的 SSA glue

还有一个最近很值钱的增强，是它不再只看“join block 第一层 binary”。

如果 join block 里是：

```text
sum  = add(phi_a, phi_g)
sum2 = add(sum, 5)
```

现在 projection 可以顺着：

- `sum`
- 再到 `sum2`

继续往每个 predecessor edge 上投影。

于是结果可以从：

```text
sum2 = add(phi(...), 5)
```

继续收成：

```text
sum2 = phi [16], [27], [...]
```

这说明这条线已经不是只能吃一层 join-local binary，而是开始能递归穿过一小段 pure expression tree。

---

## 6. 这次 dead-store 为什么算“真往前推了一层”

以前更容易删的是：

1. zero-use store
2. 直接被 later store 覆盖的 store

现在更强的是：

### 6.1 `store -> phi -> later overwrite`

如果一条旧 store 先流进了某个 memory phi，但这个 phi 只是把旧 memory state 中转给后面，而后面又立刻被 later overwrite 覆盖，并且中间没有：

- `load`
- `call`
- 真正 observing phi use

那么旧 store 也仍然可以删。

### 6.2 `loop-carried phi -> later overwrite`

loop 里同理：

- 旧 store 先进 loop-carried phi
- 后面真正生效的是 later overwrite
- 中间没有 observing use

那么旧 store 仍然是 dead。

这说明这次不是“多加了几个 case”，而是：

`store-side reasoning 终于沿着 memory version/use 链完整走起来了`

---

## 7. `canonicalize_memory_values` 和 `run_pipeline` 的区别

### 7.1 `memory_ssa_pass_canonicalize_memory_values(...)`

这是 memory-only canonicalize 层。

它当前循环跑：

1. `scalar_replace_slots`
2. `eliminate_redundant_stores`
3. `eliminate_dead_stores`
4. 最小 Value-SSA cleanup tail：
   - `simplify_trivial_values`
   - `eliminate_dead_value_defs`
   - `simplify_cfg`
   - 再做一次 trivial simplify / DCE revisit

直到 dump-stable fixed point。

它的目标是：

`把 memory-derived value form 收稳`

把它写成 lesson 伪代码会更容易记：

```text
canonicalize_memory_values(program):
    repeat until dump stable or iteration limit:
        scalar_replace_slots(program)
        eliminate redundant stores
        eliminate dead stores
        simplify trivial values
        eliminate dead value defs
        simplify cfg
        simplify trivial values again
        eliminate dead value defs again
```

所以这条 pipeline 的核心是：

- scalar replacement consumer 在最前
- 只接一小段 value_ssa cleanup tail
- 目标是把 memory 这层造成的 value 形状稳定下来

### 7.2 `memory_ssa_pass_run_pipeline(...)`

full pipeline 则是在上面的基础上继续接：

- `eliminate_redundant_memory_binaries`
- `normalize_binary_operands`
- `fold_constants`
- `simplify_algebraic_identities`
- 更多 trivial simplify / DCE / CFG revisit

所以它更像：

`full memory-aware mini-pipeline`

也就是：

- 前者：先把 memory cleanup 做稳
- 后者：继续把 arithmetic/value-only cleanup 也吃掉

同样可以补成伪代码：

```text
run_pipeline(program):
    repeat until dump stable or iteration limit:
        eliminate_redundant_memory_binaries(program)
        eliminate dead value defs
        simplify cfg
        simplify trivial values
        eliminate dead value defs
```

所以两者的差别不是“一个多跑几条 pass”这么简单，而是：

- `canonicalize_memory_values` 停在 memory-derived normalization
- `run_pipeline` 则继续把 memory-aware binary CSE 也一起吃掉，并收成更强终点

---

## 8. lower-ir one-shot bridge 现在已经分层

当前 bridge 不是只有一条了。

### memory-only tier

```c
memory_ssa_pass_build_memory_canonicalized_from_lower_ir(...)
```

做的是：

`lower_ir -> value_ssa -> memory-only canonicalize`

### full tier

```c
memory_ssa_pass_build_canonicalized_from_lower_ir(...)
```

做的是：

`lower_ir -> value_ssa -> full memory-aware pipeline`

然后 `value_ssa_pass` 现在也直接给了 Value-SSA-facing wrapper：

- `value_ssa_build_memory_value_canonicalized_from_lower_ir(...)`
- `value_ssa_build_memory_canonicalized_from_lower_ir(...)`

所以现在 lower-ir one-shot 已经明确分成三档：

1. classic canonicalize
2. memory-value canonicalize
3. full memory-aware canonicalize

如果把这一层再和 `value_ssa_pass` 的 facade 对起来，可以记成：

```text
lower_ir
  -> value_ssa_build_canonicalized_from_lower_ir(...)              // classic
  -> value_ssa_build_memory_value_canonicalized_from_lower_ir(...) // memory-value
  -> value_ssa_build_memory_canonicalized_from_lower_ir(...)       // memory-full
```

也就是说，`memory_ssa_pass` 负责“真正的 memory-aware bridge 能力”，而 `value_ssa_pass` 负责“把这三档能力以 Value-SSA caller 能直接消费的方式暴露出去”。

### 8.1 哪些 caller 该用哪一档

lesson 里可以把使用建议写得直接一点：

**classic**

- 你只想要 old Value-SSA canonicalize
- 不需要 join/loop-aware memory cleanup

**memory-value**

- 你想先吃掉 memory-derived cleanup
- 但还不想继续折 arithmetic/value-only 形状
- 例如你还想保留 `add 7, 5` 这种值层结构给后续实验看

**memory-full**

- 你想直接拿当前仓库里最强的 one-shot canonicalized Value-SSA
- 普通生产式 caller 更适合从这里起步

---

## 9. 当前支持 / 不支持什么

**已经支持：**

- local/global load forwarding
- dominating equivalent-value reuse
- acyclic partial PRE on joins
- dominating memory-phi -> value-phi promotion
- local/global scalar replacement consumers
- redundant store cleanup
- dead store cleanup
- same-value join / loop forwarding
- phi-overwrite / loop-overwrite deadness
- memory-only canonicalization fixed point
- full memory-aware mini-pipeline fixed point
- lower-ir one-shot memory bridge

**还没有：**

- alias-sensitive memory optimization
- pointer/object memory model
- full general PRE framework
- full mem2reg / full scalar replacement framework
- unrestricted loop-header/backedge promotion
- global-side激进策略之外的更完整优化面
- default whole-compiler pass pipeline integration

如果把这层的工程边界压成一句更准的话，当前应该讲成：

**已经有的是：**

- 真实可调用的 memory-aware cleanup
- join/loop/call-aware memory reasoning
- 第一小块 memory-aware PRE
- 第一小块 slot promotion
- 第一批 scalar replacement consumer
- memory-only 与 full 两层固定点 pipeline
- lower-ir one-shot bridge

**还没有的是：**

- alias-heavy memory optimization
- pointer/object/heap pass
- 更激进的跨过程 memory optimization
- 真正 whole-compiler 默认总管线整合

---

## 10. 测试看什么

最相关的是：

- `tests/memory_ssa/memory_ssa_pass_test.c`

当前 authority 主要锁：

- join-preserved local/global forwarding
- same-value phi forwarding
- dominating equivalent-value reuse
- acyclic partial PRE
- scrambled / multi-pred partial PRE
- ancestor-value-preferred partial PRE
- loop-header negative boundary
- local/global slot promotion
- local/global/mixed scalar replacement
- differing-value phi 保守停下
- call barrier 下 global load 不 forward
- branch-exit dead local store 删除
- dead global store 删除
- phi-overwrite / loop-overwrite deadness
- verifier-gated fixed-point pipeline
- lower-ir one-shot memory bridge dump authority

---

## 11. 一句话总结

`memory_ssa_pass` 现在已经是一层独立的 memory-aware transform layer：它以 Memory-SSA version/use 事实为依据，在 Value-SSA 程序上稳定做 forwarding、narrow PRE、slot promotion、scalar replacement consumer、redundant/dead store cleanup，以及两层 lower-ir bridge canonicalization。
