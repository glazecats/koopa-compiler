# Machine Log Lesson（compiler_lab）

> 目标：解释 `machine_log` 为什么会出现在 `machine_timeline` 之后，它当前到底新增了什么“single-line log”语义，这层为什么虽然叫 log 却不是自由文本，以及头文件/测试到底锁了哪些单行日志 contract。

## 一句话定位

`machine_log` 是 single-tick timeline 正式被翻译成单行日志语境的层。

## 常见误解

- 误解一：log 这里就是自由文本输出。
  - 当前它仍然是 verifier-backed 的结构化单行 artifact。
- 误解二：有了 timeline 就已经等于日志。
  - timeline 还偏时间容器，log 则开始进入 consumer-facing log naming。

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_timeline_lesson.md`
2. `lesson/machine/observe/machine_history_lesson.md`

因为 log 的前提就是：

- timeline 已经回答了 tick / entry-index
- 现在要继续把它翻成单行日志语言

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_journal_lesson.md`
2. `lesson/machine/README.md`

因为 log 之后最自然的问题就是：

- 怎么继续收束成最终 journal record
- 再回头看整条 observe/backend 主线在什么位置结束

---

## 导学

如果说：

- `machine_timeline` 回答的是“这里有一个位于 tick 0 的时间条目”

那么：

- `machine_log` 回答的是“把这个 tick 用一条日志行的语言表达出来，它应该叫什么、排在第几行”

所以这层的核心身份是：

`single-tick timeline -> single-line log`

同时要继续记住：

- 这不是完整 replay transcript
- 这不是任意文本 logger
- 这是 verifier-backed 的结构化单行日志 artifact

最终目标方向仍然是 `RISC-V`，现在 lesson 里出现的 generic / i386 只是 bridge。

---

## 1. 为什么需要 `machine_log`

timeline 已经有时序位置了，但它更偏结构化时间容器。

log 则再往 consumer-facing 走一步，开始明确：

- 这是哪类日志行
- 这条日志在当前日志容器里排第几行

因此 `machine_log` 的边界是：

`timeline entry -> log line`

这对后面的 journal 很重要，因为 journal 会继续把“日志行”再收束成“记录条目”。

---

## 2. 这层和前后两层的区别

一句话区分：

- `machine_timeline`
  - 时间容器语言
- `machine_log`
  - 单行日志语言
- `machine_journal`
  - 单记录条目语言

所以 log 层的重点不是增加更多执行信息，而是：

`把时间容器翻译成日志行容器`

---

## 3. 文件定位

- 接口：`include/machine/log.h`
- 实现：`src/machine/observe/machine_log/machine_log.c`
- 测试：`tests/machine/observe/machine_log/machine_log_test.c`
- 规划文档：`docs/backend/MACHINE_LOG_PLAN.md`

当前这层也是单 `.c` 文件承担：

- file lifecycle
- verifier
- `timeline -> log` lowering
- report build
- 从 `timeline / history / outcome / event / trace / delta / observe / apply / commit / step / machine_ir` 直接桥接
- dump

也就是说，虽然文件没有拆很多份，但 contract 已经很完整。

---

## 4. `include/machine/log.h`：当前数据模型

## 4.1 `MachineLogResolutionKind`

当前 resolution kind：

- `EXACT_LOG`
- `PREVIEW_LOG`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

resolution 仍然沿用上游 exact / preview / blocked 主线，只是现在语境变成 log。

## 4.2 `MachineLogKind`

当前 log family：

- `PROGRAM_STOP_LOG`
- `VALUE_LOG`
- `LOCAL_UPDATE_LOG`
- `GLOBAL_UPDATE_LOG`
- `CALL_LOG`
- `BLOCKED_CONTROL_LOG`
- `BLOCKED_UNSUPPORTED_LOG`

可以把它理解成：

`timeline family 的日志命名版`

## 4.3 `MachineLogSummary`

这层 lesson 里最值得盯的字段有：

- `resolution_kind`
- `log_kind`
- `timeline_resolution_kind`
- `timeline_kind`
- `history_kind`
- `outcome_kind`
- `event_kind`
- `trace_kind`
- `is_exact_log`
- `is_single_line_log`
- `log_line_count`
- `log_line_index`
- `origin_status`
- `observed_status`
- `has_status_change`
- `has_program_counter_change`
- `has_stack_pointer_change`
- `has_fetch_change`
- `primary_target_block_index`
- `secondary_target_block_index`
- `return_immediate`

这说明 log 层并不是只有一个“打印字符串”，而是保留了整条上游摘要，再加上一层日志行 framing。

## 4.4 这层最关键的 contract

当前 verifier 锁死：

- `log_line_count == 1`
- `log_line_index == 0`
- `is_single_line_log = yes`

所以当前 log 的真实含义是：

`只有一条结构化日志行`

不是多行 transcript。

---

## 5. build 主线：`timeline -> log`

核心入口是：

- `machine_log_build_from_machine_timeline_file(...)`
- `machine_log_build_from_machine_timeline_report(...)`

但头文件里还能看到很多更上游桥：

- `machine_log_build_from_machine_history_file(...)`
- `machine_log_build_from_machine_outcome_file(...)`
- `machine_log_build_from_machine_event_file(...)`
- `machine_log_build_from_machine_step_file(...)`
- `machine_log_build_from_machine_ir_report(...)`

主逻辑可以先记成：

```text
verify timeline
clone timeline context
log_kind = classify_from_timeline_kind(timeline_kind)

if timeline is exact:
  resolution = exact-log
elif timeline is preview:
  resolution = preview-log
elif timeline is blocked-control:
  resolution = blocked-on-control
else:
  resolution = blocked-unsupported

log_line_count = 1
log_line_index = 0
verify log again
```

所以 log 层真正新增的是：

- log family
- 单行日志编号
- consumer-facing log naming

---

## 6. verifier 在保什么边界

`machine_log_verify_file(...)` 主要在保：

- 上游 `MachineTimelineFile` 必须先合法
- `log_kind` 必须能从 `timeline_kind` 重新推导
- `resolution_kind` 不能和 log family 冲突
- `log_line_count` / `log_line_index` 必须保持当前单行边界

它在防止的其实也是两类错：

- family 翻译错
- 假装自己已经是多行 replay log

---

## 7. 测试里的关键例子

## 7.1 `test_machine_log_mainline`

这组主线测试继续从手工构造的 `MachineIrAllocateRewriteReport` 直接桥到 `machine_log`。

lesson 角度上，这意味着：

- `machine_ir -> ... -> machine_log` 主线已经被锁住
- log 层不是一个“只能在上游 report 已经准备好时才可用”的薄壳

## 7.2 `load-local`：preview value log

主线 dump 里常见的是：

```text
log: resolution=preview-log
kind=value-log
timeline=value-timeline
single-line=yes
lines=1
line-index=0
```

这说明：

- `value-timeline` 被翻译成 `value-log`
- 这条记录已经变成一条正式日志行

## 7.3 `return-imm`：exact program-stop log

`test_machine_log_custom_step_cases` 会锁：

```text
log: resolution=exact-log kind=program-stop-log
... name=return-imm ... exact=yes single-line=yes lines=1 line-index=0 ... return-imm=7
```

这条很适合教学，因为它把整条翻译链继续往下接了一层：

`control-halt -> program-stopped -> program-stop-history -> program-stop-timeline -> program-stop-log`

## 7.4 `jump`：blocked-control log

测试还锁：

```text
log: resolution=blocked-on-control kind=blocked-control-log
... name=jump ... single-line=yes lines=1 line-index=0 ... targets=[1]
```

它说明：

- blocked control 到 log 层仍然保持 blocked
- 只不过被放进了“单行日志”的命名体系

## 7.5 `call-void-imm`：preview call log

测试里还有：

```text
log: resolution=preview-log kind=call-log
... name=call-void-imm ... single-line=yes lines=1 line-index=0
```

这说明：

- `call-timeline` 在这里被翻译成 `call-log`
- 当前仍然不是 resolved completion transcript，所以还是 preview

## 7.6 `test_machine_log_custom_step_cases` 到底在补什么

这组 case 补的仍然是一张 taxonomy 总表：

- `return-imm` -> `exact-log + program-stop-log`
- `jump` -> `blocked-on-control + blocked-control-log`
- `store-local-imm` -> `preview-log + local-update-log`
- `store-global-imm` -> `preview-log + global-update-log`
- `call-void-imm` -> `preview-log + call-log`

所以它真正锁住的是：

`timeline family 到 log family 的第一版稳定翻译`

## 7.7 `test_machine_log_i386_bridge`

这组 case 锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_log`
- `value-log / program-stop-log / local/global/call-log / blocked-*` 这套命名在 `i386-preview` 下保持稳定

lesson 里仍然要提醒：

- `i386-preview` 是桥接面
- 最终目标仍然是 `RISC-V`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了：

- `MachineLogKind`
- `log_line_count`
- `log_line_index`
- `is_single_line_log`
- `MachineLogReport`

### 8.2 改了什么

把 timeline entry 改造成了一条正式的日志行 artifact。

### 8.3 翻译了什么

最典型的翻译就是：

- `value-timeline`
  - 翻译成 `value-log`
- `program-stop-timeline`
  - 翻译成 `program-stop-log`
- `call-timeline`
  - 翻译成 `call-log`

---

## 9. 和 RISC-V 目标的关系

以后如果真的做更完整的 RISC-V 执行日志，这一层可能会扩成：

- 多行日志
- 更接近真实 transcript 的 ordering
- 更细的 architectural details

但当前仓库只诚实地承诺：

- `lines = 1`
- `line-index = 0`

这样就不会把现在的 preview/generic staging surface 误读成“已经有成熟的 RISC-V replay log”。

---

## 10. 一句话总结

`machine_log` 的核心价值，是把 single-tick timeline 再翻译成一条结构化的 single-line log：它不靠自由文本，而是把“这是一条什么日志、排在第几行”正式锁进 API、verifier 和测试。  
