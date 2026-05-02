# Machine Apply Lesson（compiler_lab）

> 目标：解释 `machine_apply` 为什么会出现在 `machine_commit` 之后，它当前到底新增了什么“application plan”语义，`src/machine/runtime/machine_apply/` 里真实在做什么，以及为什么这层开始显式带上 payload/immediate hint 和 preview state。

## 一句话定位

`machine_apply` 是 committed/deferred runtime result 第一次被正式讲成“后续如何应用到架构状态”的层。

## 常见误解

- 误解一：apply 这里已经把所有状态真正应用完了。
  - 当前它更准确地说是在建立 application plan 和 preview state。
- 误解二：commit 之后就已经可以直接观察最终状态。
  - observe 这条线还要继续回答 exact-vs-preview observation contract。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_commit_lesson.md`
2. `lesson/machine/runtime/machine_writeback_lesson.md`

因为 apply 的前提就是：

- committed/deferred 结果已经建立
- 现在才问这些结果对后续真正应用意味着什么计划

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_observe_lesson.md`
2. `lesson/machine/observe/machine_delta_lesson.md`

因为 apply 之后最自然的问题就是：

- 对外观察时这份结果算 exact 还是 preview
- origin 和 observed state 摆在一起时变化又是什么

---

## 导学

如果说：

- `machine_commit` 回答的是“有没有 committed artifact”

那么：

- `machine_apply` 回答的是“这份 committed/deferred 结果，对后续真正应用到架构状态意味着什么计划”

这层的关键词是：

- `applied-state`
- `pending-register-application`
- `pending-local-application`
- `pending-global-application`
- `pending-call-application`
- `blocked-*`

同时它还新增了两类非常关键的信息：

- `payload_bytes`
- `immediate_hint`

以及：

- `applied_state`
- `preview_state`

这意味着它不只是继续换标签，而是开始显式给后续层提供：

`如果现在还不能真正 apply，至少我先把 application plan 和 preview 暴露出来`

同样提醒一下目标方向：

- `generic-elf32` / `i386-preview` 只是阶段性 surface
- 真正终点是 `RISC-V`

所以 `machine_apply` 这层很像是在为将来真正的 RISC-V architectural application engine 预留“计划层”。

---

## 1. 为什么要有 `machine_apply`

到 `machine_commit` 为止，我们已经知道：

- 哪些已经是 committed state
- 哪些还是 deferred register/local/global/call commit

但我们还不知道：

- 对于 deferred family，后面到底要 apply 什么
- 当前能不能给一个 preview state
- payload / immediate 这些执行相关提示，要不要保留给后续层

所以 `machine_apply` 的边界是：

`commit artifact -> application plan`

这层是第一次明确在说：

- 这条指令已经 applied 了没有
- 如果没有，欠的是哪种 application
- 现在能不能先给一个 preview state

---

## 2. 这层和 `machine_commit` 的区别

一句话区分：

- `machine_commit`
  - 判断 committed artifact 是否成立
- `machine_apply`
  - 判断这份 artifact 对后续架构应用意味着什么计划

特别是 deferred families：

- 在 `machine_commit`
  - 只是 `deferred-register-commit` 之类的名字
- 到了 `machine_apply`
  - 会进一步变成
    - `pending-register-application`
    - `pending-local-application`
    - `pending-global-application`
    - `pending-call-application`

所以它是在把“commit family”翻成“apply family”。

---

## 3. 文件定位

- 接口：`include/machine/apply.h`
- 实现：`src/machine/runtime/machine_apply/machine_apply.c`
- 测试：`tests/machine/runtime/machine_apply/machine_apply_test.c`
- 规划文档：`docs/backend/MACHINE_APPLY_PLAN.md`

这层当前已经有：

- file / report lifecycle
- verifier
- commit -> apply lowering
- payload/immediate hint surfacing
- applied/preview state surfacing
- report / overview artifact
- dump
- 从 `commit / step / machine_ir` 往上游桥接

---

## 4. `include/machine/apply.h`：数据模型

## 4.1 `MachineApplyResolutionKind`

当前 resolution kind：

- `NONE`
- `APPLIED_STATE`
- `PENDING_REGISTER_APPLICATION`
- `PENDING_LOCAL_APPLICATION`
- `PENDING_GLOBAL_APPLICATION`
- `PENDING_CALL_APPLICATION`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

lesson 里最值得背的是：

- `APPLIED_STATE`
- 各种 `PENDING_*_APPLICATION`

因为它们正好定义了这层的核心身份：

`已经应用`
vs
`还只是应用计划`

## 4.2 `MachineApplyKind`

apply kind 也很直观：

- `STATE`
- `REGISTER`
- `LOCAL_SLOT`
- `GLOBAL_SLOT`
- `CALL`

它几乎就是把“后面要 apply 到哪里”直接写在类型里。

## 4.3 `MachineApplyFile`

可以先记成：

```text
MachineApplyFile =
  MachineCommitFile
  + resolution/apply_kind
  + payload bytes
  + immediate hint
  + applied state
  + preview state
```

这里比前几层新出的重点就是：

- `payload_byte_count`
- `payload_bytes[3]`
- `has_immediate_hint`
- `immediate_hint`
- `has_applied_state`
- `has_preview_state`

### 为什么 payload/immediate 很重要

因为后续如果真要做 RISC-V 寄存器/内存/调用应用，光知道“这是 pending local”还不够。

还需要知道：

- payload 里到底带了什么
- immediate hint 是多少

这层就是第一次把这些信息显式托出来。

---

## 5. build 主线：`commit -> apply`

核心入口是：

- `machine_apply_build_from_machine_commit_file(...)`

它做的事可以分成两段看。

### 5.1 先抽 payload / immediate hint

这段代码会：

- 从 `payload_decode` 里拿 `payload_byte_count` 和 `payload_bytes`
- 默认把 `payload_summary.immediate_value` 作为 immediate hint
- 如果 `interp_summary.has_return_immediate`
  - 就用 `return_immediate` 覆盖 immediate hint

所以 lesson 里可以直接说：

`machine_apply` 是第一层开始把“后面 apply 可能会用到的操作数提示”显式留下来。`

### 5.2 再做 resolution 映射

大致逻辑是：

```text
if commit == COMMITTED_STATE:
  resolution = APPLIED_STATE
  applied_state = committed_state
  preview_state = state_summary

if commit == DEFERRED_REGISTER_COMMIT:
  resolution = PENDING_REGISTER_APPLICATION
  preview_state = state_summary

if commit == DEFERRED_LOCAL_COMMIT:
  resolution = PENDING_LOCAL_APPLICATION
  preview_state = state_summary

if commit == DEFERRED_GLOBAL_COMMIT:
  resolution = PENDING_GLOBAL_APPLICATION
  preview_state = state_summary

if commit == DEFERRED_CALL_COMMIT:
  resolution = PENDING_CALL_APPLICATION
  preview_state = state_summary

if commit == BLOCKED_ON_CONTROL:
  blocked-on-control

if commit == BLOCKED_UNSUPPORTED:
  blocked-unsupported
```

最重要的新增就是：

- `committed-state` 会落成 `applied-state`
- deferred families 虽然还没 apply，但能带一个 preview state

---

## 6. verifier 在保什么边界

`machine_apply_verify_file(...)` 是这一层最值得细读的代码。

## 6.1 payload hint 必须和上游一致

它先检查：

- `payload_byte_count`
- `payload_bytes`
- `has_immediate_hint`
- `immediate_hint`

必须和：

- `payload_summary`
- `interp_summary.return_immediate`

推导出来的结果一致。

所以这里不是“随手附带一些提示”，而是：

`payload / immediate hint 也是这层 public contract 的一部分`

## 6.2 `APPLIED_STATE`

如果 resolution 是 `APPLIED_STATE`，要求：

- commit 必须是 `COMMITTED_STATE`
- `apply_kind = STATE`
- `has_applied_state = 1`
- `has_preview_state = 1`
- applied state 必须和 commit summary 对齐
- preview state 必须和上游 `state_summary` 对齐

这里很重要的一点是：

`即使已经 applied，preview 也仍然保留。`

这样后续层就能同时看到：

- 已应用结果
- 上游 state 视角

## 6.3 pending register/local/global/call

对于各类 pending application，verifier 要求：

- commit resolution 必须匹配
- `has_applied_state = 0`
- `has_preview_state = 1`
- 上游 `state_summary.has_state = 1`
- preview fields 必须和 `state_summary` 对齐

这说明这层的核心诚实性是：

- 我还没真的 apply
- 但我能给一个 honest preview

## 6.4 blocked families

对于 blocked control / unsupported：

- `has_applied_state = 0`
- `has_preview_state = 0`

也就是说，当前系统不会为了“看起来完整”去伪造 preview。

---

## 7. 测试里的关键例子

## 7.1 `load-local`

主线测试 dump：

```text
apply: resolution=pending-register-application
kind=register
payload=[]
imm=-
has-applied-state=no
has-preview-state=yes
preview-pc=0x1001
preview-byte=0x8a
```

这条最适合说明：

- 当前还没有真正 register application
- 但后续如果要 apply，这里已经给了一个 preview state

## 7.2 `return-imm`

halt case：

```text
apply: resolution=applied-state
kind=state
payload=[0x17]
imm=7
has-applied-state=yes
applied-status=halted
has-preview-state=yes
preview-status=halted
return-imm=7
```

这条同时展示了三件事：

1. `committed-state` 会被提升成 `applied-state`
2. `return-imm` 会作为 immediate hint 暴露
3. even halt case 也保留 preview state

## 7.3 `jump`

测试 dump：

```text
apply: resolution=blocked-on-control
kind=none
payload=[0x01]
imm=1
has-applied-state=no
has-preview-state=no
targets=[1]
```

这里特别适合拿来讲：

- payload 和 immediate 可以保留
- 但 blocked-control 不能伪造 preview state

## 7.4 `store-local-imm`

测试 dump：

```text
apply: resolution=pending-local-application
kind=local-slot
payload=[0x07]
imm=7
has-preview-state=yes
preview-pc=0x1002
preview-byte=0xaa
```

这说明 `machine_apply` 不只是“local slot pending”，还带出了：

- 立即数提示
- 预览状态

## 7.5 `store-global-imm`

测试 dump：

```text
apply: resolution=pending-global-application
kind=global-slot
payload=[0x05]
imm=5
has-preview-state=yes
```

## 7.6 `call-void-imm`

测试 dump：

```text
apply: resolution=pending-call-application
kind=call
payload=[0x02]
imm=2
has-preview-state=yes
```

这两条都说明：

`apply` 层已经开始给 future application engine 组织“计划数据”。`

## 7.7 `test_machine_apply_custom_step_cases`

这组 case 的课堂价值很高，因为它正好把几条最能代表 `apply` 层的分支都锁住了：

- `return-imm` -> `applied-state`
- `jump` -> `blocked-on-control`
- `store-local-imm` -> `pending-local-application`
- `store-global-imm` -> `pending-global-application`
- `call-void-imm` -> `pending-call-application`

也就是说，这组测试几乎就在替 lesson 回答：

`commit` 层的几大 verdict，到 `apply` 层以后分别会变成哪一种 application plan。`

## 7.8 `test_machine_apply_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_apply`
- `i386-preview` 下 payload bytes / immediate hint / preview state 这些 surface 依然稳定存在

所以这一层真正被锁住的不是“generic 下一条 happy path”，而是：

`pending application 计划这件事本身的稳定性。`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了 application plan 语义：

- applied state
- pending register/local/global/call application
- payload bytes
- immediate hint
- preview state

### 8.2 改了什么

把 `machine_commit` 的 committed/deferred 结论，改写成“现在已经应用”还是“将来怎么应用”的结论。

### 8.3 翻译了什么

它把：

- `COMMITTED_STATE`
  - 翻译成 `APPLIED_STATE`
- `DEFERRED_REGISTER_COMMIT`
  - 翻译成 `PENDING_REGISTER_APPLICATION`
- `DEFERRED_LOCAL_COMMIT`
  - 翻译成 `PENDING_LOCAL_APPLICATION`
- `DEFERRED_GLOBAL_COMMIT`
  - 翻译成 `PENDING_GLOBAL_APPLICATION`
- `DEFERRED_CALL_COMMIT`
  - 翻译成 `PENDING_CALL_APPLICATION`

同时把 payload/immediate/state preview 一起往后搬。

---

## 9. 和上下游的边界

## 9.1 相对 `machine_commit`

- `machine_commit`
  - 有没有 committed artifact
- `machine_apply`
  - 这个 artifact 现在是否已经 applied，如果没 applied，后续计划是什么

## 9.2 相对 `machine_observe`

`machine_apply` 到这里为止还没回答：

- 对外观察时，这个状态算 exact 还是 preview
- 统一的 observed-state artifact 长什么样

这就是 `machine_observe` 的事情。

---

## 10. 和 RISC-V 目标的关系

如果最终要做 RISC-V，这一层会非常自然地成为：

- register write application plan
- memory slot application plan
- call application plan

的统一前置层。

现在虽然还没有真的改 RISC-V 寄存器或内存，但 lesson 里最好把它理解成：

`RISC-V architectural apply engine 之前的计划表达层`

---

## 11. 一段伪代码看完整主线

```text
build_apply(commit):
  verify(commit)
  payload_hint = payload bytes + immediate

  if commit == committed-state:
    return applied-state {
      applied = committed state
      preview = upstream state
    }

  if commit == deferred-register-commit:
    return pending-register-application {
      preview = upstream state
    }

  if commit == deferred-local-commit:
    return pending-local-application {
      preview = upstream state
    }

  if commit == deferred-global-commit:
    return pending-global-application {
      preview = upstream state
    }

  if commit == deferred-call-commit:
    return pending-call-application {
      preview = upstream state
    }

  if commit is blocked:
    return blocked without preview
```

---

## 12. 一句话总结

`machine_apply` 的核心价值，是在 `machine_commit` 之后第一次把“后续要怎么真的作用到架构状态”这件事显式化：已经完成的变成 `applied-state`，没完成的变成 `pending-* application`，同时把 payload/immediate hint 和 preview state 一并准备好，给未来的 RISC-V 真正 application engine 使用。  
