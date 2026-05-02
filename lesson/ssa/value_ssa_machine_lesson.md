# Value SSA Machine Lesson（compiler_lab）

> 目标：解释 `value_ssa_machine` 为什么会从 `value_ssa_alloc` 旁边单独拆出来，它当前到底把 allocator 的哪些结果翻成了 machine-facing 语义，为什么它不等于 `machine_ir`，以及这层代码里已经真实存在的 register bank / machine view / call-clobber risk / protection / planning report 都在做什么。

## 一句话定位

`value_ssa_machine` 是 SSA allocator 结果正式进入“机器寄存器词汇表”的桥接层，但它自己还不是 machine backend IR。

## 常见误解

- 误解一：`value_ssa_machine` 就是早期版 `machine_ir`。
  - 更准确地说，它更像 machine planning / machine vocabulary bridge，而不是 program/block/instruction IR。
- 误解二：allocator 结果已经够 backend 用了，不需要这层。
  - 没有这层，后面 consumer 还得自己重复解释 color / spill 对机器寄存器到底意味着什么。

## 导学

如果说：

- `value_ssa_alloc` 回答的是“某个 SSA value 分到了 color 几、spill slot 几”

那么：

- `value_ssa_machine` 回答的是“这些 abstract color 在机器寄存器世界里到底意味着什么”

所以这层不是 instruction selection，也不是 machine IR。

它更准确的身份是：

`allocator result -> machine register vocabulary / machine planning artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 allocator 之后还要单独有 `value_ssa_machine`
2. 再看 `include/value_ssa_machine.h`，建立 register bank / machine allocation view / planning report 的整体印象
3. 再看 `src/value_ssa_machine/` 的文件分工，理解它当前不只是一个 `color -> r0` 小映射
4. 最后带着测试例子去看：
   - machine view 到底新增了什么
   - call-clobber risk / protection / hotspot / preservation hint 在表达什么

学完你应该能：

1. 解释 `value_ssa_machine` 和 `value_ssa_alloc` / `machine_ir` 的边界
2. 说明这层真正新增的是“机器寄存器语义”和“机器规划分析”，不是后端 IR
3. 看懂一个 `allocation layout -> machine view` 的 before/after 例子
4. 说明为什么这层会先于 `machine_ir`、但又不能被 `machine_ir` 直接替代

---

## 前置阅读

最推荐先读：

1. `lesson/ssa/value_ssa_alloc_lesson.md`
2. `lesson/ssa/value_ssa_lesson.md`

如果你还没先知道：

- allocator 现在产出了哪些 abstract result
- color / spill / allocation layout 到底在说什么

那 `value_ssa_machine` 里这些 machine-facing 翻译和 planning artifact 会很难落地。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/lowering/machine_ir_lesson.md`
2. `lesson/machine/lowering/machine_select_lesson.md`

因为这层正好就是：

`SSA / allocator 世界`
到
`machine backend 世界`

之间最重要的桥。

---

## 1. 为什么需要 `value_ssa_machine`

allocator 当前已经能产出很多结果：

- `has_color`
- `color`
- `spill_intended`
- `spill_confirmed`
- `spill_recovered`
- `spill_slot`

但这些信息仍然有一个明显问题：

`它们还是 abstract allocator vocabulary`

例如：

- “color 0”
- “color 1”
- “color 2”

对 allocator 自己来说够用了，但对后面的 machine-facing consumer 来说还不够。

后面的消费者真正想问的是：

- color 0 现在对应哪台机器寄存器？
- 这个寄存器是 caller-clobbered 还是 callee-preserved？
- 当前函数实际用了哪些 machine registers？
- 哪些值被压在同一个机器寄存器上？
- 哪些值跨 call 活着，所以如果落在 caller-clobbered register 上会比较危险？

所以 `value_ssa_machine` 引入的真正边界是：

`abstract allocator result -> explicit machine register world`

它不是想替 allocator 重新做分配，而是想把 allocator 结果变成：

- 更明确的 machine-facing artifact
- 更适合后面 machine 侧规划和分析的消费面

---

## 2. 为什么不能把这层继续塞进 `value_ssa_alloc`

`value_ssa_alloc` 负责的是：

- interference / affinity / plan / select / color / spill
- spill rewrite
- allocation layout / report

但它不应该继续直接负责：

- machine register bank vocabulary
- caller-clobbered / callee-preserved 分类
- machine-facing value grouping
- call-clobber risk 报告
- register protection pressure / hotspot / preservation hint 这些 machine-planning artifact

不然 `value_ssa_alloc` 会同时变成：

- allocator
- machine vocabulary layer
- machine-planning analysis layer

这样边界会很快混掉。

项目现在的拆法非常清楚：

- `value_ssa_alloc`
  - 回答“怎么把 SSA value 分成 color/spill”
- `value_ssa_machine`
  - 回答“这些 color/spill 放到机器寄存器语境里意味着什么”
- `machine_ir`
  - 回答“这些 machine-facing placement 再变成真正的 machine-side IR artifact 长什么样”

一句话压缩：

- `value_ssa_alloc`：分配
- `value_ssa_machine`：解释分配结果的机器含义
- `machine_ir`：把这些机器含义落成 IR

---

## 3. 为什么它不等于 `machine_ir`

这是最重要的边界之一。

`value_ssa_machine` 虽然已经 machine-facing，但它还不是 `machine_ir`。

因为它当前关心的是：

- register bank
- placement view
- value grouping
- call risk
- protection pressure
- preservation hints

它还**没有**关心：

- CFG block 里的 machine instruction
- machine operand-level instruction stream
- phi elimination 后的 machine-side block program
- `load_local` / `binary` / `call` / `ret` 这些 instruction family 怎么落成 machine IR 指令

所以这层是：

`report / view / planning artifact`

不是：

`independent machine IR`

也正因为这样，`docs/backend/MACHINE_IR_PLAN.md` 才会明确说：

- `value_ssa_machine` 解释 abstract color 的 machine meaning
- `machine_ir` 才是第一层真正独立的 machine-facing IR

---

## 4. 文件定位

- 接口：`include/value_ssa_machine.h`
- 聚合入口：`src/value_ssa_machine/value_ssa_machine.c`
- core：`src/value_ssa_machine/value_ssa_machine_core.inc`
- query：`src/value_ssa_machine/value_ssa_machine_query.inc`
- dump：`src/value_ssa_machine/value_ssa_machine_dump.inc`
- call clobber：`src/value_ssa_machine/value_ssa_machine_call_clobber.inc`
- protection / hotspot / preservation hint / planning：`src/value_ssa_machine/value_ssa_machine_protection.inc`
- report bridge：`src/value_ssa_machine/value_ssa_machine_report.inc`
- 测试：`tests/value_ssa/value_ssa_machine_test.c`
- 规划文档：`docs/ssa/VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md`

这一层现在已经不是“一个 header + 一个 dump helper”。

从目录就能看出来，它大致分成三大块：

1. machine register bank + machine allocation view
2. call-clobber / protection / pressure / hint 分析
3. machine-facing report / direct entrypoint / dump convenience

所以如果你只把它理解成：

`color 0 -> r0`

那已经明显过时了。

### 4.1 这一层的源码最好按哪四组职责来读

如果你直接顺着文件名读，很容易被 header 里那一大串 artifact 吓到。

更容易记的方式是按四组职责分。

#### A. bank + machine view 基础层

- `value_ssa_machine_core.inc`
- `value_ssa_machine_query.inc`
- `value_ssa_machine_dump.inc`

这组负责：

- register bank
- function/program machine allocation view
- query helper
- dump helper

也就是：

`先把 allocator 结果变成 machine-facing 视图，并让它可查、可看`

#### B. call 风险层

- `value_ssa_machine_call_clobber.inc`

这组负责：

- call-crossing 风险分类
- function/program call-clobber risk report

也就是：

`谁落在 caller-clobbered register 上之后会开始危险`

#### C. protection / hotspot / hint 层

- `value_ssa_machine_protection.inc`

这组负责：

- protection agenda
- register protection pressure
- hotspot
- preservation hint
- planning report

也就是：

`既然有风险，那我们该优先保护谁、哪个寄存器最热、建议往哪搬`

#### D. direct-entry / report bridge 层

- `value_ssa_machine_report.inc`

这组负责：

- 从 layout report 直接到 machine report
- 从 `program + color_budget + bank` 直接到 machine view / machine report
- matching direct dump entrypoints

也就是：

`把这层 artifact 接到更高层调用点上`

所以 lesson 口径上，最稳妥的读取顺序其实是：

```text
core/view
-> risk
-> protection/planning
-> report/direct-entry
```

---

## 5. `include/value_ssa_machine.h`：当前数据模型

### 5.1 `ValueSsaMachineRegisterDesc` / `ValueSsaMachineRegisterBank`

这是这层最基础的静态机器词汇表。

每个 register desc 当前包含：

- `register_id`
- `name`
- `register_class`
- `allocatable`
- `caller_clobbered`
- `callee_preserved`

而整个 bank 目前就是：

```text
MachineRegisterBank =
  register_count
  + registers[]
```

所以第一版 machine bank 主要回答：

- 机器寄存器一共有多少个
- 每个寄存器叫什么
- 能不能分配
- 是 caller-clobbered 还是 callee-preserved

当前 class 还很简单，只有：

- `VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL`

这也符合规划文档里的思路：

- 先建立稳定 machine vocabulary
- 暂时不急着进入复杂多 class / constrained allocation

### 5.2 `ValueSsaMachineAllocatedValuePlacement`

这是最直接的“一个 SSA value 最后住哪儿”的 machine-facing 版本。

字段包括：

- `value_id`
- `kind`
- `machine_register_id`
- `machine_register_name`
- `spill_slot`
- `spill_intended`
- `spill_confirmed`
- `spill_recovered`

它的重要性在于：

原来的 allocator 结果里，你主要记的是：

- color id
- spill slot

到了这里，变成了：

- `ssa.3` 在 `r0`
- `ssa.0` spill 到 slot 0
- 这个 spill 是 intended / confirmed / recovered 哪一类

也就是说，`kind/color/spill` 这些 allocator 事实，现在第一次被翻成了：

`machine register name + spill artifact`

### 5.3 `ValueSsaFunctionMachineAllocationView`

这是当前最重要的 function-level artifact。

它不只保存 `placements[]`，还缓存了很多已经整理好的 machine-facing summary：

- `value_count`
- `machine_register_count`
- `spill_slot_count`
- `machine_colored_count`
- `spilled_count`
- `caller_clobbered_value_count`
- `callee_preserved_value_count`
- `used_machine_register_count`
- `machine_register_group_count`

以及几组很实用的过滤结果：

- `machine_colored_value_ids`
- `spilled_value_ids`
- `caller_clobbered_value_ids`
- `callee_preserved_value_ids`
- `used_machine_register_ids`
- `machine_register_groups`

这一点特别值得记住：

`function machine view 不是平铺 placement 表，而是已经整理好的“机器消费视图”。`

### 5.4 `ValueSsaProgramMachineAllocationView`

program 级别上，又会再汇总：

- 总函数数
- 总 value 数
- 总 colored / spilled 数
- 总 caller-clobbered / callee-preserved value 数

还会给出程序级过滤：

- `function_indices_with_machine_colors`
- `function_indices_with_spills`
- `function_indices_with_caller_clobbered_values`
- `function_indices_with_callee_preserved_values`

所以这一层很明显已经不只是：

`逐函数孤立投影`

而是有了 program-level machine summary。

### 5.5 `ValueSsaAllocateRewriteMachineReport`

这层还有一个很关键的上层 artifact：

```text
AllocateRewriteMachineReport =
  allocate+rewrite stats
  + program machine allocation view
```

也就是说，如果你已经有了 allocate+rewrite 的结构化报告，现在可以直接继续升成 machine-facing report，而不必自己重新拆 layout / placement。

这是这层和更高层调用点连接得最实用的一条桥。

### 5.6 这一层的 artifact 家族其实有明显分层

如果你面对 header 一眼看过去觉得“结构太多”，最好的压缩方式不是硬背名字，而是先记三层：

#### 第一层：machine allocation view

- `ValueSsaMachineRegisterBank`
- `ValueSsaFunctionMachineAllocationView`
- `ValueSsaProgramMachineAllocationView`
- `ValueSsaAllocateRewriteMachineReport`

这一层回答：

`分配结果投影到机器寄存器世界之后长什么样`

#### 第二层：risk / protection artifact

- `ValueSsaMachineCallClobberRiskReport`
- `ValueSsaMachineProtectionAgenda`
- `ValueSsaMachineRegisterProtectionPressureView`

这一层回答：

`哪些值/寄存器因为 call-preservation 问题而变得危险`

#### 第三层：planning artifact

- `ValueSsaFunctionMachineRegisterProtectionHotspotView`
- `ValueSsaFunctionMachinePreservationHintView`
- `ValueSsaFunctionMachinePlanningView`
- 各自的 program-level report

这一层回答：

`如果要进一步机器规划，现在最值得盯哪一块、往哪挪`

这样一拆，你会发现当前 `value_ssa_machine` 不是一堆杂项，而是：

```text
projection
-> risk
-> protection
-> planning
```

---

## 6. 这层到底“改了什么 / 增加了什么”

按你一直强调的口径，这里专门讲：

- 从 allocator 结果翻译了什么
- 新增了什么信息
- 为什么这些新增信息重要

### 6.1 例子一：`color 0` 不再只是抽象编号

原来 allocator 结果里，你看到的是：

```text
ssa.1 -> color 0
ssa.2 -> color 1
ssa.0 -> spill slot 0
```

到了 `value_ssa_machine`，会被翻成更像：

```text
ssa.1 reg=r0
ssa.2 reg=r1
ssa.0 spill=0 spill-confirmed
```

这里真正新增的信息不是“名字更好看了”。

而是：

1. color 映射到了真实 machine register id / name
2. spill 结果和 machine-colored 结果开始能放在同一个 machine-facing视图里看
3. caller-clobbered / callee-preserved 分类也可以立即附着到这些值上

### 6.2 例子二：新增了 register-group 视角

在 function machine view 的 dump 里，现在还会看到：

```text
register-group r0: ssa.1 ssa.3 ssa.4
register-group r1: ssa.2
```

这类信息在原始 allocator result 里并没有直接整理好。

它是这一层新增出来的：

`每个机器寄存器当前承载了哪些 SSA values`

这对后面做 machine-side planning 很有用，因为你可以直接问：

- 哪个寄存器最拥挤
- 哪个寄存器承载的值更可能跨 call 活

### 6.3 例子三：新增了 caller-clobbered / callee-preserved value bucket

当前 function dump 还会直接给：

```text
caller-clobbered-values: ssa.1 ssa.2 ssa.3 ssa.4
```

也就是说，这层开始不再只是看“值有没有颜色”，而是开始看：

`这个颜色落到的机器寄存器，在 call-preservation 语义上属于哪类`

这一步就是后面 call-clobber risk / protection agenda 的基础。

### 6.4 例子四：新增了“程序入口级 machine artifact”

以前如果高层调用者想拿到 machine-facing 结果，通常要自己分几步：

```text
program
-> allocate result
-> allocation layout
-> attach context
-> build machine bank
-> build machine view / machine report
```

现在这层已经把这些 glue 收成了直接入口，例如：

- `value_ssa_allocate_function_machine_view(...)`
- `value_ssa_allocate_program_machine_view(...)`
- `value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report(...)`

所以它新增的不只是数据结构，还有：

`更高层的 machine-facing caller surface`

---

## 7. 一个最小 bank 例子

`tests/value_ssa/value_ssa_machine_test.c` 里有一个最基础的例子：

```text
machine-register-bank registers=3
r0 name=r0 class=general alloc=1 caller_clobbered=1 callee_preserved=0
r1 name=r1 class=general alloc=1 caller_clobbered=1 callee_preserved=0
r2 name=r2 class=general alloc=1 caller_clobbered=1 callee_preserved=0
```

这个例子说明了：

1. 当前 flat bank 构造器已经落地
2. 每个 register 都有稳定 id 和 printable name
3. 目前 flat bank 默认都当 caller-clobbered

而测试里另一个手工 bank 例子则会故意构造：

- `r0` caller-clobbered
- `r1` callee-preserved

这说明 current API 并不是只能做全平坦 caller-clobbered bank，它已经能表达最基本的 preservation split。

---

## 8. function machine allocation view：最该先学会的一层

测试里有一个代表性 dump：

```text
machine-allocation func spill regs=2 values=5 colored=4 spilled=1 spill_slots=1 caller_clobbered=4 callee_preserved=0 used_regs=2 reg_groups=2
used-registers: r0 r1
caller-clobbered-values: ssa.1 ssa.2 ssa.3 ssa.4
ssa.0 spill=0 spill-confirmed
ssa.1 reg=r0
ssa.2 reg=r1 spill-intended
ssa.3 reg=r0
ssa.4 reg=r0
register-group r0: ssa.1 ssa.3 ssa.4
register-group r1: ssa.2
```

这个例子里最值得学的点有四个。

### 8.1 值最终住在哪里已经 machine-facing

- `ssa.1 reg=r0`
- `ssa.2 reg=r1`
- `ssa.0 spill=0`

### 8.2 spill 标签不会丢

比如：

- `spill-intended`
- `spill-confirmed`
- `spill-recovered`

说明这一层没有把 allocator 内部语义抹平，它只是把它们带进 machine-facing 视角。

### 8.3 used-registers 和 register-group 已经整理好

这让下游不用自己扫全表去算：

- 函数实际用了哪些 registers
- 每个 register 上堆了哪些 values

### 8.4 caller-clobbered bucket 已经能直接看

这一步是后来 protection 线的基础。

### 8.5 这里其实已经能看出和 `machine_ir` 的边界

虽然这个 dump 已经很 machine-facing，但你会注意到它仍然没有：

- block
- terminator
- machine instruction
- operand-level op family

它只有：

- value placement
- register groups
- spill state
- preservation class

这说明它当前仍然是：

`machine-facing allocation artifact`

而不是：

`machine program artifact`

---

## 9. program machine allocation view：程序级过滤已经有了

program-level dump 当前会给出：

```text
machine-allocation program functions=2 values=10 colored=9 spilled=1 caller_clobbered=9 callee_preserved=0 colored_funcs=2 spilled_funcs=1 caller_clobbered_funcs=2 callee_preserved_funcs=0
functions-with-machine-colors: 0 1
functions-with-spills: 1
functions-with-caller-clobbered-values: 0 1
```

这说明程序级视角下，现在已经能直接问：

- 哪些函数有颜色值
- 哪些函数有 spill
- 哪些函数含 caller-clobbered values

这类信息如果放在 allocator 层会显得别扭，但放在 machine-facing view 层就很自然。

---

## 10. call-clobber risk：这层已经不只是“映射寄存器”

这是当前 `value_ssa_machine` 最容易被低估的一部分。

当前 header 里已经有一整套：

- `ValueSsaMachineCallClobberRiskClass`
- `ValueSsaMachineCallClobberRiskItem`
- `ValueSsaMachineCallClobberRiskReport`
- function/program risk report

也就是说，这层已经开始回答：

`某个值如果落在 caller-clobbered register 上，会不会因为跨 call 活跃而更危险？`

### 10.1 风险分类当前长什么样

当前 risk class 有：

- `NONE`
- `CROSSING`
- `REPEATED`
- `HOT`

这说明风险不是二元的“危险/不危险”，而是有分级。

### 10.2 一个最小风险例子

测试里有个非常代表性的 dump：

```text
machine-call-clobber-risk values=3 risky=1 crossing=0 repeated=1 hot=0
ssa.0 reg=r0 class=repeated call_cross=1 calls=1 use_calls=1
```

这里表达的意思是：

- 一共看了 3 个值
- 其中只有 1 个值真的是 risky
- 这个值当前落在 `r0`
- 它属于 `repeated` 风险类
- 它跨过 call 的次数、相关 call 密度，都被统计出来了

这说明 `value_ssa_machine` 当前已经不是“静态寄存器映射表”，而是开始利用：

- allocation prep
- call density
- liveness crossing

来构造 machine-facing 风险报告。

### 10.3 这里的重要边界

它现在还没有直接改分配策略。

它只是把：

`哪些值在 caller-clobbered register 上看起来更危险`

做成一个独立 artifact。

这也是为什么它应该留在 `value_ssa_machine`，而不是直接硬塞回 allocator 主循环。

### 10.4 风险分类源码其实很适合 lesson 里直接讲

`src/value_ssa_machine/value_ssa_machine_call_clobber.inc` 里的分类规则是很清楚的：

```text
if call_crossing_count == 0:
    NONE
else if call_crossing_count >= 2 or use_call_density_sum >= 2:
    HOT
else if call_density_sum >= 2 or use_call_density_sum >= 1:
    REPEATED
else:
    CROSSING
```

这段伪代码非常适合放进 lesson，因为它能直接回答：

- 为什么某个值只是 `crossing`
- 为什么某个值会升级成 `repeated`
- 什么情况下才会变成 `hot`

也说明这层现在不是“主观打标签”，而是：

`基于 allocation prep 统计量做稳定分类`

---

## 11. protection agenda：从“有风险”走到“优先保护谁”

有了 call-clobber risk，下一步自然是：

`如果要优先保护一些值，先保护谁？`

于是当前就有了：

- `ValueSsaMachineProtectionAgenda`
- `ValueSsaMachineProtectionAgendaItem`

测试里的代表性 dump 是：

```text
machine-protection-agenda values=3 items=1 crossing=0 repeated=1 hot=0
ssa.0 reg=r0 class=repeated priority=114 call_cross=1 calls=1 use_calls=1 loops=0 use_loops=0 xblock=0 remat=1
```

这说明 agenda item 里已经不只是“这个值有风险”，还带了：

- `protection_priority`
- `call_crossing_count`
- `call_density_sum`
- `use_call_density_sum`
- `loop_depth_sum`
- `use_loop_depth_sum`
- `single_block_live_range`
- `rematerializable`

所以 protection agenda 的准确身份是：

`风险分层之后的优先级排序 artifact`

不是直接做寄存器搬迁，但已经开始回答：

`如果后面真要做保护/搬迁，应该先看谁`

### 11.1 agenda 的排序规则也值得单独记

`value_ssa_machine_protection.inc` 里当前排序大致按下面顺序：

1. `protection_priority` 更高的在前
2. `risk_class` 更重的在前
3. `rematerializable` 的比较
4. `single_block_live_range` 的比较
5. 最后再按 `value_id`

这说明 agenda 不是简单的“按 risk class 排”。

它已经在综合：

- call 风险
- rematerialization 可能性
- live range 形状

所以：

- risk report 更像“哪些值危险”
- agenda 更像“如果真要动手，先动谁”

---

## 12. register pressure / hotspot：更像机器规划报告

再往上，当前代码已经继续长出：

- `ValueSsaMachineRegisterProtectionPressureView`
- `ValueSsaMachineRegisterProtectionHotspotView`

这两类 artifact 的重点已经不再是“单个值”，而是：

- 哪个寄存器更有保护压力
- 哪个寄存器是当前热点

### 12.1 register protection pressure

测试里的一个 dump：

```text
machine-register-protection-pressure regs=1 pressured=1 values=3 items=1 total_priority=114 crossing=0 repeated=1 hot=0
reg r0 items=1 total_priority=114 max_priority=114 crossing=0 repeated=1 hot=0 values: ssa.0
```

这里真正新增的是：

- 以 machine register 为中心的压力统计
- 这个寄存器上有多少需要保护的值
- 它们的总 priority / max priority 是多少

这一步已经很像“机器寄存器规划视角”了。

### 12.2 hotspot

hotspot view 又会进一步压缩成：

```text
function-register-protection-hotspot call_density reg=r0 items=1 total_priority=114 max_priority=114 crossing=0 repeated=1 hot=0
```

也就是说，它在问：

`当前函数里，哪个寄存器最像保护热点`

所以：

- pressure view 更像完整统计表
- hotspot view 更像高层摘要

---

## 13. preservation hint：这层已经开始给“建议”

这是 `value_ssa_machine` 里最有意思也最容易漏讲的一部分。

当前它已经可以生成：

- `ValueSsaFunctionMachinePreservationHintView`
- `ValueSsaProgramMachinePreservationHintReport`

测试里的代表性 dump 是：

```text
function-machine-preservation-hint call_pressure move=r0->r2 candidates=2 items=1 total_priority=114 max_priority=114 crossing=0 repeated=1 hot=0
candidate 0 move=r0->r2 reason=target-less-occupied target_items=1 target_priority=0 target_max=0 items=1 total_priority=114 max_priority=114
candidate 1 move=r0->r1 reason=target-less-occupied target_items=3 target_priority=0 target_max=0 items=1 total_priority=114 max_priority=114
```

这个 artifact 在表达什么？

它其实是在说：

- 当前某个源寄存器 `r0` 上有需要保护的高优先值
- 如果要把保护压力搬走，候选目标可以是 `r2` 或 `r1`
- 当前最推荐的是 `r2`
- 主要原因是 `target-less-occupied`

也就是说，这层现在已经不只是“看问题”，还会给出：

`机器寄存器层面的保守迁移建议`

但注意边界：

- 它只是 hint / planning artifact
- 还不是 allocator 主循环里的强制重分配逻辑

### 13.1 preservation hint 候选为什么会按那个顺序排

`value_ssa_machine_protection.inc` 里对 candidate 的排序大致偏向：

1. 目标寄存器当前总保护压力更小
2. 目标寄存器当前承载 item 更少
3. 目标寄存器当前最大压力更小
4. 最后再按 register id

这很适合 lesson 里直接讲成：

`preservation hint 不是随便挑一个空位，而是优先挑“更空、更轻、更不热”的目标寄存器`

而 `primary_reason` 当前又会被归成：

- `target-empty`
- `target-less-occupied`
- `target-lower-protection-pressure`

所以这层不仅给建议，还会解释“为什么是这个建议”。

---

## 14. planning report：把 pressure / hotspot / hint 串成一个总报告

当前 header 最上层还有：

- `ValueSsaFunctionMachinePlanningView`
- `ValueSsaProgramMachinePlanningReport`

它把三件事绑在一起：

1. register protection pressure
2. hotspot
3. preservation hint

测试里的最小 program dump 大致是：

```text
program-machine-planning functions=2 values=10 pressure_items=0 pressure_priority=0 hotspot_priority=0 hint_priority=0 pressure_funcs=0 hotspot_funcs=0 hint_funcs=0
function-machine-planning main
machine-register-protection-pressure ...
function-register-protection-hotspot main hotspot=none
function-machine-preservation-hint main hint=none
function-machine-planning spill
machine-register-protection-pressure ...
function-register-protection-hotspot spill hotspot=none
function-machine-preservation-hint spill hint=none
```

所以 planning report 的准确身份是：

`面向后续机器规划的高层组合视图`

它把前面分散的 machine-facing artifact 打包成一个更适合整体观察的入口。

### 14.1 这一层很像“后端前的机器规划控制台”

如果只从 lesson 的角度打一个比方，当前 planning report 最像：

`在真正 machine_ir / machine_select 之前，先做一份机器寄存器健康报告`

它不会直接发指令，但它会告诉你：

- 哪些函数有压力
- 哪些寄存器是热点
- 哪些位置有 preservation hint

所以这层虽然还不是后端 IR，但已经很像：

`后端前的 machine planning dashboard`

---

## 15. 一段伪代码看懂这层主线

把当前主线压缩一下，大致就是：

```text
build_machine_bank():
    define r0 / r1 / r2 ...
    attach class / allocatable / caller-clobbered / callee-preserved

build_machine_view(allocation_layout, machine_bank):
    for each value:
        if colored:
            map color C -> register R[C]
            record register id/name
            bucket into caller-clobbered or callee-preserved sets
            append to register-group[R]
        if spilled:
            keep spill slot and spill status flags

build_call_clobber_risk(machine_view, allocation_prep):
    inspect call-crossing live values
    classify risky values into crossing/repeated/hot

build_protection_agenda(risk_report, allocation_prep):
    prioritize risky values using call/loop/remat/live-range signals

build_pressure_and_hints(machine_view, agenda, bank):
    aggregate protection load by register
    find hotspot registers
    suggest lower-pressure target registers

build_planning_report():
    package pressure + hotspot + hint for function/program consumers
```

这段伪代码里最值得记住的四件事是：

1. allocator 结果先被投影成 machine register 语义
2. 然后才开始谈 caller-clobber 风险
3. 再往上是 register-level pressure / hotspot / hint
4. 直到这一步都还只是 view/report/planning，不是 machine IR

### 15.1 再补一条“从调用者角度”的伪代码

如果你是高层 caller，当前最常见的使用路径其实更像：

```text
if I only want a machine-facing placement dump:
    allocate_program_machine_view_dump(program, color_budget, reg_count)

if I already have allocate+rewrite report:
    build_allocate_rewrite_machine_report(layout_report, bank)

if I want machine planning insight:
    compute_program_machine_planning_report(program, color_budget, bank)
```

也就是说，这层当前不仅有“artifact builder”，还有很明显的：

`convenience entrypoint ladder`

这也是为什么 `value_ssa_machine_report.inc` 值得在 lesson 里单独强调。

---

## 16. 和 `machine_ir` 的真实边界

这条线和 `machine_ir` 很容易被混掉，所以这里专门再压一次。

### 16.1 `value_ssa_machine` 负责

- register bank
- color -> register 投影
- spill / recovered / intended machine-facing表述
- register grouping
- caller-clobbered / callee-preserved 分类
- call-clobber risk
- protection agenda
- pressure / hotspot / preservation hint / planning report

### 16.2 `machine_ir` 负责

- machine operand
- machine block / terminator
- machine-side instruction artifact
- cleanup / call effects / constraints
- 真正独立的 machine IR program

所以：

- `value_ssa_machine` 还在“解释 allocator 结果”
- `machine_ir` 才在“建 machine-side program”

这也是为什么仓库里既需要：

- `lesson/ssa/value_ssa_machine_lesson.md`
- 也需要 `lesson/machine/lowering/machine_ir_lesson.md`

它们不是同一层的重复讲法。

---

## 17. 当前支持 / 不支持什么

### 17.1 当前已经支持

- flat machine register bank
- split caller/callee-preserve machine bank
- function/program machine allocation view
- allocate+rewrite machine report
- direct machine view / report entrypoints
- direct dump entrypoints
- call-clobber risk report
- protection agenda
- register protection pressure
- register protection hotspot
- preservation hint
- combined planning report

### 17.2 当前还没有故意去做的

- allocator mainline里的强制 machine-aware recoloring
- class-constrained coloring
- fixed-operand instruction constraints
- parallel copy resolution
- machine instruction stream
- selected op / layout / emit / encode

所以这层现在最准确的身份不是：

`早期 machine backend`

而是：

`allocator-adjacent machine register model + planning/report layer`

### 17.1 这层当前最适合谁来消费

如果把 lesson 再落到工程角色上，当前最适合消费这层的其实是三类调用者：

1. 想把 allocator 结果说成人类可读 machine 语言的调试/报告调用者
2. 想在进 `machine_ir` 之前先做 machine-preservation 风险观察的规划调用者
3. 想拿到“函数/程序级 machine-facing摘要”，而不想自己重搭 layout + bank + query 的高层入口调用者

这也解释了为什么它现在会有那么多：

- `build_*`
- `compute_*`
- `dump_*`
- `*_flat_*`

入口族谱。

---

## 18. 最后怎么读这份模块

如果你准备顺着源码继续读，我建议按这个顺序：

1. 先看 `include/value_ssa_machine.h`
   - 建立 artifact 全景图
2. 再看 `value_ssa_machine_core.inc`
   - 搞懂 bank 和 machine view
3. 再看 `value_ssa_machine_query.inc` / `dump.inc`
   - 了解这层怎么被消费
4. 再看 `value_ssa_machine_call_clobber.inc`
   - 看风险分类是怎么来的
5. 最后看 `value_ssa_machine_protection.inc`
   - 看 pressure / hotspot / hint / planning 是怎么逐层搭起来的

### 18.1 如果你想顺着“一个例子”读，而不是顺着文件读

也可以用下面这条路径：

1. 先看 `test_value_ssa_build_function_machine_allocation_view`
   - 理解 `spill -> reg/spill/group`
2. 再看 `test_value_ssa_machine_call_clobber_risk_report`
   - 理解跨 call 风险分类
3. 再看 `test_value_ssa_machine_protection_agenda`
   - 理解风险怎么变成保护优先级
4. 再看 `test_value_ssa_machine_register_protection_pressure_view`
   - 理解 value 级问题怎么汇总成 register 级压力
5. 最后看 `test_value_ssa_function_machine_preservation_hint_view`
   - 理解为什么最终会得出 `r0 -> r2` 这类建议

这条路径特别适合你现在这种“按 lesson 学仓库”的节奏，因为它能把：

`projection -> risk -> protection -> hint`

串成一个完整故事。

如果你是从 lesson 角度只想先抓重点，那就先记下面这句。

---

## 19. 一句总记

`value_ssa_machine` 不是 machine IR，它是 allocator 结果进入机器寄存器语义世界之前的解释层和规划层：先把 color/spill 变成 r0/r1/slot，再把 caller-clobber 风险、保护压力、热点和迁移提示整理成 machine-facing artifact，供后面的真实 machine IR 和 backend 消费。`
