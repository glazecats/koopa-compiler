# Machine Journal Lesson（compiler_lab）

> 目标：解释 `machine_journal` 为什么会出现在 `machine_log` 之后，它当前到底新增了什么“single-record journal”语义，这层头文件/测试锁了哪些最终 consumer-facing contract，以及为什么 `docs/NEXT_STEPS.md` 现在把它视为当前 backend 默认最下游 snapshot。

## 一句话定位

`machine_journal` 是当前 machine lesson 主线最下游的最终记录层，它把前面的结果一路收束成一条 single-record consumer artifact。

## 常见误解

- 误解一：journal 只是把 log 改个名字。
  - 它更强调最终 record 语义，并且是当前 README / plan 默认承认的最下游 snapshot。
- 误解二：到了 journal 就说明已经有完整 replay ledger。
  - 当前 contract 仍然很克制，只承诺 single-record，而不是多记录执行史。

## 导学

如果说：

- `machine_log` 回答的是“这里有一条单行日志”

那么：

- `machine_journal` 回答的是“把这条日志再组织成一条更像最终消费记录的 journal record”

所以这层的核心身份是：

`single-line log -> single-record journal`

当前 contract 依然非常克制：

- `journal_record_count = 1`
- `journal_record_index = 0`

也就是说，它不是多记录 ledger，而是第一版单记录 journal。

最终目标方向仍然是 `RISC-V`；现在看到的 generic / i386 只是在验证 bridge 和 contract。

---

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_outcome_lesson.md`
2. `lesson/machine/observe/machine_history_lesson.md`
3. `lesson/machine/observe/machine_timeline_lesson.md`
4. `lesson/machine/observe/machine_log_lesson.md`

如果你直接跳到 `machine_journal`，最容易漏掉的是：

- 结果语言是怎么一步步从 `outcome` 收束下来的
- 为什么这里坚持 `single-record`
- `exact / preview / blocked` 这套 contract 是怎么一路保留下来的

## 读完后接哪篇

这篇基本已经是当前 machine lesson 主线的最下游。

所以最自然的后续不是“继续往下读”，而是：

1. 回看 `lesson/machine/README.md`
2. 对照 `docs/backend/MACHINE_JOURNAL_PLAN.md`
3. 如果想逆着追来源，再回看：
   - `machine_log_lesson.md`
   - `machine_timeline_lesson.md`
   - `machine_history_lesson.md`
   - `machine_outcome_lesson.md`

也就是说，这篇更像：

- 当前主线的终点
- 也是回溯整条 observe/runtime/backend 链的观察窗

---

## 最近同步

最近这层最值得同步的，不是 journal family 又多了什么，而是：

- 它现在已经和 `docs/NEXT_STEPS.md` 里的当前 backend 默认最下游 snapshot 完全对齐

也就是说，现在最好把这层额外记成：

1. **当前 machine lesson 主线的正式终点**
2. **回看整条 runtime/observe/backend 收束链时最稳定的观察窗**

同时也要继续记住当前边界：

- 这不代表已经有 multi-record replay ledger
- 当前依然只是 single-record journal

---

## 1. 为什么需要 `machine_journal`

log 层已经很像日志了，但它还是偏：

- line
- log naming

journal 再往 consumer-facing 走一步，开始明确：

- record
- journal family
- 对最终下游更友好的条目容器

所以 `machine_journal` 的边界是：

`log line -> journal record`

这层出现之后，整条 observe 末端链条才真正完成：

`outcome -> history -> timeline -> log -> journal`

---

## 2. 这层和前后两层的区别

一句话区分：

- `machine_log`
  - 单行日志语言
- `machine_journal`
  - 单记录条目语言

而且从 `docs/NEXT_STEPS.md` 现在的描述看，当前默认回答“现在做到哪了”时，最下游 snapshot 已经是：

- `MJR1` / `MJR2` / `MJR3`
- 对应 `machine_journal`

这说明它不是附属小尾巴，而是当前 backend mainline 的最下游记录层。

---

## 3. 文件定位

- 接口：`include/machine/journal.h`
- 实现：`src/machine/observe/machine_journal/machine_journal.c`
- 测试：`tests/machine/observe/machine_journal/machine_journal_test.c`
- 规划文档：`docs/backend/MACHINE_JOURNAL_PLAN.md`

当前这层同样还是一个主 `.c` 文件承担：

- file lifecycle
- verifier
- `log -> journal` lowering
- report build
- 从 `log / timeline / history / outcome / event / trace / delta / observe / apply / commit / step / machine_ir` 直接桥接
- dump

所以它虽然还没拆很多分片，但已经是完整的最下游 consumer artifact 层。

---

## 4. `include/machine/journal.h`：当前数据模型

## 4.1 `MachineJournalResolutionKind`

当前 resolution kind：

- `EXACT_JOURNAL`
- `PREVIEW_JOURNAL`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

resolution 主线依然继承上游，只是现在名字正式落到了 journal 语境里。

## 4.2 `MachineJournalKind`

journal family 当前包括：

- `PROGRAM_STOP_JOURNAL`
- `VALUE_JOURNAL`
- `LOCAL_UPDATE_JOURNAL`
- `GLOBAL_UPDATE_JOURNAL`
- `CALL_JOURNAL`
- `BLOCKED_CONTROL_JOURNAL`
- `BLOCKED_UNSUPPORTED_JOURNAL`

它其实就是把 log family 再翻译成“最终记录条目”的命名。

## 4.3 `MachineJournalSummary`

这一层最该盯的字段有：

- `resolution_kind`
- `journal_kind`
- `log_resolution_kind`
- `log_kind`
- `is_exact_journal`
- `is_single_record_journal`
- `journal_record_count`
- `journal_record_index`
- `MachineLogSummary log_summary`

这点非常关键：

- journal summary 不是重新复制每个上游字段
- 它直接把完整 `MachineLogSummary` 当成内部载荷挂下来

也就是说，到这层为止，前面的 timeline/history/outcome/event/... 摘要都还在，只是被打包成一条 record。

## 4.4 当前最关键的 contract

当前 verifier 锁死：

- `journal_record_count == 1`
- `journal_record_index == 0`
- `is_single_record_journal = yes`

所以 current journal 的真实含义是：

`只有一条正式 journal record`

不是多记录 replay ledger。

---

## 5. build 主线：`log -> journal`

核心入口是：

- `machine_journal_build_from_machine_log_file(...)`
- `machine_journal_build_from_machine_log_report(...)`

头文件里还能看到更多直接桥：

- `machine_journal_build_from_machine_timeline_file(...)`
- `machine_journal_build_from_machine_history_file(...)`
- `machine_journal_build_from_machine_outcome_file(...)`
- `machine_journal_build_from_machine_event_file(...)`
- `machine_journal_build_from_machine_step_file(...)`
- `machine_journal_build_from_machine_ir_report(...)`

主逻辑可以先记成：

```text
verify log
clone log context
journal_kind = classify_from_log_kind(log_kind)

if log is exact:
  resolution = exact-journal
elif log is preview:
  resolution = preview-journal
elif log is blocked-control:
  resolution = blocked-on-control
else:
  resolution = blocked-unsupported

journal_record_count = 1
journal_record_index = 0
verify journal again
```

所以这层不重新发明行为语义，它主要做三件事：

- journal naming
- single-record numbering
- 更终端的 consumer-facing record packaging

---

## 6. verifier 在保什么边界

`machine_journal_verify_file(...)` 主要在保：

- 上游 `MachineLogFile` 先合法
- `journal_kind` 必须能从 `log_kind` 重新推导
- `resolution_kind` 不能和 journal family 冲突
- `journal_record_count / journal_record_index` 必须保持当前单记录边界

也就是说，它防止的是：

- family 翻译错
- 假装自己已经是多 record journal

---

## 7. 测试里的关键例子

## 7.1 `test_machine_journal_mainline`

主线测试会继续从手工构造的 `MachineIrAllocateRewriteReport` 走完整桥接到 `machine_journal`。

lesson 角度上，它说明：

- `machine_ir -> ... -> machine_journal` 整条链已经能打通
- journal 现在真的可以当作默认最下游 inspect artifact 来看

## 7.2 `load-local`：preview value journal

主线 dump 常见的是：

```text
journal: resolution=preview-journal
kind=value-journal
log=value-log
timeline=value-timeline
single-record=yes
records=1
record-index=0
```

这说明：

- `value-log` 到这里被翻译成 `value-journal`
- 整条上游链仍然挂在 record 背后

## 7.3 `return-imm`：exact program-stop journal

`test_machine_journal_custom_step_cases` 会锁：

```text
journal: resolution=exact-journal kind=program-stop-journal
... name=return-imm ... exact=yes single-record=yes records=1 record-index=0 ... return-imm=7
```

这条很适合教学，因为它把整条命名翻译链完整收束到了最下游：

`control-halt -> program-stopped -> program-stop-history -> program-stop-timeline -> program-stop-log -> program-stop-journal`

## 7.4 `jump`：blocked-control journal

测试还锁：

```text
journal: resolution=blocked-on-control kind=blocked-control-journal
... name=jump ... single-record=yes records=1 record-index=0 ... targets=[1]
```

它说明：

- journal 层不会把 blocked state 美化成正常记录
- 它只是把 blocked control 收束成一个 honest blocked journal record

## 7.5 `call-void-imm`：preview call journal

测试还锁：

```text
journal: resolution=preview-journal kind=call-journal
... name=call-void-imm ... single-record=yes records=1 record-index=0
```

它说明：

- `call-log` 到这里变成 `call-journal`
- 但当前仍然只是 preview record，而不是 resolved external execution journal

## 7.6 `test_machine_journal_custom_step_cases` 到底在补什么

这组 case 补的就是最终 journal taxonomy 表：

- `return-imm` -> `exact-journal + program-stop-journal`
- `jump` -> `blocked-on-control + blocked-control-journal`
- `store-local-imm` -> `preview-journal + local-update-journal`
- `store-global-imm` -> `preview-journal + global-update-journal`
- `call-void-imm` -> `preview-journal + call-journal`

所以它锁住的是：

`log family 到 journal family 的第一版稳定最终记录语言`

## 7.7 `test_machine_journal_i386_bridge`

这组 case 锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_journal`
- `value-journal / program-stop-journal / local/global/call-journal / blocked-*` 这套 taxonomy 在 `i386-preview` 下依然稳定

这里也还是同一句提醒：

- `i386-preview` 是 staging bridge
- 仓库最终目标仍然是 `RISC-V`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了：

- `MachineJournalKind`
- `journal_record_count`
- `journal_record_index`
- `is_single_record_journal`
- `MachineJournalReport`

### 8.2 改了什么

把单行 log artifact 改造成了更像最终消费记录的一条 journal record。

### 8.3 翻译了什么

最典型的翻译就是：

- `value-log`
  - 翻译成 `value-journal`
- `program-stop-log`
  - 翻译成 `program-stop-journal`
- `call-log`
  - 翻译成 `call-journal`

---

## 9. 为什么这层现在是默认最下游 snapshot

`docs/NEXT_STEPS.md` 现在已经把当前 backend 默认进度 authority 放在：

- `docs/backend/MACHINE_JOURNAL_PLAN.md`

而且 `MJR1`、`MJR2`、`MJR3` 都已经标成 effectively `100%`。

这很合理，因为到这一步为止，整条链已经从：

- state
- mutation
- writeback
- commit
- apply
- observe
- delta
- trace
- event
- outcome
- history
- timeline
- log

一路收束成：

`single-record journal artifact`

也就是说，当前 lesson 主线走到这里，已经形成了一个相对完整的“执行语义 -> 可消费记录”闭环。

---

## 10. 和 RISC-V 目标的关系

将来如果仓库继续沿着 RISC-V 执行记录往下推进，这一层可能会扩成：

- 多 record journal
- 更细的 sequence / causality
- 更完整的 replay-facing record model

但当前第一版只承诺：

- `records = 1`
- `record-index = 0`

这让 journal 层非常诚实，不会把现在的 staging surface 误读成“已经有成熟的 RISC-V 多记录执行日志”。

---

## 11. 一句话总结

`machine_journal` 的核心价值，是把 `machine_log` 最终收束成一条 single-record journal：它把前面整条 observe/runtime/backend 摘要都挂在 record 背后，同时把“现在只有 1 条 record”的边界正式锁进 API、verifier、测试和当前进度主线。  

## 8. 和 RISC-V 目标的关系

未来真正做 RISC-V 时，这层可能会演化成：

- 更丰富的 journal schema
- 多 record 序列
- 更接近真实 replay / inspection 工具的 artifact

但当前先做 single-record journal 非常对路，因为它避免过早假装自己已经知道完整 RISC-V execution stream。

---

## 9. 一句话总结

`machine_journal` 的核心价值，是把上游单行 log 最终收束成 single-record journal：它没有发明新的底层执行语义，而是把整条 observe 链包装成更面向消费方的最终记录形态，这也是当前 backend lesson 主线的自然收束点。  
