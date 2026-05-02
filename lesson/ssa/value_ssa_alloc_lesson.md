# Value SSA Alloc Lesson（compiler_lab）

> 目标：专门讲 `value_ssa_alloc`。这里聚焦 allocator 这一层在做什么、它依赖哪些 shared analysis、当前 simplify/freeze/spill 状态机是怎么工作的、reverse-select coloring / spill / rewrite 又是怎么落地的。

## 一句话定位

`value_ssa_alloc` 是 SSA 世界里第一次真正开始回答“这些值最后住哪”的层，但它回答的还是 allocator vocabulary，而不是最终 machine IR vocabulary。

## 常见误解

- 误解一：allocator 一出来就已经等于后端寄存器分配完成。
  - 这里先得到的是 color / spill / slot 等 abstract result，后面还要接 machine-facing 翻译。
- 误解二：allocator 这里只有分析，没有真实改写。
  - 当前这条线已经有真实 spill rewrite，不只是报表。

## 导学

现在 `value_ssa` 这一条线已经更适合拆成四份来读：

1. `ssa/value_ssa_lesson.md`
   - 讲 core representation / verifier / conversion / shared analysis
2. `ssa/value_ssa_pass_lesson.md`
   - 讲 SSA-side cleanup / canonicalization
3. `ssa/value_ssa_alloc_lesson.md`
   - 讲 allocator plan / coloring / spill / rewrite
4. `value_ssa_interp_lesson.md`
   - 讲 SSA 解释器 / oracle

这份就是第三份，也就是 **allocator lesson**。

---

## 前置阅读

最推荐先读：

1. `lesson/ssa/value_ssa_lesson.md`
2. `lesson/ssa/value_ssa_pass_lesson.md`

如果你还没先搞清楚：

- SSA core 里的值、block、phi、use-def 关系
- SSA-side cleanup / canonicalization 已经做了什么

那 allocator 这里的 interference、coalescing、spill、rewrite 会比较难定位。

## 读完后接哪篇

最自然往下接：

1. `lesson/ssa/value_ssa_machine_lesson.md`
2. `lesson/machine/lowering/machine_ir_lesson.md`

因为 allocator 的最重要后续问题就是：

- abstract color / spill 结果怎么进入 machine register vocabulary
- 后面怎么正式进入 machine backend

---

## 1. 这层现在是什么

当前 allocator 不是 machine backend，也不是完整寄存器分配器。

更准确地说，它现在是一条：

`SSA-level conservative allocator experiment line`

也就是：

- 输入是 `ValueSsaFunction`
- 依赖 shared analysis / interference / affinity / allocation prep
- 输出是：
  - color
  - spill flag
  - spill priority
  - spill slot
- 还能对一部分 spill 结果做真实 rewrite

所以它已经不是“只有分析结果，没有 allocator”，但也还不是完整 Briggs/Chaitin 主循环。

---

## 2. 文件定位

- 接口：`include/value_ssa_alloc.h`
- 聚合入口：`src/value_ssa_alloc/value_ssa_alloc.c`
- core/result helper：`src/value_ssa_alloc/value_ssa_alloc_core.inc`
- spill/slot helper：`src/value_ssa_alloc/value_ssa_alloc_spill.inc`
- spill-cost helper：`src/value_ssa_alloc/value_ssa_alloc_spill_cost.inc`
- coalescing analysis：`src/value_ssa_alloc/value_ssa_alloc_coalesce.inc`
- coalescing opportunity agenda：`src/value_ssa_alloc/value_ssa_alloc_coalesce_agenda.inc`
- move family analysis：`src/value_ssa_alloc/value_ssa_alloc_move.inc`
- move family worklist：`src/value_ssa_alloc/value_ssa_alloc_move_worklist.inc`
- move transition trace：`src/value_ssa_alloc/value_ssa_alloc_move_transition.inc`
- allocator plan：`src/value_ssa_alloc/value_ssa_alloc_plan.inc`
- retry family agenda：`src/value_ssa_alloc/value_ssa_alloc_retry_agenda.inc`
- retry phase trace：`src/value_ssa_alloc/value_ssa_alloc_retry_phase_trace.inc`
- reverse select：`src/value_ssa_alloc/value_ssa_alloc_select.inc`
- coloring：`src/value_ssa_alloc/value_ssa_alloc_color.inc`
- spill rewrite：`src/value_ssa_alloc/value_ssa_alloc_rewrite.inc`
- layout / placement：`src/value_ssa_alloc/value_ssa_alloc_layout.inc`
- layout dump：`src/value_ssa_alloc/value_ssa_alloc_layout_dump.inc`
- dump：`src/value_ssa_alloc/value_ssa_alloc_dump.inc`

相关测试：

- `tests/value_ssa/value_ssa_alloc_test.c`

---

## 2.1 public surface 总表

如果你以后只是想快速定位“某个 allocator artifact 属于哪层”，可以先看这张总表。

### 2.1.1 分配主线入口

- `value_ssa_allocate_function(...)`
- `value_ssa_allocate_program(...)`
- `value_ssa_allocate_function_from_plan(...)`

这几条入口负责：

- 普通分配
- 程序级批量分配
- artifact-driven select / spill-slot 实验入口

### 2.1.2 结果与查询

- `ValueSsaAllocationResult`
- `value_ssa_allocation_result_get_color(...)`
- `value_ssa_allocation_result_is_spilled(...)`
- `value_ssa_allocation_result_is_spill_intended(...)`
- `value_ssa_allocation_result_is_spill_confirmed(...)`
- `value_ssa_allocation_result_is_spill_recovered(...)`
- `value_ssa_allocation_result_used_retry_eviction(...)`

这层负责：

`逐值最终结果`

### 2.1.3 plan / select 工件

- `ValueSsaAllocatorPlan`
- `value_ssa_compute_allocator_plan(...)`
- `value_ssa_dump_allocator_plan(...)`
- `ValueSsaAllocationSelectStats`
- `value_ssa_compute_allocation_select_stats(...)`
- `value_ssa_dump_allocation_select_stats(...)`
- `value_ssa_dump_allocation_select_trace(...)`

这层负责：

- remove/freeze/spill 计划
- select 摘要
- select 逐值轨迹

### 2.1.4 coalescing / family 工件

- `ValueSsaAllocatorCoalesceAnalysis`
- `value_ssa_compute_allocator_coalesce_analysis(...)`
- `value_ssa_dump_allocator_coalesce_analysis(...)`
- `ValueSsaAllocatorMoveFamilyAnalysis`
- `value_ssa_compute_allocator_move_family_analysis(...)`
- `ValueSsaAllocatorMoveWorklist`
- `value_ssa_compute_allocator_move_worklist(...)`
- `ValueSsaAllocatorMoveTransitionTrace`
- `value_ssa_compute_allocator_move_transition_trace(...)`

这层负责：

- pair -> family
- family 当前状态
- family 待办顺序
- family 状态变化轨迹

### 2.1.5 agenda / protocol 工件

- `ValueSsaAllocatorCoalesceOpportunityAgenda`
- `value_ssa_compute_allocator_coalesce_opportunity_agenda(...)`
- `ValueSsaAllocatorRetryFamilyAgenda`
- `value_ssa_compute_allocator_retry_family_agenda(...)`
- `ValueSsaAllocatorRetryPhaseTrace`
- `value_ssa_compute_allocator_retry_phase_trace(...)`

这层负责：

- “先并谁”的 family-pair 机会表
- retry outer field / family agenda
- retry family phase 序列

### 2.1.6 layout / placement / report 工件

- `ValueSsaFunctionAllocationLayout`
- `ValueSsaProgramAllocationLayout`
- `ValueSsaAllocateRewriteLayoutReport`
- `value_ssa_allocate_function_layout(...)`
- `value_ssa_allocate_program_layout(...)`
- `value_ssa_allocate_and_rewrite_program_single_block_spills_layout(...)`
- `value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(...)`
- `value_ssa_dump_function_allocation_layout(...)`
- `value_ssa_dump_program_allocation_layout(...)`
- `value_ssa_dump_allocate_rewrite_layout_report(...)`

这层负责：

- 把逐值 result 变成 placement artifact
- 把颜色 / spill-slot 分组整理出来
- 把 allocate+rewrite 的结果做成更适合汇总/比较的报告

### 2.1.7 rewrite 工件

- `value_ssa_rewrite_program_single_block_spills(...)`
- `value_ssa_rewrite_program_single_block_spills_and_canonicalize(...)`
- `value_ssa_allocate_and_rewrite_program_single_block_spills(...)`

这层负责：

- 真正改写 SSA

一句话总记法：

```text
入口
-> 计划/选择
-> coalescing/family
-> agenda/protocol
-> rewrite/layout/report
```

---

## 2.2 按文件分职责总图

如果你是顺着目录读代码，最省事的方式不是记文件名，而是先记它们属于哪一层。

### 2.2.1 orchestration 层

- `value_ssa_alloc.c`
- `value_ssa_alloc_color.inc`

这层负责：

- 总装入口
- 把分析、plan、select、spill-slot、rewrite 串起来

可以先记成：

`high-level driver / orchestration`

### 2.2.2 planning 层

- `value_ssa_alloc_plan.inc`
- `value_ssa_alloc_move_engine.inc`
- `value_ssa_alloc_spill_cost.inc`

这层负责：

- remove/freeze/spill 主计划
- family-aware runtime state
- spill cost / rematerializable / split-child 这些 planner 侧信号

可以先记成：

`planner / runtime family state`

### 2.2.3 coalescing / move-family 层

- `value_ssa_alloc_coalesce.inc`
- `value_ssa_alloc_coalesce_agenda.inc`
- `value_ssa_alloc_move.inc`
- `value_ssa_alloc_move_worklist.inc`
- `value_ssa_alloc_move_transition.inc`

这层负责：

- pair -> family
- family 状态摘要
- family 待办排序
- family transition 轨迹
- “先并谁”的机会表

可以先记成：

`family-side control surfaces`

### 2.2.4 select / retry / spill-choice 层

- `value_ssa_alloc_select.inc`
- `value_ssa_alloc_spill.inc`
- `value_ssa_alloc_retry_agenda.inc`
- `value_ssa_alloc_retry_phase_trace.inc`

这层负责：

- reverse select
- preferred color
- local eviction / blocker recolor
- retry family agenda
- retry family phase trace

可以先记成：

`choose / evict / recover`

### 2.2.5 rewrite / layout / dump 层

- `value_ssa_alloc_rewrite.inc`
- `value_ssa_alloc_layout.inc`
- `value_ssa_alloc_layout_dump.inc`
- `value_ssa_alloc_dump.inc`
- `value_ssa_alloc_core.inc`

这层负责：

- spill rewrite
- placement / layout artifact
- dump / query / summary
- result / report 基础结构

可以先记成：

`result shaping / observability`

### 2.2.6 目录总图

把上面五层压成一张图，大概就是：

```text
orchestration
    -> planning
    -> coalescing/family
    -> select/retry/spill-choice
    -> rewrite/layout/dump
```

如果你以后打开一个新文件，不确定它大概是干嘛的，先问：

- 它是在“做决定”？
- 还是“组织 family”？
- 还是“做结果排版 / 观测”？

大多数时候这个问题一问，层次就出来了。

---

## 2.3 推荐阅读顺序

现在 `value_ssa_alloc/` 已经不小了，最容易踩的坑就是：

- 一上来就钻进 `move_engine.inc`
- 然后半小时后已经忘了 allocator 主线到底长什么样

所以这里给一个更省脑的阅读顺序。

### 2.3.1 第一轮：先抓主线

第一轮建议只读这几个文件：

1. `include/value_ssa_alloc.h`
2. `src/value_ssa_alloc/value_ssa_alloc_color.inc`
3. `src/value_ssa_alloc/value_ssa_alloc_plan.inc`
4. `src/value_ssa_alloc/value_ssa_alloc_select.inc`
5. `src/value_ssa_alloc/value_ssa_alloc_rewrite.inc`

这一轮的目标不是读细节，而是回答：

- allocator 输入是什么
- 主计划怎么形成
- 颜色怎么选
- spill 后怎么 rewrite

也就是先把这条线吃下来：

```text
prep
-> plan
-> select
-> spill slots
-> rewrite
```

### 2.3.2 第二轮：再看 family-aware 中层

第二轮再读：

1. `src/value_ssa_alloc/value_ssa_alloc_coalesce.inc`
2. `src/value_ssa_alloc/value_ssa_alloc_move.inc`
3. `src/value_ssa_alloc/value_ssa_alloc_move_worklist.inc`
4. `src/value_ssa_alloc/value_ssa_alloc_move_transition.inc`
5. `src/value_ssa_alloc/value_ssa_alloc_coalesce_agenda.inc`

这一轮回答的是：

- pair 怎么变成 family
- family 当前是什么状态
- family 谁该先处理
- family 的 transition trace 怎么记
- “先并谁”的 agenda 怎么组织

这一轮读完，你就会开始觉得 allocator 已经不是单纯 value-level 了。

### 2.3.3 第三轮：最后再碰 `move_engine`

第三轮再去看：

- `src/value_ssa_alloc/value_ssa_alloc_move_engine.inc`

因为这个文件最像：

`把前面所有 family-aware runtime 状态揉在一起的共享调度核心`

如果前两轮没打底，直接看它基本只会头晕。

这一轮重点回答：

- 当前 family-aware planner/runtime 是怎么共用一套核心的
- `coalesce / freeze / remove` 事件在内部怎么推进
- 哪些状态是增量更新，哪些还是重算

### 2.3.4 第四轮：最后读 observability / report

最后再读：

1. `src/value_ssa_alloc/value_ssa_alloc_core.inc`
2. `src/value_ssa_alloc/value_ssa_alloc_dump.inc`
3. `src/value_ssa_alloc/value_ssa_alloc_layout.inc`
4. `src/value_ssa_alloc/value_ssa_alloc_layout_dump.inc`
5. `src/value_ssa_alloc/value_ssa_alloc_retry_agenda.inc`
6. `src/value_ssa_alloc/value_ssa_alloc_retry_phase_trace.inc`

这一轮回答的是：

- allocator 现在到底暴露了哪些结果面
- 哪些 surface 是给测试/调参/诊断用的
- layout/report 这层怎么读

### 2.3.5 一个最短版顺序

如果你只想记一条超短顺序，就记：

```text
头文件
-> plan
-> select
-> rewrite
-> coalesce/family
-> move_engine
-> dump/layout/report
```

---

## 2.4 按问题查文件

如果你不是线性读代码，而是“我现在只想知道某个问题在哪”，可以按这个表查。

### 2.4.1 “为什么这个值最后是 simplify / freeze / spill？”

先看：

- `value_ssa_alloc_plan.inc`
- `value_ssa_alloc_move_engine.inc`

如果还想看外显结果：

- `value_ssa_dump_allocator_plan(...)`

### 2.4.2 “为什么它拿了这个颜色？”

先看：

- `value_ssa_alloc_select.inc`

重点关注：

- preferred color
- targeted eviction
- retry recovery

如果想看输出：

- `value_ssa_dump_allocation_select_trace(...)`

### 2.4.3 “为什么这个 family 还会 freeze？”

先看：

- `value_ssa_alloc_move.inc`
- `value_ssa_alloc_move_worklist.inc`
- `value_ssa_alloc_move_transition.inc`
- `value_ssa_alloc_move_engine.inc`

如果想看工件：

- move family analysis
- move worklist
- move transition trace

### 2.4.4 “为什么这个 spill 值后来又活了？”

先看：

- `value_ssa_alloc_select.inc`
- `value_ssa_alloc_retry_agenda.inc`
- `value_ssa_alloc_retry_phase_trace.inc`

这条线的关键词是：

- `spill_recovered`
- `retry-evict`
- `retry-phase`

### 2.4.5 “spill 为什么选中了这个人？”

先看：

- `value_ssa_alloc_spill_cost.inc`
- `value_ssa_alloc_plan.inc`
- `value_ssa_alloc_spill.inc`

重点关注：

- `spill_cost`
- `rematerializable`
- `split_child`
- blocker 选择 tie-break

### 2.4.6 “rewrite 后为什么长这样？”

先看：

- `value_ssa_alloc_rewrite.inc`

如果你想知道最后怎么被整理成报表，再看：

- `value_ssa_alloc_layout.inc`
- `value_ssa_alloc_layout_dump.inc`

---

## 2.5 术语对照表

这一节专门解决一个现实问题：

`allocator 现在的术语已经很多了，而且不少词看起来很像。`

如果你后面读 trace / dump / plan，先把这张表吃掉，会省很多脑力。

### 2.5.1 value / family / root

- `value`
  - 一个具体的 `ssa.N`
- `family`
  - 一组当前被视为同一 coalesced class 的值
- `root`
  - 这个 family 当前的代表值 id

一句话：

`value 是单点，family 是一组，root 是这一组的代表。`

### 2.5.2 work class / move work class / phase

- `work_class`
  - allocation worklist 层的值级分类
  - 例如 `move-hint / constrained / single-block / global / isolated`
- `move_work_class`
  - family 级 move/coalesce 队列分类
  - 例如 `coalesce-ready / freeze-pending / released / internal`
- `phase`
  - 当前 planner 准备对这个 value 做的动作
  - `simplify / freeze / spill`

一句话：

```text
work_class
= 值长得像什么

move_work_class
= family 当前像什么 move 对象

phase
= 这一轮准备怎么处理它
```

### 2.5.3 removal_kind / phase

- `phase`
  - 当前过程中的分类
- `removal_kind`
  - 最后到底是按什么方式离开 worklist

当前 `removal_kind` 只有：

- `simplify-remove`
- `spill-remove`

所以：

- `phase` 更像过程标签
- `removal_kind` 更像已发生的动作记录

### 2.5.4 raw / effective

- `degree`
  - 按普通 value 图看的压力
- `effective_degree`
  - 按 coalesced family 修正后的压力
- `move_related`
  - 按原始 affinity 图看的 move 压力
- `effective_move_related`
  - 把 family 内部 accepted move 吃掉以后，真正剩下的外部 move 压力

一句话：

`raw 是原图视角，effective 是 coalescing-aware 视角。`

### 2.5.5 preferred / targeted / retry

- `preferred color`
  - 当前最想要的颜色
  - 可能来自：
    - `coalesce-direct`
    - `coalesce-class`
    - `affinity`
- `targeted eviction`
  - 为了保住这个 preferred color，定向挤掉唯一 blocker
- `retry`
  - 先掉进 spill，再在后续 family-aware retry 阶段尝试救回来

一句话：

```text
preferred
= 想要哪个颜色

targeted eviction
= 为了保这个颜色，优先挤谁

retry
= 先没活下来，后面再救
```

### 2.5.6 intended / confirmed / recovered

- `spill_intended`
  - 一开始是按 spill 候选摘掉的
- `spill_confirmed`
  - 最后真的 spill 了
- `spill_recovered`
  - 本来掉进 spill 了，后来又恢复成 colored

所以：

```text
spill-remove
!=
spill-confirmed
```

而：

```text
spill-intended + not spill-confirmed
```

通常就是某种 optimistic color / recovery 成功。

### 2.5.7 agenda / trace / layout

- `agenda`
  - 下一步“谁先来”的候选表
- `trace`
  - 过程里“发生了什么”的时间序列
- `layout`
  - 最终结果怎么被整理成 placement/group/report

一句话：

```text
agenda = 队列
trace = 过程
layout = 排版后的结果
```

---

## 2.6 最容易混的几组概念

如果你只想记最常见的混淆点，就记下面这几组。

### 2.6.1 `move_related` 和 `preferred color`

这两个很容易被混成一件事，但其实不一样。

- `move_related`
  - 是 planner/worklist 侧状态
  - 影响 `freeze`
- `preferred color`
  - 是 select 侧状态
  - 影响最后选哪个颜色

也就是说：

一个值完全可能：

- planner 里已经不再 `effective_move_related`
- 但 select 时仍然会去追一个 coalesced family 的 preferred color

### 2.6.2 `freeze` 和 `coalesce`

- `coalesce`
  - 是“把两组 family 真正并起来”
- `freeze`
  - 是“先别再让 move 语义继续拖着主循环走”

所以：

- coalesce 是合
- freeze 是先不继续围着 move 纠缠

### 2.6.3 `local eviction` 和 `retry recovery`

- `local eviction`
  - 发生在当前 reverse-select 那一步
- `retry recovery`
  - 发生在后续 retry field / retry phase

所以：

- 前者更像“当场补救”
- 后者更像“事后修复”

### 2.6.4 `family` 和 `merged-node allocator`

现在有 family，不等于已经是完整 merged-node allocator。

当前更准确的状态是：

- family-aware planning / select / retry
- 但最终结果仍然落回 value-level placement

所以：

`family-aware`
不等于
`fully merged-node`

### 2.6.5 `result` 和 `report`

- `result`
  - 逐值原始分配结果
- `report`
  - layout / group / summary 层的再组织工件

如果你想知道：

- `ssa.7` 最后有没有颜色

看 `result` 更直。

如果你想知道：

- 哪个函数有 recovered colors
- 哪个 spill slot 上挤了哪些值

看 `layout/report` 更省脑。

---

## 3. allocator 在分配什么

allocator 当前分配的是每个 `ssa.N` 的：

- `color`
- 或 `spill`

你可以先把 color 理解成：

`一个抽象寄存器编号`

可用颜色数由：

- `color_budget`

给出。

所以这层现在回答的问题是：

1. `ssa.X` 有没有颜色？
2. 如果有，是哪个颜色？
3. 如果没有，是不是 spill 了？
4. spill priority 是多少？
5. spill 到哪个 spill slot？

这五个结果都体现在：

- `ValueSsaAllocationResult`

里。

---

## 4. 结果表面

当前结果结构最重要的字段就是：

- `has_color[value]`
- `colors[value]`
- `spill_flags[value]`
- `spill_priorities[value]`
- `spill_slots[value]`
- `spill_intended_flags[value]`
- `spill_confirmed_flags[value]`
- `spill_recovered_flags[value]`
- `used_preferred_color_flags[value]`
- `preferred_color_sources[value]`
- `preferred_color_partner_value_ids[value]`
- `used_targeted_eviction_flags[value]`
- `targeted_eviction_blocker_value_ids[value]`
- `used_retry_eviction_flags[value]`
- `retry_eviction_blocker_value_ids[value]`

也就是说，这层已经不只是：

- “这个值分到寄存器没”

而是把：

- spill intent / confirm / recovered
- preferred-color 来源
- targeted eviction / retry eviction 痕迹

这些选择语义也显式暴露出来了。

当前查询 API 也已经齐了：

```c
int value_ssa_allocation_result_get_color(...);
int value_ssa_allocation_result_is_spilled(...);
int value_ssa_allocation_result_get_spill_priority(...);
int value_ssa_allocation_result_get_spill_slot(...);
int value_ssa_allocation_result_count_spilled_values(...);
int value_ssa_allocation_result_count_colored_values(...);
int value_ssa_allocation_result_is_spill_intended(...);
int value_ssa_allocation_result_is_spill_confirmed(...);
int value_ssa_allocation_result_is_spill_recovered(...);
int value_ssa_allocation_result_used_retry_eviction(...);
```

所以 allocator 结果现在已经不是 dump-only artifact。

### 4.1 现在还有一个 artifact-driven 入口

除了普通生产入口：

- `value_ssa_allocate_function(...)`

现在还多了一个更窄的实验/调试入口：

- `value_ssa_allocate_function_from_plan(...)`

它不是正常生产调用面，而是给 allocator policy 实验用的。

它吃的不是“从头算全部分析”，而是调用方直接给定：

- `prep`
- `interference`
- `coalesce_analysis`
- `plan`

然后只跑：

- select
- spill-slot assignment

所以它更像：

`artifact-driven select entrypoint`

这条入口特别适合：

- 锁 targeted eviction
- 锁 retry recovery
- 锁某些非常精细的 policy case

而不必每次都强迫测试绕完整个“程序形状 -> liveness -> full allocation”链。

### 4.2 现在还有 layout / placement 结果面

allocator 结果现在也不再只是一堆“每个值有没有颜色”的平铺数组了。

最近又多了一层更适合汇总/报告/工具查询的 layout surface：

- function allocation layout
- program allocation layout
- allocate+rewrite layout report

你可以先把它理解成：

`把原始 allocation result 重新整理成更稳定的 placement/summary 工件`

它会回答例如：

- 某个值最终是 `color` placement 还是 `spill-slot` placement
- 某个函数一共用了几个颜色
- 某个 spill slot 里有哪些值
- 哪些函数有 confirmed spills
- 哪些函数有 optimistic / recovered colors

所以当前 allocator 其实已经有两层结果面：

1. 原始 result
   - 面向逐值查询 / select trace
2. layout/report
   - 面向分组摘要 / placement 视图 / 跨函数汇总

粗写成伪代码就是：

```text
for each value:
    if spilled:
        placement = spill(slot)
        append to spilled list
        maybe append to confirmed-spilled list
        group by spill slot
    else if colored:
        placement = color(c)
        append to colored list
        maybe append to optimistic / recovered list
        group by color

then build:
    used-colors
    color groups
    spill-slot groups
    function summary
    program summary
    allocate+rewrite report
```

所以 layout/report 不是新的分配算法，而是：

`把 allocator 的最终结果重新整理成更像分析报表和 placement 视图的工件`

---

## 5. 这层依赖哪条分析链

这里要先把一件事讲严谨。

allocator 当前确实完整吃前面那条 shared analysis 主线，但 **shared analysis** 和 **allocator 自己的执行过程 / 输出** 不是一回事。

更准确应该拆成三层：

### 5.1 shared analysis / prep 输入层

```text
def/use
-> cfg
-> liveness
-> interference
-> copy affinity
-> allocation prep
-> allocation worklist
```

这部分回答的是：

- 值在哪里定义 / 使用
- 哪些块里活跃
- 谁和谁冲突
- 谁和谁有 copy affinity
- 哪些值更像 move-hint / constrained / isolated
- 初始 heuristic priority 是多少

也就是说，这一层是：

`allocator 的输入事实`

### 5.2 allocator 过程层

```text
allocation worklist
-> coalesce analysis
-> allocator plan
-> reverse-select coloring
```

这部分已经不是“观察程序”，而是在做 allocator 自己的动态过程：

- decide simplify / freeze / spill 候选
- decide conservative coalesced pairs / families
- 维护 active graph 状态
- 构造 remove order
- 逆序给值着色

另外，现在 allocator 旁边还多了三条“辅助主线”，它们不直接驱动 `allocate_function(...)` 的生产路径，但已经是正式公共表面：

```text
coalesce analysis
-> move family analysis
-> move worklist
-> move transition trace
```

它们的作用不是替代 plan/select，而是把：

- coalesced family 的状态
- family 级待办顺序
- family 级状态变化轨迹

变成可 query / 可 dump / 可测试的工件。

### 5.3 allocator 结果层

```text
color / has_color
spill flags
spill priorities
spill slots
optional spill rewrite
canonicalize
reallocate
```

这部分是本次分配跑出来的结果，不再是共享分析事实。

例如：

- `interference graph` 换一个 allocator 也还能复用
- `copy affinity` 换一个 allocator 也还能复用
- `spill slot = 3` 就完全是这次 allocator 跑出来的结果了

所以不能写成：

```text
def/use
-> cfg
-> liveness
-> interference
-> copy affinity
-> allocation prep
-> allocation worklist
-> allocator plan
-> coloring
-> spill slots
```

因为最后三步已经不属于 shared analysis，而属于 allocator 的过程 / 输出。

这点非常重要，因为它说明：

- `allocation_prep`
- `allocation_worklist`

现在已经不只是调试工具，而是真的成为 allocator 输入了。

---

## 5.4 跨文件主流程伪代码

如果把 allocator 当前主线按“跨文件协作”压成一个粗伪代码，大概是这样：

```text
allocate_function(function, K):
    compute def/use
    compute cfg
    compute liveness
    compute interference
    compute copy affinity
    compute allocation prep
    compute allocation worklist

    compute conservative coalesce analysis
    build family-aware allocator plan

    reverse-select colors:
        preferred color
        free color
        targeted preferred-color eviction
        generic local eviction
        else provisional/confirmed spill

    build retry family agenda
    drain retry family phases

    assign spill slots
    return allocation result
```

如果再把 high-level driver 算进去，就是：

```text
allocate_and_rewrite(program, K):
    allocate program
    if some spills are rewriteable:
        rewrite spills
        canonicalize
        reallocate
        repeat until fixed point or cap
```

所以现在这条线已经不是：

- 一个 plan
- 一个 colorer

而是：

- 分析
- coalesce/family 中层
- reverse select
- retry field/phase
- spill-slot / rewrite / relayout

串在一起的整条小后端链。

---

## 5.4.1 当前最容易误读的流程点

这里顺手把几个最常见的误读直接拆掉。

### “有 family 了，所以以后都是 family 单位”

不是。

当前更像：

- 过程越来越 family-aware
- 结果仍然落回 value-level placement

所以 family 是：

`中层控制对象`

不是：

`彻底取代 value 的唯一对象`

### “freeze 了就彻底不用 mov 了”

也不是。

更准确地说：

- planner 不再把它当成活 move 压力继续拖着走
- 但 select 仍然可能利用 preferred color / coalesced class 颜色

所以：

`freeze 关掉的是 move-side worklist 语义，不是彻底放弃同色机会`

### “local eviction 会马上给 blocker 改个新颜色”

也不是。

当前 local eviction 本身还是：

`evict-to-spill`

后面如果 blocker 又活了，那通常已经属于：

- retry
- recolor
- recovered

这已经是后续协议，不是 local eviction 本体。

### “retry 就是再扫一遍 spilled values”

现在也不对了。

当前 retry 已经更像：

- outer family agenda
- begin one phase
- drain that phase
- 记录 retry phase trace

也就是说，它已经明显比“末尾再扫一遍”更结构化了。

---

## 5.5 三条子协议的伪代码

如果再把 allocator 拆细一点，当前其实可以看成三条互相咬合的小协议：

### 5.5.1 family-aware planner 协议

```text
seed runtime family state
while values remain:
    maybe coalesce one best family pair
    else maybe freeze one family
    else remove one value from one chosen family
    record family before/after snapshot
```

这条协议更像：

`谁先 coalesce / freeze / simplify / spill`

### 5.5.2 reverse-select + retry 协议

```text
for value in reverse(plan):
    choose color if possible
    else maybe evict blocker
    else provisional/confirmed spill

build retry family agenda
while agenda has phases:
    begin one retry phase
    recover family members if legal
    maybe free-color / evict / recolor blocker
    record retry phase artifacts
```

这条协议更像：

`谁最后怎么活下来，谁最后真的 spill`

### 5.5.3 rewrite + relayout 协议

```text
if some spills are rewriteable:
    insert stores / reloads
    reuse reloads when possible
    canonicalize
    reallocate
    rebuild placement/layout/report
```

这条协议更像：

`spill 以后程序会被怎么改写，结果又会被怎么重新整理`

所以你以后看 allocator，不一定非得整条主线一口气吞掉，也可以先问自己：

- 我现在看的问题属于 planner？
- select/retry？
- 还是 rewrite/layout？

一旦先把问题归到这三条子协议里，读代码会顺很多。

---

## 6. allocation worklist 在分什么类

`value_ssa_compute_allocation_worklist(...)` 会根据 `allocation_prep` 里的摘要，把每个值归到一个稳定的启发式 work class：

- `move-hint`
- `constrained`
- `single-block`
- `global`
- `isolated`

当前分类大致可以记成：

```text
if affinity_sums[value] > 0:
    move-hint
else if interference_degrees[value] == 0:
    isolated
else if single_block_live_ranges[value]:
    single-block
else if interference_degrees[value] >= 2:
    constrained
else:
    global
```

这里有两个很关键的点。

### 6.1 这些不是“语义类别”

它们不是 IR 契约，不是 verifier 定义，也不是最终分配结果。

它们只是：

`allocator-prep 的启发式工作分类`

也就是“这个值更像哪种处理对象”。

### 6.2 判断顺序本身有意义

这是按顺序抢占分类，不是并列打标签。

所以一个值如果：

- 既有 affinity
- 又 degree 很高

那它仍然会先被放进：

- `move-hint`

因为当前分类先看 `affinity_sums > 0`。

### 6.3 每一类直觉上在说什么

- `move-hint`
  - 有 copy affinity，最好尽量同色
- `isolated`
  - interference degree 为 0，几乎不受约束
- `single-block`
  - live range 很局部，主要活在一个 block 里
- `constrained`
  - 当前粗粒度规则下 degree 已经明显不低
- `global`
  - 跨块活跃，但没被前面几类截走

注意这里的 `constrained` 不是经典着色理论里“degree >= K”的那个严格术语。

当前实现里的 `interference_degrees[value] >= 2` 只是：

`一个很粗的 heuristic bucket`

不是严谨的“高压节点”数学定义。

### 6.4 priority 怎么和 work class 配合

当前 worklist 排序会综合：

- `work_class`
- `priority`
- `value_id`

其中 priority 大致来自：

```text
affinity_sums * 8
+ interference_degrees * 4
+ live_block_counts * 2
+ use_counts
```

所以：

- work class 决定“先按哪类分桶”
- priority 决定“同一类里谁更靠前”

### 6.5 现在还有一个 `rematerializable` 维度

最近 allocator-prep 里还多了一层：

- `rematerializable`

你可以先把它理解成：

`这个值如果丢掉颜色，将来是不是比较容易重新 materialize`

它现在还不是一个完整的 rematerialization framework，但已经开始进入 allocator tie-break：

- plan 的 spill-phase 比较里会把 `rematerializable` 作为一个偏好
- select / eviction 的候选选择里也会把它当成一个“更容易被牺牲”的信号

所以当前 allocator 不只是看：

- degree
- priority
- spill_cost

也开始看：

- 这个值是不是更像一个“丢掉也没那么疼”的值

最近还多了一个相邻维度：

- `split_child_value`
- `split_child_depth`

你可以先把它理解成：

`这个值是不是更像某个 split / 派生 family 里的后续孩子，以及它离原始 split 根有多深`

当前它主要还在：

- planner tie-break
- retry family agenda 的代表项摘要

这说明 allocator 现在已经不只是“整值看压力”，也开始给未来更正式的 splitting / retry family policy 留位了。

---

## 7. allocator plan 在做什么

最近这条线最关键的推进，不是“再调一点 priority”，而是：

`value_ssa_compute_allocator_plan(...)`

已经开始像一个小型图着色 allocator 的 remove/freeze 状态机。

### 7.1 先把三层概念拆开

allocator 这里最容易混掉的，是下面三种东西：

1. `work class`
   - `move-hint`
   - `constrained`
   - `single-block`
   - `global`
   - `isolated`
2. `phase`
   - `simplify`
   - `freeze`
   - `spill`
3. 动态内部状态
   - `scheduled[]`
   - `frozen[]`
   - `active_degree[]`
   - `active_move_related[]`

这三者不是一回事。

- work class：静态启发式分组
- phase：当前这一步准备怎么处理它
- 动态状态：支撑 phase 判断和状态传播的内部运行时数据

尤其要注意：

- `freeze` 不是 work class
- `move-hint` 不是 phase
- `active_degree` 也不是值的固有属性

### 7.2 它维护哪些动态状态

当前内部维护：

- `scheduled[]`
  - 哪些值已经被摘下来了
- `frozen[]`
  - 哪些值已经执行过 freeze
- `active_degree[]`
  - 当前活图里的度数
- `active_move_related[]`
  - 当前活 affinity 图里是不是还 move-related

所以它不是一次性给每个值贴个永远不变的标签，而是在模拟：

`图在 remove / freeze 之后会怎么变化`

你可以把这四个量先粗记成：

- `scheduled`
  - 这个值已经被正式摘进 remove order 了
- `frozen`
  - 这个值已经执行过 freeze，不再参与活 move-related 关系
- `active_degree`
  - 当前活干涉图里的度数
- `active_move_related`
  - 当前活 affinity 图里是不是还算 move-related

### 7.3 当前 phase 是怎么分的

当前分类规则非常直接：

```text
if active_move_related && active_degree < color_budget:
    phase = freeze
else if active_degree >= color_budget:
    phase = spill
else:
    phase = simplify
```

也就是：

- **simplify**
  - 度数够低
  - 而且当前已经不是活 move-related
- **freeze**
  - 度数够低
  - 但仍然活 move-related
- **spill**
  - 当前 active degree 已经不小于颜色预算

### 7.4 当前不是静态分类器，而是真状态机

plan builder 大致按下面这样运行：

```text
while still have unscheduled values:
    recompute active_degree
    recompute active_move_related

    if there is a simplify candidate:
        record simplify-remove
        mark scheduled
        continue

    if there is a freeze candidate:
        mark frozen
        recompute active_move_related
        continue

    record spill-remove
    mark scheduled
```

关键在于：

- `freeze` 不再只是 phase label
- 它真的会修改 active move-related 状态
- 有些值会从 `move_related=yes` 变成 `move_related=no`
- 然后真正 thaw 成 simplify 候选

所以现在 dump 里出现：

- `degree=initial->active`
- `move_related=yes->no`

不是显示层花活，而是状态机真实跑过后的结果。

### 7.5 为什么这里不直接 simplify，而要先 freeze

这个问题非常关键。

如果一个值：

- `active_degree < color_budget`
- 但 `active_move_related = yes`

那从“图着色 legality”角度看，它很多时候并不是不能 simplify。

真正的区别是：

- `freeze` 只把它从活 move/affinity 关系里拿掉
- `simplify` 会把它从活干涉图里整个摘掉

也就是：

```text
freeze:
    frozen[value] = 1
    recompute active_move_related

simplify:
    scheduled[value] = 1
    recompute active_degree
    recompute active_move_related
```

所以两者的差别不是“小动作和大动作都差不多”，而是：

- freeze 不降低别人的 `active_degree`
- simplify 会真的降低别人的 `active_degree`

这就是为什么“关掉 move 关系”和“直接 simplify 节点”不是一回事。

### 7.6 那 freeze 现在到底有什么用

这个也要诚实一点讲。

当前实现里，freeze 的直接收益还没有经典 coalescing allocator 里那么大，因为现在还没有：

- 完整 coalescing 主循环
- 更强的 spill-cost / retry 语义
- freeze 作为更重的独立 remove kind

所以当前很多 case 确实会呈现出：

```text
freeze
-> recompute active_move_related
-> 很快就变成 simplify
```

这个直觉没有错。

但它现在仍然有几个很实在的作用：

1. 把“取消 move 关系”和“真正 remove 节点”分开
2. 避免因为过早 simplify 而让邻居 `active_degree` 过早下降
3. 让 `active_move_related=yes->no` 这条状态传播真实发生
4. 让 allocator 从“静态排序器”变成一个真的小状态机

所以它现在最大的价值更偏：

`结构收益 / 语义收益`

而不只是“立刻减少很多 spill”。

### 7.7 removal kind 现在也开始记录了

当前 plan item 不只记：

- `phase`

还记：

- `removal_kind`

当前已有：

- `simplify-remove`
- `spill-remove`

这很重要，因为它说明 allocator 已经开始区分：

- 这个值是被当 simplify 摘掉的
- 还是被当 spill 候选摘掉的

这会直接帮助后面把 select stack / spill semantics 再往前推。

---

## 8. 当前启发式怎么做

### 8.1 先选哪一类 phase

phase 优先级固定是：

1. `simplify`
2. `freeze`
3. `spill`

也就是：

- 能 simplify 就绝不先 spill
- 能 freeze 把图继续做松，就先 freeze

### 8.2 `choose` 不是直接拿 worklist 第一个

plan builder 每一轮都会扫完整个 worklist 里所有还没 `scheduled` 的值，然后在当前候选里选一个“最优”的。

它不是：

```text
扫到第一个 simplify 候选就直接拿
```

而更像：

```text
for each unscheduled value in worklist:
    classify current phase from active state
    compare against current best simplify/freeze/spill candidate

if any simplify exists:
    choose best simplify
else if any freeze exists:
    choose best freeze
else:
    choose best spill
```

所以它和 worklist 的关系是：

- worklist 提供初始桶和基础优先级
- plan builder 每一轮再从“当前所有候选”里重选一个最佳者

### 8.3 同类候选里的 tie-break

当前大致会看：

- `work_class`
- `priority`
- `active_degree`
- `value_id`

更准确地说：

- 同 phase 里会先比较 `work_class`
- spill 候选更偏向 `active_degree` 高
- freeze 候选更偏向 `active_degree` 低
- simplify 候选不额外插一个 degree 比较

当前 `work_class` 的优先顺序可以直接记成：

```text
move-hint
-> constrained
-> single-block
-> global
-> isolated
```

也就是：

- `move-hint` 最先
- `isolated` 最后

这和很多人直觉里“最简单的 isolated 应该最先清掉”不一样。

当前实现更偏向：

`先处理更值得 allocator 关注的值，而不是先处理最无聊的边缘值`

原因大致是：

- `move-hint` 带着 copy-affinity 信息，更值得围着它组织 remove/select 顺序
- `constrained` 图压力更明显，也更值得尽早纳入决策
- `isolated` 几乎没压力，晚一点处理通常也不吃亏

所以这里的偏向不是“最容易先做”，而是：

`更有结构价值的先做`

所以这套启发式的特点是：

- 保守
- deterministic
- 容易通过 dump 和测试解释

它还不是完整 spill-cost model，但非常适合当前阶段。

---

## 9. 为什么是 reverse-select coloring

plan 记录的不是“着色顺序”，而是：

`remove order`

也就是“这些值是按什么顺序从活图里被摘掉的”。

真正分颜色的时候，要做的是：

`按相反顺序把它们一个个放回来`

所以 coloring 必须是：

`reverse(plan)`

### 9.1 最小直觉

如果一个点 `v` 在被 simplify-remove 时满足：

- `degree(v) < K`

那这个结论真正依赖的是：

> 先把摘掉 `v` 之后剩下的图着色，再把 `v` 放回来，总还能给 `v` 找到一个颜色。

而“摘掉 `v` 之后剩下的图”，正是那些：

- 比 `v` 更晚被摘掉的点

所以 select 必须逆序。

### 9.2 最小例子

假设有一条链：

```text
a -- b -- c
```

可能 remove 顺序是：

```text
[a, b, c]
```

那 coloring 就要按：

```text
c -> b -> a
```

因为 `a` 最早被摘掉时，它“以后总能着色”的保证，是建立在“后面先把 `b/c` 这部分着好色”的前提上。

### 9.3 当前实现里 reverse-select 在做什么

当前主线就是：

1. 先构造 remove order / allocator plan
2. 再按 plan **逆序**给值选颜色

也就是：

```text
for value in reverse(plan):
    mark all interfering colored neighbors as blocked
    try preferred color from affinity neighbors
    else choose first available color
    else try local eviction
    else mark spilled
```

这已经非常像经典图着色 allocator 的：

- remove stack
- reverse select

只是当前还没把它完全命名成正式 select stack 而已。

---

## 10. affinity-biased color preference

当前 color 选择不是纯“第一个没挡住的颜色”，而是先看：

- copy-affinity 最强的已着色邻居
- 它的颜色当前是否可用

如果可用，就优先拿那个颜色。

所以 copy-affinity 现在已经不只停在分析表面，而是真正进入：

`allocator decision`

里了。

这是一种很保守的 coalescing-preference：

- 不做显式 coalescing
- 但会尽量偏向同色

到最近这批实现，这条 preferred-color 语义已经不只是一句“尽量同色”了，而是有了明确来源：

- `coalesce-direct`
- `coalesce-class`
- `affinity`

也就是说，当前一个值如果用了 preferred color，结果层现在已经能回答：

- 它为什么用了这个颜色
- 这个颜色是从哪个 partner 来的
- 它是直接 pair coalesce，还是来自整个 class 的颜色偏好

这让 select 不再只是输出“最后颜色是什么”，而是开始输出：

`为什么是这个颜色`

---

## 11. `spill-remove` 不等于最终 spill

这里也必须单独拆开。

plan 阶段里的：

- `spill-remove`

表示的是：

`这个值在 remove order 里是作为 spill 候选被摘掉的`

但这不等于最终一定 spill。

当前实现里，select/coloring 阶段还会继续尝试：

1. affinity 偏好的颜色
2. 任意空颜色
3. local eviction

只有这些都失败了，才会进入最终 spill。

所以现在结果里有两个很重要的标记：

- `spill_intended`
  - 当初是按 `spill-remove` 摘掉的
- `spill_confirmed`
  - 最终真没颜色，真的 spill 了

这意味着：

```text
spill-remove
!=
final spill
```

也就是说，一个值完全可能：

- `spill_intended = yes`
- `spill_confirmed = no`

也就是：

`本来按 spill 候选处理，但最后还是成功拿到了颜色`

这就是当前 dump 里所谓的：

`optimistic-colored`

---

## 12. 没颜色时怎么办

当前不是“当前值没空颜色就直接 spill 当前值”。

它还会尝试一个很小的本地驱逐规则：

- 找一个已经着色的干涉邻居
- 这个邻居优先级比当前值低
- 并且它必须是那个颜色的**唯一 blocker**

如果满足，就：

- 驱逐那个邻居
- 当前值接管这个颜色
- 被驱逐者标成 spill

这个规则的意义是：

- 比最原始的 greedy spill 强
- 但又比真正复杂的 eviction/coalescing 保守很多

所以当前更像：

`small local eviction refinement`

### 12.1 “没颜色”为什么会发生

这不是理论边角，而是分配问题本身。

如果颜色预算是 `K`，而当前值的已着色干涉邻居正好把 `0..K-1` 都占满了，那它此刻就真的没有可用颜色。

最小例子：

- 只有 2 个颜色：`R0`, `R1`
- 当前值 `v`
- 邻居 `a` 拿了 `R0`
- 邻居 `b` 拿了 `R1`

那 `v` 现在就没有空颜色。

### 12.2 local eviction 到底是什么

它发生在 select/coloring 阶段，不是 plan 阶段。

可以把它记成：

```text
给当前值 v 选颜色
if 有空颜色:
    直接用
else:
    看看能不能驱逐一个已着色邻居 u
    if 能驱逐:
        u 改成 spill
        v 接管 u 的颜色
    else:
        v 自己 spill
```

### 12.3 为什么叫 local

因为它非常局部，只做一跳范围内的判断：

- 只看当前值的干涉邻居
- 只试图抢一个已经存在的颜色
- 不做连锁式重着色
- 不重建整个 plan

### 12.4 它和 spill 的关系

local eviction 不是独立于 spill 的东西。

它本质上是：

`在本来快要 spill 的那个时刻，先试一次很保守的补救`

如果成功：

- 当前值保住颜色
- 被挤掉的低优先级邻居 spill

如果失败：

- 当前值自己 spill

所以它就是 spill 边界上的一个小优化。

### 12.5 当前不会给被驱逐邻居重新着色

这也是现在实现边界里一个很重要的点。

当前 local eviction 做的不是：

- 把邻居从颜色 `c0` 挪到另一个空颜色 `c1`
- 或者触发一串链式 recoloring

它做的是更保守的版本：

```text
如果邻居 u 是某个颜色的唯一 blocker
并且 u 的优先级更低
那么：
    撤掉 u 的颜色
    把 u 标成 spill
    当前值 v 接管 u 原来的颜色
```

所以当前 allocator 的 local eviction 本质上是：

`evict-to-spill`

不是：

`evict-and-recolor`

这意味着：

- 它会改变“谁 spill”
- 但不会给被驱逐邻居再找一个新颜色
- 也不会做更复杂的全局重着色

这里要补一句边界：

- **local eviction 本身** 仍然是 `evict-to-spill`
- 但 **select 后续新增的 retry 层**，现在已经可以把某些先前 spilled 的值再救回颜色里

所以：

- 不是“驱逐时立刻给邻居改色”
- 而是“先驱逐成 spill，后续如果 retry 条件成立，再把某些 spilled value 恢复成 colored”

这就是为什么当前它仍然是一个：

`small local eviction refinement`

而不是完整 eviction / recoloring allocator。

---

## 13. spill slot 现在怎么分

当前 spilled values 不只会被打 `spill_flag`，还会进一步分：

- `spill_slot`

而且分法不是简单按 value_id 编号，而是：

- spilled values 之间如果互相干涉，就不能共用 spill slot
- 不干涉的 spilled values 可以复用同一个 spill slot

所以现在 allocator 已经开始回答：

- 谁 spill
- spill 到哪里

而不只是“spill=yes/no”。

---

## 14. `spill_priority` 和 `spill_slot` 到底是什么

### 14.1 `spill_priority` 不是 shared analysis 输出

`spill_priority` 容易让人误会成“前面 analysis 已经算好的最终答案”。

更准确的说法是：

- 它来自前面 `allocation_prep / allocation_worklist` 的 heuristic 信息
- 但它是在 allocator 结果里被重新暴露出来的

所以它最合适的身份是：

`analysis-derived heuristic metadata, consumed and re-exposed by allocator`

它不是：

- shared analysis 的最终公共结论

也不是：

- allocator 的唯一决策依据

它更像：

- allocator 输入的一部分启发式权重
- 同时被 allocator 结果带出来，方便 dump / 查询 / rewrite

这里还要和 `spill_cost` 区分开。

- `spill_priority`
  - 更像 worklist / result 层的总体优先级信号
- `spill_cost`
  - 更像 planner/select 在“谁更该被牺牲”时专门看的代价模型

### 14.1.1 `spill_cost` 现在已经比早期版本细很多

当前 planner item 里已经会带：

- `spill_use_cost`
- `spill_use_loop_depth_cost`
- `spill_use_call_density_cost`
- `spill_live_range_cost`
- `spill_live_block_cost`
- `spill_loop_depth_cost`
- `spill_call_density_cost`
- `spill_call_crossing_cost`
- `spill_cross_block_cost`
- `spill_affinity_cost`

所以现在的 `spill_cost` 已经不再只是一个小总分，而是开始把：

- use 次数
- loop 深度
- call 密度
- live block 覆盖
- 跨 block / 跨 call 压力
- affinity 粘性

拆成多个子项记录。

这还不等于“full spill-cost model”，但已经明显比最早那版：

```text
uses + blocks + affinity
```

更厚了。

### 14.2 `spill slot` 是什么

`spill slot` 可以先理解成：

`当一个 SSA 值没能留在颜色里时，给它找的那个临时栈槽编号`

当前这层还没进入真实机器寄存器 / 栈帧建模，所以这里的 slot 先是抽象的 spill local / spill storage 编号。

后续 rewrite 会把它落实成类似：

```text
store_local spill.N, ssa.X
...
ssa.Y = load_local spill.N
```

也就是说：

- color = 留在“抽象寄存器”里
- spill slot = 落到“抽象栈槽 / spill local”里

---

## 15. spill rewrite 现在已经能真实改写 SSA

这条线现在也不再只是理论层了。

当前 spill rewrite 主线会对 spilled value 做：

1. 在 def 后插 `store_local spill.N, ssa.X`
2. 在每个 use 前插 `load_local spill.N`
3. 把 use 改成读 reload 出来的值

也就是标准的：

`store after def + reload before use`

### 15.1 当前支持哪些 use

现在已经支持：

- ordinary instruction uses
- terminator uses
  - `ret`
  - `br` condition
- phi-input uses
  - 必要时还会 split edge

所以它已经是一个真正的：

`allocation result -> IR rewrite`

闭环了。

### 15.2 当前还有哪些 reuse 优化

它也不是每次都盲插 reload。

当前会先尝试：

- 同块复用已有 reload
- 沿唯一前驱链复用 reload
- 某些 `mov` use 原地改成 `load_local`

所以 rewrite 也已经开始有一点本地优化，而不是纯机械插桩。

---

## 16. coalescing 和当前 allocator 的关系

`coalescing` 一般是指：

`看到 mov/copy 关系后，尽量让两端值分到同一个颜色/位置，从而把 move 变成免费甚至直接消掉`

最小例子：

```text
ssa.0 = ...
ssa.1 = mov ssa.0
```

如果最后：

- `ssa.0 -> color 0`
- `ssa.1 -> color 0`

那这个 `mov` 基本就白送了。

### 16.1 当前还没有完整 coalescing

现在这版 allocator 还没有真正做：

- merge nodes
- explicit coalescing mainline
- full simplify/coalesce/freeze/spill classic loop

### 16.2 但已经在做 coalescing-preference

现在已经有几块明显是在给 coalescing 打地基：

- `copy affinity graph`
- `move-hint` work class
- affinity-biased preferred color
- freeze-driven move handling

所以当前最准确的说法是：

`not full coalescing, but clearly coalescing-aware`

---

## 17. 一条贯穿式状态流

把最近这些新概念串起来，最清楚的方式就是看一条完整状态流：

```text
allocation_prep
-> allocation_worklist
-> allocator_plan
    -> phase=simplify/freeze/spill
    -> removal_kind=simplify-remove/spill-remove
-> reverse select
    -> preferred color / first available / local eviction
    -> spill_intended?
    -> spill_confirmed?
    -> retry family agenda / retry phase
-> spill slot assignment
-> spill rewrite
-> canonicalize
-> reallocate
```

这条线里每一层回答的问题都不一样：

- `work_class`
  - 这个值像哪类处理对象
- `phase`
  - 当前这一步更像 simplify / freeze / spill 哪类动作
- `removal_kind`
  - 它最后是按 simplify 还是按 spill 意图离开 worklist
- `spill_intended`
  - reverse-select 前，它是不是按 spill-remove 下来的
- `spill_confirmed`
  - reverse-select 后，它是不是真的没颜色、确认 spill 了

如果一个值：

- `spill_intended = yes`
- `spill_confirmed = no`
- `has_color = yes`

那它就是：

`optimistic-colored`

也就是：

`本来按 spill 候选处理，但最后还是成功着色了`

如果把当前 select + retry 主线粗写成伪代码，可以先记成：

```text
for value in reverse(plan):
    try preferred color
    else try any free color
    else try targeted preferred-color eviction
    else try generic local eviction
    else mark spill-confirmed

build retry family agenda from current spilled values
while retry family agenda is non-empty:
    begin one retry family phase
    recover one or more members from that family if legal
    maybe use:
        free color
        preferred-evict
        generic-evict
        blocker recolor
    record retry phase metadata
```

这说明当前已经不是：

`reverse select 一次跑完就结束`

而是：

`reverse select -> retry field -> retry family phases`

---

## 18. 两个贯穿式小例子

一个例子很难同时把所有概念都讲透，所以这里分成两个：

1. 一个最小 `2-color / 3-value` 例子，讲 confirmed spill / spill slot / rewrite
2. 一个更贴近当前测试语义的例子，讲 `spill-intended -> optimistic-colored`

### 18.1 例子 A：2 个颜色，3 个两两干涉的值

假设颜色预算只有 2：

- `color 0`
- `color 1`

又有三个值：

- `a`
- `b`
- `c`

而且三者互相都干涉，也就是一个 3-clique：

```text
a -- b
 \  /
  c
```

那直觉上就已经知道：

`2 个颜色不够给 3 个互相冲突的值全部上色`

#### plan 阶段

plan 可能会把其中某个值标成：

- `remove=spill-remove`

意思是：

`这个值先按高风险 spill 候选摘掉`

注意，这时还不是最终 spill。

#### reverse-select 阶段

当这个值被放回来时，它会先尝试：

1. affinity 偏好的颜色
2. 任意空颜色
3. local eviction

但因为这是个 3-clique，而且只有 2 色，最后还是不够。

于是它会进入：

- `spill_intended = yes`
- `spill_confirmed = yes`
- `spill_flags = yes`

这就是真正的 confirmed spill。

#### spill slot 阶段

确认 spill 以后，它还需要一个 spill slot。

所以结果里不只是：

- `spill=yes`

还会进一步出现：

- `spill slot = N`

#### rewrite 阶段

后面 rewrite 时，这个 spilled value 会被改写成：

```text
def value
-> store_local spill.N, value

use value
-> load_local spill.N
-> 用 reload 出来的 SSA 值继续算
```

所以这个最小例子主要帮助你抓住：

- `spill-remove` 不等于最终 spill
- 真 spill 之后还会分配 `spill slot`
- rewrite 会把它变成 `store_local spill.N` / `load_local spill.N`

### 18.2 例子 B：本来按 spill 候选处理，但最后活下来了

现在再看另一种更贴近当前 allocator 测试的形状：

- 某个值在 plan 里是 `remove=spill-remove`
- 但 reverse-select 时，前面已经着好色的邻居布局让它其实还能拿到颜色

这时状态会变成：

- `spill_intended = yes`
- `spill_confirmed = no`
- `has_color = yes`

也就是：

`optimistic-colored`

这个例子的重点不在于“它一定来自某个特定图形”，而在于你要理解：

`spill-remove 只是 plan 的风险判断，不是最终判死刑`

reverse-select 还是可能把它救回来。

### 18.3 如果 local eviction 参与了，会发生什么

再把这条线和 local eviction 接起来看。

假设当前值 `v` 在 reverse-select 时没有空颜色，但它发现：

- 邻居 `u` 已着色
- `u` 的优先级更低
- `u` 是某个颜色的唯一 blocker

那当前实现会做：

```text
u: 取消颜色，标记为 spill
v: 接管 u 原来的颜色
```

注意这里仍然不会：

- 给 `u` 再找一个新颜色
- 做连锁 recoloring

所以如果你把这件事放进整条状态流里，它更像：

```text
v 本来快要 confirmed spill
-> 尝试 local eviction
-> 成功则 v 被救回，u 改成 spill
-> 失败则 v 自己 confirmed spill
```

这就是 current allocator 里“本地驱逐”的真正地位：

- 它会改变“谁 spill”
- 但不会做更复杂的重着色

---

## 18.4 targeted eviction 和 retry recovery 现在也已经落地了

前面讲的 local eviction 还是一个比较朴素的版本：

- 没颜色
- 找个低优先级唯一 blocker
- 驱逐掉

现在 select 又往前推了一层，主要是两件事：

### 18.4.1 targeted preferred-color eviction

如果当前值有一个 preferred color，比如：

- 直接 coalesce partner 的颜色
- coalesced class 里别的成员的颜色
- affinity 邻居的颜色

而且：

- 这个 preferred color 没有空着
- 但只被一个更低优先级 blocker 挡住

那 allocator 现在可以优先尝试：

`为了保住 preferred color，定向把那个 blocker 挤掉`

这和更早的 generic eviction 不一样。

generic eviction 更像：

- 随便找一个能腾出来的颜色

targeted eviction 则更像：

- 我已经知道最想要哪个颜色
- 我先专门试着保这个颜色

### 18.4.2 retry / recovered

当前 select 还多了一层真正的 retry 语义：

- 某个值一开始是 `spill-intended`
- 但后来因为 free color / preferred-color eviction / generic eviction，又重新拿到了颜色

这时结果层不只会说：

- `spill_intended = yes`
- `spill_confirmed = no`

还会进一步记录：

- `spill_recovered = yes`

所以现在已经可以区分：

- `optimistic-colored`
  - 按 spill 候选摘掉，但最后 colored
- `optimistic-recovered`
  - 按 spill 候选摘掉，而且是通过后续 retry/recovery 路线救回来的
- `recovered-colored`
  - 不是 spill-intended，但经过 retry 路线重新得到颜色

这些状态现在都已经进入：

- allocation result
- select stats
- select trace

也就是说，当前 allocator 已经不只是：

`能不能染上`

而是开始回答：

`这次是直接染上的，还是从 spill 边缘又救回来的`

---

## 19. 现在应该怎么看 allocator 的几种输出

最近这几轮 allocator 不只是“结果更多了”，而是观察面已经分成了几层。

如果你在调 allocator，最推荐的读法不是只盯一个 dump，而是按下面顺序看：

1. `allocator plan dump`
2. `allocation result dump`
3. `select stats`
4. `select trace`

### 19.1 plan dump 看什么

plan dump 主要回答：

- remove order 是怎么形成的
- 每个值最后是 `phase=simplify/freeze/spill` 哪类
- 它是 `remove=simplify-remove` 还是 `remove=spill-remove`
- `degree=initial->active`
- `move_related=yes->no`
- `effective_degree=initial->active`
- `effective_move_related=yes->no`
- `coalesce_root / class_size`
- `move_work_class -> move_work_next_class`
- family move pressure before/after

所以它最适合看的是：

- freeze/thaw 有没有真的发生
- coalesced family 有没有把类内 move 压力吃掉
- family 级 move-work 状态是 `coalesce-ready`、`freeze-pending`、`released` 还是 `internal`
- 哪些值最后是按 spill 意图离开的
- 当前 heuristic 在 worklist 上先挑了谁

一句话讲：

`plan dump 看“为什么这样摘”`

### 19.2 allocation result dump 看什么

allocation result dump 主要回答最终结果：

- 哪些值有颜色
- 颜色是多少
- 哪些值 spill
- spill slot 是多少
- 哪些着色值其实带着 `spill-intended`
- 哪些 spill 是 `spill-confirmed`
- 哪些值是 `spill-recovered`
- 哪些值用了 preferred color
- 哪些值发生过 targeted eviction / retry eviction

所以它最适合看的是：

- allocator 最终给了什么结果
- spill slot 分配出来没有
- 某个值是普通 colored，还是 optimistic-colored

一句话讲：

`allocation result dump 看“最后分成了什么”`

### 19.3 select stats 看什么

select stats 是摘要层，不告诉你具体是谁，但会告诉你数量：

- `colored_count`
- `spilled_count`
- `spill_intended_count`
- `spill_confirmed_count`
- `spill_recovered_count`
- `retry_eviction_recovered_count`
- `optimistic_direct_count`
- `optimistic_recovered_count`
- `optimistic_colored_count`

它最适合干的事是：

- 改 heuristic 前后做快速对比
- 看某轮 policy 调整有没有把 optimistic-colored 变多
- 看 spill-confirmed 有没有下降

一句话讲：

`select stats 看“总体发生了多少”`

### 19.4 select trace 看什么

select trace 是最近最值钱的新观察面，因为它把 reverse-select 的逐值结果直接打印出来了。

它会按 reverse-select 顺序显示：

- `remove=simplify-remove / spill-remove`
- `outcome=colored / optimistic-colored / recovered-colored / optimistic-recovered / spill-confirmed / spill`
- `preferred=coalesce-direct / coalesce-class / affinity`
- `targeted-evict=ssa.X`
- `retry-evict=ssa.X`
- 如果有颜色，打印 `color=N`
- 如果 spill，打印 `slot=N`

所以它最适合回答：

- 哪个具体值是 optimistic-colored
- 哪个具体值是真正 confirmed spill
- reverse-select 到底是按什么顺序处理的
- 某个 `spill-remove` 值到底是死了，还是最后被救回来了

一句话讲：

`select trace 看“谁在 select 阶段发生了什么”`

#### 19.4.a 一行 select trace 怎么拆

看到类似：

```text
ssa.0 remove=spill-remove ... outcome=optimistic-recovered preferred=coalesce-class(ssa.4) retry-evict=ssa.7 color=1
```

可以按这个顺序读：

1. `remove=spill-remove`
   - 它最初是高风险 spill 候选
2. `outcome=optimistic-recovered`
   - 它不是直接成功，而是后来被救回来的
3. `preferred=coalesce-class(ssa.4)`
   - 恢复时它仍然在追 family 颜色
4. `retry-evict=ssa.7`
   - 这次 recovery 还用到了 retry 阶段的 eviction
5. `color=1`
   - 最终落到颜色 1

所以你读 trace 时，不要第一眼只看 `color=1`。

更有信息量的通常是前面这几个词：

- 它最初是什么 remove kind
- 它是直接成功还是 recovered
- preferred color 从哪来
- 有没有 eviction

### 19.4.1 现在还多了第五种输出：move transition trace

前面四种输出主要围着：

- plan
- final result
- select 摘要
- select 逐值过程

最近又多了一个专门看 family 级 move 状态变化的输出：

- `move transition trace`

它回答的问题不是：

- 某个值最后什么颜色

而是：

- 某个 coalesced family 是在哪一步 `freeze`
- 是哪一步 `remove`
- move-work class 是怎么从 `freeze-pending -> internal`
  或 `released -> released` 变化的
- family active members 是怎么从 `1 -> 0` 的

所以它最适合在你调：

- coalescing-aware freeze
- family move pressure
- move worklist / planner 协同

这些层次时看。

一句话讲：

`move transition trace 看“family 级 move 状态是怎么一步步变的”`

### 19.4.2 现在还有 layout / report 输出

除了这些更偏“过程”的 dump，现在 allocator 还多了一层更偏“结果排版 / placement 摘要”的输出：

- function allocation layout
- program allocation layout
- allocate+rewrite layout report

这层和 raw result 的区别是：

- raw result 更适合逐值查：
  - `ssa.X color=?`
  - `ssa.Y spill slot=?`
- layout/report 更适合分组看：
  - 某个 color group 里有哪些值
  - 某个 spill slot group 里有哪些值
  - 哪些函数有 colors / spills / confirmed spills / recovered colors

一句话讲：

`layout/report 看“把最终结果整理成了什么 placement 视图”`

### 19.5 一个实际读法

如果你怀疑某个 case “为什么最后还是 spill 了”，最顺手的读法通常是：

1. 先看 plan dump
   - 它是不是 `remove=spill-remove`
   - freeze/thaw 有没有发生
2. 再看 select trace
   - 它在 reverse-select 时 outcome 是什么
   - 有没有被 local eviction 救回
3. 再看 allocation result dump
   - 最终 slot 是多少
4. 最后看 select stats
   - 这只是个个例，还是整批值都在 confirmed spill

如果你怀疑某个 family：

- 为什么明明 coalesced 了还会 freeze
- 为什么会从 `coalesce-ready` 变成 `internal`
- 为什么轻 family 比重 family 先 freeze

那更顺手的读法是：

1. 先看 plan dump
2. 再看 move worklist dump
3. 最后看 move transition trace

如果你怀疑某次 heuristic 改动“是不是让 allocator 更 aggressive 了”，那读法反过来更快：

1. 先看 select stats
2. 再抽样看 select trace
3. 最后回 plan dump 对原因

### 19.6 这几种输出不要混着理解

这几个输出最容易混的地方是：

- plan dump 不是最终结果
- allocation result dump 不是过程解释
- select stats 不是逐值 trace
- select trace 不是 heuristic 原因本身

也就是说：

- `plan` 负责讲“怎么摘”
- `result` 负责讲“最后怎样”
- `stats` 负责讲“多少个”
- `trace` 负责讲“哪个值经历了什么”
- `move transition trace` 负责讲“family 状态怎么变”

把这几层分开看，allocator 这条线就不会再像一团毛线球。

---

## 20. coalescing-aware family 层现在已经走到哪里

如果把最近这批 allocator 改动压缩成一条线，最重要的是：

allocator 已经不再只是“pair coalesce + select 偏色”，而是开始有一整条 family-aware 的中层：

```text
copy affinity
-> coalesce pair analysis
-> coalesced class / representative
-> plan-aware effective pressure
-> move family analysis
-> move family worklist
-> move family transition trace
```

这几层分别在干不同的事：

### 20.1 coalesce analysis

讲的是：

- 哪些 pair `can_coalesce`
- 最终每个值属于哪个 `root`
- `class_size` 是多少

它负责：

`pair -> family`

### 20.2 family-aware plan

讲的是：

- `effective_degree`
- `effective_move_related`
- `coalesce_root_value_id`
- `coalesce_class_size`
- `move_work_class`
- `move_work_next_class`

它负责：

`让 planner 真正看见 coalesced family`

最近这层最关键的语义变化就是：

- family 内部 accepted move edges 不再继续把值拖去 freeze
- family 外部的 move pressure 才算真的 effective move-related
- freeze 已经开始按 family 粒度传播
- plan 现在还会携带 `final_runtime_coalesce_roots / class_sizes`
  这类动态 merged-family 结果，后面的 select 不再只能依赖“最初分析时的静态 family”

也就是说，当前 plan 已经不只是：

- removal order

也开始变成：

- one-step-later consumers 可以读取的 runtime alias artifact

如果把当前 family-aware plan 粗写成伪代码，可以先记成：

```text
build coalesce families
seed active value-level pressure
seed active family-level pressure

while there are unscheduled values:
    compute effective degree / effective move-related per value
    compute move-work class / priority per family
    choose one representative candidate per family
    compare family representatives globally

    if chosen phase is coalesce:
        merge the two families in runtime alias state
        refresh affected family/value pressure
        continue

    if chosen phase is freeze:
        freeze the whole chosen family
        refresh affected family/value pressure
        continue

    if chosen phase is simplify or spill:
        schedule one concrete value
        refresh affected family/value pressure
        record before/after family state snapshot
```

这里最重要的变化是：

- 已经不是“全 worklist 里哪个单值最好”
- 而是“先在每个 family 里选一个代表，再让 family 级候选竞争”

### 20.3 move family analysis

讲的是：

- 这个 family 当前更像 `internal` / `released` / `frozen`
- 它的 external move-neighbor count / affinity weight 是多少
- 它在 plan 里更多经历 simplify-remove 还是 spill-remove

它负责：

`family 的静态摘要`

### 20.4 move family worklist

讲的是：

- 这个 family 当前属于
  - `coalesce-ready`
  - `freeze-pending`
  - `released`
  - `internal`
- 它的 family-level priority 是多少

它负责：

`family 的排序 / 待办队列`

### 20.5 move transition trace

讲的是：

- 哪一步 freeze
- 哪一步 coalesce
- 哪一步 remove
- coalesce 时双方 family 是谁
- move work class before/after
- family members / external pressure before/after

它负责：

`family 状态变化的事件轨迹`

所以这层现在已经不只是：

- “这个 family 最后是不是 frozen / internal”

而是能显式回答：

- 哪个 `coalesce` 事件真的发生了
- 合并前后双方 family 的压力怎么变
- 后面的 `remove` 究竟是在新 merged runtime family 下发生的，还是在旧 family 下发生的

### 20.5.1 coalesce opportunity agenda

最近还多了一个更前置的 family-level surface：

- `coalesce opportunity agenda`

它不是在问：

- 某个 pair 理论上能不能并

而是在问：

- 在当前 family/worklist 状态下
- 哪些 family-pair 更像“下一步真的值得 coalesce 的机会”

所以它会按 family pair 记录例如：

- `lhs_root <-> rhs_root`
- 双边是不是互相把对方当 best partner
- 两边 preference weight
- 两边 move-work class
- 两边 move-work priority
- 总 priority / mutual-best 情况

这层的作用可以先记成：

`pair-level can_coalesce -> family-level coalesce agenda`

它是从“能不能并”往“先并谁”跨的一步。

粗写成伪代码就是：

```text
for each family F:
    if F has a best coalesce-ready partner G:
        record one opportunity pair (F, G)

merge symmetric F<->G facts into one agenda item
score by:
    mutual-best?
    total preference weight
    stronger family move-work priority
sort descending
```

所以它不是简单列 pair，而是在问：

`下一步最像真的值得拿去做 coalesce 动作的是哪几个 family-pair`

### 20.5.2 retry family agenda / retry phase trace

当前 optimistic retry 这条线现在也已经不只是“最后多扫一遍 spilled values”。

它已经多了两层正式工件：

- `retry family agenda`
- `retry phase trace`

`retry family agenda` 负责讲：

- 当前哪些 family 还有 recoverable 成员
- 哪些是 `spill-intended` recoverable
- 这次 recovery 入口更像：
  - `free`
  - `preferred-evict`
  - `generic-evict`
- 代表值的 priority / spill_cost / split-child 情况
- 是否需要 eviction
- blocker 是谁
- blocker 是否可以 recolor

所以它更像：

`retry outer field / family agenda`

而 `retry phase trace` 负责讲：

- 某个 retry family phase 的 `phase_id`
- 这个 phase 属于哪个 `root`
- 它是从哪个 agenda entry 开始的
- 这个 phase 一共恢复了几个成员
- recovery order 范围是多少

所以它更像：

`retry family phase sequence`

如果把 retry 线压缩成一句话，现在已经不只是：

```text
spill -> maybe recover
```

而更像：

```text
retry family agenda
-> begin family phase
-> drain recoverable members in that phase
-> record retry phase trace
```

你也可以把它先记成两层伪代码：

```text
retry family agenda:
    collect one representative retry entry per runtime family
    annotate:
        recoverable count
        intended count
        preferred source
        eviction / recolor requirement
        split-child hints
    sort families for outer retry field

retry phase:
    take agenda front entry
    begin one family phase
    keep recovering members from that family while phase rules allow
    record:
        phase id
        phase root
        entry summary
        recovered member count
        recovery order range
```

所以 `retry family agenda` 更像：

`选下一段 phase 从哪个 family 开始`

而 `retry phase trace` 更像：

`这个 phase 实际排空了多少成员，顺序是什么`

### 20.6 这还不等于 full move-worklist mainline

虽然这条 family-aware 中层已经很厚了，但它还不是经典教材里那种：

- active moves
- worklist moves
- frozen moves
- direct coalesce/freeze transitions driving the whole allocator

所以当前更准确的身份还是：

`mainline-adjacent family control surfaces`

而不是：

`full George/Briggs move-worklist allocator`

### 20.7 谁调用谁、谁总结谁

这一层最容易绕晕，所以直接画成两张图来记。

#### 20.7.1 生产分配主线

当前真正的生产入口还是：

```text
value_ssa_allocate_function(...)
```

它大致是这样往下调的：

```text
def/use + cfg + liveness
-> interference
-> copy affinity
-> allocation prep
-> allocation worklist
-> coalesce analysis
-> allocator plan
-> reverse select
-> spill slots
```

也就是说，当前主线里真正“往下跑分配”的核心还是：

- `coalesce analysis`
- `plan`
- `select`

而不是 `move worklist` 或 `move transition trace`。

#### 20.7.2 family-aware 辅助主线

另外一条线是围着 coalesced family 做的摘要/排序/轨迹层：

```text
coalesce analysis
-> move family analysis
-> move worklist
-> move transition trace
```

你可以把它理解成：

- `coalesce analysis`
  - 先告诉你 family 是谁
- `move family analysis`
  - 再告诉你这组 family 当前是什么 move 状态
- `move worklist`
  - 再告诉你这些 family 谁该先关注
- `move transition trace`
  - 最后把 family 状态怎么一步步变化记下来

所以这条线更像：

`family-level observation/control surfaces`

不是生产分配主线本身。

#### 20.7.3 它们各自“总结”的对象不一样

这个也很关键。

- `coalesce analysis`
  - 总结的是：**pair -> class**
- `move family analysis`
  - 总结的是：**class 当前状态**
- `move worklist`
  - 总结的是：**class 的待办顺序**
- `move transition trace`
  - 总结的是：**class 的状态变化过程**
- `plan`
  - 总结的是：**每个 value 的 remove/freeze/spill 决策**
- `select`
  - 总结的是：**每个 value 最后怎么拿到颜色或 spill**

所以如果你只记一句，可以记成：

```text
coalesce / move family / move worklist / move transition
讲的是 family

plan / select / result
讲的是 value
```

#### 20.7.4 为什么两条线要并存

因为当前 allocator 还不是“family 直接取代 value”的 merged-node allocator。

所以现在合理的结构是：

- **family 这一层**
  - 用来组织 move/coalesce 语义
- **value 这一层**
  - 用来做最终着色、spill、rewrite

这也是为什么你会看到：

- `plan item` 里已经开始带 `coalesce_root_value_id`
- `move_work_class`
- `family_active_member_count`

但最终 `result` 仍然还是：

- `ssa.X color=N`
- `ssa.Y spill slot=M`

#### 20.7.5 当前最准确的总图

把两条线合起来，可以先记成：

```text
copy affinity
-> coalesce analysis
   -> plan (生产主线消费)
   -> move family analysis
      -> move worklist
      -> move transition trace
-> select
-> result
-> rewrite
```

如果你以后看到一个新 surface，不确定它属于哪层，就问自己：

- 它是在讲 **family** 吗？
- 还是在讲 **具体 value 的最终分配结果**？

大多数时候，这个问题一问，位置就清楚了。

### 20.8 还有一个内部 `move_engine`，但它不是公共主入口

在实现上，现在还多了一个内部文件：

- `src/value_ssa_alloc/value_ssa_alloc_move_engine.inc`

它主要是在内部复用 family-aware 的：

- active degree 更新
- active move-related 更新
- family member / pressure 更新
- move transition trace 追加

你可以把它理解成：

`family-aware planner/trace 的共享运行时 helper`

而不是新的公共 allocator API 层。

也就是说：

- public surface 仍然是 `move family analysis` / `move worklist` / `move transition trace`
- `move_engine.inc` 更像这些 surface 背后的内部共用状态机 helper

---

## 21. 高层 driver 现在是一个小 fixed point

当前最高层入口不是：

- allocate once and stop

而更像：

```text
allocate
-> rewrite rewriteable spills
-> canonicalize
-> reallocate
-> if still rewriteable, repeat
```

所以现在还多了：

- `allocation_rounds`
- `rewrite_rounds`

这说明 allocator 已经开始有：

`allocate / rewrite / canonicalize / reallocate`

的小型驱动语义。

这也是为什么它不再只是一个“算结果”的函数，而是开始像 allocator driver 了。

---

## 22. 当前支持 / 不支持什么

**当前已经支持：**

- interference + affinity 驱动分配
- coalescing pair/class analysis
- family-aware effective pressure
- move family analysis
- move family worklist
- move transition trace
- simplify / freeze / spill 小状态机
- reverse-select coloring
- affinity-biased preferred color
- preferred-color source / partner observability
- targeted preferred-color eviction
- retry-based spill recovery observability
- rematerializable-aware spill / eviction tie-break
- 保守 local eviction
- spill slot assignment
- narrow spill rewrite
- allocate + rewrite + canonicalize + reallocate fixed point

**当前还没有：**

- full Briggs/Chaitin allocator
- explicit coalescing mainline
- full move-worklist machinery
- full spill-cost model
- live-range splitting
- machine-facing retry/recolor mainline
- target / machine register model

所以现在最准确的身份是：

`SSA-level conservative allocator experiment`

---

## 23. 两个真实测试家族怎么读

如果前面那些字段你都认识了，接下来最值钱的能力就是：

`看着 trace / dump，能脑补 allocator 到底刚才干了什么`

这里直接用两类已经被测试锁住的真实 family 来讲。

### 23.1 targeted coalesce-color eviction case

这类 case 的核心故事是：

```text
ssa.0 和 ssa.1 在同一个可 coalesce 的 family 里
ssa.1 本来想跟 ssa.0 共色
但那个 preferred color 被某个低优先级 blocker 挡住了
于是 select 优先定向挤掉那个 blocker，保住 preferred color
```

你在结果/trace 里最应该找这些信号：

- `preferred=coalesce-direct(ssa.0)`
- `outcome=optimistic-colored`
- `targeted-evict=ssa.X`

它们合起来的意思是：

- 当前值不是随便拿了个能用的颜色
- 它明确知道自己最想贴谁的颜色
- 它为了保这个颜色，特意挤掉了那个挡路 blocker

所以这个 case 最适合教你读懂：

- preferred-color source
- coalesce-direct 偏好
- targeted eviction

也就是：

`不是“染上了就行”，而是“为什么染成这个颜色”`

### 23.2 retry-recovered case

第二类更有意思，因为它不是“一次就染上”，而是：

```text
某个值先被当成 spill
后面随着别的颜色/邻居状态变化
它又被从 spill 边缘救回来了
```

你在结果/trace 里要找这些信号：

- `spill_intended = yes`
- `spill_recovered = yes`
- `outcome=optimistic-recovered`

如果还有：

- `retry-evict=ssa.X`

那就说明：

- 它不只是被动等到一个空颜色
- 它是在 retry 阶段又通过一次 eviction 路线被救回来的

这个 case 最适合教你读懂：

- spill-intended 和 final spill 的区别
- recovered 和 optimistic-colored 的区别
- retry recovery 不是一句抽象概念，而是真正可观察的路径

### 23.3 怎么把这两类 case 连起来读

你可以把当前 allocator 的 select 行为先粗分成三层：

1. **直接成功**
   - free color
   - 或 preferred color 本来就可用
2. **压力下保 preferred color**
   - targeted eviction
3. **先 spill，后恢复**
   - retry recovery

所以当你看一个 trace 时，可以先问自己：

- 它是一开始就染上的？
- 它是为了保 preferred color 挤掉了别人？
- 还是它先掉到 spill 里，后面又被救回来了？

这三个问题一旦能分清，allocator 的新结果面就会突然好读很多。

### 23.4 为什么这些 walkthrough 值得写进 lesson

因为现在 allocator 的公共 contract 已经不只是：

- `has_color`
- `spill=yes/no`

而是已经上升到：

- preferred-color source
- preferred partner
- targeted eviction
- retry eviction
- spill recovered

如果讲义只停在“最后有几个 spill”，就会落后代码一层。

所以这两类 case 的真正作用，不是再给你两个新名词，而是帮你建立一套读 allocator trace 的顺序感。

---

## 24. 一个最短总结

当前 `value_ssa_alloc` 在做的是：

**先用 interference + affinity + coalescing family 构造一个 family-aware 的 simplify/freeze/spill 动态摘除计划，再按逆序做带 preferred-color / targeted-eviction / retry-recovery 的 coloring；如果最终 spill 了，再分配 spill slot、重写 SSA、canonicalize，并在需要时重新分配。**

这已经不是“只有 allocator-prep 分析”，而是一条真实的 allocator 主线了。

---

## 25. 一条从 plan 到 retry phase 的完整 walkthrough

最后补一条“把这些工件串起来”的流程故事。

假设有这样一种 family：

- `ssa.0 / ssa.1` 属于同一个 coalesced family
- 它们希望尽量同色
- 当前图压力又不算完全轻松

这时 allocator 大概会经历下面几层。

### 25.1 先看 coalesce analysis

这一层会先告诉你：

- `ssa.0` 和 `ssa.1` 是不是 `can_coalesce`
- 它们最终属于哪个 `root`
- `class_size` 是多少

如果这里都没进 family，后面很多 family-aware 语义都还不会发生。

### 25.2 再看 plan

plan 会把这两个值放进 family-aware 的压力世界里看：

- raw `degree`
- effective `degree`
- raw `move_related`
- effective `move_related`
- `move_work_class -> move_work_next_class`

如果它们只是被 family 内部的 mov 牵着走，那么现在这版 plan 往往会把这种内部压力吃掉，不再无意义地先 freeze。

所以你在 plan dump 里最常看到的是：

- `coalesce=ssa.R/N`
- `effective_move=no`
- `move_work=released->released`

而不是单纯盯着 raw `move_related=yes`。

### 25.3 再看 select

到了 reverse-select：

- 如果 family 颜色本来就可用
  - 直接拿 preferred color
- 如果 preferred color 被唯一低优先级 blocker 挡住
  - 尝试 targeted eviction
- 如果这一步还是没救回来
  - 它可能先变成 `spill-intended`

所以 select trace 里最关键的线索通常是：

- `preferred=coalesce-direct(...)`
- `preferred=coalesce-class(...)`
- `targeted-evict=ssa.X`
- `outcome=optimistic-colored`

### 25.4 如果还掉进 spill，再看 retry family agenda

如果某些值先掉进 spill，不代表故事结束。

现在 retry 会先整理一个 family agenda：

- 哪个 family 还有 recoverable 成员
- 哪个 family 还有多少 `spill-intended`
- 入口更像 `free` / `preferred-evict` / `generic-evict`
- blocker 是否可 recolor

所以这时你看的不再是“哪个单值先恢复”，而是：

`哪个 family phase 先开`

### 25.5 再看 retry phase trace

一旦 retry 真的进入某个 family，它就开始像一个小 phase：

- `begin phase`
- drain 这个 family 当前可恢复的成员
- 记录 phase 内 recovery order
- phase 结束后再回到外层 field

所以 retry phase trace 会回答：

- phase 0 是哪个 root
- 这个 phase 从哪个 entry 开始
- 一共恢复了几个成员
- order 范围是多少

你就能区分：

- 这是一个单值偶然恢复
- 还是一个 family 被成批 drain 掉了一截

### 25.6 最后回到 result / layout

所有这些过程最后都会沉淀成两层结果：

1. raw result
   - `spill-intended`
   - `spill-confirmed`
   - `spill-recovered`
   - preferred-color source
   - retry coordinates
2. layout/report
   - color group
   - spill slot group
   - 函数/程序级汇总

所以你如果要完整复盘一个 case，最顺的顺序其实是：

```text
coalesce analysis
-> plan
-> select trace
-> retry family agenda
-> retry phase trace
-> allocation result
-> layout/report
```

### 25.7 这条 walkthrough 最重要的直觉

当前 allocator 已经不再是：

`单值 -> 有没有颜色 -> spill`

而更像：

`family-aware plan`
`-> family-aware select`
`-> family-aware retry field / phase`
`-> value-level final result`

也就是说：

- 过程越来越 family-aware
- 最终结果仍然落回 value-level placement

这就是现在这版 allocator 最值得抓住的总形状。

---

## 26. 三段真实输出怎么逐行读

前面已经讲了“哪些输出看什么”，这一节直接用接近真实测试的输出形状，逐行示意怎么读。

### 26.1 读 plan dump

看到类似：

```text
ssa.1 phase=spill remove=spill-remove class=move-hint coalesce=ssa.0/2
priority=50 spill_cost=... effective_degree=1->1 move_work=coalesce-ready->released
family_members=2->2 ... effective_move=yes->yes
```

可以这样拆：

1. `phase=spill`
   - 当前 planner 在这一步把它看成 spill family
2. `remove=spill-remove`
   - 它最后确实是按 spill 候选离开 worklist 的
3. `class=move-hint`
   - 原始 value worklist 上它还是一个 move-hint 值
4. `coalesce=ssa.0/2`
   - 它属于 root 为 `ssa.0`、大小为 2 的 coalesced family
5. `move_work=coalesce-ready->released`
   - 这一步前 family 还像一个 ready-to-coalesce 对象
   - 这一步后 family 已经被释放到 released 状态
6. `family_members=2->2`
   - 这一步不是在减少 family 活跃成员数
   - 更像状态变化，不是 family 成员被耗尽
7. `effective_move=yes->yes`
   - 它仍然被看成带外部 move 压力

所以这类 plan line 的阅读顺序，最推荐是：

```text
remove
-> coalesce family
-> move_work before/after
-> effective pressure
```

而不是上来先盯 `priority=50`。

### 26.2 读 select trace

看到类似：

```text
ssa.1 remove=spill-remove ... outcome=optimistic-colored
preferred=coalesce-direct(ssa.0) targeted-evict=ssa.2 color=1
```

可以这样拆：

1. `remove=spill-remove`
   - 这不是普通 simplify 下来的值，它是高风险候选
2. `outcome=optimistic-colored`
   - 虽然是 spill-remove，但最后没有真的 spill
3. `preferred=coalesce-direct(ssa.0)`
   - 它最想贴的是 `ssa.0` 的颜色
4. `targeted-evict=ssa.2`
   - 为了保住这个 preferred color，它专门挤掉了 `ssa.2`
5. `color=1`
   - 最终它拿到了颜色 1

所以这一行的真正故事是：

`这个值本来像要 spill，但它为了保住 direct-coalesce partner 的颜色，定向挤掉了一个 blocker，最后成功同色。`

### 26.3 读 retry phase trace

看到类似：

```text
phase=0 root=ssa.0 entry=ssa.0 summary=2/2 recovered=2 orders=0..1
phase=1 root=ssa.1 entry=ssa.1 summary=1/1 recovered=1 orders=2..2
```

可以这样拆：

#### 第一行

- `phase=0`
  - 第一个 retry family phase
- `root=ssa.0`
  - 这一 phase 围绕 family root `ssa.0`
- `entry=ssa.0`
  - 这一 phase 是从 `ssa.0` 这个代表项进来的
- `summary=2/2`
  - 这个 family 当时有 2 个 recoverable 成员，其中 2 个还是 intended
- `recovered=2`
  - 这一 phase 最后真的救回了 2 个成员
- `orders=0..1`
  - 它们占据全局 recovery 顺序里的第 0 和第 1 位

#### 第二行

- `phase=1`
  - 后续另开的一段 phase
- `root=ssa.1`
  - 轮到另一个 family
- `summary=1/1`
  - 这次只有一个 recoverable intended 成员
- `recovered=1`
  - 这一 phase 只救回 1 个
- `orders=2..2`
  - 它在全局 recovery 顺序里排第 2

所以这一小段 trace 在讲的不是：

`哪个单值先恢复`

而是在讲：

`retry 先批量排空了 family A，再回头处理 family B`

### 26.4 把三段输出连起来

如果把这三段合起来读，一条完整故事通常是：

```text
plan:
    这个值属于哪个 family
    它为什么会被当成 spill/remove/freeze 候选

select:
    它最后为什么拿到这个颜色
    有没有定向挤掉 blocker

retry phase:
    它是直接活下来的
    还是在某个 family phase 里被成批救回来的
```

所以你以后如果要讲一个 allocator case，不妨就按这个三段结构讲：

1. `plan` 怎么看它
2. `select` 怎么处理它
3. `retry` 有没有又改写它的命运

---

## 27. 一个排查 checklist

如果你后面真在看一个 allocator regression，下面这个 checklist 会很省事。

### 27.1 如果结果不对，先问五个问题

1. 它属于哪个 coalesced family？
2. 它在 plan 里是 `simplify-remove` 还是 `spill-remove`？
3. 它有没有 preferred color？来源是什么？
4. 它有没有发生：
   - targeted eviction
   - retry eviction
   - spill recovery
5. 它最后是：
   - direct colored
   - optimistic-colored
   - optimistic-recovered
   - spill-confirmed

### 27.2 如果是 family 行为不对，再问四个问题

1. family 是：
   - `coalesce-ready`
   - `freeze-pending`
   - `released`
   - `internal`
2. 这一步 family 的外部 pressure 是多少？
3. 这一步前后 `move_work_class -> move_work_next_class` 是什么？
4. move transition trace 里有没有：
   - `freeze`
   - `coalesce`
   - `remove`
   事件和你预期不一样

### 27.3 如果是 retry 行为不对，再问三件事

1. retry family agenda 的 front entry 是谁？
2. 它的 entry kind 是：
   - `free`
   - `preferred-evict`
   - `generic-evict`
3. retry phase trace 里是不是先排空了另一个 family phase，才轮到你看的这个值？

这套 checklist 的目标不是让你机械排表，而是让你避免一种常见误区：

`看到最终颜色不对，就直接怪 select。`

实际上很多时候问题更早就发生在：

- family 归类
- move_work 排序
- retry phase 入口

---

## 28. 一个最短总结

当前 `value_ssa_alloc` 在做的是：

**先用 interference + affinity + coalescing family 构造一个 family-aware 的 simplify/freeze/spill 动态摘除计划，再按逆序做带 preferred-color / targeted-eviction / retry-recovery 的 coloring；如果最终 spill 了，再分配 spill slot、重写 SSA、canonicalize，并在需要时重新分配。**

这已经不是“只有 allocator-prep 分析”，而是一条真实的 allocator 主线了。

---

## 29. 演化时间线

最后补一张“这个模块是怎么一步步长成现在这样”的时间线。

这块的目的不是记历史细节，而是帮你理解：

`为什么现在会同时存在 plan / family / retry / layout 这么多 surface`

### 29.1 第 0 阶段：只有 prep，没有 allocator

最开始只有：

- def/use
- liveness
- interference
- copy affinity
- allocation prep
- allocation worklist

这一阶段回答的是：

- 谁和谁冲突
- 谁更像 move-hint
- 谁更像 constrained

但还没有真正的：

- color
- spill
- rewrite

所以这一阶段本质上还是：

`allocator-prep`

### 29.2 第 1 阶段：有了第一版 color / spill

然后才开始有：

- `value_ssa_allocate_function(...)`
- 抽象颜色预算 `K`
- 最早期的 greedy coloring
- spill flag

这一阶段第一次把问题从：

- “谁看起来难分”

推进成：

- “谁最后真的 colored / spill 了”

### 29.3 第 2 阶段：remove/select 形状出现

再往前，才长出：

- allocator plan
- `simplify / freeze / spill`
- `remove kind`
- reverse-select coloring

也就是 allocator 第一次开始像一个真正的图着色骨架，而不是单纯 greedy pass。

这一阶段最关键的变化是：

`process matters now`

也就是：

- 不再只是最后颜色是什么
- 而是开始在乎“这个值是怎么离开 worklist 的”

### 29.4 第 3 阶段：spill 不再只是一面旗

再往前是：

- spill slot
- spill rewrite
- canonicalize
- reallocate fixed point

这一步把 allocator 从：

- “谁 spill？”

推进成：

- “谁 spill 到哪？”
- “spill 以后程序怎么改？”

也就是说，这时 allocator 已经开始影响 IR 本身，而不只是吐一个分配结果。

### 29.5 第 4 阶段：coalescing 进入主线

再往前是：

- coalesce pair analysis
- class / representative family
- class-aware preferred color

这一阶段最关键的变化是：

allocator 不再只看 scalar value，也开始承认：

`有些值应该被当成一组来看`

但此时还主要是：

- pair
- family
- select 偏色

还没有那么强的 family-aware planner / retry 协议。

### 29.6 第 5 阶段：family-aware plan / move 中层

接下来又长出：

- effective degree
- effective move-related
- move family analysis
- move worklist
- move transition trace
- internal `move_engine`

这一阶段是一个非常大的结构变化，因为它把 allocator 从：

- “coalescing 只影响选色”

推进成：

- “coalescing / family 已经开始影响 planner 本身”

也就是：

- family 内 accepted move 不再无意义拖去 freeze
- family 外 move pressure 才算真的 effective move pressure
- freeze / simplify 的过程已经开始 family-aware

### 29.7 第 6 阶段：retry 开始 phase 化

再往前就是最近这批更复杂的结果面：

- targeted preferred-color eviction
- spill recovered
- retry family agenda
- retry phase trace

这一阶段最关键的变化是：

retry 已经不再是：

`尾巴上再扫一遍 spilled values`

而开始像：

`outer field -> family phase -> drain phase`

这也是为什么现在会有：

- `retry-phase`
- `retry-phase-step`
- `retry-phase-size`
- `retry-entry`

这些更 protocol-like 的字段。

### 29.8 第 7 阶段：layout / report / artifact 化

最后又补上：

- function/program allocation layout
- allocate+rewrite layout report
- richer dumps / query helpers

这一步的意义是：

allocator 不只是“能跑”，而是已经开始：

- 可观察
- 可测试
- 可汇总
- 可比较

也就是说，这时它已经变成一个真正适合长期演化的大模块，而不是一段实验代码。

### 29.9 一句话压缩整条时间线

如果把这七个阶段压成一行，可以记成：

```text
prep
-> first color/spill
-> remove/select
-> spill rewrite
-> coalescing
-> family-aware planner
-> retry phases
-> layout/report
```

### 29.10 为什么这张时间线有用

因为它能解释一个很常见的感受：

> “为什么现在这个 allocator 目录里会同时有：
> plan、move_worklist、move_transition、retry_agenda、layout_report
> 这么多层？”

答案不是“过度设计”，而是：

**每一层都是在上一层真的不够用了之后，才被加出来的。**

所以现在这个目录看起来复杂，不是因为它一开始就想当大全家桶，而是因为它已经从：

- 一个小 greedy allocator

长成了：

- 一个带 family-aware planning、retry protocol、rewrite、layout/report 的小后端骨架

这也是整份讲义最想帮你抓住的总感觉。
