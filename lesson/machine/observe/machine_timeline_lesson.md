# Machine Timeline Lesson（compiler_lab）

> 目标：解释 `machine_timeline` 为什么会出现在 `machine_history` 之后，它当前到底新增了什么“single-tick timeline”语义，这层头文件/测试已经锁了哪些时间轴 contract，以及为什么 `entry_count=1 / entry_index=0` 是这一层最重要的诚实边界。

## 一句话定位

`machine_timeline` 是 single-entry history 正式进入“时间容器”语境的层。

## 常见误解

- 误解一：timeline 这里已经是完整 replay 时间轴。
  - 当前 contract 仍然只承诺 single-tick timeline。
- 误解二：history 和 timeline 只是多了两个字段。
  - timeline 真正新增的是 temporal container 与 entry/index contract。

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_history_lesson.md`
2. `lesson/machine/observe/machine_outcome_lesson.md`

因为 timeline 的前提就是：

- outcome 已经被收束成 history record
- 现在才开始讨论它在时间容器里的位置

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_log_lesson.md`
2. `lesson/machine/observe/machine_journal_lesson.md`

因为 timeline 之后最自然的问题就是：

- 这个 tick 怎么继续被表达成 log line
- 再怎么最终收束成 journal record

---

## 导学

如果说：

- `machine_history` 回答的是“这里有一条 history entry”

那么：

- `machine_timeline` 回答的是“把这条 history entry 放进时间轴容器后，它位于哪个 tick”

所以这层的核心身份是：

`single-entry history -> single-tick timeline`

而且这里的“时间轴”一定要读得很克制：

- 不是完整 replay timeline
- 不是多步执行序列
- 只是第一版单 tick 时间容器

最终目标方向仍然是 `RISC-V`，现在看到的 generic / i386 只是 staging bridge。

---

## 1. 为什么需要 `machine_timeline`

history 已经把 outcome 变成了正式记录，但它还只是在说：

- “有一条记录”

timeline 则开始进一步明确：

- 这条记录在时间容器里的位置
- 后面 log / journal 可以按什么时序外壳来消费它

因此 `machine_timeline` 的边界是：

`history record -> temporal container`

即使当前只有一个 tick，这个边界也非常重要，因为 consumer 关心的不只是“有记录”，还关心“这条记录在时间序列里怎么编号”。

---

## 2. 这层和前后两层的区别

一句话区分：

- `machine_history`
  - 记录语言
- `machine_timeline`
  - 时间轴语言
- `machine_log`
  - 单行日志语言

所以 timeline 的重点不是新增更多执行细节，而是：

`把记录正式放到时间容器里`

---

## 3. 文件定位

- 接口：`include/machine/timeline.h`
- 实现：`src/machine/observe/machine_timeline/machine_timeline.c`
- 测试：`tests/machine/observe/machine_timeline/machine_timeline_test.c`
- 规划文档：`docs/backend/MACHINE_TIMELINE_PLAN.md`

这层目前也还是一个主 `.c` 文件，里面已经同时承担：

- file lifecycle
- verifier
- `history -> timeline` lowering
- report build
- 从 `history / outcome / event / trace / delta / observe / apply / commit / step / machine_ir` 直接桥接
- dump

所以它虽然还没拆成很多 `.inc`，但并不是薄层。

---

## 4. `include/machine/timeline.h`：当前数据模型

## 4.1 `MachineTimelineResolutionKind`

当前 resolution kind：

- `EXACT_TIMELINE`
- `PREVIEW_TIMELINE`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

resolution 仍然沿用 exact / preview / blocked 大主线，只是语境从 history 切到了 timeline。

## 4.2 `MachineTimelineKind`

timeline family 是：

- `PROGRAM_STOP_TIMELINE`
- `VALUE_TIMELINE`
- `LOCAL_UPDATE_TIMELINE`
- `GLOBAL_UPDATE_TIMELINE`
- `CALL_TIMELINE`
- `BLOCKED_CONTROL_TIMELINE`
- `BLOCKED_UNSUPPORTED_TIMELINE`

它其实就是把 history family 再往“时间轴条目”语言翻译一层。

## 4.3 `MachineTimelineSummary`

lesson 里最该盯的字段有：

- `resolution_kind`
- `timeline_kind`
- `history_resolution_kind`
- `history_kind`
- `outcome_kind`
- `event_kind`
- `trace_kind`
- `is_exact_timeline`
- `is_single_tick_timeline`
- `timeline_entry_count`
- `timeline_entry_index`
- `origin_status`
- `observed_status`
- `has_status_change`
- `has_program_counter_change`
- `has_stack_pointer_change`
- `has_fetch_change`
- `primary_target_block_index`
- `secondary_target_block_index`
- `return_immediate`

这里最关键的一点是：

- timeline 并没有丢掉上游 history/outcome/event/trace 信息
- 它只是多加了一层“时间位置”的 framing

## 4.4 这层最重要的 contract

当前 verifier 锁死了：

- `timeline_entry_count == 1`
- `timeline_entry_index == 0`
- `is_single_tick_timeline = yes`

所以当前第一版 timeline 的真实含义是：

`一条 entry 的时间轴，而且这条 entry 就是 tick 0`

不是完整多 tick replay。

---

## 5. build 主线：`history -> timeline`

核心入口是：

- `machine_timeline_build_from_machine_history_file(...)`
- `machine_timeline_build_from_machine_history_report(...)`

但头文件还能看到很多更上游的桥：

- `machine_timeline_build_from_machine_outcome_file(...)`
- `machine_timeline_build_from_machine_event_file(...)`
- `machine_timeline_build_from_machine_step_file(...)`
- `machine_timeline_build_from_machine_ir_report(...)`

所以 timeline 已经是整条链一个可直接跳达的 consumer artifact。

主逻辑可以记成：

```text
verify history
clone history context
timeline_kind = classify_from_history_kind(history_kind)

if history is exact:
  resolution = exact-timeline
elif history is preview:
  resolution = preview-timeline
elif history is blocked-control:
  resolution = blocked-on-control
else:
  resolution = blocked-unsupported

timeline_entry_count = 1
timeline_entry_index = 0
verify timeline again
```

所以 timeline 层真正新增的是：

- timeline family
- 时间容器编号
- 单 tick contract

---

## 6. verifier 在保什么边界

`machine_timeline_verify_file(...)` 主要在保：

- 上游 `MachineHistoryFile` 先合法
- `timeline_kind` 必须能从 `history_kind` 重新推导
- `resolution_kind` 不能和 timeline family 打架
- `timeline_entry_count` 和 `timeline_entry_index` 必须保持当前单 tick 边界

它在防止的其实是两类错：

- family 翻译错
- 假装自己已经是多 tick 时间轴

---

## 7. 测试里的关键例子

## 7.1 `test_machine_timeline_mainline`

这组测试和 history 一样，会先从手工构造的 `MachineIrAllocateRewriteReport` 走完整桥接。

它说明：

- `machine_ir -> ... -> machine_timeline` 主线已经可用
- timeline 层不是只能吃 history 测试桩，而是能承接真实 backend bridge

## 7.2 `load-local`：preview value timeline

主线 dump 里很典型的一条是：

```text
timeline: resolution=preview-timeline
kind=value-timeline
history=value-history
single-tick=yes
entries=1
entry-index=0
```

这说明：

- `value-history` 被翻译成 `value-timeline`
- timeline 已经开始显式说“它在 tick 0”

## 7.3 `return-imm`：exact program-stop timeline

`test_machine_timeline_custom_step_cases` 会锁：

```text
timeline: resolution=exact-timeline kind=program-stop-timeline
... name=return-imm ... exact=yes single-tick=yes entries=1 entry-index=0 ... return-imm=7
```

这条适合教学，因为它把整条翻译链直接摆在你面前：

`control-halt -> program-stopped -> program-stop-history -> program-stop-timeline`

## 7.4 `jump`：blocked-control timeline

测试还锁：

```text
timeline: resolution=blocked-on-control kind=blocked-control-timeline
... name=jump ... single-tick=yes entries=1 entry-index=0 ... targets=[1]
```

这说明：

- blocked control 不会被 timeline 层洗白成正常时间推进
- timeline 只是给它一个 honest blocked tick 容器

## 7.5 `call-void-imm`：preview call timeline

还有：

```text
timeline: resolution=preview-timeline kind=call-timeline
... name=call-void-imm ... single-tick=yes entries=1 entry-index=0
```

它说明：

- `call-history` 到这层变成 `call-timeline`
- 但当前仍然只是 preview tick，而不是完整 external completion timeline

## 7.6 `test_machine_timeline_custom_step_cases` 到底在补什么

这组测试补的是整张 timeline taxonomy 表：

- `return-imm` -> `exact-timeline + program-stop-timeline`
- `jump` -> `blocked-on-control + blocked-control-timeline`
- `store-local-imm` -> `preview-timeline + local-update-timeline`
- `store-global-imm` -> `preview-timeline + global-update-timeline`
- `call-void-imm` -> `preview-timeline + call-timeline`

所以 lesson 里可以直接把它理解成：

`history family 放进时间容器后，会变成哪种 tick`

## 7.7 `test_machine_timeline_i386_bridge`

这组 case 锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_timeline`
- `value-timeline / program-stop-timeline / local/global/call-timeline / blocked-*` 这套 taxonomy 在 `i386-preview` 下仍稳定

这里同样要提醒：

- 这是 bridge/staging contract
- 不代表目标架构从 `RISC-V` 改成 `i386`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了：

- `MachineTimelineKind`
- `timeline_entry_count`
- `timeline_entry_index`
- `is_single_tick_timeline`
- `MachineTimelineReport`

### 8.2 改了什么

把 history record 改造成了带显式时序位置的 timeline entry。

### 8.3 翻译了什么

最典型的翻译就是：

- `value-history`
  - 翻译成 `value-timeline`
- `program-stop-history`
  - 翻译成 `program-stop-timeline`
- `call-history`
  - 翻译成 `call-timeline`

---

## 9. 和 RISC-V 目标的关系

真正的 RISC-V timeline 以后可能会继续长出：

- 多 tick
- 更丰富的 temporal ordering
- 更完整的 replay ordering

但当前 lesson 看到的 contract 只承诺：

- `entries = 1`
- `entry-index = 0`

这非常宝贵，因为它明确告诉你：

`现在不是完整时间轴，只是时间轴骨架的第一格`

---

## 10. 一句话总结

`machine_timeline` 的核心价值，是把 single-entry history 再提升成 single-tick timeline：它没有假装自己已经会做多步 replay，只是把“这是一条位于 tick 0 的时间记录”这件事，正式锁进 API、verifier 和测试。  
