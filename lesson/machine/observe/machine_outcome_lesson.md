# Machine Outcome Lesson（compiler_lab）

> 目标：解释 `machine_outcome` 为什么会出现在 `machine_event` 之后，它当前到底新增了什么“consequence / outcome family”语义，`src/machine/observe/machine_outcome/` 里真实在做什么，以及它为什么比 `event` 更像“这一步最后得到了什么结果”。

## 一句话定位

`machine_outcome` 是 observe 末端链里第一次把“发生了什么事件”翻成“这一步最后意味着什么结果”的层。

## 常见误解

- 误解一：outcome 和 event 只是两套名字。
  - 更准确地说，event 更偏动作/事件，outcome 更偏 consequence/result。
- 误解二：到 outcome 就已经是最终记录层了。
  - 后面还要继续经过 history、timeline、log、journal，才能真正收束成最终记录 artifact。

## 导学

如果说：

- `machine_event` 回答的是“发生了哪类事件”

那么：

- `machine_outcome` 回答的是“这类事件对这一步来说最终意味着什么结果”

当前 outcome 家族很清楚：

- `program-stopped`
- `value-available`
- `local-updated`
- `global-updated`
- `call-issued`
- `blocked-control`
- `blocked-unsupported`

所以这层的核心身份是：

`event family -> step consequence`

同时继续记住大方向：

- 现在 lesson 里看到的 `generic-elf32` / `i386-preview` 只是 staging surface
- 这条 backend 的最终目标仍然是 `RISC-V`

所以这里的 outcome 也应该被理解成：

`RISC-V 最终执行结果语义之前的第一版保守 consequence artifact`

---

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_event_lesson.md`
2. `lesson/machine/observe/machine_trace_lesson.md`

如果你还没先理解：

- event 这层在表达“发生了什么”
- trace 这层已经保留了哪些 before/after / change-class 事实

那 outcome 这里“为什么要从 event family 翻译成 consequence family”会不够直观。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_history_lesson.md`
2. `lesson/machine/observe/machine_timeline_lesson.md`

因为 outcome 的最重要后续问题就是：

- 结果怎么继续收束成 single-entry history
- 然后再怎么进入 timeline / log / journal 这条记录主线

---

## 1. 为什么需要 `machine_outcome`

只看 `machine_event`，我们已经知道：

- 这是 `register-result`
- 这是 `local-store`
- 这是 `control-halt`

但 event 还更像“发生了什么动作/事件”，不够像“这一步最终得到什么后果”。

例如：

- `register-result`
  - 更像事件
- `value-available`
  - 更像结果

所以 `machine_outcome` 的边界是：

`event -> consequence`

---

## 2. 这层和 `machine_event` 的区别

一句话区分：

- `machine_event`
  - 更偏“发生了什么”
- `machine_outcome`
  - 更偏“所以结果是什么”

举例：

- `control-halt`
  - 到 outcome 变成 `program-stopped`
- `register-result`
  - 到 outcome 变成 `value-available`
- `call-effect`
  - 到 outcome 变成 `call-issued`

所以这层是在把执行事件翻译成更面向 consumer 的结果语言。

---

## 3. 文件定位

- 接口：`include/machine/outcome.h`
- 实现：`src/machine/observe/machine_outcome/machine_outcome.c`
- 测试：`tests/machine/observe/machine_outcome/machine_outcome_test.c`
- 规划文档：`docs/backend/MACHINE_OUTCOME_PLAN.md`

---

## 4. `include/machine/outcome.h`：数据模型

## 4.1 `MachineOutcomeResolutionKind`

当前 resolution kind：

- `NONE`
- `EXACT_OUTCOME`
- `PREVIEW_OUTCOME`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

和 `event` 一样，先维持 exact / preview / blocked 主线不变。

## 4.2 `MachineOutcomeKind`

这层真正的核心是：

- `PROGRAM_STOPPED`
- `VALUE_AVAILABLE`
- `LOCAL_UPDATED`
- `GLOBAL_UPDATED`
- `CALL_ISSUED`
- `BLOCKED_CONTROL`
- `BLOCKED_UNSUPPORTED`

这套词比 event 层更像“结果汇总”。

## 4.3 `MachineOutcomeFile`

可以先记成：

```text
MachineOutcomeFile =
  MachineEventFile
  + outcome resolution
  + outcome kind
```

也就是说，outcome 层最重要的新增就是：

- `outcome_kind`

---

## 5. build 主线：`event -> outcome`

核心入口：

- `machine_outcome_build_from_machine_event_file(...)`

它的逻辑很直白：

```text
verify event
clone event
outcome_kind = classify_kind(event_summary)

if event == EXACT_EVENT:
  exact-outcome

if event == PREVIEW_EVENT:
  preview-outcome

if event == BLOCKED_ON_CONTROL:
  blocked-on-control

if event == BLOCKED_UNSUPPORTED:
  blocked-unsupported
```

所以 outcome 层和前几层一样：

- resolution 主要继承上游 exact/preview/blocked
- 真正新增的是 family 命名

---

## 6. outcome family 到底怎么分类

从头文件、实现和测试一起看，当前映射基本可以直接背：

- `control-halt`
  - `program-stopped`
- `register-result`
  - `value-available`
- `local-store`
  - `local-updated`
- `global-store`
  - `global-updated`
- `call-effect`
  - `call-issued`
- `blocked-control`
  - `blocked-control`
- `blocked-unsupported`
  - `blocked-unsupported`

所以可以把它理解成：

`事件名 -> 结果名`

---

## 7. verifier 在保什么边界

`machine_outcome_verify_file(...)` 的重点仍然是：

- 重新计算 `expected_outcome_kind`
- 强制要求 file 里的 `outcome_kind` 与其一致

同时还要求：

- `EXACT_OUTCOME`
  - 不能来自 blocked family
- `PREVIEW_OUTCOME`
  - 不能来自 blocked family
- blocked resolutions
  - 必须来自对应 blocked family

所以 outcome 层也不会把 blocked case 包装成正常结果。

---

## 8. 测试里的关键例子

## 8.1 `load-local`

主线测试 dump：

```text
outcome: resolution=preview-outcome
kind=value-available
event=register-result
trace=preview-trace
name=load-local
exact=no
```

这条正好说明：

- 事件是 `register-result`
- 结果是 `value-available`

## 8.2 `return-imm`

halt case：

```text
outcome: resolution=exact-outcome
kind=program-stopped
event=control-halt
trace=exact-trace
name=return-imm
exact=yes
return-imm=7
```

这里最适合讲：

- 事件是 halt
- 结果是 program stopped

## 8.3 `jump`

控制流 blocked case 会得到：

```text
outcome: resolution=blocked-on-control
kind=blocked-control
event=blocked-control
trace=blocked-on-control
exact=no
```

这一条很适合直接点明边界：

`只要 control transfer 还没 honest 落成真正的下一状态，outcome 层也不会假装已经有一个“正常结果”。`

## 8.4 `store-local-imm` / `store-global-imm` / `call-void-imm`

分别得到：

- `local-updated`
- `global-updated`
- `call-issued`

这几条几乎就是 outcome 层最核心的教学材料。

## 8.5 `test_machine_outcome_custom_step_cases`

这组 case 的价值在于，它几乎把 outcome 层最关键的 family 全都锁了一遍：

- `register-result` -> `value-available`
- `control-halt` -> `program-stopped`
- `blocked-control` -> `blocked-on-control`
- `local/global/call` -> `local-updated / global-updated / call-issued`

也就是说，这组测试几乎就是这层 lesson 的“事件语义 -> 结果语义”总表。

## 8.6 `test_machine_outcome_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_outcome`
- `value-available / program-stopped / local-updated / global-updated / call-issued / blocked-*` 这套 outcome taxonomy 在 `i386-preview` 下依然稳定

所以 outcome 层当前锁下来的，是：

`consequence language`

而不是某个 ISA 专属的历史模型。

---

## 9. 这层到底“新增 / 改了 / 翻译了”什么

### 9.1 新增了什么

新增了第一版 step consequence 语义：

- value available
- local/global updated
- call issued
- program stopped

### 9.2 改了什么

把 `machine_event` 的事件族，改写成更偏结果/后果的 outcome 语言。

### 9.3 翻译了什么

它把：

- `register-result`
  - 翻译成 `value-available`
- `control-halt`
  - 翻译成 `program-stopped`
- `call-effect`
  - 翻译成 `call-issued`

---

## 10. 一句话总结

`machine_outcome` 的核心价值，是把 `machine_event` 的“发生了什么”翻译成“所以结果是什么”：值可用、局部更新、全局更新、调用发出、程序停止，这些词开始更接近后续 history/timeline/log/journal 想讲给人的故事。  
