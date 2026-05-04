# Machine Mutation Lesson（compiler_lab）

> 目标：解释 `machine_mutation` 为什么会出现在 `machine_state` 之后，它当前到底新增了什么“non-control mutation classification”语义，`src/machine/runtime/machine_mutation/` 里真实在做什么，以及这层如何把 ready/halt/control/unsupported 进一步翻译成 register / slot / call / blocked 这几类副作用。

## 一句话定位

`machine_mutation` 是 applied state 结果第一次被正式翻译成“还欠哪些非控制副作用”的层。

## 常见误解

- 误解一：mutation 这里已经把副作用真正写回了。
  - 当前更准确地说，它先做 mutation effect classification。
- 误解二：state 已经把该表达的都表达完了。
  - state 更偏状态快照，mutation 才开始讲 register/slot/call/blocked effect family。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_state_lesson.md`
2. `lesson/machine/runtime/machine_transition_lesson.md`

因为 mutation 的前提就是：

- applied state snapshot 已经形成
- 现在要问的是这条指令还欠哪类非控制副作用

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_writeback_lesson.md`
2. `lesson/machine/runtime/machine_commit_lesson.md`

因为 mutation 之后最自然的问题就是：

- 哪些 effect 现在已经能 committed
- 哪些仍然必须 defer

---

## 最近同步

最近这层最值得同步的，是 mutation family 现在更像：

- state 之后的正式副作用分类面

而不只是给 writeback 临时看的标签。

当前最好再多记两点：

1. **register/local/global/call/blocked 这几类 effect 现在更像固定 taxonomy**
2. **后面的 writeback/commit/apply 都默认建立在这层 effect family 之上**

所以它现在不只是“状态之后再分一层”，而是后面整条 deferred/committed/application 语义链的上游分类器。

---

## 导学

如果说：

- `machine_state` 回答的是“下一份状态有没有成立”

那么：

- `machine_mutation` 回答的是“这条指令还欠哪些非控制副作用”

这层最重要的新增不是新 PC，也不是 next fetch。

它新增的是：

`state artifact -> mutation family classification`

也就是把状态进一步翻译成：

- `no-mutation`
- `deferred-register-result`
- `deferred-local-slot`
- `deferred-global-slot`
- `deferred-call-effect`
- `blocked-on-control`
- `blocked-unsupported`

这份讲义建议按下面顺序读：

1. 先理解 `machine_mutation` 和 `machine_state` 的边界
2. 再看 `include/machine/mutation.h`
3. 再看 `machine_mutation_classify_known_op(...)` 和 build / verify 主线
4. 最后用测试里的 `load-local / store-local-imm / store-global-imm / call-void-imm / halt / jump / unsupported` 七个例子记住整个分类表

学完你应该能：

1. 解释为什么 `machine_state` 之后还需要单独一层 `machine_mutation`
2. 说明这层真正新增的是“副作用分类”，不是执行提交
3. 读懂 ready op family 为什么会被分成 register/local/global/call 四类
4. 知道为什么 `machine_writeback` 必须建立在这层之后

---

## 1. 为什么需要 `machine_mutation`

到了 `machine_state`，我们已经知道：

- 当前有没有 state
- 是 ready 还是 halted
- control transfer 是不是 deferred

但我们还不知道：

- 这条 ready 指令是不是只产生一个 value result
- 它是不是要写 local slot
- 它是不是要写 global slot
- 它是不是 call，有额外 effect
- halt / blocked-control / unsupported 在 mutation 语义上分别该怎么记

所以 `machine_mutation` 的边界是：

`applied state snapshot -> explicit non-control effect classification`

这层的重点不是“真的写回”，而是先把欠着的 effect 说清楚。

---

## 2. 为什么不能直接跳到 `machine_writeback`

因为如果没有 mutation 这一层，`machine_writeback` 就得同时负责：

- 识别 effect family
- 决定 commit 还是 defer

这会把两种不同工作揉在一起。

当前项目的拆法是：

- `machine_state`
  - 状态是不是已经成立
- `machine_mutation`
  - 成立之后还欠什么 effect
- `machine_writeback`
  - 这些 effect 哪些已经 commit，哪些继续 defer

也就是说：

- `mutation` 是 effect taxonomy
- `writeback` 是 commit policy

---

## 3. 文件定位

- 接口：`include/machine/mutation.h`
- 实现：`src/machine/runtime/machine_mutation/machine_mutation.c`
- 测试：`tests/machine/runtime/machine_mutation/machine_mutation_test.c`
- 规划文档：`docs/backend/MACHINE_MUTATION_PLAN.md`

这层当前已经有：

- public enums / structs
- verifier
- state -> mutation lowering
- report / overview artifact
- dump
- 从 `state / transition / interp / payload / decode / step / machine_ir` 往上游桥接

所以它不是只有一张设计图，而是已经有真实行为合同。

---

## 4. `include/machine/mutation.h`：数据模型

## 4.1 `MachineMutationResolutionKind`

当前 resolution kind：

- `NONE`
- `NO_MUTATION`
- `DEFERRED_REGISTER_RESULT`
- `DEFERRED_LOCAL_SLOT`
- `DEFERRED_GLOBAL_SLOT`
- `DEFERRED_CALL_EFFECT`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

lesson 里最应该背下来的就是中间这几项。

因为它们就是当前 runtime line 对“副作用”的第一版分类。

## 4.2 `MachineMutationEffectKind`

除了 resolution，这层还单独保留了 effect kind：

- `NONE`
- `CONTROL_ONLY`
- `VALUE_RESULT`
- `LOCAL_SLOT`
- `GLOBAL_SLOT`
- `CALL`

这两个字段一起看更容易懂：

- resolution 更像“处理状态”
- effect kind 更像“副作用类型”

例如：

- `DEFERRED_REGISTER_RESULT + VALUE_RESULT`
- `BLOCKED_ON_CONTROL + CONTROL_ONLY`
- `NO_MUTATION + CONTROL_ONLY`

## 4.3 `MachineMutationFile`

可以先把它记成：

```text
MachineMutationFile =
  MachineStateFile
  + mutation resolution
  + effect kind
```

所以这层没有自己重新发明 PC / segment / current byte。

它是基于上一层的 state，再补充一层 effect classification。

## 4.4 `MachineMutationSummary`

这个 summary 已经把很多前面层的信息都串起来了：

- state resolution
- transition resolution
- interp action
- payload kind
- decode tag class
- raw / tag / known / name / bytes
- has_state / pc / current byte
- targets / return immediate

这意味着 `machine_mutation` 已经是一个真正能独立讲解的 artifact：

你看一条 summary，基本就能把上游 context 和本层结论同时读出来。

---

## 5. 真正的分类表藏在哪

这层最关键的 helper 是：

- `machine_mutation_classify_known_op(...)`

它直接把 selected op family 分成四类。

### 5.1 register result

下面这些 op 会被归到：

- `DEFERRED_REGISTER_RESULT`
- `VALUE_RESULT`

包括：

- `COPY`
- `MATERIALIZE_IMM`
- `ALU`
- `ALU_IMM`
- `CMP`
- `CMP_IMM`
- `LOAD_LOCAL`
- `LOAD_GLOBAL`

这组 op 的共同点是：

`当前主要欠的是“结果值怎么写回/承接”`

### 5.2 local slot

- `STORE_LOCAL`
- `STORE_LOCAL_IMM`

会被归到：

- `DEFERRED_LOCAL_SLOT`
- `LOCAL_SLOT`

### 5.3 global slot

- `STORE_GLOBAL`
- `STORE_GLOBAL_IMM`

会被归到：

- `DEFERRED_GLOBAL_SLOT`
- `GLOBAL_SLOT`

### 5.4 call effect

- `CALL`
- `CALL_IMM`
- `CALL_SPILL`
- `CALL_IMM_SPILL`
- `CALL_VOID`
- `CALL_VOID_IMM`

会被归到：

- `DEFERRED_CALL_EFFECT`
- `CALL`

这一段代码几乎就是整层 lesson 的核心表格。

---

## 6. build 主线：`state -> mutation`

核心入口是：

- `machine_mutation_build_from_machine_state_file(...)`

它的主逻辑可以概括成：

```text
verify state
read state summary
read decode tag summary

switch state resolution:
  READY:
    classify_known_op(tag_value)

  HALTED:
    no-mutation + control-only

  DEFERRED_CONTROL_TRANSFER:
    blocked-on-control + control-only

  UNSUPPORTED:
    blocked-unsupported + none

clone state file
write resolution/effect
verify mutation file
```

这里最关键的不是代码量，而是分类边界非常清楚。

---

## 7. verifier 在保什么

`machine_mutation_verify_file(...)` 很值得读，因为它告诉我们：

`mutation classification 不是拍脑袋，是有上游形状约束的`

## 7.1 `READY` 必须是 op

如果 state 是 `READY`，verifier 要求：

- `payload_summary.kind == MACHINE_PAYLOAD_DECODE_KIND_OP`
- `decode_tag_summary.tag_class == MACHINE_DECODE_TAG_OP`
- tag value 必须能被 `machine_mutation_classify_known_op(...)` 分类

这意味着：

- ready state 不等于一定能随便生成 mutation
- 它必须真的是一个认识的 op family

## 7.2 `HALTED` 必须是 terminator + halt

如果 state 是 `HALTED`，要求：

- payload kind 是 `TERMINATOR`
- tag class 是 `TERMINATOR`
- `action_kind == HALT`

然后才会被归成：

- `NO_MUTATION`
- `CONTROL_ONLY`

也就是说：

`halt` 的 mutation 语义不是“什么都没发生”，而是“控制层已经完成，没有新的非控制 mutation 要处理”`

## 7.3 deferred control 也必须真的是 control transfer

如果 state 是 `DEFERRED_CONTROL_TRANSFER`，要求：

- payload kind 是 `TERMINATOR`
- tag class 是 `TERMINATOR`
- `action_kind == CONTROL_TRANSFER`

然后分类成：

- `BLOCKED_ON_CONTROL`
- `CONTROL_ONLY`

## 7.4 unsupported

`UNSUPPORTED` 会得到：

- `BLOCKED_UNSUPPORTED`
- `NONE`

这说明 unknown/unsupported 不会被错误塞进某个 effect family。

---

## 8. 七个测试例子，把整层记住

## 8.1 `load-local`

测试 dump：

```text
mutation: resolution=deferred-register-result
effect=value-result
transition=next-fetch
action=advance
name=load-local
pc=0x1001
current-byte=0x8a
```

这说明：

- `load-local` 的控制流已经 ready
- 但值结果还只是“deferred register result”

换句话说：

`状态能前进，但数据结果还没 commit`

## 8.2 `return-imm`

测试 dump：

```text
mutation: resolution=no-mutation
effect=control-only
transition=halt
action=halt
name=return-imm
status=halted
return-imm=7
```

这一条很重要，因为它说明：

- halt 不需要后续 register/local/global/call mutation
- 所以这层把它归成 `no-mutation`

## 8.3 `jump`

测试 dump：

```text
mutation: resolution=blocked-on-control
effect=control-only
transition=deferred-control-transfer
action=control-transfer
targets=[1]
```

这里 lesson 要讲清楚：

- 这不是“jump 没副作用”
- 而是“当前主要卡在 control resolution，还没到数据 mutation 那一步”

## 8.4 `unsupported`

测试 dump：

```text
mutation: resolution=blocked-unsupported
effect=none
```

也就是：

`系统连这条是什么都不确定，更别说 effect family 了`

## 8.5 `store-local-imm`

测试 dump：

```text
mutation: resolution=deferred-local-slot
effect=local-slot
name=store-local-imm
pc=0x1002
current-byte=0xaa
```

这里新增的讲课点是：

- `state` 只知道 ready
- `mutation` 进一步指出 ready 的后续工作是 local slot effect

## 8.6 `store-global-imm`

测试 dump：

```text
mutation: resolution=deferred-global-slot
effect=global-slot
name=store-global-imm
pc=0x1002
current-byte=0xab
```

这和 local slot 的区别正好能说明：

- 这层不只是笼统说“有 store”
- 而是已经把 local/global 分开

## 8.7 `call-void-imm`

测试 dump：

```text
mutation: resolution=deferred-call-effect
effect=call
name=call-void-imm
pc=0x1002
current-byte=0xac
```

这一条很关键，因为它说明 call family 已经被单独提升成一个 effect bucket，而不是混在 register/store 里。

另外，测试文件里还有两组很值得一起看。

## 8.8 `test_machine_mutation_custom_step_cases`

这组 case 的课堂价值很高，因为它不是只锁一条默认 happy path，而是把整张分类表压成了几条很直观的自定义 step 例子：

- `store-local-imm` -> `DEFERRED_LOCAL_SLOT`
- `store-global-imm` -> `DEFERRED_GLOBAL_SLOT`
- `call-void-imm` -> `DEFERRED_CALL_EFFECT`
- `jump` -> `BLOCKED_ON_CONTROL`
- 未知字节 -> `BLOCKED_UNSUPPORTED`

也就是说，这组测试几乎就在替 lesson 回答：

`ready/halt/control/unsupported 再往下一层，分别会落到哪一个 mutation bucket。`

## 8.9 `test_machine_mutation_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_mutation`
- `i386-preview` 下 register/local/global/call/control 这些 mutation family 不会因为 target profile 变化而失真

这很适合提醒读者：

`machine_mutation` 当前的第一版 effect taxonomy 主要按行为分类，不按 ISA 分叉分类。`

---

## 9. report 层新增了什么

这层也有完整 report：

- `MachineMutationReport`
- `MachineMutationReportOverviewArtifact`
- `machine_mutation_report_refresh(...)`

测试里的 report overview 是：

```text
report_overview:
  origin: state=ready status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000
  policy: profile=generic-elf32 control-only=yes register=yes slot=yes call=yes
  mutation: resolution=deferred-register-result effect=value-result has-state=yes status=ready pc=0x1001 targets=[] return-imm=-
```

这说明当前 report 层已经能把：

- 这层从哪个 state 来
- 这个 target profile 支持哪几类 policy
- 最后 mutation 怎么分类

一次性讲清楚。

---

## 10. 这层到底“新增 / 改了 / 翻译了”什么

### 10.1 新增了什么

新增了第一版显式 mutation family：

- register result
- local slot
- global slot
- call effect
- no-mutation
- blocked families

### 10.2 改了什么

把 `machine_state` 里“状态已经 ready / halted / deferred / unsupported”的结论，改写成“副作用处理还差哪一步”的结论。

### 10.3 翻译了什么

它把：

- `READY + load/copy/alu/...`
  - 翻译成 `DEFERRED_REGISTER_RESULT`
- `READY + store_local`
  - 翻译成 `DEFERRED_LOCAL_SLOT`
- `READY + store_global`
  - 翻译成 `DEFERRED_GLOBAL_SLOT`
- `READY + call`
  - 翻译成 `DEFERRED_CALL_EFFECT`
- `HALTED`
  - 翻译成 `NO_MUTATION`
- `DEFERRED_CONTROL_TRANSFER`
  - 翻译成 `BLOCKED_ON_CONTROL`

---

## 11. 和上下游的边界

## 11.1 相对 `machine_state`

`machine_state` 回答：

- 当前状态有没有成立

`machine_mutation` 回答：

- 状态成立以后，还有什么 effect family 待处理

所以：

- `state` 是控制流层面的状态成立
- `mutation` 是数据/副作用层面的待处理分类

## 11.2 相对 `machine_writeback`

`machine_mutation` 到这里为止，并没有说：

- register result 现在就能不能写回
- local/global slot 现在能不能真的提交
- call effect 现在能不能 commit

这些是 `machine_writeback` 的事。

一句话区分：

- `mutation`
  - 先说欠的是什么
- `writeback`
  - 再说这些欠项现在能不能 commit

---

## 12. 一段伪代码看完整主线

```text
build_mutation(state):
  verify(state)

  if state == READY:
    return classify_known_op(tag_value)

  if state == HALTED:
    return (no-mutation, control-only)

  if state == DEFERRED_CONTROL_TRANSFER:
    return (blocked-on-control, control-only)

  if state == UNSUPPORTED:
    return (blocked-unsupported, none)
```

更具体地说：

```text
classify_known_op(tag):
  if tag in {copy, imm, alu, cmp, load-local, load-global}:
    return deferred-register-result
  if tag in {store-local, store-local-imm}:
    return deferred-local-slot
  if tag in {store-global, store-global-imm}:
    return deferred-global-slot
  if tag in call family:
    return deferred-call-effect
```

---

## 13. 读代码时最推荐盯的点

建议重点看：

- `machine_mutation_classify_known_op(...)`
  - 这是整层分类表本体
- `machine_mutation_build_from_machine_state_file(...)`
  - 这是 lowering 主线
- `machine_mutation_verify_file(...)`
  - 这是合同边界
- `machine_mutation_dump_file(...)`
  - 这是对外暴露字段的最短路径

---

## 14. 一句话总结

`machine_mutation` 的核心价值，是把 `machine_state` 的 ready/halt/deferred/unsupported 结果，进一步翻译成“当前还欠哪一类非控制副作用”的显式分类；它还不负责 commit，只负责把欠账类型说清楚。  
