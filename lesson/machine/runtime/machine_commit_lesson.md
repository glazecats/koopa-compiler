# Machine Commit Lesson（compiler_lab）

> 目标：解释 `machine_commit` 为什么会出现在 `machine_writeback` 之后，它当前到底新增了什么“committed artifact”语义，`src/machine/runtime/machine_commit/` 里真实在做什么，以及为什么这层现在只把 `committed-no-op` 提升成真正的 committed state，而不会假装 register/local/global/call 已经真的落地。

## 一句话定位

`machine_commit` 是 commit-vs-defer 判断第一次被正式收束成 committed artifact 的层。

## 常见误解

- 误解一：commit 这里已经把所有架构状态都真实落地了。
  - 当前这层仍然很保守，只把一小部分 committed-no-op 场景提升成 committed state。
- 误解二：writeback 已经等于 commit。
  - writeback 更偏判断，commit 更偏 committed artifact 成形。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_writeback_lesson.md`
2. `lesson/machine/runtime/machine_mutation_lesson.md`

因为 commit 的前提就是：

- 你已经知道哪些 effect 现在能 committed
- 现在才问能不能产出真正 committed artifact

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_apply_lesson.md`
2. `lesson/machine/observe/machine_observe_lesson.md`

因为 commit 之后最自然的问题就是：

- 这份 committed/deferred 结果接下来意味着什么 application plan
- 然后对外观察时，它又算 exact 还是 preview

---

## 最近同步

最近这层最值得同步的，是它现在更明确地成为：

- committed judgement
- 到 application/observation

之间那层 committed artifact authority。

当前最好再多记两点：

1. **commit 不是 writeback 的重命名**
   它现在更像一层专门负责“把可 committed 的东西正式落成 artifact”的边界。

2. **后面的 apply/observe 都默认把 commit 当成固定上游**
   所以它不只是 runtime 链里的一个中间词，而是 downstream observation chain 的重要前置层。

---

## 导学

如果说：

- `machine_writeback` 回答的是“现在能不能把某类 effect 当作 committed”

那么：

- `machine_commit` 回答的是“既然做了这个 commit/defer 判断，能不能产出一份真正的 committed artifact”

它当前做的事情比 `machine_writeback` 再具体一点，但依然很保守。

它不会直接去做：

- register file 更新
- runtime memory byte 更新
- call side effect 模拟

它做的是：

`writeback classification -> committed artifact classification`

也就是把上一层再翻译成：

- `committed-state`
- `deferred-register-commit`
- `deferred-local-commit`
- `deferred-global-commit`
- `deferred-call-commit`
- `blocked-on-control`
- `blocked-unsupported`

这里有个项目方向提醒也值得直接记住：

- `generic-elf32` / `i386-preview` 只是当前验证 surface
- 这条 backend 的最终目标架构是 `RISC-V`

所以我们讲这层时，要把它理解成：

`为将来的 RISC-V architectural application 预留 committed-artifact 边界`

---

## 1. 为什么还要单独有 `machine_commit`

`machine_writeback` 已经告诉我们：

- `no-mutation` 可以当成 `committed-no-op`
- register/local/global/call 目前还是 deferred
- blocked family 仍 blocked

但它还没有回答：

- 哪些东西现在已经能称为一个“committed execution artifact”
- 哪些只是 commit policy，不该被误认为真实已提交状态

所以 `machine_commit` 的边界是：

`writeback policy -> committed artifact`

这听起来像只多走一步，但工程上很有价值，因为它把：

- “策略判断”
- “最终 artifact 形状”

分开了。

---

## 2. 这层和 `machine_writeback` 的区别

一句话说：

- `machine_writeback`
  - 关注的是“commit 还是 defer”
- `machine_commit`
  - 关注的是“有没有一份 committed artifact 已经成立”

比如 halt 这类 case：

- 在 `machine_writeback`
  - 只是 `committed-no-op`
- 到了 `machine_commit`
  - 才被正式提升成 `committed-state`

所以 lesson 里最好把这层理解成：

`policy verdict -> artifact verdict`

---

## 3. 文件定位

- 接口：`include/machine/commit.h`
- 实现：`src/machine/runtime/machine_commit/machine_commit.c`
- 测试：`tests/machine/runtime/machine_commit/machine_commit_test.c`
- 规划文档：`docs/backend/MACHINE_COMMIT_PLAN.md`

这层当前已经有：

- file / report lifecycle
- verifier
- writeback -> commit lowering
- current fetch summary
- report / overview artifact
- dump
- 从 `writeback / mutation / state / transition / interp / payload / decode / step / machine_ir` 往上游桥接

所以它已经是一层完整 contract，不只是 roadmap 名字。

---

## 4. `include/machine/commit.h`：数据模型

## 4.1 `MachineCommitResolutionKind`

当前 resolution kind：

- `NONE`
- `COMMITTED_STATE`
- `DEFERRED_REGISTER_COMMIT`
- `DEFERRED_LOCAL_COMMIT`
- `DEFERRED_GLOBAL_COMMIT`
- `DEFERRED_CALL_COMMIT`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

这套命名和 `machine_writeback` 很像，但语义已经从：

- writeback family

变成：

- committed artifact family

## 4.2 `MachineCommitKind`

这层也保留了更抽象的 `commit kind`：

- `NONE`
- `STATE`
- `REGISTER`
- `LOCAL_SLOT`
- `GLOBAL_SLOT`
- `CALL`

最重要的一项是 `STATE`。

因为这说明当前第一版 `machine_commit` 真正能产出的 committed artifact，不是“某个寄存器更新成功”，而是：

`这一整份状态现在可以被当作 committed state`

## 4.3 `MachineCommitFile`

可以先记成：

```text
MachineCommitFile =
  MachineWritebackFile
  + resolution_kind
  + commit_kind
  + committed_state fields
```

也就是在上一层基础上，多了：

- `has_committed_state`
- `committed_status`
- `program_counter`
- `stack_pointer`
- `current_fetch`

所以 commit 层已经重新把“状态字段”显式托起来了。

## 4.4 `MachineCommitSummary`

这层 summary 继续把上游全串下来：

- commit resolution / kind
- writeback resolution / commit kind
- mutation / state / transition / action
- raw/tag/known/name/bytes
- committed state / current fetch
- targets / return immediate

从教学角度可以把它理解成：

```text
commit summary =
  上游整条执行链上下文
  + 现在有没有 committed state artifact
```

---

## 5. build 主线：`writeback -> commit`

核心入口是：

- `machine_commit_build_from_machine_writeback_file(...)`

它的伪代码非常适合直接记：

```text
verify writeback
clone writeback

switch writeback resolution:
  COMMITTED_NO_OP:
    require state_summary.has_state
    build committed-state

  DEFERRED_REGISTER_WRITEBACK:
    deferred-register-commit

  DEFERRED_LOCAL_WRITEBACK:
    deferred-local-commit

  DEFERRED_GLOBAL_WRITEBACK:
    deferred-global-commit

  DEFERRED_CALL_WRITEBACK:
    deferred-call-commit

  BLOCKED_ON_CONTROL:
    blocked-on-control

  BLOCKED_UNSUPPORTED:
    blocked-unsupported
```

这里最关键的是第一条：

`COMMITTED_NO_OP` 不会直接变成一个抽象标签，而会真正读取上游 state summary，把 committed state 字段物化出来。`

---

## 6. verifier 在保什么边界

`machine_commit_verify_file(...)` 很值得读，因为它把“哪些事情已经 honest committed”说得很死。

## 6.1 `COMMITTED_STATE`

如果 resolution 是 `COMMITTED_STATE`，verifier 要求：

- 上一层必须是 `MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP`
- `has_committed_state = 1`
- 上游 `state_summary.has_state = 1`
- `commit_kind = MACHINE_COMMIT_KIND_STATE`
- `committed_status / pc / sp / current_fetch`
  - 必须和上游 `state_summary` 一致
- 如果有 current fetch
  - 还要再次检查 segment、memory offset、virtual address 和 byte value 的一致性

这说明：

`machine_commit` 不是把 halt 随便换个名字，它是真的把上游 state artifact 重新认证成 committed artifact。`

## 6.2 deferred register/local/global/call

对于：

- `DEFERRED_REGISTER_COMMIT`
- `DEFERRED_LOCAL_COMMIT`
- `DEFERRED_GLOBAL_COMMIT`
- `DEFERRED_CALL_COMMIT`

verifier 的要求很直接：

- 必须来自对应的 writeback deferred family
- `commit_kind` 必须匹配
- `has_committed_state` 必须为假

也就是说，这层现在非常诚实：

- 它知道这类东西属于 register/local/global/call
- 但它拒绝谎称“已经 commit 成真实状态”

## 6.3 blocked families

`BLOCKED_ON_CONTROL` 和 `BLOCKED_UNSUPPORTED` 也一样：

- 必须来自对应 blocked writeback family
- `commit_kind = NONE`
- `has_committed_state = 0`

---

## 7. 测试里最值得记的几个例子

## 7.1 `load-local`

主线测试 dump：

```text
commit: resolution=deferred-register-commit
kind=register
writeback=deferred-register-writeback
mutation=deferred-register-result
name=load-local
has-committed-state=no
```

这说明：

- `machine_writeback` 已经知道这是 register writeback
- `machine_commit` 再往前推进一步，只是把它正式命名成 register-commit family
- 但 still 没有 committed state

## 7.2 `return-imm`

测试里的 halt case 非常重要：

```text
commit: resolution=committed-state
kind=state
writeback=committed-no-op
mutation=no-mutation
transition=halt
name=return-imm
has-committed-state=yes
status=halted
pc=0x1000
return-imm=7
```

这条正好说明整层存在的意义：

- 上一层只有 `committed-no-op`
- 这一层才把它提升成真正的 `committed-state`

## 7.3 `jump`

测试得到：

```text
commit: resolution=blocked-on-control
kind=none
has-committed-state=no
targets=[1]
```

lesson 里要强调：

- current backend line 还没有 block target -> final runtime state 的完整落地
- 所以 control blocked 不能被误判成 committed

## 7.4 `store-local-imm` / `store-global-imm` / `call-void-imm`

测试分别得到：

- `deferred-local-commit`
- `deferred-global-commit`
- `deferred-call-commit`

这些例子很好用来讲：

`commit 层已经把它们提升成“commit family”，但还没把它们变成 committed state。`

## 7.5 `test_machine_commit_custom_step_cases`

这组 case 最有价值的地方在于，它把主线 happy path 以外的几种分支都锁住了：

- `return-imm` -> `committed-state`
- `jump` -> `blocked-on-control`
- `store-local/global/call` -> 各自对应的 deferred commit family

所以 lesson 里可以把它理解成：

`machine_commit` 不是只会把 halt case 提升成 committed state，它也会把其余 effect family 清楚地分流到各自的 deferred/bocked 桶里。`

## 7.6 `test_machine_commit_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_commit`
- `i386-preview` 下 `committed-state / deferred-register/local/global/call / blocked-*` 这整套 committed-artifact 结论依然成立

这说明这层当前的 contract 主要是：

`artifact honesty`

而不是：

`某个特定 ISA 的最终提交细节`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了第一版显式 committed artifact 语义：

- 哪些已经构成 committed state
- 哪些只是 deferred commit family

### 8.2 改了什么

把 `machine_writeback` 的 commit/defer verdict，改写成“有没有 committed artifact 成立”的 verdict。

### 8.3 翻译了什么

它把：

- `COMMITTED_NO_OP`
  - 翻译成 `COMMITTED_STATE`
- `DEFERRED_REGISTER_WRITEBACK`
  - 翻译成 `DEFERRED_REGISTER_COMMIT`
- `DEFERRED_LOCAL_WRITEBACK`
  - 翻译成 `DEFERRED_LOCAL_COMMIT`
- `DEFERRED_GLOBAL_WRITEBACK`
  - 翻译成 `DEFERRED_GLOBAL_COMMIT`
- `DEFERRED_CALL_WRITEBACK`
  - 翻译成 `DEFERRED_CALL_COMMIT`

---

## 9. 和上下游的边界

## 9.1 相对 `machine_writeback`

- `machine_writeback`
  - 主要在说 commit policy
- `machine_commit`
  - 主要在说 committed artifact 是否成立

## 9.2 相对 `machine_apply`

`machine_commit` 还没有开始回答：

- 如果是 deferred register/local/global/call，后面要怎么 apply
- 有没有 preview state 可供后续层观察
- payload/immediate hint 要不要继续往后带

这些是 `machine_apply` 才开始显式回答的问题。

---

## 10. 和 RISC-V 目标的关系

这层虽然现在还只是在 generic/preview profile 上验证，但它的工程价值其实正好和最终 `RISC-V` 目标相关：

- 将来真正做 RISC-V register writeback
- 真正做 stack/local/global slot application
- 真正做 call side effect application

都需要一个明确边界来区分：

- “现在只是 deferred commit family”
- “这里已经是 committed state”

所以 `machine_commit` 可以看作：

`RISC-V 真正 architectural apply 之前的最后一层 committed-artifact 缓冲带`

---

## 11. 一段伪代码看完整主线

```text
build_commit(writeback):
  verify(writeback)

  if writeback == committed-no-op:
    return committed-state {
      committed_state = upstream state snapshot
    }

  if writeback == deferred-register-writeback:
    return deferred-register-commit

  if writeback == deferred-local-writeback:
    return deferred-local-commit

  if writeback == deferred-global-writeback:
    return deferred-global-commit

  if writeback == deferred-call-writeback:
    return deferred-call-commit

  if writeback == blocked-on-control:
    return blocked-on-control

  if writeback == blocked-unsupported:
    return blocked-unsupported
```

---

## 12. 一句话总结

`machine_commit` 的核心价值，是把 `machine_writeback` 的“策略判断”再推进成“artifact 判断”：只有 `committed-no-op` 会被提升成真正的 `committed-state`，而 register/local/global/call 这些未来要落到 RISC-V architectural 模型里的家伙，现在仍然只保持为 deferred commit families。  
