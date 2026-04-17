# Memory SSA Lesson（compiler_lab）

> 目标：解释当前 `memory_ssa` 模块为什么存在、它和 `value_ssa` 的边界是什么、当前数据模型/analysis 能做到哪，以及这层为什么还不是完整 memory optimizer。

## 导学

当前项目的中端层次已经不只停在：

`lower_ir -> value_ssa`

而是开始继续往：

`value_ssa -> memory_ssa`

推进。

这份讲义建议按下面顺序读：

1. 为什么 `value_ssa` 之后还需要 `memory_ssa`
2. `include/memory_ssa.h` 的数据模型
3. `src/memory_ssa/` 的 builder / dump / verifier / analysis
4. `tests/memory_ssa/` 的 authority

---

## 1. 为什么还需要 `memory_ssa`

`value_ssa` 规范化的是：

- temp / computed value flow
- phi-based value join

但它没有系统化的是：

- local/global slot 的 memory version
- 某个 `load_*` 究竟读到了哪次 store 的状态
- 某个 `store_*` 覆盖掉的旧 memory state 是什么

所以 `memory_ssa` 的意义不是“把 SSA 再做一遍”，而是：

`把 slot/memory state 的流动也显式化`

这一步一旦有了，后面这些问题才会有正式语义抓手：

- load forwarding
- redundant store cleanup
- dead store elimination
- join / loop 上的 memory merge
- call barrier 建模

---

## 2. 文件定位

- 接口：`include/memory_ssa.h`
- 聚合入口：`src/memory_ssa/memory_ssa.c`
- dump：`src/memory_ssa/memory_ssa_dump.inc`
- verifier：`src/memory_ssa/memory_ssa_verify.inc`
- analysis：`src/memory_ssa/memory_ssa_analysis.inc`
- bridge：`src/memory_ssa/memory_ssa_from_value_ssa.inc`

相关测试：

- `tests/memory_ssa/memory_ssa_regression_test.c`
- `tests/memory_ssa/memory_ssa_verifier_test.c`
- `tests/memory_ssa/memory_ssa_analysis_test.c`

---

## 3. 这层现在是什么，不是什么

### 当前它是什么

当前 `memory_ssa` 已经是一层独立模块，具备：

1. 独立数据模型
2. builder API
3. dump
4. verifier
5. `value_ssa -> memory_ssa` bridge
6. version def/use analysis
7. 一批跨层 query helper

### 当前它不是什么

它现在**不是**：

1. 完整 alias analysis
2. 指针/对象图建模
3. backend memory model
4. 完整 memory optimizer

所以更准确地说：

`memory_ssa` 现在是 tracked local/global slot 的 memory-version layer`

---

## 4. 数据模型

### 4.1 tracked slot

当前这层最底层对象不是任意地址，而是：

`MemorySsaTrackedSlot`

它记录：

- 对应哪个 `ValueSsaSlotRef`
- debug name
- 是否有 entry version
- entry version id

所以第一版 scope 很明确：

- 只跟踪现有 local/global slot
- 不引入任意 address / pointer

### 4.2 memory version

这层真正的核心身份不是 `ssa.N`，而是：

`mem.N`

每个 version 都会记录：

- defining slot
- defining site kind
- use count
- use-site list

定义种类当前分三类：

- `MEMORY_SSA_VERSION_DEF_ENTRY`
- `MEMORY_SSA_VERSION_DEF_PHI`
- `MEMORY_SSA_VERSION_DEF_INSTR`

### 4.3 memory phi

join 处除了 value phi，还会有：

`MemorySsaPhi`

直观上像：

```text
mem.3 = phi(slot a.0) [bb.1: mem.1], [bb.2: mem.2]
```

它表示的是：

- 某个 slot 在 block-entry 处的 memory state
- 来自多个 predecessor 的不同 memory version

### 4.4 instruction-local access

当前 `MemorySsaInstruction` 不是发明了一套新指令，而是：

- 继续带着原来的 `ValueSsaInstruction`
- 再额外挂上 `MemorySsaAccess[]`

每个 access 记录：

- `slot_id`
- `use_version`
- `def_version`

所以这层更像：

`value instruction + memory-version annotation`

这也是为什么 dump 里现在会有：

```text
mem.1 = store_local a.0 @ mem.0, 7
```

这种“写前态 + 写后态”都显式可见的形状。

### 4.5 version use role 也要一起看

如果只记“有 use-site list”，其实还不够，因为 `memory_ssa` 现在会区分：

- `MEMORY_SSA_VERSION_USE_ROLE_PHI_INPUT`
- `MEMORY_SSA_VERSION_USE_ROLE_LOAD`
- `MEMORY_SSA_VERSION_USE_ROLE_STORE_PREVIOUS`
- `MEMORY_SSA_VERSION_USE_ROLE_CALL`

这四类 role 很关键，因为它们决定了：

- 这次 use 是“值被读到了”
- 还是“只是作为 store 的前态”
- 还是“流进了 phi”
- 还是“被 call 当作 barrier/observation 消费”

后面的 dead-store reasoning，正是沿着这些 use role 才能把“真观察”和“中转 use”区分开。

---

## 5. 一个最小直线例子

如果 `value_ssa` 里是：

```text
func main() {
  bb.0:
    store_local a.0, 7
    ssa.0 = load_local a.0
    ret ssa.0
}
```

那么 memory-ssa 视角下，更重要的不是“有条 store 和 load”，而是：

- 这次 `store_local a.0, 7` 产生了一个新的 memory version
- 这次 `load_local a.0` 明确使用这个 version

也就是说，问题从：

- “有个 load”

变成：

- “这个 load 读的是哪个 `mem.N`”

如果把它补成更接近 dump 的视角，可以粗略记成：

```text
func main() {
  bb.0:
    mem.0 = store_local a.0, 7
    ssa.0 = load_local a.0 @ mem.0
    ret ssa.0
}
```

当前 lesson 的重点不在精确 dump 字符串，而在这个结构：

- `store_local` 产出 `mem.0`
- `load_local` 显式 use `mem.0`
- 从此以后，“load 读到了谁”就不再需要靠 ad hoc 猜测

---

## 6. Bridge：`value_ssa -> memory_ssa`

当前 bridge 入口是：

```c
int memory_ssa_build_from_value_ssa(const ValueSsaProgram *program,
    MemorySsaProgram *out_program,
    MemorySsaError *error);
```

它当前做的事可以概括成：

1. 遍历 `ValueSsaProgram`
2. 为 tracked slot 建 memory object
3. 在 block-entry 建 memory phi
4. 在 `load_* / store_* / call` 上挂 memory access
5. 产出 memory version graph

所以它不是“重写 `value_ssa`”，而是：

`在 value_ssa 上方叠一层 memory-version view`

### 6.1 bridge 的伪代码

如果按 lesson 的口径把 bridge 压成一段伪代码，最适合记成：

```text
build_from_value_ssa(program):
    for each function:
        create tracked slot table for locals/globals we care about
        assign entry version to parameter-like / seed memory states when needed
        pre-create block-entry memory phis for tracked slots at joins

        walk blocks in CFG order:
            on block entry:
                bind current slot-state to entry version or phi-result version

            for each value_ssa instruction:
                if load(slot):
                    attach access(use = current_version(slot))

                else if store(slot, value):
                    new_version = allocate_version(slot)
                    attach access(use = current_version(slot), def = new_version)
                    current_version(slot) = new_version

                else if call(...):
                    for each tracked slot touched by the call:
                        attach use and/or def access conservatively
                        update current_version(slot) when call may write it

            on edge to successor:
                feed current slot versions into successor phi inputs
```

这段伪代码里最重要的不是细节，而是 bridge 的精神：

- `value_ssa` 还是原程序
- `memory_ssa` 只是把每个 tracked slot 的 reaching version 显式挂出来

### 当前支持 / 不支持什么

**当前支持：**

- tracked local/global slot
- block-entry memory phi
- instruction-local access
- version def/use
- `value_ssa -> memory_ssa` bridge

**当前不支持：**

- 任意 pointer/object/heap
- alias set
- escaping address
- rich source-level memory object graph

---

## 7. Analysis：这层现在最值钱的三类问题

### 7.1 version def/use

`memory_ssa_compute_def_use_analysis(...)` 当前会整理：

- 每个 version 的 defining slot
- defining site kind
- defining block/phi/instruction 位置
- use count
- use-site list

这是后续 `memory_ssa_pass` 的真正语义基础。

一个更像实现的伪代码可以写成：

```text
compute_def_use(function):
    for each version:
        record:
            def_slot
            def_kind
            def_block / def_phi / def_instruction

    for each phi input:
        add use-site(version, role = PHI_INPUT)

    for each instruction access:
        if access has use_version:
            role =
                LOAD            if instruction is load
                STORE_PREVIOUS  if instruction is store
                CALL            if instruction is call
            add use-site(version, role)
```

所以 `def/use` 这里不是 value-SSA 里那种“ssa.N 被谁用了”的单一概念，而是：

`某个 memory version 在什么语义角色下被谁消费`

### 7.2 “这次 load 读到了什么”

通过：

```c
memory_ssa_resolve_load_store_value(...)
```

现在可以回答：

`某个 load 最终读到的值是什么`

而且现在已经不只认：

- direct reaching store

还可以穿过：

- same-value memory phi

但它仍然会保守停在：

- entry seed
- call-defined version
- cyclic self-dependence
- differing incoming value

可以把它压成下面这种伪代码直觉：

```text
resolve_version_value(version):
    if version defined by store(slot, value):
        return value

    if version is entry seed:
        return unknown

    if version defined by call:
        return unknown

    if version defined by phi:
        resolve every incoming version recursively
        ignore pure self-cycle backedge when checking agreement
        if all non-cyclic incoming values are the same:
            return that value
        else:
            return unknown
```

于是：

- same-value phi 能继续推
- differing-value phi 必须停
- loop 里自环输入不会直接把结果污染成“永远 unknown”

### 7.3 “这次 store 覆盖了什么 / 死没死”

现在 store-side reasoning 已经补到了两步：

1. `memory_ssa_resolve_store_previous_value(...)`
   - 看这次 store 覆盖掉的旧值
2. `memory_ssa_is_trivially_dead_store(...)`
   - 看这次 store 产出的 memory version 后面是否真有任何 use

所以当前 reasoning 已经不再只是“后面有没有下一条 store”，而是：

`这个 memory version 到底有没有被真正观察`

这也是为什么当前 authority 已经覆盖到：

- zero-use dead store
- overwritten-store deadness
- phi-overwrite deadness
- loop-overwrite deadness

这里也值得直接补一段伪代码，因为这是很多人第一次接触 `memory_ssa` 时最容易模糊的地方：

```text
is_trivially_dead_store(store_version):
    look at all uses of store_version

    if there are no uses:
        return dead

    for each use:
        if use is load:
            return live
        if use is call:
            return live
        if use is phi input:
            if phi-result is later observed:
                return live
            if phi-result is only forwarded into a later overwrite:
                continue
        if use is store-previous:
            continue

    return dead
```

这段伪代码要表达的是：

- `phi` 本身不自动让旧 store 变 live
- 真正让它变 live 的，是后面有没有“观察”这条 memory state
- 纯粹被 later overwrite 覆盖掉的版本，不该因为经过 phi 就被保活

### 7.4 两个跨层 query 现在也很值钱

这层最近还多了两条很适合写进 lesson 的 API：

```c
int memory_ssa_value_matches_slot_version(...);
int memory_ssa_find_block_equivalent_value(...);
```

它们的作用可以分别理解成：

1. `value_matches_slot_version`
   - 问：某个 SSA value，是不是就等于“slot S 在 version V 时的值”
2. `find_block_equivalent_value`
   - 问：在 block B 里，能不能找到一个 SSA value，等于“slot S 在 version V 时的值”

这两条 query 的意义非常实际：

- pass 层想知道“这个 store 写回的，是不是原来那个值”
- pass 层想知道“join block 里能不能用现有 SSA value 复用，而不是再 load 一次”

这也是后来 `memory_ssa_pass` 能做 parameter-local / post-call global reuse 的分析基础。

---

## 8. 两个最典型例子

### 8.1 same-value phi 可以继续解析

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

join 处虽然有 memory phi，但两个 incoming 都是同一个值 `7`。

所以当前 analysis 仍然能推出：

- `load_local a.0` 读到的还是 `7`

### 8.2 differing-value phi 必须停下

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

这里 incoming 值不同，所以当前 analysis 必须停下，不能把 load 继续解析成一个唯一 immediate。

这两个例子合在一起，正好说明：

- `memory phi` 不等于“不能继续推值”
- 但也不等于“看到 phi 就能乱推”

### 8.3 parameter-local entry version 为什么通常不继续解析

还有一类很适合放进 lesson 的边界是 parameter local：

```text
func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    ret ssa.0
}
```

这里 `a.0` 在 entry 上有一个 memory seed/version，但 analysis 当前并不会瞎猜它等于哪个具体 immediate。

所以：

- `load_local a.0` 的 reaching version 是已知的
- 但 `resolve_load_store_value(...)` 往往仍然会给出 `unknown value`

这正好说明：

`知道 load 读哪个 version` 和 `知道这个 version 的具体值` 是两件事

### 8.4 call barrier 为什么会停下

再看 global：

```text
store_global g.0, 9
call touch()
ssa.0 = load_global g.0
ret ssa.0
```

在 current authority 下，这里通常不会把 load 直接解析成 `9`，因为：

- call 可能读/写这个 global
- call 产出的 memory version 不是“已知 store value”

所以 analysis 在这里停下，不是能力不够，而是：

`memory barrier 本来就应该切断不安全的值推理`

---

## 9. 当前支持 / 不支持什么

**已经支持：**

- memory-SSA skeleton
- tracked slot builder
- entry version
- memory phi
- instruction-local access
- dump / verifier
- `value_ssa -> memory_ssa` bridge
- version def/use
- load/store value-resolution query
- slot-version/value equivalence query
- block-equivalent value query

**还没有：**

- alias analysis
- heap/object graph
- pointer-based memory modeling
- full memory optimizer
- allocator/backend use of memory versioning

如果把 lesson 口径讲得更像工程边界，当前最稳妥的是：

**已经稳了的，是“语义地基”：**

- slot 跟踪
- version graph
- phi merge
- load/store/call access 注记
- def/use
- value resolution
- dead-store reasoning query

**还没做的，是“更大一层的策略”：**

- 更激进的 whole-program memory optimization
- alias/pointer/object 建模
- heap-like memory object
- backend 直接消费 memory version 做寄存器或调度

---

## 10. 测试看什么

最相关的是：

### `tests/memory_ssa/memory_ssa_regression_test.c`

锁：

- bridge 之后的 representative dump authority
- tracked slot / entry version / access annotation / phi 形状

### `tests/memory_ssa/memory_ssa_verifier_test.c`

锁：

- duplicate version definition
- load without access
- phi input count mismatch

### `tests/memory_ssa/memory_ssa_analysis_test.c`

锁：

- version def/use
- load/store resolution
- same-value phi resolution
- call barrier
- entry seed / differing-value phi 保守停下

---

## 11. 一句话总结

当前 `memory_ssa` 已经把：

`load 读谁、store 覆盖谁、join 处 memory state 怎么合流`

这套基础语义正式做出来了。它还不是完整 memory optimizer，但已经是后续 memory-aware pass 的正式基础层。
