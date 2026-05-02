# Machine Event Lesson（compiler_lab）

> 目标：解释 `machine_event` 为什么会出现在 `machine_trace` 之后，它当前到底新增了什么“event family”语义，`src/machine/observe/machine_event/` 里真实在做什么，以及这层如何把 trace 记录正式归类成 `control-halt / register-result / local-store / global-store / call-effect / blocked-*` 这些事件族。

## 一句话定位

`machine_event` 是 observe 末端链里第一次把 execution record 正式归类成稳定事件族的层。

## 常见误解

- 误解一：event 已经在回答最终结果是什么。
  - event 更偏“发生了哪类事件”，结果收束还要到 outcome/history/... 继续推进。
- 误解二：trace 里已有变化类别，event 就只是换个名字。
  - event 这里真正新增的是稳定的 event-family taxonomy。

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_trace_lesson.md`
2. `lesson/machine/observe/machine_delta_lesson.md`

因为 event 的前提就是：

- trace 已经把变化收束成 execution record
- 现在要在这个记录之上正式归类事件族

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_outcome_lesson.md`
2. `lesson/machine/observe/machine_history_lesson.md`

因为 event 之后最自然的问题就是：

- 这类事件最终意味着什么结果
- 再怎么把结果装进记录条目语言

---

## 导学

如果说：

- `machine_trace` 回答的是“这一步执行记录是什么，变化类别是什么”

那么：

- `machine_event` 回答的是“这条记录到底属于哪一类事件”

这层新增的最关键东西是：

- `MachineEventKind`

当前已有的事件族包括：

- `control-halt`
- `register-result`
- `local-store`
- `global-store`
- `call-effect`
- `blocked-control`
- `blocked-unsupported`

所以 event 层的身份非常明确：

`trace record -> event family`

这也是后面：

- outcome
- history
- timeline
- log
- journal

这些层真正开始变得“可讲故事”的基础。

---

## 1. 为什么需要 `machine_event`

只看 trace，我们知道：

- exact 还是 preview
- change class 是 `program-counter-and-fetch` 还是 `status-and-fetch`

但这 still 不够表达“发生了什么事件”。

例如同样是：

- `program-counter-and-fetch`

它可能来自：

- register result
- local store
- global store
- call effect

所以 `machine_event` 的边界是：

`trace record -> semantic event family`

---

## 2. 这层和 `machine_trace` 的区别

一句话区分：

- `machine_trace`
  - 更像执行记录
- `machine_event`
  - 更像执行记录的语义标签

`change_class` 告诉你：

- 变了哪些大类字段

而 `event_kind` 告诉你：

- 这是哪种事件

这两个是互补的。

---

## 3. 文件定位

- 接口：`include/machine/event.h`
- 实现：`src/machine/observe/machine_event/machine_event.c`
- 测试：`tests/machine/observe/machine_event/machine_event_test.c`
- 规划文档：`docs/backend/MACHINE_EVENT_PLAN.md`

这层当前已经有：

- file / report lifecycle
- verifier
- trace -> event lowering
- event family 分类
- report / overview artifact
- dump
- 从 `trace / delta / observe / apply / step / machine_ir` 往上游桥接

---

## 4. `include/machine/event.h`：数据模型

## 4.1 `MachineEventResolutionKind`

当前 resolution kind：

- `NONE`
- `EXACT_EVENT`
- `PREVIEW_EVENT`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

和 trace 层一样，先保留 exact / preview / blocked 这条大主线。

## 4.2 `MachineEventKind`

这层真正的核心是这个枚举：

- `CONTROL_HALT`
- `REGISTER_RESULT`
- `LOCAL_STORE`
- `GLOBAL_STORE`
- `CALL_EFFECT`
- `BLOCKED_CONTROL`
- `BLOCKED_UNSUPPORTED`

这一组名字几乎就是整份 lesson 的关键词。

它说明 event 层终于开始把“发生了什么”说成人能直接理解的语义族。

## 4.3 `MachineEventFile`

可以先记成：

```text
MachineEventFile =
  MachineTraceFile
  + event resolution
  + event kind
```

所以 event 层不像 delta 层那样新增 before/after 字段，也不像 trace 层新增 change_class。

它最重要的新增就是：

- `event_kind`

---

## 5. 事件族是怎么分类出来的

这层最值得盯的 helper 是：

- `machine_event_classify_kind(&trace_summary)`

虽然我们没必要在 lesson 里逐行重写，但从测试和 verifier 可以看出现在的分类表：

- `exact-trace + no-mutation/control-only + halt`
  - `control-halt`
- `preview-trace + deferred-register-result`
  - `register-result`
- `preview-trace + deferred-local-slot`
  - `local-store`
- `preview-trace + deferred-global-slot`
  - `global-store`
- `preview-trace + deferred-call-effect`
  - `call-effect`
- `blocked-on-control`
  - `blocked-control`
- `blocked-unsupported`
  - `blocked-unsupported`

所以这层是第一次把 mutation side 的 effect family 真正翻到观察/记录语义里。

---

## 6. build 主线：`trace -> event`

核心入口：

- `machine_event_build_from_machine_trace_file(...)`

逻辑大致是：

```text
verify trace
clone trace
event_kind = classify_kind(trace_summary)

if trace == EXACT_TRACE:
  exact-event

if trace == PREVIEW_TRACE:
  preview-event

if trace == BLOCKED_ON_CONTROL:
  blocked-on-control

if trace == BLOCKED_UNSUPPORTED:
  blocked-unsupported
```

也就是说：

- resolution 主要继承 trace
- 真正新增的语义是 `event_kind`

---

## 7. verifier 在保什么边界

`machine_event_verify_file(...)` 的重点有两个。

## 7.1 `event_kind` 必须和 trace summary 可推导一致

它会先算：

- `expected_event_kind = machine_event_classify_kind(&trace_summary)`

然后强制要求：

- `event_file->event_kind == expected_event_kind`

所以 event family 不是人为 annotation，而是 verifier-backed contract。

## 7.2 exact / preview / blocked 对应的家族也有限制

- `EXACT_EVENT`
  - 必须来自 `EXACT_TRACE`
  - 不能是 blocked 家族
  - 不能是 `none`
- `PREVIEW_EVENT`
  - 必须来自 `PREVIEW_TRACE`
  - 不能是 `control-halt`
  - 不能是 blocked 家族
- `BLOCKED_ON_CONTROL`
  - 必须来自 blocked control trace
  - family 必须是 `BLOCKED_CONTROL`
- `BLOCKED_UNSUPPORTED`
  - 必须来自 blocked unsupported trace
  - family 必须是 `BLOCKED_UNSUPPORTED`

这说明 event 层也不会乱给 preview trace 贴上 exact event 的标签。

---

## 8. 测试里的关键例子

## 8.1 `load-local`

主线测试 dump：

```text
event: resolution=preview-event
kind=register-result
trace=preview-trace
change-class=program-counter-and-fetch
name=load-local
exact=no
```

这条很适合讲：

- trace 只说是 preview-trace
- event 再进一步说：这是 `register-result`

## 8.2 `return-imm`

halt case：

```text
event: resolution=exact-event
kind=control-halt
trace=exact-trace
change-class=status-and-fetch
name=return-imm
exact=yes
return-imm=7
```

这条非常关键，因为它说明：

- `exact-trace` 会进一步落成 `exact-event`
- 事件语义被正式归类为 `control-halt`

## 8.3 `jump`

测试 dump：

```text
event: resolution=blocked-on-control
kind=blocked-control
trace=blocked-on-control
change-class=none
```

这说明 blocked control 在 event 层仍然保持 blocked 家族，不会被包装成正常执行事件。

## 8.4 `store-local-imm` / `store-global-imm` / `call-void-imm`

测试分别得到：

- `preview-event kind=local-store`
- `preview-event kind=global-store`
- `preview-event kind=call-effect`

这三条非常适合直接背下来，因为它们几乎就是第一版 `MachineEventKind` 的主要内容。

## 8.5 `test_machine_event_custom_step_cases`

这组 case 的课堂价值很高，因为它会把 event 层的几种核心 family 一次性锁齐：

- `return-imm` -> `exact-event + control-halt`
- `jump` -> `blocked-on-control + blocked-control`
- `store-local/global/call` -> `preview-event + local-store/global-store/call-effect`

也就是说，这组测试几乎就在替 lesson 说明：

`trace` 层的 exact / preview / blocked 记录，到 event 层以后会分别被翻译成哪一种事件族。`

## 8.6 `test_machine_event_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_event`
- `control-halt / register-result / local-store / global-store / call-effect / blocked-*` 这套 event family 在 `i386-preview` 下依然稳定

这说明 event 层当前真正稳定下来的，是：

`event family taxonomy`

而不是某个 ISA 的最终低层执行细节。

---

## 9. 这层到底“新增 / 改了 / 翻译了”什么

### 9.1 新增了什么

新增了第一版事件族语义：

- control halt
- register result
- local/global store
- call effect
- blocked families

### 9.2 改了什么

把 `machine_trace` 的执行记录，改写成对人更友好的“发生了哪类事件”。

### 9.3 翻译了什么

它把：

- `EXACT_TRACE + halt family`
  - 翻译成 `EXACT_EVENT + CONTROL_HALT`
- `PREVIEW_TRACE + register result family`
  - 翻译成 `PREVIEW_EVENT + REGISTER_RESULT`
- `PREVIEW_TRACE + local/global/call family`
  - 翻译成相应的 store/call 事件族

---

## 10. 和上下游的边界

## 10.1 相对 `machine_trace`

- `machine_trace`
  - 记录一步执行的结构变化
- `machine_event`
  - 把这一步记录归成事件族

## 10.2 相对 `machine_outcome`

event 层到这里为止，还没有判断：

- 这件事的总体 outcome 是什么
- 它在 history / timeline / journal 里该怎么被组织

这些就是后面的：

- `machine_outcome`
- `machine_history`
- `machine_timeline`

要继续做的事。

---

## 11. 和 RISC-V 目标的关系

将来真正做 RISC-V 时，这层会自然变成更强的事件层：

- 某类寄存器写结果事件
- 某类内存写事件
- 某类 call / control 事件

当前版本先把大类立起来，是非常合理的 staging：

- 不过度承诺底层细节
- 先把事件族语言建立起来

这对未来 RISC-V backend 的 lesson 和调试都会很重要。

---

## 12. 一段伪代码看完整主线

```text
build_event(trace):
  verify(trace)
  kind = classify_kind(trace)

  if trace == exact-trace:
    return exact-event(kind)

  if trace == preview-trace:
    return preview-event(kind)

  if trace == blocked-control:
    return blocked-on-control(blocked-control)

  if trace == blocked-unsupported:
    return blocked-unsupported(blocked-unsupported)
```

---

## 13. 一句话总结

`machine_event` 的核心价值，是把 `machine_trace` 的结构化执行记录真正翻译成语义事件族：halt 成为 `control-halt`，值结果成为 `register-result`，store/call 各自成族，而 blocked family 也继续保持诚实的 blocked 语义，为后面的 outcome/history/journal 主线打基础。  
