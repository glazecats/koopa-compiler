# Value SSA Lesson（compiler_lab）

> 目标：把 `value_ssa` core 这一层讲清楚。这里重点讲表示、verifier、strict conversion、shared analysis，以及它和 `value_ssa_pass` / `value_ssa_interp` 的边界。

## 一句话定位

`value_ssa` core 是把 lower IR 的值流正式变成 SSA world 的那一层，它提供的是表示、verifier、conversion 和 shared analysis 地基。

## 常见误解

- 误解一：value SSA 已经把所有 SSA 相关逻辑都包了。
  - 现在 pass、interp、alloc 都已经拆成 sibling 模块，不再塞回 core。
- 误解二：SSA 只是给每个值改个编号。
  - 真正关键的是 def-use、phi、rename 和后续可分析的值流结构。

## 导学

现在这条线已经不适合再用“一份讲义全包”来理解了。更自然的拆法是：

1. `ssa/value_ssa_lesson.md`
   - 讲 core representation / verifier / conversion / shared analysis
2. `ssa/value_ssa_pass_lesson.md`
   - 讲 SSA-side cleanup / canonicalization / pass pipeline
3. `ssa/value_ssa_alloc_lesson.md`
   - 讲 allocator plan / coloring / spill / rewrite
4. `value_ssa_interp_lesson.md`
   - 讲 SSA 解释器、执行状态、oracle/test 用法

这份是第一份，也就是 **core lesson**。

---

## 前置阅读

最推荐先读：

1. `lesson/core/lower_ir_lesson.md`
2. `lesson/core/ir_lesson.md`

尤其是 `lower_ir_lesson.md`，因为现在这条线的真实主线就是：

`canonical IR -> lower IR -> value SSA`

如果你没先理解 lower IR 已经把哪些 value/storage 边界显式化，SSA 这里很多设计选择会显得像平地起高楼。

## 读完后接哪篇

最自然往下接：

1. `lesson/ssa/value_ssa_pass_lesson.md`
2. `lesson/ssa/value_ssa_alloc_lesson.md`
3. `lesson/ssa/value_ssa_interp_lesson.md`

如果你更关心：

- SSA 上还能做什么 cleanup / pass
  - 先接 `value_ssa_pass`
- SSA 值最后怎么进入 allocator
  - 先接 `value_ssa_alloc`

---

## 1. 这层现在是什么

当前主线已经明确成：

$$
\text{AST} \rightarrow \text{semantic} \rightarrow \text{canonical IR} \rightarrow \text{lower IR} \rightarrow \text{value SSA}
$$

其中 `value_ssa` core 现在负责的是：

1. SSA 数据模型
2. builder API
3. dump
4. verifier
5. strict `lower_ir -> value_ssa` conversion
6. shared analysis
7. rename / alpha-renaming groundwork

它**不直接负责**：

1. SSA-side optimization pass
2. canonicalization pipeline 编排
3. SSA 执行/解释

这些现在已经分别拆到：

- `src/value_ssa_pass/`
- `src/value_ssa_interp/`

所以最准确的理解是：

`value_ssa` 现在已经是一个稳定的 core layer，而不是把所有 SSA 相关逻辑都塞在一个文件夹里。

---

## 2. 文件定位

- 接口：`include/value_ssa.h`
- 聚合入口：`src/value_ssa/value_ssa.c`
- dump：`src/value_ssa/value_ssa_dump.inc`
- verifier：`src/value_ssa/value_ssa_verify.inc`
- shared analysis：`src/value_ssa/value_ssa_analysis.inc`
- allocation prep：`src/value_ssa/value_ssa_alloc_prep.inc`
- allocation worklist：`src/value_ssa/value_ssa_alloc_worklist.inc`
- strict conversion：`src/value_ssa/value_ssa_from_lower_ir.inc`
- rename：`src/value_ssa/value_ssa_rename.inc`

相关 sibling 模块：

- pass 接口：`include/value_ssa_pass.h`
- pass 实现：`src/value_ssa_pass/`
- interp 接口：`include/value_ssa_interp.h`
- interp 实现：`src/value_ssa_interp/`
- alloc 接口：`include/value_ssa_alloc.h`
- alloc 实现：`src/value_ssa_alloc/`
- memory 层：`include/memory_ssa.h` / `src/memory_ssa/`
- memory-backed pass：`include/memory_ssa_pass.h` / `src/memory_ssa_pass/`

相关测试：

- `tests/value_ssa/value_ssa_regression_test.c`
- `tests/value_ssa/value_ssa_verifier_test.c`
- `tests/value_ssa/value_ssa_analysis_test.c`
- `tests/value_ssa/value_ssa_alloc_test.c`

---

## 3. 数据模型

### 3.1 值与槽位

当前这层仍然保留 lower IR 的 value / slot split。

值引用：

- `VALUE_SSA_VALUE_IMMEDIATE`
- `VALUE_SSA_VALUE_ID`

槽位引用：

- `VALUE_SSA_SLOT_LOCAL`
- `VALUE_SSA_SLOT_GLOBAL`

也就是：

$$
Value = Imm(c) \mid SSA(v)
$$

$$
Slot = Local(i) \mid Global(g)
$$

所以：

- `mov/binary/call/load_*` 产出 SSA value
- `store_*` 仍然是 effect instruction
- `local/global` 仍然不是 SSA value

这点很重要，因为当前 `value_ssa` 是：

`value flow SSA`

不是：

`memory SSA`

### 3.2 phi

phi 现在是 block-entry 一等公民：

- `ValueSsaPhi`
- `ValueSsaPhiInput`

抽象形式：

$$
v_k = \phi[(bb_i: v_i), (bb_j: v_j), \dots]
$$

### 3.3 指令与 terminator

指令种类仍然很接近 lower IR：

- `MOV`
- `BINARY`
- `CALL`
- `ADDR_LOCAL`
- `ADDR_GLOBAL`
- `LOAD_LOCAL`
- `STORE_LOCAL`
- `LOAD_GLOBAL`
- `STORE_GLOBAL`
- `LOAD_INDIRECT`
- `STORE_INDIRECT`

terminator：

- `RETURN`
- `JUMP`
- `BRANCH`

所以这层仍然是：

`lower IR CFG + SSA value namespace + phi`

不是新的控制流模型。

如果按 `lv9` 这轮同步，最好再多加一句：

- `value_ssa` 现在也开始承接第一批 address / indirect-memory op
- 所以这层不再只覆盖“slot name 直接读写”的 lower-IR 子集

但它当前仍然不是：

- 完整 pointer IR
- 完整 memory SSA replacement

而是：

- “把 lower IR 里第一批 address-value / indirect-memory 结构继续保留下来”

### 3.3.1 最近同步：bare `ret` 现在会原样穿过 Value SSA

这轮前后端同步里，一个很容易漏讲的小点是：

- `value_ssa` 现在不会把上游 `void` return 偷偷改写成 `ret 0`

也就是说：

- `LOWER_IR_TERM_RETURN + has_return_value = 0`
  - 会继续变成
- `VALUE_SSA_TERM_RETURN + has_return_value = 0`

这个区分后来会继续传到：

- `memory_ssa`
- `machine_ir`
- `machine_select`
- `machine_bytes`

所以 lesson 口径上要把它看成：

- 不只是 verifier 小修
- 而是整条 pipeline 开始认真保留 return shape

### 3.3.2 当前支持 / 不支持什么

把 core representation 本身单独看，当前最稳妥的口径是：

**当前支持：**

- block-based CFG
- SSA value namespace
- `phi`
- `mov / binary / call / load_* / store_*`
- `ret / jmp / br`

**当前不支持：**

- memory SSA node
- address-taking / pointer-like value
- target/backend register object
- allocator state 直接内嵌进 IR

所以当前这层的准确身份仍然是：

`下游 value SSA IR`

不是：

`机器 IR`

### 3.4 一个最小直线例子

如果把下面这种 lower-IR 风格的形状：

```text
tmp.0 = load_local a.0
tmp.1 = add tmp.0, 1
store_global g.0, tmp.1
ret tmp.1
```

放进 `value_ssa`，最直接的对应 dump 会更像：

```text
func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    ssa.1 = add ssa.0, 1
    store_global g.0, ssa.1
    ret ssa.1
}
```

这里最值得注意的是两件事：

1. `tmp.N` 被替换成了 `ssa.N`
2. `store_global` 仍然保留成 effect instruction

所以这层不是“把所有操作都变成纯 SSA value”，而是：

`让 value flow SSA 化，同时保留 slot side-effect`

---

## 4. Builder 与 Verifier

### 4.1 Builder 的风格

builder 现在的策略很明确：

`尽早 fail-fast`

当前已经锁住的构造期约束包括：

- declaration-only function 不能 append block
- declaration-only function 不能 allocate SSA value
- parameter locals 必须保持连续前缀
- phi 必须先于普通 instruction
- terminator 之后不能再 append 指令
- terminator 不能被覆盖

所以它不是“先随便造，后面 verifier 再兜底”，而是：

`明显错误在构造期就拒绝`

### 4.3 一个最小 builder 例子

如果你手工造一个最小 straight-line function，当前 builder 风格会接近：

```c
ValueSsaProgram program;
ValueSsaFunction *function = NULL;
ValueSsaBasicBlock *block = NULL;
size_t local_a_id;
size_t value0;
size_t value1;
ValueSsaInstruction instruction;

value_ssa_program_init(&program);
value_ssa_program_append_function(&program, "main", 1, &function, &error);
value_ssa_function_append_local(function, "a", 1, &local_a_id, &error);
value_ssa_function_append_block(function, NULL, &block, &error);

value0 = value_ssa_function_allocate_value(function);
value1 = value_ssa_function_allocate_value(function);

memset(&instruction, 0, sizeof(instruction));
instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
instruction.has_result = 1;
instruction.result = value_ssa_value_id(value0);
instruction.as.load_slot = value_ssa_slot_local(local_a_id);
value_ssa_block_append_instruction(block, &instruction, &error);

memset(&instruction, 0, sizeof(instruction));
instruction.kind = VALUE_SSA_INSTR_BINARY;
instruction.has_result = 1;
instruction.result = value_ssa_value_id(value1);
instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
instruction.as.binary.lhs = value_ssa_value_id(value0);
instruction.as.binary.rhs = value_ssa_value_immediate(1);
value_ssa_block_append_instruction(block, &instruction, &error);

value_ssa_block_set_return(block, value_ssa_value_id(value1), &error);
```

这段代码有个很鲜明的 lesson 风格：

- `load_local` / `binary` 自己带结果值
- `ret` 走 terminator API
- block shape 由 builder 直接维护

### 4.2 Verifier 的边界

verifier 当前会检查：

1. 表结构合法
2. phi 输入合法
3. SSA 定义唯一
4. use 在使用点可用
5. block 可达
6. `load_* / store_*` 的 slot/value 边界
7. `call` / `ret` / `br` 等 terminator/instruction 形状

也就是说，它已经不是只看“有没有这个字段”，而是会继续看：

`这个 SSA value 在这里用的时候，值流上到底可不可用`

### 4.2.1 verifier 的直觉伪代码

如果把 verifier 压成 lesson 级伪代码，最值得记的是下面这层顺序：

```text
verify_function(function):
    check table counts / capacities / block structure
    check every block is reachable from entry

    for each phi:
        check predecessor count matches CFG
        check every phi input refers to a real predecessor
        check phi result id is unique

    walk blocks in dominance-aware availability order:
        check every instruction/terminator use is available here
        check every result id is defined once
        check load/store/call/branch operand kinds are legal
```

这里最重要的 lesson 点是：

- verifier 不只是“结构检查器”
- 它已经带了第一层 value-availability 语义检查

所以如果一个 `ssa.7` 在它真正可达/可用之前就被拿来 `ret` 或 `br`，当前 verifier 是会拒绝的。

---

## 5. Strict Conversion

### 5.1 当前入口

当前真实 conversion 入口是：

```c
int value_ssa_build_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
```

### 5.2 当前主线

这条 conversion 现在已经不是旧的过渡路径，而是：

`lower_ir -> analysis -> conversion -> value_ssa`

其中：

- `lower_ir` 输入层先提供 CFG / dominator / phi-candidate / successor-phi-use 事实
- `value_ssa` conversion 再消费这些输入事实

当前真正的 construction 形状是：

1. verify lower IR
2. 复制 globals / functions / locals / blocks 骨架
3. 计算 lower-IR-side analysis
4. 预创建 pruned candidate phi
5. 按 dominator tree walk 转换 block
6. block entry 绑定 phi result
7. block 内 use/def 只通过 rename state
8. predecessor leave 直接填 successor phi incoming
9. finalize 只做 completeness check
10. 最后 alpha-rename 成 dense/stable dump
11. verify 输出 SSA

### 5.2.1 strict conversion 的伪代码

如果把当前 conversion 主线写成更像实现的伪代码，可以记成：

```text
build_from_lower_ir(program):
    verify lower_ir input
    clone globals / functions / locals / block shells

    for each function:
        cfg = compute lower-ir cfg / dominator / phi facts
        pre-create candidate phis for temps that need join values
        init rename state

        dominator_walk(entry):
            on enter block:
                bind block-entry phis into current rename state
                rewrite and emit block-local instructions using current bindings
                create fresh SSA ids for new defs

            on leave block:
                for each successor:
                    fill successor phi input from current rename state

        finalize phi completeness
        alpha-rename function to dense stable ids

    verify final value_ssa output
```

这段伪代码里最该抓住的，是当前 strict conversion 的三根主骨架：

1. **phi 先预创建**
2. **use/def 全靠 rename state 走**
3. **predecessor leave 时直接填 phi incoming**

所以它不是“先转一遍，再回头到处修补”，而是：

`带着 dominator/rename 骨架，一次构造出 verifier-legal SSA`

### 5.3 当前支持边界

当前 representative authority 已经包括：

- straight-line CFG
- diamond
- simple slot-carried loop
- representative loop-header temp phi
- multi-backedge loop-carried temp
- multi-carried-temp loop
- 一批 representative nested-loop carried-temp family

但它仍然不是：

`任意 cyclic CFG 上的 full SSA construction`

所以当前准确口径是：

- strict conversion 已经 landed
- conversion 自身现在 maintenance-first
- 但它不是“所有 CFG 家族都完全通用”

### 5.4 一个 diamond conversion 例子

当前一条很典型的 representative 形状，是 branch join：

lower IR 直觉上像：

```text
bb.0:
  tmp.0 = load_local a.0
  br tmp.0, bb.1, bb.2
bb.1:
  tmp.1 = mov 1
  jmp bb.3
bb.2:
  tmp.1 = mov 2
  jmp bb.3
bb.3:
  ret tmp.1
```

转换后的 SSA dump 当前会稳定成：

```text
func main(a.0) {
  bb.0:
    ssa.0 = load_local a.0
    br ssa.0, bb.1, bb.2
  bb.1:
    ssa.1 = mov 1
    jmp bb.3
  bb.2:
    ssa.2 = mov 2
    jmp bb.3
  bb.3:
    ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]
    ret ssa.3
}
```

这就是 strict conversion 最核心的“join-temp 正规化”：

- lower IR 里允许的 join-temp 形状
- 在 `value_ssa` 里被正规化成显式 `phi`

### 5.5 一个 loop-header phi 例子

再看一个更关键的 loop-carried temp 例子：

```text
bb.0:
  tmp.0 = mov 1
  jmp bb.1
bb.1:
  br tmp.0, bb.2, bb.3
bb.2:
  tmp.0 = mov 2
  jmp bb.1
bb.3:
  ret tmp.0
```

当前 authority 下会转换成：

```text
func main() {
  bb.0:
    ssa.0 = mov 1
    jmp bb.1
  bb.1:
    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]
    br ssa.1, bb.2, bb.3
  bb.2:
    ssa.2 = mov 2
    jmp bb.1
  bb.3:
    ret ssa.1
}
```

这个例子正好说明：

- phi 是在 DFS 前预创建的
- 进入 header 时先绑定 phi result
- backedge 离开时再填 incoming

### 5.5.1 multi-backedge loop 的 authority dump

当前 strict conversion 已经锁到的不只是“单回边 loop”，还包括 multi-backedge loop。

当前一条代表 authority dump 是：

```text
func main() {
  bb.0:
    ssa.0 = mov 1
    jmp bb.1
  bb.1:
    ssa.1 = phi [bb.0: ssa.0], [bb.3: ssa.2], [bb.4: ssa.3]
    br ssa.1, bb.2, bb.5
  bb.2:
    br ssa.1, bb.3, bb.4
  bb.3:
    ssa.2 = mov 2
    jmp bb.1
  bb.4:
    ssa.3 = mov 3
    jmp bb.1
  bb.5:
    ret ssa.1
}
```

这条 dump 现在锁住的是：

- 一个 header phi 可以同时接 entry seed
- 也可以接多个 backedge
- strict conversion 不需要回退到旧 backfill 主路径

### 5.5.2 nested-loop carried-temp 的 authority dump

再往前一步，当前 representative nested-loop family 也已经被 dump 锁住。

其中一条很适合讲课的 authority 形状是：

```text
func main() {
  bb.0:
    ssa.0 = mov 1
    jmp bb.1
  bb.1:
    ssa.1 = phi [bb.0: ssa.0], [bb.5: ssa.5]
    br ssa.1, bb.2, bb.6
  bb.2:
    ssa.2 = mov 10
    jmp bb.3
  bb.3:
    ssa.3 = phi [bb.2: ssa.2], [bb.4: ssa.4]
    br ssa.3, bb.4, bb.5
  bb.4:
    ssa.4 = mov 11
    jmp bb.3
  bb.5:
    ssa.5 = mov 2
    jmp bb.1
  bb.6:
    ret ssa.1
}
```

这条 dump 很适合说明两件事：

1. 外层 loop-carried temp 和内层 loop-carried temp 可以各自 materialize 自己的 phi
2. nested-loop 当前已经不是设计草图，而是有 exact dump authority 的真实支持面

### 5.5.3 conversion 现在和 memory-aware one-shot 的关系

虽然严格来说 memory-aware bridge API 落在 `value_ssa_pass` / `memory_ssa_pass`，但 lesson 里这里顺手记一句很有用：

- raw core conversion 入口仍然是 `value_ssa_build_from_lower_ir(...)`
- 更高一层的 one-shot canonicalized bridge 则已经分成：
  - classic
  - memory-value
  - memory-full

也就是说，当前 `value_ssa` core 的职责仍然是：

`把 lower_ir 严格转成 verifier-legal SSA`

而不是自己同时承担所有后续 canonicalization policy。

### 5.5.1 当前 conversion 支持 / 不支持什么

把 strict conversion 本身列成对照表，会更好记：

**当前支持的 representative family：**

- straight-line CFG
- diamond join
- simple slot-carried loop
- representative loop-header temp phi
- multi-backedge carried-temp loop
- multi-carried-temp loop
- representative nested-loop carried-temp family

**当前不要误讲成已支持的东西：**

- 任意 cyclic CFG family
- 完整 mem2reg
- memory SSA construction
- backend/regalloc-aware conversion

---

## 6. Shared Analysis

### 6.1 这层现在不只 def/use

`src/value_ssa/value_ssa_analysis.inc` 现在已经长成一条完整 shared analysis 主线。

它当前至少包括：

1. CFG analysis
2. phi placement
3. dominator preorder / walk
4. def/use + use-site list
5. block-level liveness
6. interference graph
7. copy-affinity graph
8. allocation-prep summary
9. allocation-worklist surface

也就是：

`def-use + liveness -> interference -> copy affinity -> allocation prep -> allocation worklist`

### 6.2 它的定位

这层现在的定位是：

1. 给已存在的 SSA 程序提供共享 analysis
2. 给 pass / tests / future allocator-prep 提供统一事实来源
3. 避免每个 consumer 都手搓一遍局部扫描

它不是：

- 旧 conversion 的 correctness oracle
- 正式 register allocator

### 6.2.1 这条 analysis 链到底怎么推

现在最适合记的不是某个具体数组名，而是分析依赖顺序：

1. 先有 CFG facts
2. 再有 def/use
3. 再从 def/use 推 liveness
4. 再从 liveness 建 interference
5. 再从 non-interfering `mov` 建 copy affinity
6. 再把这些 facts 收成 allocation-prep summary
7. 最后整理成 allocation worklist

也就是：

$$
\text{CFG}
\rightarrow
\text{def/use}
\rightarrow
\text{liveness}
\rightarrow
\text{interference}
\rightarrow
\text{copy affinity}
\rightarrow
\text{allocation prep}
\rightarrow
\text{allocation worklist}
$$

这样看会更清楚：

- interference 不是凭空来的
- worklist 也不是“又多了一个表”，而是 summary 之上的组织层

### 6.2.2 用伪代码看这条分析链

把这条链再压成一版伪代码，会更容易把各层关系记牢：

```text
compute_shared_analysis(function):
    cfg = compute predecessors / reachability / dominators
    def_use = record every SSA def and every use site
    liveness = solve backward live-in/live-out on blocks
    interference = for each def, connect it with values live at that point
    copy_affinity = for each mov x <- y:
        if x and y do not interfere:
            add affinity weight between them
    allocation_prep = summarize counts / live blocks / degrees / affinity
    allocation_worklist = bucket + sort summarized values
```

这段伪代码有一个教学上的好处：

- `copy_affinity` 的输入不是“所有值对”
- 而是已经经过 interference 过滤过的 `mov` 关系

所以它天然更像：

`coalescing hint`

而不是“又一张冲突图”。

### 6.3 `allocation_prep`

`value_ssa_compute_allocation_prep(...)` 现在会整理这些事实：

- `def_block_ids`
- `use_counts`
- `live_block_counts`
- `single_block_live_ranges`
- `live_block_offsets`
- `live_blocks`
- `interference_degrees`
- `affinity_sums`
- `move_related`
- `sorted candidates`

所以它是 allocator-prep 的**厚 summary**。

### 6.4 `allocation_worklist`

`value_ssa_compute_allocation_worklist(...)` 再往上做：

- classify
- prioritize
- sort

也就是把 allocator-prep facts 组织成真正可消费的 worklist。

当前五类是：

1. `move-hint`
2. `constrained`
3. `single-block`
4. `global`
5. `isolated`

这里最容易误会的是 `constrained`。

当前实现里：

```c
interference_degrees[value] >= 2
```

就会进 `constrained`。

这要理解成：

`当前 worklist 的 heuristic bucket boundary`

而不是：

`经典 regalloc 里和寄存器数 K 绑定的 high-degree / heavy 节点判断`

也就是说：

- degree >= 2 只是说明“这个值已经不是几乎无约束的边缘 case 了”
- 不是说它已经是正式 allocator 里的高压 spill 候选

当前 priority 也是确定性的 heuristic：

- `affinity_sums * 8`
- `interference_degrees * 4`
- `live_block_counts * 2`
- `use_counts`

再按 `value_id` 打平 tie。

所以它现在更像：

- allocation prep
- coalescing prep
- live-range experiment 入口

不是完整 allocator。

### 6.5 一个 allocation-prep 例子

考虑最简单的 copy-affinity case：

```text
ssa.0 = mov 7
ssa.1 = mov ssa.0
ret ssa.1
```

在当前 allocator-prep 视角下，这类值往往会得到：

- `interference_degrees[ssa.0] == 0`
- `interference_degrees[ssa.1] == 0`
- `affinity_sums[ssa.0] == 1`
- `affinity_sums[ssa.1] == 1`
- `move_related[ssa.0] == 1`
- `move_related[ssa.1] == 1`

所以 worklist 会先把它们归到：

- `move-hint`

这正好体现出这层的目标：

`先把 copy-friendly 的值提出来，而不是直接开始真实 coalescing`

### 6.5.1 一个 two-block range summary 例子

除了 copy case，另一个很适合讲 `allocation_prep` 的 authority 是“两块 live range”。

这类值当前往往会总结成：

- `def_block_ids[value] == 0`
- `use_counts[value] == 1`
- `live_block_counts[value] == 2`
- `single_block_live_ranges[value] == 0`
- `interference_degrees[value] == 0`
- `affinity_sums[value] == 0`

也就是说：

- 它不再是单块局部值
- 但也还没有 move affinity
- 也还不是高冲突 value

这正是 `allocation_prep` 要把“值的形状”从原始 SSA 程序里抽出来的意义。

### 6.6 一个 `constrained` 边界例子

再看 loop 或 branch 合流下的 value，通常会出现：

- live range 跨多个 block
- interference degree 不再接近 0

当前如果某个值满足：

```c
interference_degrees[value] >= 2
```

它就会被归到 `constrained` bucket。

这句最容易被误解成“degree=2 已经很重”，但 lesson 里应该这样记：

- 这是当前 worklist 的**粗分桶**
- 不是正式 allocator 的 `degree >= K`
- 只是说明“已经值得比 isolated / 单块局部值更早关注”

### 6.7 当前 analysis 层支持 / 不支持什么

如果把 shared analysis 单独列个表，当前更准确的是：

**已经支持：**

- verifier-legal SSA CFG analysis
- def/use + use-site list
- block-level liveness
- conservative interference graph
- non-interfering copy-affinity graph
- allocation-prep summary
- stable allocation-worklist ordering

**还没有：**

- spill cost model
- live-range splitting
- `simplify/freeze/spill/select-stack` 这类 allocator worklist
- 与真实寄存器数 `K` 绑定的正式 degree policy
- coalescing legality判断本体

### 6.8 这条 allocator-prep 线现在也有直接可用的工具面

最近这条线又往前走了一小步，但不是再补一个新分析，而是把已有 allocator-prep/worklist 做得更**可观察、可查询、可调试**了。

新增的这批 surface 可以分成两类。

第一类是 **dump/debug surface**：

- `value_ssa_dump_allocation_prep(...)`
- `value_ssa_dump_allocation_worklist(...)`
- `value_ssa_dump_allocation_worklist_for_class(...)`
- `value_ssa_allocation_work_class_name(...)`

它们的意义很直接：

- 不再只是“内部有一堆数组”
- 而是可以把当前 allocator-prep summary / worklist 稳定地转成文本
- 某个 work class 也可以单独拉出来看

这让这一层从“内部分析副产品”开始变成：

`可直接观察的调试/展示工具面`

第二类是 **query/helper surface**：

- `value_ssa_allocation_prep_get_live_blocks(...)`
- `value_ssa_allocation_prep_find_affinity_candidate(...)`
- `value_ssa_allocation_prep_get_affinity_weight(...)`
- `value_ssa_allocation_prep_get_top_affinity_neighbors(...)`
- `value_ssa_allocation_worklist_find_value(...)`
- `value_ssa_allocation_worklist_get_class_range(...)`

这类 helper 的价值在于，后续调用方不需要再手写一遍“怎么解 raw array”：

- 想问某个 value live 在哪些 block
- 想问它和谁 affinity 最强
- 想问它在 worklist 里排第几
- 想问某个 class 在整个 worklist 里覆盖哪一段

现在都已经有直接 API 了。

所以这波变化最适合记成一句：

`allocator-prep 已经不只是能算出来，还开始能被别的实验代码和调试工具直接消费`

### 6.10 interference / copy-affinity 最小伪代码

如果你前面问过 “interference graph 是不是通过 def/use 做出来的”，lesson 里最适合落成下面这版直觉：

```text
for each block from end to beginning:
    live = live_out(block)

    for each instruction in reverse:
        kill values defined here from live
        for each value defined here:
            add interference edges(def, every value currently in live)
        add values used here into live
```

这也是为什么你会觉得“是不是 def 的时候看 live 就行”。

当前这条线确实可以这么理解：

- `def/use` 提供基本事实
- liveness 算出“某点之后哪些值还活着”
- interference 则在 def 点把“新定义值”和当时 live 的值连边

然后 copy-affinity 再在这上面多加一层：

```text
for each mov dst <- src:
    if dst does not interfere with src:
        affinity[dst, src] += weight
```

所以：

- interference 说“不能放一起”
- copy affinity 说“如果不冲突，最好尽量靠一起”

### 6.11 allocator 这一层现在已经是独立 sibling 了

前面 shared analysis 讲的是：

`怎么为未来 allocator 准备事实`

而最近这条线已经不再只停在 prep/worklist 了，`value_ssa` 旁边现在已经有一个真正的 sibling allocator 模块：

- `include/value_ssa_alloc.h`
- `src/value_ssa_alloc/`

但 lesson 边界上，它不该继续塞在 core 这一份里讲细了。更合适的方式是：

- 这里知道它已经存在
- 知道 shared analysis 已经成为 allocator 的输入
- 具体 allocator 语义、策略、spill/rewrite，移到独立 lesson

一句话说：

`allocation_prep / allocation_worklist` 现在已经不是孤立调试层，而是真的被 allocator 消费了。

### 6.9 一个“工具面”视角的最小例子

以前如果后续 experiment 想回答：

1. `ssa.3` live 在哪些 block？
2. `ssa.3` 最像和谁 coalesce？
3. `ssa.3` 在 worklist 里属于哪一类？

调用方往往得自己去拆：

- `live_block_offsets`
- `live_blocks`
- `candidates`
- `items`

现在更自然的路径已经变成：

1. 先算 `allocation_prep`
2. 再算 `allocation_worklist`
3. 然后直接调用：
   - `value_ssa_allocation_prep_get_live_blocks(...)`
   - `value_ssa_allocation_prep_get_top_affinity_neighbors(...)`
   - `value_ssa_allocation_worklist_find_value(...)`
   - `value_ssa_allocation_work_class_name(...)`

也就是说，这层已经开始有点像：

`shared analysis + allocator-prep toolbox`

而不只是“给当前一个测试用的临时结构”。

---

## 7. Rename 与 Alpha-Renaming

当前 core 还提供：

- dominator-tree walk scaffold
- rename-state scope stack
- block-local use rewrite helper
- predecessor-specific phi-input rewrite helper
- function-level alpha-renaming

其中 `value_ssa_rename_function_values(...)` 的定位是：

`对已有 SSA 做 verifier-safe 的 alpha-renaming`

它会把乱掉的 `ssa.N` 重新收成：

- dense
- stable
- dominator-order friendly

这也是为什么它现在是：

- canonicalization 的最后一环
- DCE / cleanup 之后 re-densify 的工具

一个最直观的 before/after 是：

```text
bb.0:
  ssa.7 = load_local a.0
  br ssa.7, bb.1, bb.2
bb.1:
  ssa.12 = mov 1
  jmp bb.3
bb.2:
  ssa.4 = mov 2
  jmp bb.3
bb.3:
  ssa.20 = phi [bb.1: ssa.12], [bb.2: ssa.4]
  ret ssa.20
```

alpha-renaming 之后会更像：

```text
bb.0:
  ssa.0 = load_local a.0
  br ssa.0, bb.1, bb.2
bb.1:
  ssa.1 = mov 1
  jmp bb.3
bb.2:
  ssa.2 = mov 2
  jmp bb.3
bb.3:
  ssa.3 = phi [bb.1: ssa.1], [bb.2: ssa.2]
  ret ssa.3
```

这里变的不是语义，而是：

- dense id
- dominator-order-friendly 命名
- 更稳定的对照输出

### 7.1 alpha-renaming 的伪代码

这层也很适合补一段伪代码，因为它能把“rename”和“重建 SSA”区别开：

```text
alpha_rename(function):
    init empty mapping old_id -> new_id
    next_id = 0

    dominator_walk(entry):
        on enter block:
            assign fresh ids to block phis
            rewrite phi results in mapping
            rewrite block-local uses through current mapping
            assign fresh ids to block-local defs

        on edge to successor:
            rewrite successor phi inputs with current mapping

        on leave block:
            pop mappings introduced in this scope
```

这说明 alpha-renaming 当前做的不是：

- 再算一次 phi placement
- 再建一次 SSA

而是：

`在已有 verifier-legal SSA 上，把名字重新收稳`

这也是为什么它特别适合放在：

- DCE 之后
- CFG simplify 之后
- dump/export 之前

---

## 8. 这份课接下来去哪读

看完 core 以后，建议继续：

1. [value_ssa_pass_lesson.md](/workspaces/compiler_lab/lesson/ssa/value_ssa_pass_lesson.md)
   - 看 pass layer、canonicalization、SSA-side cleanup
2. [value_ssa_alloc_lesson.md](/workspaces/compiler_lab/lesson/ssa/value_ssa_alloc_lesson.md)
   - 看 allocator plan、coloring、spill、rewrite
3. [value_ssa_interp_lesson.md](/workspaces/compiler_lab/lesson/ssa/value_ssa_interp_lesson.md)
   - 看解释器、执行状态、oracle/test 用法
4. [tests_lesson.md](/workspaces/compiler_lab/lesson/core/tests_lesson.md)
   - 对照测试目标和 authority

---

## 9. 测试

这份 core lesson 最相关的测试是三类：

### `tests/value_ssa/value_ssa_regression_test.c`

主要锁：

- exact dump authority
- strict conversion representative family
- rename-related authority

### `tests/value_ssa/value_ssa_verifier_test.c`

主要锁：

- verifier 对 malformed SSA 的拒绝路径
- availability / duplicate def / unreachable block 等约束

### `tests/value_ssa/value_ssa_analysis_test.c`

主要锁：

- liveness
- interference
- copy affinity
- allocation prep
- allocation worklist

尤其是：

- copy case 的 `move-hint` ordering
- two-block range summary
- loop case 下 constrained values 排前
- worklist 分类和 priority 边界

另外最近这一组测试也已经不只是“底层数组对不对”，而是把分析工具面一起锁进 authority 了，包括：

- allocation-prep dump
- allocation-worklist dump
- class-specific worklist dump
- live-block query
- affinity-weight query
- top-neighbor query
- worklist item/class-range lookup

这点很值得写进 lesson，因为它说明 `value_ssa` analysis 现在已经有一层真正给后续实验代码复用的“公共表面”。

---

## 10. 一句话总结

当前 `value_ssa` core 真正已经完成的，是：

$$
\text{same CFG as lower IR} + \text{SSA value ids} + \phi + \text{strict verifier} + \text{strict conversion} + \text{shared analysis}
$$

而 pass 和 interpreter 现在已经不再糊在一起，而是分别去了各自的 sibling module。

还有一点这次也该记住：

当前 `value_ssa` 面向 lower-ir 的 one-shot bridge 已经不再只有 classic canonicalize 这一档了。

现在实际有三档：

1. classic canonicalized Value-SSA
2. memory-value canonicalized Value-SSA
3. full memory-aware canonicalized Value-SSA

也就是说，`value_ssa` 现在不只是通往自己的 classic cleanup 层，也已经开始正式对接 `memory_ssa_pass` 这条 sibling bridge。
