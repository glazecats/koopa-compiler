# Machine IR Lesson（compiler_lab）

> 目标：把 `machine_ir` 这一层讲清楚。这里重点讲它为什么存在、它和 `value_ssa_alloc` / `value_ssa_machine` 的边界是什么、`value_ssa` 是怎么被翻译成 `machine_ir` 的，以及这层为什么是后面整条 `machine_*` 流水线的起点。

## 一句话定位

`machine_ir` 是 machine/backend 主线里第一个真正的程序表示层，它把前面 SSA/allocator/machine-planning 的结果落成了可继续往后消费的 machine-facing CFG IR。

## 常见误解

- 误解一：`machine_ir` 已经等于 instruction selection 结果。
  - 它仍然保持 generic machine-facing op，selected family 要到 `machine_select` 才出现。
- 误解二：`machine_ir` 只是 allocator report 的另一个视图。
  - 它已经是 program / function / block / instruction artifact，而不只是报表。

## 导学

当前后端主线已经不再停在：

`value_ssa -> allocator -> machine report`

而是继续推进成：

`value_ssa -> value_ssa_alloc -> value_ssa_machine -> machine_ir -> machine_select -> ...`

所以 `machine_ir` 的角色，不是“再来一个 report”，而是：

`第一个真正独立的 machine-facing IR`

这份讲义建议按下面顺序读：

1. 先理解为什么 `value_ssa_machine` 还不够，必须再落一层 `machine_ir`
2. 再看 `include/machine/ir.h`，建立 operand / phi / instruction / terminator 的数据模型
3. 再看 bridge 口径，理解 `value_ssa` 里的值是怎么被翻译成 `reg/spill/imm`
4. 最后对照 cleanup / verifier / tests，理解这层当前已经稳定到什么程度

学完你应该能：

1. 解释 `machine_ir` 和 `value_ssa_machine` 的职责边界
2. 解释为什么这层还保留 CFG、phi、slot，而不是直接跳去 instruction selection
3. 能看懂一个 `value_ssa -> machine_ir` 的最小翻译例子
4. 能说明这层当前“已经做了什么”和“刻意还没做什么”

---

## 前置阅读

最推荐先读：

1. `lesson/ssa/value_ssa_machine_lesson.md`
2. `lesson/ssa/value_ssa_alloc_lesson.md`

如果你没先明白：

- allocator 结果长什么样
- machine register vocabulary 是怎么建立起来的

那 `machine_ir` 里的 `reg/spill/imm` operand 很容易被看成凭空出现。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/lowering/machine_select_lesson.md`
2. `lesson/machine/lowering/machine_layout_lesson.md`

最常见的读法是：

- 先看 `machine_select`
  - 理解 generic machine-facing op 怎么继续被选成 selected op
- 再看 `machine_layout`
  - 理解 selected blocks 怎么继续走向线性布局

---

## 最近同步

最近 `machine_ir` 最值得同步进 lesson 的，不是“又多了一组 cleanup”，而是它现在更明确地服务于：

- selected-side compatibility chain
- preview bytes/object 诚实边界

当前最好再多记三点：

1. **它已经不是孤立的 machine-facing CFG IR**
   后面的 `machine_select -> machine_layout -> machine_emit -> machine_encode -> machine_bytes` 现在越来越把它当成一条真实 `RISC-V` preview 主链的上游入口。

2. **machine-facing consumer/report surface 比以前更强了**
   lesson 里现在最好把它理解成：
   - 不只是 IR 本体
   - 也是后面 selected/report/query surface 的稳定起点

3. **它的价值现在更偏“为后面的 honesty chain 立好 program/block/instruction ground truth”**

一句话压缩：

- 以前：`value_ssa_machine -> machine_ir`
- 现在：`value_ssa_machine -> machine_ir -> selected/report/preview bytes/object` 这条链已经更完整了

最近这层还要再补一个很具体、很容易在讲课时漏掉的点：

- `machine_ir` 现在已经正式有 `machine_ir_block_set_void_return(...)`

也就是说它不再假装：

- every return == `return immediate 0`

而是开始认真区分：

- value-carrying return
- bare return

这件事虽然看起来小，但它会直接影响后面：

- `machine_select` 是出 `ret` 还是 `reti`
- `machine_bytes` 最后 summary/dump 里是不是还保留 bare-return 事实

---

## 1. 为什么需要 `machine_ir`

在 `value_ssa` 里，我们关注的是：

- `ssa.N`
- phi
- value flow

在 `value_ssa_alloc` 里，我们已经能回答：

- 这个 SSA value 分到了哪个颜色
- 谁被 spill 了
- 谁仍然待在颜色集合里

在 `value_ssa_machine` 里，我们进一步能回答：

- 这个颜色在机器寄存器世界里意味着什么
- 哪个颜色对应哪个 machine register
- 哪些值只能从 spill slot 取

但这些层还不等于一个真正能继续往后消费的 IR。

它们更像：

- allocation result
- machine interpretation
- report / query surface

而不是：

- program / function / block / instruction artifact

所以 `machine_ir` 的意义就是把“机器放置结果”变成正式程序表示。

它做的核心边界变化是：

- value 不再写成 `ssa.N`
- 而是写成：
  - `reg.N`
  - `spill.N`
  - immediate

所以它是：

`分配结果显式化后的 CFG IR`

不是：

- final assembly
- instruction selection
- byte encoding

---

## 2. 为什么不直接跳去 instruction selection

这点很重要。

很多后端实现会想直接从：

`allocator result -> selected op`

但当前项目刻意没有这么做。

因为我们还需要一个稳定层，专门回答这些问题：

- 哪个 SSA value 最后住进了哪个 machine register？
- 哪个值已经被强制改成 spill-slot operand？
- 在这些放置决策都显式化之后，CFG / phi / call / branch 还长什么样？

也就是说，`machine_ir` 先回答：

`值已经住在哪里`

而 `machine_select` 才回答：

`这些 machine-facing generic op 最后选成哪类更低层 op`

所以这两层分开是刻意设计，不是代码拆分习惯问题。

---

## 3. 文件定位

- 接口：`include/machine/ir.h`
- 实现：`src/machine/lowering/machine_ir/`
- 测试：`tests/machine/lowering/machine_ir/`
- 规划文档：`docs/backend/MACHINE_IR_PLAN.md`

这层后面的直接消费者，现在已经是一整条 sibling 流水线：

- `src/machine/lowering/machine_select/`
- `src/machine/lowering/machine_layout/`
- `src/machine/lowering/machine_emit/`
- `src/machine/lowering/machine_encode/`
- `src/machine/object/machine_bytes/`
- `src/machine/object/machine_object/`
- `src/machine/object/machine_reloc/`
- `src/machine/object/machine_container/`
- `src/machine/object/machine_elf/`
- `src/machine/runtime/machine_image/`

所以当前最稳妥的理解是：

- `machine_ir` 是 machine-side pipeline 的起点
- 不是 machine-side pipeline 的终点

### 3.1 这次目录整理之后，`machine_ir` 所在位置的含义也更清楚了

现在它不再单独挂在旧的：

- `src/machine/lowering/machine_ir/`

而是被放进：

- `src/machine/lowering/machine_ir/`

这其实是在用目录结构把层次关系讲明白：

- `src/machine/lowering/`
  - 放 machine lowering 这条线
  - 也就是 `machine_ir -> machine_select -> machine_layout -> machine_emit -> machine_encode`
- `src/machine/object/`
  - 放 object / relocation / container / ELF 这条线
- `src/machine/runtime/`
  - 放 image / exec / load / runtime / step / decode / interp ... 这条线
- `src/machine/observe/`
  - 放 delta / trace / event / history / journal 这条线

所以这次路径变化不是“随便挪了挪文件夹”，而是在把：

`lowering`
`object`
`runtime`
`observe`

这几条 machine 侧子主线显式分层。

---

## 4. `src/machine/lowering/machine_ir/` 目录怎么读

这一层现在其实已经不只是一个 `machine_ir.c`。

当前目录里至少有这些分片：

- `machine_ir.c`
- `machine_ir_core.inc`
- `machine_ir_lower.inc`
- `machine_ir_verify.inc`
- `machine_ir_dump.inc`
- `machine_ir_query.inc`
- `machine_ir_report.inc`
- `machine_ir_phi_elim.inc`
- `machine_ir_cleanup.inc`
- `machine_ir_copy_cleanup.inc`
- `machine_ir_slot_cleanup.inc`
- `machine_ir_value_cleanup.inc`
- `machine_ir_call_effects.inc`
- `machine_ir_constraints.inc`

最简单的理解方式不是按文件名死记，而是按四组职责记：

### 4.0 为什么它现在被放进 `lowering/`

如果只看 lesson 旧版本，你可能还会觉得：

- `machine_ir`
- `machine_select`
- `machine_layout`
- `machine_emit`

只是“几个平铺模块”

但新目录结构其实在强调另一件事：

它们四个是一整段连续的：

`machine lowering pipeline`

也就是说，从 `machine_ir` 开始，后面这些层虽然职责不同，但都还在做：

- 把程序一步步 lower 到更接近最终机器表示

所以：

- `machine_ir` 放进 `src/machine/lowering/`

是很合理的，因为它就是这条 lowering 线的起点。

### 4.1 core / artifact 基础层

- `machine_ir_core.inc`
- `machine_ir_dump.inc`
- `machine_ir_verify.inc`
- `machine_ir_query.inc`
- `machine_ir_report.inc`

这几块负责的是：

- 数据结构生命周期
- dump / verifier
- 面向调用方的 query / report artifact

也就是说，它们在回答：

`machine_ir 这份 artifact 怎么被建、被看、被验`

### 4.2 bridge / lowering 层

- `machine_ir_lower.inc`

这块在回答：

`value_ssa + allocation + machine bank 怎么翻成 machine_ir`

也就是本讲义后面要讲的 operand projection 主线。

### 4.3 cleanup 层

- `machine_ir_phi_elim.inc`
- `machine_ir_cleanup.inc`
- `machine_ir_copy_cleanup.inc`
- `machine_ir_slot_cleanup.inc`
- `machine_ir_value_cleanup.inc`

这几块合起来才是当前 machine-ir “越来越像真实后端输入”的关键。

它们分别大致对应：

- phi elimination
- cleanup driver / fixed-point orchestration
- copy 清理
- slot 已知值清理
- machine value 清理

也就是说，当前 `machine_ir` 不只是“桥接之后放着不动”，而是已经有一条小而真实的 cleanup 主线。

### 4.4 约束 / effect 辅助层

- `machine_ir_call_effects.inc`
- `machine_ir_constraints.inc`

这两块更像 shared helper，它们在回答：

- call 会 kill / observe 什么
- 某些 cleanup / rewrite 受哪些机器侧约束限制

所以如果你后面看 cleanup 代码，不要只盯着 `machine_ir_cleanup.inc`，很多保守边界其实是靠这两块在支撑。

### 4.5 这次目录整理后，也更容易看出它和后继层是“兄弟关系”

现在你如果直接看目录，会看到：

```text
src/machine/lowering/
  machine_ir/
  machine_select/
  machine_layout/
  machine_emit/
  machine_encode/
```

这比旧结构更直观地表达了：

- `machine_ir`
  - 先把 placement 变成 IR
- `machine_select`
  - 再把 generic machine-op family 选成 selected family
- `machine_layout`
  - 再决定 block order / fallthrough
- `machine_emit`
  - 再给出 label-facing emitted artifact
- `machine_encode`
  - 再给出 offset-aware encoded artifact

也就是说，lesson 里现在最好把它们讲成：

`同一条 lowering sibling line`

而不是旧式的“若干独立 machine_* 模块”。

---

## 5. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_ir` 已经是独立模块，负责：

1. machine-facing program/function/block 表示
2. phi / instruction / terminator 的 machine-side 版本
3. register-bank / global / local / spill-slot 元数据
4. 从 `value_ssa + allocation + machine bank` 桥接到 `machine_ir`
5. verifier
6. dump
7. query / report artifact
8. 第一批 machine-ir cleanup / canonicalization

### 当前它不是什么

它现在还不是：

1. 指令选择层
2. block layout 层
3. label emit 层
4. 真正编码/字节层
5. object / relocation / ELF 层

所以最准确的理解是：

`machine_ir = machine placement 已显式化，但 operation family 还比较通用的 IR`

---

## 6. `include/machine/ir.h`：当前数据模型

### 5.1 `MachineIrOperand`

当前值操作数只有三类：

- immediate
- machine register
- spill slot

可以写成：

$$
Operand = Imm(c) \mid Reg(r) \mid Spill(s)
$$

这说明：

- `ssa.N` 在这层已经消失
- “值住在哪里”从隐式结果变成了显式 operand

### 5.2 `MachineIrSlotRef`

当前仍然保留显式 slot：

- local
- global

也就是：

$$
Slot = Local(i) \mid Global(g)
$$

所以这层仍然允许：

- `load_local`
- `store_local`
- `load_global`
- `store_global`

这说明它还没有进入“所有内存都彻底 lower 完”的阶段。

### 5.3 phi / instruction / terminator

当前 `machine_ir` 仍然保留：

- phi
- `mov`
- `binary`
- `call`
- `load_local`
- `store_local`
- `load_global`
- `store_global`
- `ret`
- `jmp`
- `br`

所以当前层刻意保留的是：

`lower-level operands + still-generic control/value families`

不是：

`最终机器指令`

### 5.4 容器层级

当前容器仍然是很标准的 block-based IR：

$$
Program = (RegisterBank, Globals, Functions)
$$

$$
Function = (Locals, Blocks, SpillSlotCount)
$$

$$
Block = (Phis, Instructions, Terminator)
$$

所以它保留了足够稳定的结构，让后面 `machine_select` / `machine_layout` 能继续按程序 artifact 去消费。

---

## 7. Bridge：`value_ssa` 是怎么翻译到 `machine_ir` 的

当前 bridge 的精神其实很简单：

1. CFG 形状尽量保留
2. phi 先保留
3. 指令种类先保留在 generic family
4. 只把 SSA value ref 改写成 `reg/spill/imm`

可以先把它记成一个小伪代码：

```text
for each function in ValueSsaProgram:
  create MachineIrFunction
  copy locals / globals / machine register bank metadata

  for each block in function:
    create MachineIrBasicBlock

    for each phi in block:
      translate each input value:
        immediate -> Imm(c)
        allocated register value -> Reg(r)
        spilled value -> Spill(s)
      append MachineIrPhi

    for each instruction in block:
      keep instruction family mostly the same
      rewrite each value operand through allocation view:
        ssa -> reg/spill
        immediate -> immediate
      append MachineIrInstruction

    rewrite terminator operands the same way
```

这段伪代码里最重要的不是“复制 block”，而是：

`每个 value use/def 都要经过 allocation view 翻译`

也就是说，bridge 的核心工作不是 CFG 变形，而是 operand 投影。

---

## 8. 一个最小直线例子：这里到底“改了什么”

假设在 `value_ssa` 视角里，我们有这样一段值流：

```text
bb.0:
  ssa.0 = load_local a.0
  ssa.1 = add ssa.0, 1
  store_global g.0, ssa.1
  ret ssa.1
```

在分配和 machine-view 投影之后，到了 `machine_ir`，最关键的变化不是 CFG 变了，而是：

- `ssa.N` 不见了
- 值住进了 `reg.N` / `spill.N`

如果 `ssa.0` 和 `ssa.1` 都分到了寄存器，一个概念上的 `machine_ir` 形状会更像：

```text
bb.0:
  reg.0 = load_local a.0
  reg.1 = add reg.0, 1
  store_global g.0, reg.1
  ret reg.1
```

如果其中一个值被分到了 spill slot，也可能变成：

```text
bb.0:
  reg.0 = load_local a.0
  spill.0 = add reg.0, 1
  store_global g.0, spill.0
  ret spill.0
```

所以这层“新增”的信息是：

- 值的机器放置结果显式化了

但它“还没有改”的是：

- 还没做 selected op family
- 还没做 layout
- 还没做 bytes

---

## 9. 一个最小 phi 例子：这里为什么还保留 phi

假设 `value_ssa` 里有一个 join：

```text
bb.0:
  br ssa.0, bb.1, bb.2
bb.1:
  jmp bb.3
bb.2:
  jmp bb.3
bb.3:
  ssa.4 = phi [bb.1: ssa.1], [bb.2: ssa.2]
  ret ssa.4
```

到了 `machine_ir`，当前层通常还是会保留这个 join 语义，只把值映射成机器操作数：

```text
bb.0:
  br reg.0, bb.1, bb.2
bb.1:
  jmp bb.3
bb.2:
  jmp bb.3
bb.3:
  reg.4 = phi [bb.1: reg.1], [bb.2: spill.0]
  ret reg.4
```

这说明当前 `machine_ir` 的重点不是“立刻把 phi 消灭掉”，而是：

`先把 machine placement 变成第一等信息`

phi elimination 是这层后续 cleanup 的职责，不是 bridge 一上来就必须完成的职责。

---

## 10. 一个最小 spill 例子：这里为什么值得单独落一层

看一个更能体现这层价值的例子。

如果我们只看 allocation report，可能只能说：

- `ssa.3` spilled to slot 0

但这还不够直观。

落到 `machine_ir` 之后，你就能直接看到：

```text
bb.2:
  spill.0 = call foo(reg.1)
  br spill.0, bb.3, bb.4
```

也就是说，“spill 了”不再只是报告信息，而是已经进入程序表示本身。

这正是 `machine_ir` 值得存在的原因：

`allocation decision 从结果表，变成了 operand-level program fact`

---

## 11. 当前 cleanup 线说明了什么

根据 `docs/backend/MACHINE_IR_PLAN.md`，这层现在已经不只有 representation / bridge。

它还在做第一批 machine-ir cleanup，尤其是：

- phi elimination
- parallel-copy emission
- cycle breaking
- critical-edge split
- self-verify cleanup boundary
- 简单 post-phi cleanup

这件事的 lesson 口径很重要：

它并不意味着 `machine_ir` 在抢 `machine_select` 的工作。

更准确地说：

- `machine_ir` cleanup 负责把 machine-ir 自己整理到更稳定、更容易被消费的形状
- `machine_select` 才负责把 generic machine-ir op 选成 selected op family

也就是说，当前 cleanup 线在回答：

`这份 machine-facing generic IR 如何先变得更规整`

而不是：

`最终选成哪条机器指令`

### 11.1 用伪代码看 phi elimination

如果只看现象，很容易以为“phi elimination 就是删掉 phi”。

但当前这层真正做的是更细的一件事：

```text
for each block B:
  for each phi p in B:
    for each predecessor P of B:
      materialize one predecessor-edge copy:
        p.result <- phi_input_from(P)

if one predecessor edge is critical:
  split that edge first

if multiple phi copies on one edge form a cycle:
  use one conservative scratch spill slot to break the cycle
```

所以当前 `machine_ir_phi_elim.inc` 的 lesson 口径不该讲成：

- “phi 被删了”

而应该讲成：

- “block-entry phi 语义被重写成 predecessor-edge copy 语义”

这也是为什么 current plan 会特别提：

- critical-edge split
- parallel-copy emission
- cycle breaking
- scratch-slot reuse

### 11.2 再用一个最小例子看 cleanup 在改什么

假设 `machine_ir` 在 phi elimination 之后出现了这种中间形状：

```text
bb.1:
  reg.2 = mov reg.2
  jmp bb.3
```

或者：

```text
bb.2:
  jmp bb.3
```

那么 cleanup 线后续会继续尝试：

- 删除 trivial self-copy
- 压缩空 jump-only block
- 在必要时继续删 unreachable block

所以当前 cleanup 不只是“做完 phi elimination 就结束”，而是：

`phi elimination -> copy cleanup -> empty/jump cleanup -> unreachable cleanup`

的一个小管线。

### 11.3 它确实有一些“优化样式”，但口径更像 canonicalization / cleanup

如果你问：

- “`machine_ir` 里是不是已经有一些优化代码？”

答案是：

- `有`

但更准确的叫法不是“大优化器”，而是：

- `machine-ir canonicalization / cleanup`

因为我刚刚对源码看下来，它现在至少已经真实在做这几类事情。

#### 1. branch simplification

`machine_ir_cleanup.inc` 里会做：

- then/else 指向同一块的 branch 改成 jump
- 条件是 immediate 的 branch 直接折成 jump

最小例子：

```text
bb.0:
  br 1, bb.1, bb.2
```

会直接塌成更接近：

```text
bb.0:
  jmp bb.1
```

所以这已经是很标准的 CFG simplify，不只是“整理打印格式”。

#### 2. identical-successor folding

它还会看 branch 两边是不是最终落到等价 successor。

如果：

- 两边 thread 之后是同一目标
- 或者两边 block 本体和 terminator 完全相同

那么 branch 也会继续被压缩。

lesson 口径上，这可以理解成：

`same-shape successor folding`

#### 3. local copy propagation + dead move cleanup

`machine_ir_copy_cleanup.inc` 这块其实很像一个 machine-side 的局部 copy cleanup。

它会：

- 在 block / CFG 范围内维护 register/spill 的 alias
- 重写 instruction operand
- 重写 terminator operand
- 删掉后面已经不需要的 `mov`

最小例子可以想成：

```text
bb.0:
  reg.1 = mov reg.0
  reg.2 = add reg.1, 1
  ret reg.2
```

清理后更接近：

```text
bb.0:
  reg.2 = add reg.0, 1
  ret reg.2
```

所以它不是只删 `mov x, x` 这种最 trivial 的东西，而是已经会做：

`copy alias rewrite + dead move cleanup`

#### 4. known-slot forwarding / slot-side cleanup

`machine_ir_slot_cleanup.inc` 这块会维护：

- known slot values
- observed slot states
- global call effect summary

也就是说，它不是“看到 slot 就不敢动”，而是会保守地做：

- 已知 slot value 的传播
- load wrapper / branch wrapper 的折叠
- 遇到 call 时按 global effect summary kill 对应 facts

一个 lesson 级最小例子可以理解成：

```text
bb.0:
  store_local a.0, reg.3
  reg.4 = load_local a.0
  ret reg.4
```

在已知值仍然成立时，后续就有机会被收成更接近：

```text
bb.0:
  store_local a.0, reg.3
  ret reg.3
```

所以这块不是 memory SSA 那么重的系统，但已经是：

`slot-aware cleanup`

而不是纯语法整理。

#### 5. machine-value simplification

`machine_ir_value_cleanup.inc` 里已经有一批很像 value canonicalization 的代码：

- immediate/immediate binary folding
- `x + 0 -> x`
- `x - 0 -> x`
- `x * 1 -> x`
- `x * 0 -> 0`
- `x / 1 -> x`
- `x % 1 -> 0`
- `x & -1 -> x`
- `x | 0 -> x`
- `x ^ 0 -> x`
- `x ^ x -> 0`

最小例子：

```text
reg.2 = xor reg.1, reg.1
```

会直接被改写成更接近：

```text
reg.2 = mov 0
```

这已经是很明确的：

`constant fold + algebraic identity cleanup`

#### 6. tail / wrapper folding

`machine_ir_cleanup.inc` 里还不只做 branch/jump 小改写，它还会折叠一些 wrapper tail：

- empty return tail
- thin return wrapper
- inlineable return wrapper
- call return wrapper
- thin jump wrapper
- branch wrapper

这部分的 lesson 口径可以先理解成：

`如果一个块只是为了再包一层 return/jump/branch 语义，而没有真正新增结构价值，就尽量把它吸回前驱`

#### 7. jump threading / empty-block compaction / unreachable removal / linear merge

cleanup driver 的后段还会继续做：

- jump threading
- empty jump block compaction
- unreachable block removal
- linear jump block merge

所以整个清理线不是单点规则，而是一个会循环到 fixed point 的小管线。

### 11.4 用伪代码看当前 cleanup driver 的味道

把源码里的主调用顺序压成 lesson 级伪代码，大致就是：

```text
eliminate_phi_nodes()
eliminate_trivial_moves()

repeat until fixed point:
  simplify_trivial_branches()
  cleanup_known_slot_values()
  cleanup_machine_values()

  for each function:
    cleanup_local_copies()
    fold_identical_branch_successors()
    fold_wrapper_tails()
    thread_jump_targets()
    compact_empty_jump_blocks()
    remove_unreachable_blocks()
    merge_linear_jump_blocks()

verify_program()
```

所以现在更准确的讲法应该是：

- `machine_ir` 里确实已经有一些优化代码

但它们当前的工程定位更像：

- `canonicalization + CFG/value/slot cleanup`

而不是：

- 完整 backend optimizer

这两个说法差很多，lesson 里最好明确区分开。

---

## 12. verifier 现在在锁什么

这一层的 verifier 当前最关键的是锁这些结构契约：

1. dense register/global/block ids
2. valid register references
3. valid spill-slot references
4. valid local/global slot references
5. per-block required terminators
6. phi predecessor validity and uniqueness
7. entry reachability

所以这层 verifier 的角色可以概括成：

`确保 machine placement 显式化以后，程序仍然是一份合法 CFG artifact`

它不是在做 target legality 校验，也不是在做 final encoding legality 校验。

---

## 13. 测试现在在锁什么

当前 `tests/machine/lowering/machine_ir/machine_ir_test.c` 这条线，至少在锁几类 authority：

1. manual construction / skeleton correctness
2. `value_ssa -> machine_ir` 的 operand projection
3. phi / instruction / terminator 位点上的 `reg/spill/imm` 翻译
4. malformed spill-slot usage 的 verifier 拒绝
5. machine-ir cleanup / phi-elimination family
6. critical-edge / scratch-slot / self-copy cleanup family

所以目前这层的测试口径，已经不只是“能不能 build 一份 machine_ir”，而是：

`bridge + verifier + cleanup` 三条线一起锁

这次目录整理之后，测试路径也应该按新位置来记：

- `tests/machine/lowering/machine_ir/machine_ir_test.c`

这和源码路径是对应的：

- `src/machine/lowering/machine_ir/`

所以如果你以后想顺着目录一起看源码和测试，最好成对去记：

```text
src/machine/lowering/machine_ir/
tests/machine/lowering/machine_ir/
```

---

## 14. 你现在应该怎么读这层

建议顺序：

1. 先看 `include/machine/ir.h`
   - 先建立 operand / phi / instruction / terminator 的整体印象
2. 再看 `docs/backend/MACHINE_IR_PLAN.md`
   - 理解它为什么不是 instruction selection
3. 再看 `src/machine/lowering/machine_ir/` 的文件分工
   - 先把 core / lower / cleanup / helper 四组职责分开
4. 再带着“operand projection”去看 bridge / dump / tests
   - 重点看 `ssa -> reg/spill/imm` 是怎么发生的
5. 最后再去看 `machine_select`
   - 你就会知道“为什么这里必须另起一层”

---

## 15. 一句话总结

`machine_ir` 是“分配结果已经落成 reg/spill/imm，但还没进入真实 selected op / layout / encoding”的第一层 machine-facing IR；它最重要的工作是把 allocation/machine-view 决策变成 operand-level program fact，而不是立刻做最终指令选择。
