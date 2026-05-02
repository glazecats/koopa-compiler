# Machine History Lesson（compiler_lab）

> 目标：解释 `machine_history` 为什么会出现在 `machine_outcome` 之后，它当前到底新增了什么“single-entry history”语义，这层头文件/测试里已经锁了哪些 contract，以及为什么它故意只做一条 history entry 而不是假装自己已经有多步 replay。

## 一句话定位

`machine_history` 是 observe 末端链里第一次把结果正式装进“记录条目”语境的层。

## 常见误解

- 误解一：history 已经是多步 replay/history engine。
  - 当前 contract 仍然非常克制，只承诺一条 single-entry history record。
- 误解二：outcome 和 history 差别不大，只是命名不同。
  - outcome 还更偏结果分类，history 则开始进入可被 timeline/log/journal 消费的记录语言。

## 导学

如果说：

- `machine_outcome` 回答的是“这一步最后得到什么结果”

那么：

- `machine_history` 回答的是“如果把这个结果记成一条正式 history entry，它应该长什么样”

所以这层的核心身份是：

`outcome family -> single-entry history artifact`

继续记住 backend 大方向：

- 现在 lesson 里出现的 `generic-elf32`、`i386-preview` 都只是 staging / bridge surface
- 这条 backend 的最终目标仍然是 `RISC-V`

因此这里的 history 也应该被理解成：

`RISC-V 最终执行历史模型之前，第一版诚实的单条 history 记录层`

---

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_outcome_lesson.md`
2. `lesson/machine/observe/machine_event_lesson.md`

因为 history 这里默认前提就是：

- event 已经回答了“发生了什么”
- outcome 已经把它翻译成“最后意味着什么结果”

如果 outcome 这层还没先读，history 这里“为什么要再收束成一条记录条目”会不够扎实。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_timeline_lesson.md`
2. `lesson/machine/observe/machine_log_lesson.md`

因为 history 之后最自然的问题就是：

- 这条 single-entry record 怎么继续进入时间容器
- 然后怎么继续收束成 log / journal

---

## 1. 为什么需要 `machine_history`

`machine_outcome` 已经很有用了，因为它把：

- `control-halt`
- `register-result`
- `local-store`

这些 event-family，翻译成了：

- `program-stopped`
- `value-available`
- `local-updated`

这些 result-family。

但 outcome 仍然更像：

- “这一步的结果类别”

还不够像：

- “一条可以被后面 timeline / log / journal 消费的记录条目”

所以 `machine_history` 的边界很清楚：

`outcome -> one history entry`

它不是在发明 replay engine，它只是在把 outcome 收束成第一条正式记录语言。

---

## 2. 这层和前后两层的区别

一句话区分：

- `machine_outcome`
  - 更偏“结果分类”
- `machine_history`
  - 更偏“把结果装进一条 history entry”
- `machine_timeline`
  - 更偏“把这条 history entry 放进时间轴容器”

所以这里最值得记住的不是“又多了一层壳”，而是：

`consumer-facing record boundary 从这层开始正式出现`

---

## 3. 文件定位

- 接口：`include/machine/history.h`
- 实现：`src/machine/observe/machine_history/machine_history.c`
- 测试：`tests/machine/observe/machine_history/machine_history_test.c`
- 规划文档：`docs/backend/MACHINE_HISTORY_PLAN.md`

当前 observe 这几层还没有像 `machine_select`、`machine_exec` 那样拆成很多 `.inc`，所以这里确实还是一个主 `.c` 文件承担：

- file lifecycle
- verifier
- `outcome -> history` lowering
- report build
- direct bridge from `step / machine_ir / outcome / event / trace / delta / observe / apply / commit`
- dump

也就是说，文件数虽然不多，但职责已经是完整 artifact 层了。

---

## 4. `include/machine/history.h`：当前数据模型

## 4.1 `MachineHistoryResolutionKind`

当前 resolution kind 是：

- `EXACT_HISTORY`
- `PREVIEW_HISTORY`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

这说明 history 层没有重写 exact / preview / blocked 主线，它只是继承并重命名到 history 语境里。

## 4.2 `MachineHistoryKind`

真正新增的 history family 是：

- `PROGRAM_STOP_HISTORY`
- `VALUE_HISTORY`
- `LOCAL_UPDATE_HISTORY`
- `GLOBAL_UPDATE_HISTORY`
- `CALL_HISTORY`
- `BLOCKED_CONTROL_HISTORY`
- `BLOCKED_UNSUPPORTED_HISTORY`

这套名字其实就是把 outcome family 往“记录条目语言”再推一步。

## 4.3 `MachineHistorySummary`

这一层 lesson 最该盯的字段有：

- `resolution_kind`
- `history_kind`
- `outcome_resolution_kind`
- `outcome_kind`
- `event_kind`
- `trace_kind`
- `trace_change_class`
- `is_exact_history`
- `is_single_entry_history`
- `history_entry_count`
- `origin_status`
- `observed_status`
- `has_status_change`
- `has_program_counter_change`
- `has_stack_pointer_change`
- `has_fetch_change`
- `primary_target_block_index`
- `secondary_target_block_index`
- `return_immediate`

这说明 history 层并不是只记一行名字，它已经把上游 outcome/event/trace/apply/commit/... 那整条链的摘要一并挂了下来。

## 4.4 这层最诚实的 stop condition

这层最关键的 contract 其实就两条：

- `is_single_entry_history = yes`
- `history_entry_count = 1`

也就是说，当前不是：

- 多步 replay history
- 多 entry 因果链

而是：

- 一条 entry 的 history artifact

这个克制非常重要，因为仓库当前还没有完整 RISC-V replay engine。

---

## 5. build 主线：`outcome -> history`

核心入口是：

- `machine_history_build_from_machine_outcome_file(...)`
- `machine_history_build_from_machine_outcome_report(...)`

同时头文件里还能看到很多直接桥接入口，比如：

- `machine_history_build_from_machine_event_file(...)`
- `machine_history_build_from_machine_trace_file(...)`
- `machine_history_build_from_machine_step_file(...)`
- `machine_history_build_from_machine_ir_report(...)`

所以它已经不是只能吃上游一个类型，而是整条 observe/runtime/backend 链都能直接桥到 history。

主逻辑可以先记成：

```text
verify upstream file
clone upstream outcome context
history_kind = classify_from_outcome_kind(outcome_kind)

if outcome is exact:
  resolution = exact-history
elif outcome is preview:
  resolution = preview-history
elif outcome is blocked-control:
  resolution = blocked-on-control
else:
  resolution = blocked-unsupported

history_entry_count = 1
verify history file again
```

所以这层真正新增的不是复杂算法，而是：

- 第一版 history family
- 第一版 single-entry history count contract
- 第一版 outcome/event/trace context 向下携带

---

## 6. verifier 在保什么边界

`machine_history_verify_file(...)` 这层 lesson 里很值得讲，因为它锁的不是“有个 struct 就行”，而是：

- history file 里包着的 `MachineOutcomeFile` 必须先合法
- `history_kind` 必须能从上游 `outcome_kind` 重新推导出来
- `resolution_kind` 不能和 history family 打架
- `history_entry_count` 必须保持当前第一版的单 entry contract

换句话说，verifier 在防止两种错：

- 名字翻译错
- 假装自己已经有多 entry history

---

## 7. 测试里的关键例子

`tests/machine/observe/machine_history/machine_history_test.c` 其实已经把 lesson 最该讲的主线锁得很清楚了。

## 7.1 `test_machine_history_mainline`

这组主线测试会先手工构造一个最小 `MachineIrAllocateRewriteReport`：

- `bb0` 里 `load-local`
- 接着做 `eq 0`
- 然后按条件跳到两个 `return-imm`

最后桥到 `machine_history`。

它的意义不是花哨，而是证明：

- `machine_ir -> ... -> machine_history` 这条桥已经是通的
- history 层能稳定承接 branch/value 这类典型主线

## 7.2 `load-local`：preview value history

主线 dump 里最常见的例子是：

```text
history: resolution=preview-history
kind=value-history
outcome=value-available
single-entry=yes
entries=1
```

这说明：

- 上游结果是 `value-available`
- 到 history 层被翻译成 `value-history`
- 仍然保持 preview
- 但已经正式是一条 history entry

## 7.3 `return-imm`：exact program-stop history

`test_machine_history_custom_step_cases` 里会锁一条很关键的 halt case：

```text
history: resolution=exact-history kind=program-stop-history
... name=return-imm ... exact=yes single-entry=yes entries=1 ... return-imm=7
```

这条很适合教学，因为它说明：

- `control-halt` 在 outcome 层先变成 `program-stopped`
- 然后在 history 层继续变成 `program-stop-history`
- 这是一条 exact history，而不是 preview history

## 7.4 `jump`：blocked-control history

同一组测试里还锁了：

```text
history: resolution=blocked-on-control kind=blocked-control-history
... name=jump ... single-entry=yes entries=1 ... targets=[1]
```

这条很重要，因为它告诉我们：

- history 层不会把 blocked control 假装成正常执行结果
- target block 信息仍然保留下来了
- 但表达语言已经切到了 history

## 7.5 `call-void-imm`：preview call history

测试还锁了：

```text
history: resolution=preview-history kind=call-history
... name=call-void-imm ... single-entry=yes entries=1
```

这说明：

- call effect 到这里被翻译成 `call-history`
- 但因为当前还不是 resolved external completion，所以仍然是 preview

## 7.6 `test_machine_history_custom_step_cases` 到底在补什么

这组 case 实际上是在补一张“总表”：

- `return-imm` -> `exact-history + program-stop-history`
- `jump` -> `blocked-on-control + blocked-control-history`
- `store-local-imm` -> `preview-history + local-update-history`
- `store-global-imm` -> `preview-history + global-update-history`
- `call-void-imm` -> `preview-history + call-history`

所以它锁住的不是某一个 opcode，而是：

`outcome family 到 history family 的完整第一版 taxonomy`

## 7.7 `test_machine_history_i386_bridge`

这组测试锁的是：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_history`
- 在 `i386-preview` 下，`value-history / program-stop-history / local/global/call-history / blocked-*` 这套命名保持稳定

lesson 里要特别强调：

- 这里出现 `i386-preview` 不代表目标方向改了
- 它只是用来验证 bridge 和命名 contract 的 staging profile
- 仓库最终目标仍然是 `RISC-V`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了：

- `MachineHistoryKind`
- `history_entry_count`
- `is_single_entry_history`
- `MachineHistoryReport`
- 从多种上游 artifact 直接桥到 history 的 API

### 8.2 改了什么

它把“只是结果分类”的 outcome artifact，改造成了“可供后续 timeline/log/journal 消费的一条正式记录”。

### 8.3 翻译了什么

最典型的翻译就是：

- `value-available`
  - 翻译成 `value-history`
- `program-stopped`
  - 翻译成 `program-stop-history`
- `local-updated`
  - 翻译成 `local-update-history`
- `global-updated`
  - 翻译成 `global-update-history`
- `call-issued`
  - 翻译成 `call-history`

---

## 9. 和 RISC-V 目标的关系

将来如果仓库真的继续往 RISC-V 执行历史推进，这一层可能会长出：

- 多 entry history
- 更细的 architectural causality
- 更真实的 replay chain

但当前第一版只承诺：

- one history entry
- honest exact / preview / blocked split

这是对的，因为现在的主线只是把执行结果往“可消费记录”方向推进，还没有完整的 RISC-V replay/history engine。

---

## 10. 一句话总结

`machine_history` 的核心价值，不是“多一层壳”，而是把 `machine_outcome` 正式翻译成一条 single-entry history record，并且把这个“现在只有一条 entry”的诚实边界明确锁进 API、verifier 和测试里。  
