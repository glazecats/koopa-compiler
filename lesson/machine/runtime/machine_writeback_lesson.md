# Machine Writeback Lesson（compiler_lab）

> 目标：解释 `machine_writeback` 为什么会出现在 `machine_mutation` 之后，它当前到底新增了什么“commit-vs-defer”语义，`src/machine/runtime/machine_writeback/` 里真实在做什么，以及为什么这层现在只敢把 `no-mutation` 提交成 committed-no-op，而不去假装已经完成真实 register / memory writeback。

## 一句话定位

`machine_writeback` 是 mutation effect 第一次被正式分成“现在能 committed”还是“还得 defer”的层。

## 常见误解

- 误解一：writeback 这里已经在做真实 register/memory 写回。
  - 当前这层仍然很克制，重点是 commit-vs-defer 分类。
- 误解二：mutation 和 writeback 差别只在名字。
  - mutation 更偏 effect family，writeback 则更偏提交时机判断。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_mutation_lesson.md`
2. `lesson/machine/runtime/machine_state_lesson.md`

因为 writeback 的前提就是：

- 你已经知道这条指令欠哪类 effect
- 现在才进一步问哪些现在能 committed

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_commit_lesson.md`
2. `lesson/machine/runtime/machine_apply_lesson.md`

因为 writeback 之后最自然的问题就是：

- 能不能产出 committed artifact
- 这份 committed/deferred 结果对后续真正应用意味着什么

---

## 最近同步

最近这层最值得同步的，是它现在更明确地立住了：

- mutation family
- 到 committed artifact

之间那条“commit-vs-defer”判断边界。

当前最好再多记两点：

1. **writeback 的保守性现在更像稳定 contract**
   当前不是“以后再说”，而是明确只在少数能诚实承诺的 case 上给 committed judgement。

2. **它现在更像 commit/apply 的正式上游判定层**
   lesson 里最好把它理解成 committed artifact 形成前的最后一道分类门。

---

## 导学

如果说：

- `machine_mutation` 回答的是“这条指令还欠哪一类副作用”

那么：

- `machine_writeback` 回答的是“这些副作用里，哪些现在已经能算 committed，哪些还必须继续 defer”

这层当前非常保守。

它没有做：

- register table 改写
- local/global memory byte 改写
- external call side effect 模拟

它真正做的是：

`mutation family -> writeback commitment classification`

也就是把上一层的结果进一步翻译成：

- `committed-no-op`
- `deferred-register-writeback`
- `deferred-local-writeback`
- `deferred-global-writeback`
- `deferred-call-writeback`
- `blocked-on-control`
- `blocked-unsupported`

这份讲义建议按下面顺序读：

1. 先理解为什么 mutation 之后还需要 writeback
2. 再看 `include/machine/writeback.h`
3. 再看 `machine_writeback_build_from_machine_mutation_file(...)`
4. 最后用测试例子记住 committed / deferred / blocked 三大类

学完你应该能：

1. 解释 `machine_writeback` 和 `machine_mutation` 的边界
2. 说明这层新增的是“提交策略”，不是完整架构状态更新
3. 看懂为什么 halt 会变成 `committed-no-op`
4. 理解为什么 register/local/global/call 现在都还只能 deferred

---

## 1. 为什么需要 `machine_writeback`

`machine_mutation` 已经告诉我们：

- 这是 register result
- 这是 local slot store
- 这是 global slot store
- 这是 call effect
- 这是 control-only / blocked / unsupported

但它还没回答：

- 哪些现在就能算 committed
- 哪些还不能 honest writeback

所以 `machine_writeback` 的边界是：

`mutation classification -> commit / defer policy`

这层的重点不是识别 effect，而是给 effect 一个“现在能不能提交”的结论。

---

## 2. 为什么不能直接把 `machine_mutation` 当成 writeback

因为“识别 effect”与“决定是否 commit”是两件事。

例如：

- `deferred-register-result`
  - 我们知道它是 register result
  - 但不代表现在已经有完整 register architectural model

- `deferred-local-slot`
  - 我们知道它是 local slot store
  - 但不代表 slot id 已经落成真实 runtime memory layout

所以当前仓库故意拆成两层：

- `machine_mutation`
  - 先回答“欠的是什么”
- `machine_writeback`
  - 再回答“现在能不能 commit”

---

## 3. 文件定位

- 接口：`include/machine/writeback.h`
- 实现：`src/machine/runtime/machine_writeback/machine_writeback.c`
- 测试：`tests/machine/runtime/machine_writeback/machine_writeback_test.c`
- 规划文档：`docs/backend/MACHINE_WRITEBACK_PLAN.md`

当前已经有：

- public enums / structs
- verifier
- mutation -> writeback lowering
- report / overview artifact
- dump
- 从 `mutation / state / transition / interp / payload / decode / step / machine_ir` 往上游桥接

所以它已经是一层完整 contract，不是 placeholder。

---

## 4. `include/machine/writeback.h`：数据模型

## 4.1 `MachineWritebackResolutionKind`

当前 resolution：

- `NONE`
- `COMMITTED_NO_OP`
- `DEFERRED_REGISTER_WRITEBACK`
- `DEFERRED_LOCAL_WRITEBACK`
- `DEFERRED_GLOBAL_WRITEBACK`
- `DEFERRED_CALL_WRITEBACK`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

可以看出这层最重要的新增词汇是：

- `COMMITTED_*`
- `DEFERRED_*`

## 4.2 `MachineWritebackCommitKind`

除了 resolution，这层还保留了 commit kind：

- `NONE`
- `NO_OP`
- `REGISTER`
- `LOCAL_SLOT`
- `GLOBAL_SLOT`
- `CALL`

这和 `mutation.effect_kind` 很像，但语义更偏“提交对象”。

例如：

- `DEFERRED_REGISTER_WRITEBACK + REGISTER`
- `COMMITTED_NO_OP + NO_OP`

## 4.3 `MachineWritebackFile`

可以先把它记成：

```text
MachineWritebackFile =
  MachineMutationFile
  + writeback resolution
  + commit kind
```

所以它本质上是：

`mutation artifact + commit/defer verdict`

## 4.4 `MachineWritebackSummary`

summary 继续把上游信息串下来：

- writeback resolution / commit kind
- mutation resolution / effect kind
- state / transition / action
- raw/tag/known/name/bytes
- has_state / pc / current byte
- targets / return immediate

这意味着看一条 writeback summary 时，你已经能看到：

- 这条指令原本是什么
- mutation 怎么分类
- 现在 commit policy 是什么

---

## 5. build 主线：`mutation -> writeback`

核心入口是：

- `machine_writeback_build_from_machine_mutation_file(...)`

逻辑非常直接：

```text
verify mutation

switch mutation resolution:
  NO_MUTATION:
    committed-no-op + no-op

  DEFERRED_REGISTER_RESULT:
    deferred-register-writeback + register

  DEFERRED_LOCAL_SLOT:
    deferred-local-writeback + local-slot

  DEFERRED_GLOBAL_SLOT:
    deferred-global-writeback + global-slot

  DEFERRED_CALL_EFFECT:
    deferred-call-writeback + call

  BLOCKED_ON_CONTROL:
    blocked-on-control + none

  BLOCKED_UNSUPPORTED:
    blocked-unsupported + none

clone mutation file
verify output
```

这里最值得注意的是：

这层现在几乎是纯 policy remap。

它不做真正数据写回，只做：

`欠账 -> 是否能认定为已提交`

---

## 6. verifier 在保什么

`machine_writeback_verify_file(...)` 的逻辑也非常直接，但很重要。

它会根据 `mutation_file->resolution_kind` 反推出：

- 期望的 `writeback resolution`
- 期望的 `commit kind`

比如：

- `MACHINE_MUTATION_RESOLUTION_NO_MUTATION`
  - 必须对应
  - `MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP`
  - `MACHINE_WRITEBACK_COMMIT_KIND_NO_OP`

- `MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT`
  - 必须对应
  - `DEFERRED_REGISTER_WRITEBACK`
  - `REGISTER`

如果对不上，就报：

- `MACHINE-WRITEBACK-103: writeback classification mismatch`

这说明 writeback 层不是“随便加个标签”，而是有一个很硬的映射合同。

---

## 7. 这层最重要的设计边界

从计划文档和代码一起看，可以提炼出一句最重要的话：

`当前 writeback 层只敢真正提交 no-op control completion，不敢假装已经完成架构级 register / memory / call writeback。`

为什么？

因为仓库现在还没有：

- 完整 register architectural model
- local/global slot 到 runtime memory 的真实写回模型
- call side effect 的真实外部世界模拟

所以现在最诚实的策略是：

- halt 这种没有额外数据副作用的情况
  - 可以提交成 `committed-no-op`
- 其他 effect family
  - 仍然只是 `deferred-*`

这正是“分层诚实”。

---

## 8. 测试里的六组例子

## 8.1 `load-local` -> `deferred-register-writeback`

测试 dump：

```text
writeback: resolution=deferred-register-writeback
commit=register
mutation=deferred-register-result
effect=value-result
name=load-local
pc=0x1001
current-byte=0x8a
```

这说明：

- 我们已经知道这是一类 register writeback
- 但当前系统还不能说已经 commit 了

## 8.2 `return-imm` -> `committed-no-op`

测试 dump：

```text
writeback: resolution=committed-no-op
commit=no-op
mutation=no-mutation
effect=control-only
transition=halt
action=halt
name=return-imm
status=halted
return-imm=7
```

这一条是整层最关键的教学点。

它说明：

- halt 在 mutation 层是 `no-mutation`
- 到 writeback 层时，可以正式认定为“提交完成”
- 但这个提交不是写寄存器/内存，而是“没有额外写回工作”

所以 `committed-no-op` 不是空洞概念，它代表：

`这条指令在当前第一版 writeback 模型里已经收尾了`

## 8.3 `jump` -> `blocked-on-control`

测试 dump：

```text
writeback: resolution=blocked-on-control
commit=none
mutation=blocked-on-control
effect=control-only
targets=[1]
```

lesson 里要强调：

- 这层没有帮 jump magically 变成 committed
- 因为上游 control target 还没落成真实 runtime `pc`

## 8.4 `unsupported` -> `blocked-unsupported`

测试 dump：

```text
writeback: resolution=blocked-unsupported
commit=none
mutation=blocked-unsupported
effect=none
```

也是一样的原则：

`不懂，就继续 blocked，不 pretend commit`

## 8.5 `store-local-imm` / `store-global-imm`

测试分别得到：

```text
writeback: resolution=deferred-local-writeback
commit=local-slot
```

和：

```text
writeback: resolution=deferred-global-writeback
commit=global-slot
```

这两条很适合讲“这层新增了什么”：

- 不是重新识别 store
- 而是明确说：
  - local/global store 的 writeback 现在仍 deferred

## 8.6 `call-void-imm`

测试得到：

```text
writeback: resolution=deferred-call-writeback
commit=call
```

说明 call family 在这层也被单独视为一个提交对象，而不是和 register/store 混在一起。

## 8.7 `test_machine_writeback_custom_step_cases`

这组 case 很值得配合前面六条一起看，因为它几乎就是在替 lesson 把整张分类表补齐：

- `store-local-imm` -> `deferred-local-writeback`
- `store-global-imm` -> `deferred-global-writeback`
- `call-void-imm` -> `deferred-call-writeback`
- `jump` -> `blocked-on-control`
- 未知字节 -> `blocked-unsupported`

也就是说，这组测试不只是“多了几个例子”，而是在证明：

`writeback` 层对所有主要 mutation family 都已经给出了稳定的 commit/defer verdict。`

## 8.8 `test_machine_writeback_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_writeback`
- `i386-preview` 下这套 `committed-no-op / deferred-* / blocked-*` 结论不会因为 target profile 变化而失真

所以 lesson 里最值得记的一句话是：

`writeback` 当前的第一版分类，主要按“是否能 honest commit”分，而不是按 ISA 分。`

---

## 9. report 层新增了什么

当前 report 包括：

- `MachineWritebackReport`
- `MachineWritebackReportOverviewArtifact`
- `machine_writeback_report_refresh(...)`

测试里的 overview：

```text
report_overview:
  origin: mutation=deferred-register-result status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000
  policy: profile=generic-elf32 no-op=yes register=yes slot=yes call=yes
  writeback: resolution=deferred-register-writeback commit=register has-state=yes status=ready pc=0x1001 targets=[] return-imm=-
```

这里已经把：

- 从哪个 mutation 来
- 当前 profile 的 writeback policy 是什么
- 最终 commit/defer 结论是什么

一次性表达出来了。

---

## 10. 这层到底“新增 / 改了 / 翻译了”什么

### 10.1 新增了什么

新增了第一版显式 writeback commitment 语义：

- 哪些是 committed
- 哪些是 deferred
- 哪些仍 blocked

### 10.2 改了什么

把 `machine_mutation` 里“副作用类型”的结论，改写成“当前能不能提交”的结论。

### 10.3 翻译了什么

它把：

- `NO_MUTATION`
  - 翻译成 `COMMITTED_NO_OP`
- `DEFERRED_REGISTER_RESULT`
  - 翻译成 `DEFERRED_REGISTER_WRITEBACK`
- `DEFERRED_LOCAL_SLOT`
  - 翻译成 `DEFERRED_LOCAL_WRITEBACK`
- `DEFERRED_GLOBAL_SLOT`
  - 翻译成 `DEFERRED_GLOBAL_WRITEBACK`
- `DEFERRED_CALL_EFFECT`
  - 翻译成 `DEFERRED_CALL_WRITEBACK`

所以 writeback 层本质上在做的是：

`effect taxonomy -> commitment taxonomy`

---

## 11. 和上下游的边界

## 11.1 相对 `machine_mutation`

`machine_mutation` 说：

- 欠的是 register/local/global/call 哪类 effect

`machine_writeback` 说：

- 这些 effect 现在是否已经能 commit

所以一句话区分：

- `mutation`：what is pending
- `writeback`：is it committed now

## 11.2 相对后面的 `machine_commit`

`machine_writeback` 当前还没有真正改：

- launch registers
- runtime memory bytes
- external world

所以它更像：

`commit policy artifact`

而不是最终应用动作本身。

后面的 `machine_commit` 才会继续把“已经决定的 writeback 语义”推进成更明确的 committed artifact。

---

## 12. 一段伪代码看完整主线

```text
build_writeback(mutation):
  verify(mutation)

  if mutation == no-mutation:
    return (committed-no-op, no-op)

  if mutation == deferred-register-result:
    return (deferred-register-writeback, register)

  if mutation == deferred-local-slot:
    return (deferred-local-writeback, local-slot)

  if mutation == deferred-global-slot:
    return (deferred-global-writeback, global-slot)

  if mutation == deferred-call-effect:
    return (deferred-call-writeback, call)

  if mutation == blocked-on-control:
    return (blocked-on-control, none)

  if mutation == blocked-unsupported:
    return (blocked-unsupported, none)
```

这个伪代码看起来很简单，但它表达的是一条非常关键的工程边界：

`先把 commit policy 说清楚，再决定未来是否真的落地架构写回。`

---

## 13. 读代码时最推荐盯的点

建议重点看：

- `machine_writeback_build_from_machine_mutation_file(...)`
  - 看映射表本体
- `machine_writeback_verify_file(...)`
  - 看合同反推逻辑
- `machine_writeback_dump_file(...)`
  - 看对外暴露哪些字段
- `tests/machine/runtime/machine_writeback/machine_writeback_test.c`
  - 看 halt / jump / store / call 四类最小例子

---

## 14. 一句话总结

`machine_writeback` 的核心价值，是在 `machine_mutation` 之后明确给出第一版 commit policy：halt 这种 control-only completion 可以记成 `committed-no-op`，而 register/local/global/call 这些真实数据副作用目前都还只能诚实地保持 `deferred-*`。  
