# Machine Trace Lesson（compiler_lab）

> 目标：解释 `machine_trace` 为什么会出现在 `machine_delta` 之后，它当前到底新增了什么“execution record”语义，`src/machine/observe/machine_trace/` 里真实在做什么，以及这层如何把一堆 change flags 再提升成一个 coarse trace change class。

## 一句话定位

`machine_trace` 是 coarse delta 第一次被提升成“这一步执行记录长什么样”的层。

## 常见误解

- 误解一：trace 这里已经是细粒度调试轨迹。
  - 当前它仍然是 coarse execution record，不是完整 byte/register timeline。
- 误解二：trace 和 event 几乎同一层。
  - trace 更偏记录与变化类别，event 才进一步回答“它属于哪类事件”。

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_delta_lesson.md`
2. `lesson/machine/observe/machine_observe_lesson.md`

因为 trace 的前提就是：

- before/after 变化已经建立
- 现在要把这些变化收束成 execution record

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_event_lesson.md`
2. `lesson/machine/observe/machine_outcome_lesson.md`

因为 trace 之后最自然的问题就是：

- 这条记录属于哪一类事件
- 这类事件又最终意味着什么结果

---

## 最近同步

最近这层最值得同步的，是它现在更明确地成为 event/outcome 这条链的固定 execution-record 壳。

当前最好再多记两点：

1. **trace 不是临时中间层**
   后面的 event/outcome/history/... 现在都默认把 trace 当成稳定上游。

2. **`MachineTraceChangeClass` 的价值比以前更高**
   它现在不是“给 dump 看着方便”，而是后面 event/outcome 家族分类的重要背景事实之一。

---

## 导学

如果说：

- `machine_delta` 回答的是“before/after 发生了什么变化”

那么：

- `machine_trace` 回答的是“把这份变化当成一步执行记录时，该怎么描述它”

这层的关键词是：

- `exact-trace`
- `preview-trace`
- `blocked-on-control`
- `blocked-unsupported`

以及一个很关键的新字段：

- `MachineTraceChangeClass`

它把零散的：

- `status_changed`
- `pc_changed`
- `stack_changed`
- `fetch_changed`

归纳成一个更高层的执行记录类型，比如：

- `program-counter-and-fetch`
- `status-and-fetch`
- `none`

所以 trace 层的身份是：

`delta artifact -> execution record artifact`

---

## 1. 为什么需要 `machine_trace`

有了 `machine_delta`，我们已经知道：

- status 变没变
- pc 变没变
- fetch 变没变

但这 still 很像原始对比数据，还不够像“记录”。

后续如果想做：

- event
- history
- journal

通常不会每次都从四个布尔值重新拼执行语义。

所以 `machine_trace` 的边界是：

`coarse delta -> coarse execution record`

---

## 2. 这层和 `machine_delta` 的区别

一句话区分：

- `machine_delta`
  - 更像 before/after comparison
- `machine_trace`
  - 更像 one-step execution record

这里的关键不是新增多少字段，而是新增一个抽象层：

`change_class`

它把具体的 change flags 收敛成更适合后续消费的一类 trace 记录。

---

## 3. 文件定位

- 接口：`include/machine/trace.h`
- 实现：`src/machine/observe/machine_trace/machine_trace.c`
- 测试：`tests/machine/observe/machine_trace/machine_trace_test.c`
- 规划文档：`docs/backend/MACHINE_TRACE_PLAN.md`

这层当前已经有：

- file / report lifecycle
- verifier
- delta -> trace lowering
- change-class 计算
- report / overview artifact
- dump
- 从 `delta / observe / apply / step / machine_ir` 往上游桥接

---

## 4. `include/machine/trace.h`：数据模型

## 4.1 `MachineTraceResolutionKind`

当前 resolution kind：

- `NONE`
- `EXACT_TRACE`
- `PREVIEW_TRACE`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

基本就是在 delta 层上再往前推一层：

- exact delta -> exact trace
- preview delta -> preview trace

## 4.2 `MachineTraceKind`

目前 kind 很保守：

- `NONE`
- `STATE_RECORD`

这说明第一版 trace 层先只把“单步状态记录”这个类型立住，不急着扩更多变种。

## 4.3 `MachineTraceChangeClass`

这是这层的核心新增。

它把四个 change flags 组合成枚举，例如：

- `STATUS_ONLY`
- `PROGRAM_COUNTER_ONLY`
- `FETCH_ONLY`
- `STATUS_AND_FETCH`
- `PROGRAM_COUNTER_AND_FETCH`
- `NONE`

你可以把它理解成：

`delta 的布尔位图 -> trace 的粗粒度类别`

## 4.4 `MachineTraceFile`

可以先记成：

```text
MachineTraceFile =
  MachineDeltaFile
  + trace resolution
  + trace kind
  + change_class
```

所以 trace 层最核心的新增不是再保存一份状态，而是：

- 给 delta 一个“记录化”的 class

---

## 5. `change_class` 是怎么来的

实现里最值得盯的一点是：

- `machine_trace_classify_change(...)`

虽然 lesson 不需要逐个抄代码，但需要理解它干的事：

它会根据：

- `status_changed`
- `program_counter_changed`
- `stack_pointer_changed`
- `fetch_changed`

组合出一个 `MachineTraceChangeClass`。

例如：

- 只有 `pc` 和 `fetch` 变
  - `PROGRAM_COUNTER_AND_FETCH`
- `status` 和 `fetch` 变
  - `STATUS_AND_FETCH`
- 全都没法比较
  - `NONE`

所以这层的本质是把低层 comparison 转成高层分类。

---

## 6. build 主线：`delta -> trace`

核心入口：

- `machine_trace_build_from_machine_delta_file(...)`

主逻辑大致是：

```text
verify delta
clone delta
change_class = classify_change(delta_summary)

if delta == EXACT_STATE_DELTA:
  exact-trace + state-record

if delta == PREVIEW_STATE_DELTA:
  preview-trace + state-record

if delta == BLOCKED_ON_CONTROL:
  blocked-on-control + none

if delta == BLOCKED_UNSUPPORTED:
  blocked-unsupported + none
```

这里最值得讲的是：

- trace resolution 和 delta resolution 几乎一一对应
- 真正新增的语义是 `change_class`

---

## 7. verifier 在保什么边界

`machine_trace_verify_file(...)` 的重点有两个。

## 7.1 `change_class` 必须是可推导的

它会先重新算：

- `expected_change_class = machine_trace_classify_change(&delta_summary)`

然后要求：

- `trace_file->change_class == expected_change_class`

这意味着：

`change_class` 不是手写备注，而是 verifier-backed public contract。`

## 7.2 exact / preview / blocked 的合同

- `EXACT_TRACE`
  - 必须来自 `EXACT_STATE_DELTA`
  - `trace_kind = STATE_RECORD`
  - `is_exact_trace = true`
- `PREVIEW_TRACE`
  - 必须来自 `PREVIEW_STATE_DELTA`
  - `trace_kind = STATE_RECORD`
  - `is_exact_trace = false`
- blocked families
  - `trace_kind = NONE`
  - `change_class = NONE`

所以 trace 层同样不会对 blocked case 乱发明执行记录。

---

## 8. 测试里的关键例子

## 8.1 `load-local`

主线测试 dump：

```text
trace: resolution=preview-trace
kind=state-record
change-class=program-counter-and-fetch
name=load-local
exact=no
status-changed=no
pc-changed=yes
fetch-changed=yes
```

这条很好地说明：

- 当前仍是 preview trace
- 但它已经从四个布尔 flag 提炼成了一个 `program-counter-and-fetch` 记录类别

## 8.2 `return-imm`

halt case：

```text
trace: resolution=exact-trace
kind=state-record
change-class=status-and-fetch
name=return-imm
exact=yes
status-changed=yes
pc-changed=no
fetch-changed=yes
```

这条很适合讲：

- halt 最终会被描述成一条 exact trace
- 而它的 coarse 变化类别是 `status-and-fetch`

## 8.3 `jump`

测试 dump：

```text
trace: resolution=blocked-on-control
kind=none
change-class=none
```

这说明 blocked control 在 trace 层仍然不 pretending 成真实执行记录。

## 8.4 `store-local-imm` / `store-global-imm` / `call-void-imm`

这些都得到：

- `preview-trace`
- `change-class=program-counter-and-fetch`

这说明当前 trace 层的分类还是很保守：

- 它不会说“这是 memory-write trace”
- 只会先说“这一步表现成 pc/fetch 变化型记录”

## 8.5 `test_machine_trace_custom_step_cases`

这组 case 很值得和前面四条放在一起看，因为它会把 trace 层最重要的几种 resolution 都锁完整：

- `return-imm` -> `exact-trace`
- `jump` -> `blocked-on-control`
- `store-local/global/call` -> `preview-trace`

也就是说，这组测试几乎就是 trace 层的分类总表：

`exact / preview / blocked`

三大类在执行记录层怎么落地。

## 8.6 `test_machine_trace_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_trace`
- `change_class` 和 `exact/preview/blocked` 这套 record contract 在 `i386-preview` 下也保持稳定

所以这层 lesson 很值得记的一句话是：

`machine_trace` 当前锁的是“记录语义”，不是某个 ISA 私有的低层 mutation 细节。`

---

## 9. 这层到底“新增 / 改了 / 翻译了”什么

### 9.1 新增了什么

新增了第一版 trace 记录语义：

- exact trace
- preview trace
- change class

### 9.2 改了什么

把 `machine_delta` 的原始变化信息，改写成适合后续层消费的单步执行记录。

### 9.3 翻译了什么

它把：

- `EXACT_STATE_DELTA`
  - 翻译成 `EXACT_TRACE`
- `PREVIEW_STATE_DELTA`
  - 翻译成 `PREVIEW_TRACE`

并把多个 change flags 进一步翻译成一个 `change_class`。

---

## 10. 和上下游的边界

## 10.1 相对 `machine_delta`

- `machine_delta`
  - before/after 变化本身
- `machine_trace`
  - 对这份变化做单步执行记录抽象

## 10.2 相对 `machine_event`

trace 层到这里为止，还没有说：

- 这条记录属于哪种 event family
- 是 register-result / local-store / call-effect / control-halt 哪种事件

这些就是 `machine_event` 才开始补上的东西。

---

## 11. 和 RISC-V 目标的关系

将来真正做 RISC-V 时，trace 记录可能会更具体：

- 哪个寄存器变了
- 哪条指令格式对应哪种 trace family

但当前仓库很克制，只保留 coarse `change_class`。

这很合理，因为：

- 当前还没有完整 RISC-V architectural mutation engine
- 所以 trace 层不该过度承诺

---

## 12. 一段伪代码看完整主线

```text
build_trace(delta):
  verify(delta)
  change_class = classify_change(delta)

  if delta == exact-state-delta:
    return exact-trace(state-record, change_class)

  if delta == preview-state-delta:
    return preview-trace(state-record, change_class)

  if delta is blocked:
    return blocked trace with class none
```

---

## 13. 一句话总结

`machine_trace` 的核心价值，是把 `machine_delta` 的 before/after 对照提升成可消费的 execution record：exact/preview trace 仍然保持诚实边界，同时用 `change_class` 把零散的 change flags 收敛成更适合后续 event/history/journal 层使用的粗粒度记录。  
