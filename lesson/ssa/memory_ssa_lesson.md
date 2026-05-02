# Memory SSA Lesson（compiler_lab）

> 目标：解释 `memory_ssa` 为什么要从 `value_ssa` 旁边单独长出来，它当前到底把什么“新信息”显式化了，`src/memory_ssa/` 里真实有哪些 builder / verifier / analysis 逻辑，以及这层代码已经能支持哪些后续优化问题。

## 一句话定位

`memory_ssa` 是把 slot / memory state flow 也正式变成 SSA-like artifact 的那一层，它补的是 value SSA 故意不直接表达的内存版本事实。

## 常见误解

- 误解一：有了 value SSA，再加点 load/store 规则就够了。
  - 真正的 memory version、memory phi、call barrier、slot state flow 需要单独的数据模型。
- 误解二：memory SSA 是把 value SSA 再做一遍。
  - 更准确地说，它补的是“内存状态怎么流”，不是“算术值怎么流”。

## 导学

如果说：

- `value_ssa` 回答的是“计算值 `ssa.N` 是怎么流动的”

那么：

- `memory_ssa` 回答的是“slot 状态 `mem.N` 是怎么流动的”

这层不是把 SSA 再做一遍。

它真正新增的是：

`slot-based memory-state versioning`

也就是：

- `load_local a.0` 读的是哪个 `mem.N`
- `store_global g.0` 覆盖的是哪个旧 `mem.N`
- join / loop header 上某个 slot 当前状态是不是要合成 memory phi
- call 到底是把哪些 global state 当成 read / write barrier

这份讲义建议按下面顺序读：

1. 先理解为什么 `value_ssa` 之后还需要 `memory_ssa`
2. 再看 `include/memory_ssa.h`，建立 tracked slot / memory version / access annotation 的整体印象
3. 再看 `src/memory_ssa/` 的 bridge 主线，理解 `value_ssa -> memory_ssa` 到底做了哪些翻译
4. 最后带着测试例子去看：
   - phi / loop / call 是怎么落到 memory SSA 上的
   - analysis 现在已经能回答哪些问题

学完你应该能：

1. 解释 `memory_ssa` 和 `value_ssa` / `memory_ssa_pass` 的边界
2. 说明这层当前真正新增的是哪些 memory facts
3. 看懂一个 `value_ssa -> memory_ssa` 的代表性 before/after
4. 说明为什么后面的 forwarding / dead-store / scalar replacement 会依赖这层

---

## 前置阅读

最推荐先读：

1. `lesson/ssa/value_ssa_lesson.md`
2. `lesson/core/lower_ir_lesson.md`

如果你还没先理解：

- value SSA 目前已经负责哪些值流事实
- lower IR 里的 local/global slot 边界怎么长

那 memory SSA 这里“为什么要单独显式化 slot state flow”会不够明显。

## 读完后接哪篇

最自然往下接：

1. `lesson/ssa/memory_ssa_pass_lesson.md`
2. `lesson/ssa/value_ssa_pass_lesson.md`

最常见的两条读法是：

- 想看 memory-aware 变换真正怎么利用这些 facts
  - 先接 `memory_ssa_pass`
- 想横向对比 value-side 和 memory-side pass 的分层
  - 再看 `value_ssa_pass`

---

## 1. 为什么需要 `memory_ssa`

`value_ssa` 已经把这些事情做好了：

- computed value 的 def/use
- ordinary value phi
- block / dominator / frontier 等共享 CFG facts

但它没有系统化回答的是：

- local/global slot 的状态版本是谁
- 某个 `load_*` 读到的是 entry seed、某次 store，还是某个 join phi
- 某个 `store_*` 写入前，slot 旧值是什么
- call 对 global state 是“读了它”“写了它”“还是根本没碰它”

所以 `memory_ssa` 的意义不是：

`在已有 value SSA 上再套一层语法糖`

而是：

`把 slot state flow 也做成 verifier-backed artifact`

一旦这层存在，后面这些问题才有正式抓手：

- load forwarding
- redundant store cleanup
- dead store elimination
- slot promotion
- scalar replacement
- memory-aware CSE / PRE

这也是为什么 `docs/ssa/MEMORY_SSA_DESIGN.md` 一开始就把它定义成：

`small slot-based memory SSA`

而不是 pointer-aware 的通用 memory model。

---

## 2. 为什么不能把它继续塞进 `value_ssa`

`value_ssa` 负责的是：

- 值表示
- verifier
- CFG/shared analysis
- strict conversion

但它不应该继续直接负责：

- memory version namespace
- slot-level phi placement
- call memory effect summary
- load/store previous-state resolution
- version observation / dead-store reasoning

不然 `value_ssa` 会变成：

- 既是 value-flow IR
- 又是 memory-state IR
- 还顺手兼做一堆 memory analysis

这样边界会很快混掉。

项目现在的拆法非常清楚：

- `value_ssa`
  - 回答 value flow
- `memory_ssa`
  - 回答 slot-state flow
- `memory_ssa_pass`
  - 消费 `memory_ssa` facts 做 memory-aware rewrite

一句话压缩：

- `value_ssa`：`ssa.N`
- `memory_ssa`：`mem.N`
- `memory_ssa_pass`：拿 `ssa.N + mem.N` 一起做变换

---

## 3. 这层当前是什么，不是什么

### 3.1 当前它是什么

当前 `memory_ssa` 已经是一层真实独立模块，具备：

1. 独立数据模型
2. builder API
3. verifier
4. dump
5. `value_ssa -> memory_ssa` bridge
6. version def/use analysis
7. version-value resolution
8. dead-store query
9. 跨层“这个 SSA value 是否等于 slot 的某个 memory version”查询

也就是说，它已经不是设计草图，而是：

`可建、可验、可查、可被 pass 消费的 artifact 层`

### 3.2 当前它不是什么

它现在仍然**不是**：

1. 完整 alias analysis
2. 指针/对象图 memory SSA
3. backend memory model
4. register allocator
5. 直接做 rewrite 的 pass 层

所以最准确的说法仍然是：

`tracked local/global slot memory-version layer`

---

## 4. 文件定位

- 接口：`include/memory_ssa.h`
- 聚合入口：`src/memory_ssa/memory_ssa.c`
- verifier：`src/memory_ssa/memory_ssa_verify.inc`
- dump：`src/memory_ssa/memory_ssa_dump.inc`
- analysis：`src/memory_ssa/memory_ssa_analysis.inc`
- bridge：`src/memory_ssa/memory_ssa_from_value_ssa.inc`

相关测试：

- `tests/memory_ssa/memory_ssa_regression_test.c`
- `tests/memory_ssa/memory_ssa_verifier_test.c`
- `tests/memory_ssa/memory_ssa_analysis_test.c`
- `tests/memory_ssa/memory_ssa_pass_test.c`

这里值得特别注意一件事：

虽然 `memory_ssa_pass_test.c` 不属于 core `memory_ssa` 模块本身，但它是这层很重要的“下游 authority”，因为它证明：

`这些 memory facts 不只是能打印出来，还真的足够支撑后面的 memory-aware passes`

---

## 5. `include/memory_ssa.h`：当前数据模型

### 5.1 `MemorySsaTrackedSlot`

第一版 memory SSA 跟踪的不是任意地址，而是：

`tracked slot`

字段包括：

- `id`
- `slot`
- `debug_name`
- `has_entry_version`
- `entry_version_id`

这表示当前 scope 很克制：

- 只跟踪 `local`
- 只跟踪 `global`
- 不跟踪 pointer / heap / address-taken object

所以 lesson 里最重要的边界要记成：

`memory_ssa 跟踪的是 slot identity，不是一般地址 identity`

### 5.2 `MemorySsaPhi`

join/header 上的 memory merge 会变成：

`MemorySsaPhi`

字段包括：

- `slot_id`
- `result_version_id`
- `inputs[]`

其中每个输入是：

- `predecessor_block_id`
- `version_id`

可以把它先记成：

```text
mem.K = phi(slot.S) [pred1: mem.A], [pred2: mem.B], ...
```

这个 phi 的意义不是“某个 SSA value 的 join”，而是：

`slot.S 在这个 block entry 的内存状态`

### 5.3 `MemorySsaAccess`

`MemorySsaInstruction` 没有发明新指令集，它还是包着原来的 `ValueSsaInstruction`。

真正新增的是每条指令旁边的：

`MemorySsaAccess[]`

每个 access 记录：

- `slot_id`
- `has_use_version`
- `use_version_id`
- `has_def_version`
- `def_version_id`

这很关键，因为它把“这条指令和 memory state 的关系”显式挂了出来。

最典型的几种形状是：

```text
ssa.0 = load_local a.0 @ mem.0
```

表示：

- use `mem.0`
- 不定义新 memory version

```text
mem.1 = store_local a.0 @ mem.0, 7
```

表示：

- use `mem.0` 作为写前状态
- def `mem.1` 作为写后状态

```text
call touch() [slot.0 use=mem.1 def=mem.2]
```

表示：

- 这次 call 把某个 tracked global 读作 `mem.1`
- 同时又把它写成了 `mem.2`

### 5.4 `MemorySsaVersionDefKind`

memory version 的定义来源当前分三类：

- `MEMORY_SSA_VERSION_DEF_ENTRY`
- `MEMORY_SSA_VERSION_DEF_PHI`
- `MEMORY_SSA_VERSION_DEF_INSTR`

这很重要，因为 analysis 后面会根据“版本定义来自哪里”做不同判断。

比如：

- entry version 通常没有可直接解析的值
- instr-defined version 可能直接解析到某个 store 的 value
- phi-defined version 可能要递归看所有输入是否一致

### 5.5 `MemorySsaVersionUseRole`

这份 header 里一个特别值得单独讲的 enum 是：

- `MEMORY_SSA_VERSION_USE_ROLE_PHI_INPUT`
- `MEMORY_SSA_VERSION_USE_ROLE_LOAD`
- `MEMORY_SSA_VERSION_USE_ROLE_STORE_PREVIOUS`
- `MEMORY_SSA_VERSION_USE_ROLE_CALL`

如果不记这个角色分类，后面很多 analysis 会看不懂。

因为 `mem.N` 被“用到”并不都代表它真被观察了：

- `LOAD`
  - 真正读值
- `CALL`
  - 被 call 当作 memory-observing barrier 消费
- `STORE_PREVIOUS`
  - 只是作为某次 store 的旧版本
- `PHI_INPUT`
  - 只是流进一个后续 merge

后面的 trivial DSE 正是靠这个区分，才不会把“只是被下一个 store 覆盖前态引用了一下”误判成 live use。

### 5.6 `MemorySsaDefUseAnalysis`

这层 analysis 不是只给你一个“use count”。

它会缓存：

- `version_count`
- `def_slot_ids`
- `def_kinds`
- `def_block_ids`
- `def_phi_indices`
- `def_instruction_indices`
- `has_def`
- `use_counts`
- `use_offsets`
- `use_sites`

也就是说，它已经是：

`per-version definition table + compact use-site table`

不是一堆 ad hoc 辅助函数。

---

## 6. 这层到底“改了什么 / 增加了什么”

这一节专门按你一直强调的口径来讲：

- 从上一层翻译了什么
- 这层新增了什么信息
- 为什么这些新增信息有用

### 6.1 从 `value_ssa` 到 `memory_ssa`，新增了第二套名字空间

在 `value_ssa` 里，我们主要看到的是：

- `ssa.0`
- `ssa.1`
- `phi`
- `load_*`
- `store_*`

到了 `memory_ssa`，最显眼的新增就是：

- `mem.0`
- `mem.1`
- `mem.2`

也就是说，系统现在不仅追踪：

- 计算值是谁

还追踪：

- slot 状态版本是谁

### 6.2 新增了“写前态 / 写后态”信息

原来看到：

```text
store_local a.0, 7
```

现在会看到：

```text
mem.1 = store_local a.0 @ mem.0, 7
```

这条信息真正新增的是：

1. 这次 store 之前，`a.0` 处于 `mem.0`
2. 这次 store 之后，`a.0` 变成 `mem.1`

从此以后，“这是不是写回同值”“之前那条 store 还活不活”都可以基于版本来问。

### 6.3 新增了 join / loop 上的 memory phi

原来 `value_ssa` 里就有 value phi。

现在 `memory_ssa` 又新增了：

```text
mem.2 = phi slot.0 [bb.1: mem.0], [bb.2: mem.1]
```

这表示：

`slot.0` 在这个 join 的 block-entry 处，不再是单一路径上的旧状态，而是多 predecessor 合流的 memory state。

### 6.4 新增了 call effect annotation

原来 `call` 在 `value_ssa` 里主要就是：

- callee name
- args
- result

现在在 `memory_ssa` 里，`call` 还可能带：

```text
call touch() [slot.0 use=mem.1 def=mem.2]
```

这表示：

- call 读了某个 tracked global 的旧状态
- call 还可能把它写成新状态

这一步非常关键，因为它把“call 是个 barrier”从模糊语义变成了显式 use/def。

---

## 7. 一个最小直线例子

来自 `tests/memory_ssa/memory_ssa_regression_test.c` 的最小直线例子，dump 现在就是：

```text
func main() {
  slots:
    slot.0 = local a.0
  bb.0:
    mem.0 = store_local a.0, 7
    ssa.0 = load_local a.0 @ mem.0
    ret ssa.0
}
```

这个例子最重要的不是语法，而是结构。

你应该看到：

1. `a.0` 被收进 tracked slots
2. store 产生 `mem.0`
3. load 显式 use `mem.0`

也就是说，原来 `load_local a.0` 只是“读 slot”，现在它已经变成：

`读 slot a.0 在 version mem.0 下的值`

---

## 8. join 例子：memory phi 到底长什么样

同一个 regression test 里，diamond join 的结果会变成：

```text
func main() {
  slots:
    slot.0 = local a.0
  bb.0:
    br 1, bb.1, bb.2
  bb.1:
    mem.1 = store_local a.0, 1
    jmp bb.3
  bb.2:
    mem.2 = store_local a.0, 2
    jmp bb.3
  bb.3:
    mem.0 = phi slot.0 [bb.1: mem.1], [bb.2: mem.2]
    ret 0
}
```

这个例子很适合回答：

`memory_ssa 增加了什么？`

答案是：

- 它把 join 处的 slot state 显式做成了 memory phi

注意这不是 value phi。

这里的 `mem.0` 不是某个运算结果，而是：

`a.0 在 bb.3 入口时的状态`

---

## 9. loop 例子：loop header 上的 memory phi

loop 例子在 regression test 里会变成：

```text
func main() {
  slots:
    slot.0 = local a.0
  bb.0:
    mem.1 = store_local a.0, 0
    jmp bb.1
  bb.1:
    mem.0 = phi slot.0 [bb.0: mem.1], [bb.2: mem.2]
    br 1, bb.2, bb.3
  bb.2:
    mem.2 = store_local a.0 @ mem.0, 1
    jmp bb.1
  bb.3:
    ssa.0 = load_local a.0 @ mem.0
    ret ssa.0
}
```

这个例子说明了两件事。

### 9.1 header 上会有真正的 loop-carried phi

`mem.0` 的两个输入分别来自：

- 进入循环前的 `mem.1`
- 循环体回边上的 `mem.2`

所以这已经不是单纯的 join，而是：

`loop-carried memory state`

### 9.2 store 会显式标出它覆盖的旧状态

循环体里的：

```text
mem.2 = store_local a.0 @ mem.0, 1
```

这里 `@ mem.0` 特别关键。

它表示：

- body store 不是凭空造出 `mem.2`
- 而是在“header 当前 state = mem.0”的基础上写出 `mem.2`

这正是后面 loop dead-store / invariant reasoning 的基础。

---

## 10. call 例子：global barrier 怎么被显式化

### 10.1 declaration-only / unknown call

regression test 里有一条很典型的 case：

```text
declare touch()

func main() {
  slots:
    slot.0 = global g.0 entry=mem.0
  bb.0:
    mem.1 = store_global g.0 @ mem.0, 9
    call touch() [slot.0 use=mem.1 def=mem.2]
    ssa.0 = load_global g.0 @ mem.2
    ret ssa.0
}
```

这里值得拆开看：

1. `global g.0` 自带 `entry=mem.0`
2. store 之后是 `mem.1`
3. `touch()` 这一类未知 call 把 `g.0` 同时当成 read + write
4. 所以 call 后 global state 不再是 `mem.1`，而是新版本 `mem.2`
5. 后面的 load 只能从 `mem.2` 读

这说明当前 call 建模已经不是一句模糊的“call 会 kill globals”。

它现在是：

`call 通过 access annotation 显式 use/def 被影响的 tracked global`

### 10.2 internal call 读别的 global 时，不会乱杀当前 global

另一个特别好的例子是：

```text
func helper() {
  slots:
    slot.0 = global h.1 entry=mem.0
  bb.0:
    ssa.0 = load_global h.1 @ mem.0
    ret ssa.0
}

func main() {
  slots:
    slot.0 = global g.0 entry=mem.0
  bb.0:
    mem.1 = store_global g.0 @ mem.0, 9
    call helper()
    ssa.0 = load_global g.0 @ mem.1
    ret ssa.0
}
```

这个例子特别能说明当前 bridge 新增了什么：

`它已经有了按函数传播的 global read/write summary`

因为 `helper()` 只读 `h.1`，不写 `g.0`，所以：

- `main` 里的 `call helper()` 不会给 `g.0` 生成新版本
- 后面的 load 仍然读 `mem.1`

这一步很重要，它说明当前 call 模型已经比“所有 call 一律杀光所有 global”更精确了。

---

## 11. parameter / global 的 entry seed 是怎么来的

看 regression test 里的 parameter-local case：

```text
func main() {
  slots:
    slot.0 = local a.0 entry=mem.0
  bb.0:
    ssa.0 = load_local a.0 @ mem.0
    ret ssa.0
}
```

这里有一个很容易忽略但很关键的设计：

`不是所有 slot 都天然有 entry version`

当前 bridge 只给两类 slot 自动建 entry seed：

1. global
2. parameter local

原因很自然：

- global 在函数入口前就已经存在某种初始状态
- parameter local 代表实参传进来的初始状态
- 普通 local 如果还没 store，就不应该自动假装“它有已知 entry value”

这也是 `memory_ssa_create_entry_seed_versions(...)` 这段逻辑在做的事。

---

## 12. bridge 主线到底怎么做

这一节直接按 `src/memory_ssa/memory_ssa_from_value_ssa.inc` 来讲。

### 12.1 第一步：先收集 tracked slots

bridge 不会先把所有 local/global 都盲目塞进 tracked table。

当前 `memory_ssa_collect_tracked_slots(...)` 是扫描函数体里真正出现的：

- `load_local`
- `load_global`
- `store_local`
- `store_global`

再把对应 slot 收进 `tracked_slots[]`。

而 debug name 也不是硬编码的，它会尽量保留 source name，拼成：

- `a.0`
- `g.0`

所以这层既追踪 slot identity，也保留了 lesson / dump 很需要的人类可读名字。

### 12.2 第二步：为 global / parameter local 建 entry seed

`memory_ssa_create_entry_seed_versions(...)` 会给：

- global
- `is_parameter` 的 local

分配 entry version。

可以先记成：

```text
if slot is global or parameter-local:
    slot.entry = fresh mem.N
```

这一步解决的是：

`函数一开始，这个 slot 的初始 memory state 怎么表示`

### 12.3 第三步：先做 call global effect summary

这一步是当前实现里非常值得单独讲的点。

`memory_ssa_compute_call_global_effects(...)` 会遍历整个 `ValueSsaProgram`，迭代求每个函数对每个 global 的：

- reads?
- writes?

规则大致是：

- declaration-only function
  - 所有 global 都视作 read + write
- `load_global g`
  - 标记 read `g`
- `store_global g`
  - 标记 write `g`
- `call callee`
  - 把 callee 的 read/write summary 传播给 caller
- callee 未知
  - 所有 global 都视作 read + write

所以当前 call 模型并不是纯本地的，它已经是一层：

`interprocedural global effect summary`

当然它还是很保守，但比“一律全 kill”更强。

### 12.4 第四步：做每个 slot 的 phi placement

`memory_ssa_mark_phi_blocks(...)` 会针对每个 tracked slot：

1. 找出这个 slot 的定义块
2. 把“call 对 global 的写”也当作定义
3. 调 `value_ssa_compute_phi_placement(...)`
4. 记录哪些 block 上要给这个 slot 放 memory phi

这一步非常关键，因为它说明：

`memory_ssa 并没有重新发明一套 CFG 逻辑，而是重用了 value_ssa 的 frontier / phi-placement infrastructure`

### 12.5 第五步：预先创建所有 phi

`memory_ssa_precreate_all_phis(...)` 会先在所有需要的 block 上，把 slot 对应的 memory phi 都建出来，并分配结果版本号。

这么做的好处是：

- 后面进入 dominator walk 时，block entry 上有哪些 phi 已经固定
- successor edge 填输入时，只需要回填 predecessor 对应的 version

### 12.6 第六步：沿 dominator tree 走，并维护 current version

这一段是 bridge 真正的主干：

- `value_ssa_walk_dominator_tree(...)`
- enter 回调里先绑定 block phi result
- 再处理 block 内指令
- leave 回调里回滚 `current_versions`

也就是说，当前 bridge 的心智模型其实和构造 value SSA 很像：

1. 维护一张“当前 slot -> 当前 mem version”表
2. 进入 block 时先让 phi result 成为当前版本
3. 遇到 load/store/call 时查询或更新当前版本
4. 退出 block 时回滚到进入前

### 12.7 第七步：给 successor phi input 回填 predecessor version

block 处理完 terminator 后，bridge 会调：

- `memory_ssa_fill_successor_phi_inputs(...)`

把当前 predecessor 的版本号写进 successor phi 的对应输入格子里。

这一步做完，memory phi 就真正闭环了。

---

## 13. 一段伪代码看懂整个 bridge

把主线压缩一下，大致是：

```text
build_memory_ssa(value_program):
    compute per-function global read/write summary

    for each function:
        collect tracked slots from load/store
        create entry versions for globals and parameter locals
        compute CFG analysis
        mark phi blocks for each tracked slot
        precreate all memory phis

        dominator_walk(function):
            on_enter(block):
                for each phi in block:
                    current[phi.slot] = phi.result_version

                for each instruction:
                    if load slot:
                        attach access(use=current[slot])
                    if store slot:
                        new = fresh mem.N
                        attach access(use=current[slot]?, def=new)
                        current[slot] = new
                    if call:
                        for each affected global slot:
                            attach use/def according to call summary
                            if writes:
                                current[slot] = fresh mem.N

                fill successor phi inputs from current[]

            on_leave(block):
                rollback current[] to entry state

    verify(output)
```

这段伪代码里最值得记住的四件事是：

1. tracked slot 是按实际 memory op 收集的
2. phi placement 是逐 slot 做的
3. current version table 在 dominator walk 中维护
4. call 不是统一粗暴杀伤，而是按 global effect summary 精确附 access

---

## 14. analysis 现在到底能回答什么

旧的简单 lesson 容易把这层讲成“有个 dump，有个 verifier”。

但现在实际代码已经更强，尤其是 `memory_ssa_analysis.inc`。

### 14.1 `memory_ssa_compute_def_use_analysis(...)`

这一步会把每个 `mem.N` 的：

- 定义点
- 定义种类
- use count
- use site 列表

全都整理出来。

所以它不是简单统计，而是一个正式索引层。

### 14.2 `memory_ssa_resolve_version_value(...)`

这是这层最有用的 API 之一。

它在问：

`mem.N 当前能不能解析成一个唯一 ValueSsaValueRef？`

当前规则大致是：

- instr-defined 版本
  - 如果定义它的是 `store_*`，那它可能直接解析成这个 store 的 value
- phi-defined 版本
  - 递归看所有输入
  - 如果所有非循环输入都解析成同一个值，就解析成功
- entry-defined 版本
  - 通常解析不出具体值

这就让 `memory_ssa` 不只是“知道版本之间连着谁”，还知道：

`这个版本对应的值到底是不是唯一可知`

### 14.3 这里还有一层很关键的 cycle-aware 处理

`memory_ssa_resolve_version_value_recursive(...)` 会在递归里处理：

- 直接自环 phi

而外面还有一个：

- `memory_ssa_resolve_version_value_iterative(...)`

它专门处理：

- 间接 phi cycle
- 例如 `phiA -> phiB -> phiA`

源码注释里已经把原因写得很清楚：

- 第一遍可能因为 cycle partner 还没稳定，先得到“解析不出”
- 第二遍把这些 phi-defined unresolved 版本重置后再跑
- 这样已经在 pass1 里稳定下来的值可以被 cycle 成员重用

这说明当前 analysis 已经不是只会做 acyclic tree 上的小递归了。

### 14.4 `memory_ssa_resolve_load_store_value(...)`

这个 API 问的是：

`某条 load 当前读到的值，能不能直接解析出来`

它会取出该 load 的 `use_version_id`，再走版本解析。

所以后面的 load forwarding 本质上就是在消费这个问题的答案。

### 14.5 `memory_ssa_resolve_store_previous_value(...)`

这个 API 问的是：

`某条 store 覆盖掉的旧状态，当前能不能解析到唯一值`

这对判断：

- 冗余 store
- “写回同值”

非常有用。

### 14.6 `memory_ssa_is_trivially_dead_store(...)`

这个 API 不只是看 `use_count == 0`。

它会递归问：

`这个版本最终有没有被真正观察到？`

在 `memory_ssa_version_is_observed_recursive(...)` 里，当前“真观察”主要包括：

- `LOAD`
- `CALL`

而：

- `STORE_PREVIOUS`
- `PHI_INPUT`

则要更细分地继续追踪。

这就是为什么：

- 被后续 store 覆盖
- 只沿 phi 传递但最后没被 load/call 观察

的版本，仍然可以被认定成 dead。

### 14.7 跨层查询：`value` 是否等于某个 `slot@mem.N`

当前 analysis 还有两条非常值得单独记的跨层 API：

- `memory_ssa_value_matches_slot_version(...)`
- `memory_ssa_find_block_equivalent_value(...)`

它们在回答两个很像但不同的问题。

第一个：

`这个 ValueSsaValueRef，是否等于 expected_slot 在 expected_version_id 时刻的值？`

第二个：

`在这个 block 里，有没有某个现成 SSA value 等于 expected_slot 在 expected_version_id 时刻的值？`

这两条 API 对后面的 scalar replacement / reuse / materialization 都很重要。

而且当前实现已经会穿过：

- `mov`
- `load_*`
- value phi
- 对应的 memory phi

所以它们不是表面匹配，而是真正在做：

`value-flow 和 memory-state-flow 之间的等价关系查询`

---

## 15. 用测试例子理解 analysis 能证明什么

### 15.1 same-value phi 可以解析出常量

`tests/memory_ssa/memory_ssa_analysis_test.c` 里有一类 case：

- 两条分支都 store `7`
- join 处有 `mem.2 = phi [...]`
- 后面 load 读 `mem.2`

当前测试会验证：

- `memory_ssa_resolve_load_store_value(...)` 成功
- `has_value == 1`
- `resolved_immediate == 7`

也就是说：

`memory phi 不是天然不可解析，只要输入都一致，它就能塌成唯一值。`

### 15.2 不同值 phi 不会乱猜

另一类测试是：

- 一条分支 store `1`
- 另一条分支 store `2`

这时测试会验证：

- `memory_ssa_resolve_version_value(...)` 返回 `has_value == 0`

说明当前 analysis 是保守的：

- 相同输入会收敛
- 不同输入不会强行猜值

### 15.3 loop-invariant same-value 也能解析

测试里还有 loop header phi 的 case：

- entry store `42`
- backedge 上也保持 `42`
- header 形成 loop-carried memory phi

当前测试要求：

- 这个 header phi 也能解析成 `42`

这说明实现不仅支持 diamond join，还支持：

`loop-carried same-value memory phi`

### 15.4 indirect phi cycle 也能解开

测试 `INDIRECT-PHI-CYCLE` 就是在验：

- `phiA -> phiB -> phiA`

这类间接环上，如果非环来源其实一致，当前两遍解析策略仍然能把值解到：

- `42`

这正是上一节提到的 iterative wrapper 在发挥作用。

### 15.5 global call barrier 会阻止直接解析

global-call 的测试会验证：

- store 后接 unknown/declaration-only call
- call 对应 access role 是 `CALL`
- 后面的 load 不能直接还原成先前 store 的值

也就是说，当前 call barrier 不是打印出来好看，它真的会影响 resolution 和 dead-store query。

---

## 16. verifier 在保什么边界

`memory_ssa_verify_program(...)` 现在已经挺严格了，绝不只是“字段不空就算过”。

### 16.1 instruction kind 和 access_count 必须对齐

当前 verifier 明确要求：

- `load_*`
  - `access_count == 1`
- `store_*`
  - `access_count == 1`
- `mov` / `binary`
  - `access_count == 0`
- `call`
  - 可以有 0 个或多个 access，由实际受影响 global 决定

这条约束非常重要，因为它把：

- 哪些 instruction 是 memory instruction
- 哪些只是普通 value instruction

钉成了 contract。

### 16.2 access 上的 slot annotation 必须和原 instruction 一致

verifier 会检查：

- `access.slot_id` 追踪到的 slot
- 和原始 `load/store` 指令里的 slot ref

必须一致。

这能防止出现：

`指令看起来在访问 a.0，但 access annotation 却偷偷指向 g.0`

### 16.3 每个 `mem.N` 必须定义恰好一次

verifier 会统计：

- entry version
- phi result version
- instruction-defined version

最后要求每个 `mem.N`：

`exactly once`

这就是 memory-version 世界里的“单一定义”硬约束。

### 16.4 phi 输入必须覆盖 predecessor，且不能重复

对于每个 memory phi：

- predecessor coverage 要合法
- 同一个 predecessor 不能出现两次

所以 memory phi 不是“输入列表差不多就行”，而是严格对应 CFG predecessor 集合。

---

## 17. dump 现在为什么很好用

`memory_ssa_dump_program(...)` 当前最值得夸的点，就是它真的把教学里最关键的信息都打出来了。

你能直接看到：

- tracked slots
- entry version
- memory phi
- load 的 `@ mem.N`
- store 的 `@ mem.N`
- call 的 `[slot.K use=... def=...]`

这让 `memory_ssa` 很适合 lesson，因为它不是只给你内存结构体。

它直接把这层新增语义用文本表出来了。

最推荐重点记住的三个 dump 形状就是：

```text
slot.0 = local a.0 entry=mem.0
```

```text
mem.2 = phi slot.0 [bb.1: mem.0], [bb.2: mem.1]
```

```text
call touch() [slot.0 use=mem.1 def=mem.2]
```

看到这三类，你基本就能判断：

- entry seed 有没有
- join state 怎么合
- call 是不是 barrier，影响了谁

---

## 18. 和 `memory_ssa_pass` 的边界

这一点必须说清楚，不然读到一半很容易混层。

`memory_ssa` 自己负责的是：

- 建 artifact
- 验 artifact
- 查 artifact
- 给出 def/use / resolution / equivalence facts

它**不负责**：

- 真正去删 load/store
- 做 slot promotion rewrite
- 做 scalar replacement rewrite
- 做 memory-aware pipeline

这些都属于：

- `memory_ssa_pass`

所以当前最合适的阅读路径是：

1. 先学 `memory_ssa`
   - 看懂 `mem.N` 是怎么来的
2. 再学 `memory_ssa_pass`
   - 看懂哪些 pass 在消费这些 `mem.N` facts

---

## 19. 当前这层的边界是什么

### 19.1 当前已经有的

- tracked local/global slot
- entry seed for globals and parameter locals
- per-slot memory phi placement
- dominator-walk builder
- call global read/write summary
- explicit load/store/call memory accesses
- def/use analysis
- version-value resolution
- trivial dead-store query
- cross-layer value-to-slot-version equivalence queries

### 19.2 当前还没有故意去做的

- pointer / alias-aware memory SSA
- heap/object partition
- arbitrary memory token for the whole world
- backend register/memory lowering
- direct rewrite pipeline inside this module

所以它现在最准确的身份仍然不是：

`通用 memory optimizer`

而是：

`small but real slot-based memory-state layer`

---

## 20. 一句总记

如果你最后只想记一句话，那就是：

`value_ssa` 把“值是谁”说清楚，`memory_ssa` 把“slot 当前状态是谁”说清楚；后面的 memory-aware pass，正是靠这层显式的 `mem.N`、memory phi、call access、version analysis 才能真正做事。`
