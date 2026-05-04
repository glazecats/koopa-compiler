# Machine Observe Lesson（compiler_lab）

> 目标：解释 `machine_observe` 为什么会出现在 `machine_apply` 之后，它当前到底新增了什么“exact-state vs preview-state observation”语义，`src/machine/observe/machine_observe/` 里真实在做什么，以及为什么从这一层开始目录也从 `runtime/` 跨到了独立的 `observe/` 主线。

## 一句话定位

`machine_observe` 是 runtime 主线正式跨进 observe 主线的第一层，它开始回答“对外现在到底能观察到 exact 还是 preview state”。

## 常见误解

- 误解一：apply 之后就已经等于最终可观察状态。
  - observe 这里才正式把 exact/preview observation contract 对外讲清楚。
- 误解二：observe 只是把 apply 的结果再打印一下。
  - 这一层开始明确建立 downstream observation artifact，而不只是 runtime 内部状态。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_apply_lesson.md`
2. `lesson/machine/runtime/machine_commit_lesson.md`

因为 observe 的前提就是：

- runtime/apply 线已经形成 committed/deferred/application-plan 视角
- 现在要决定这些结果对外观察时算 exact 还是 preview

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_delta_lesson.md`
2. `lesson/machine/observe/machine_trace_lesson.md`

因为 observe 之后最自然的问题就是：

- origin step 和 observed state 摆在一起时变化是什么
- 再怎么把这些变化提升成 execution record

---

## 最近同步

最近这层最值得同步的，是它现在和上游 provenance 链接得更紧了。

当前 observe 最好再多记两点：

1. **它已经不是纯 runtime 内部视图的“外壳”**
   结合最近 image/runtime 侧开始承接 ELF provenance，这里最好把 observe 理解成：
   - exact / preview state 的外部观察边界
   - 也是后面 delta/trace/event/outcome/... 整条记录链的正式起点

2. **lesson 主线上它的“目录跨线意义”比以前更重了**
   现在不只是“代码在 observe/ 目录”，而是：
   - 这里开始，关注点真的从 runtime-internal state 转到 downstream consumer artifact

所以这层现在除了“exact vs preview”，还要再补一句：

- `machine_observe` 是当前 provenance-aware downstream observation chain 的第一层

---

## 导学

如果说：

- `machine_apply` 回答的是“已经 applied 了没有；如果没有，application plan 和 preview 是什么”

那么：

- `machine_observe` 回答的是“对外观察时，这份东西到底算 exact observed state，还是只算 preview observed state”

它当前的核心分类非常清楚：

- `exact-state`
- `preview-state`
- `blocked-on-control`
- `blocked-unsupported`

这层还有一个非常重要的结构变化：

- 代码已经不在 `src/machine/runtime/`
- 而是在 `src/machine/observe/`

这意味着项目自己也在强调：

`从这里开始，重点已经不是“执行内部怎么推进”，而是“对外怎么观察这份结果”`

同样要带着目标方向看：

- 现在的 `generic-elf32` / `i386-preview` 是 staging surface
- 最终目的地是 `RISC-V`

所以 `machine_observe` 可以理解成：

`在真正 RISC-V architectural execution 完整落地前，先给出一个 honest observed-state surface`

---

## 1. 为什么需要 `machine_observe`

到 `machine_apply` 为止，我们已经知道：

- 某些 case 已经 `applied-state`
- 某些 case 还是 `pending-* application`
- 某些 case 有 preview state

但我们还不知道：

- 对观察者来说，哪些是 exact
- 哪些只是 preview
- blocked family 是否还能提供 observed state

所以 `machine_observe` 的边界是：

`application plan -> observation artifact`

它把“内部 apply 计划”翻译成“对外可观察语义”。

---

## 2. 为什么要从 `runtime/` 跨到 `observe/`

这一点特别值得在 lesson 里点出来。

前面的：

- `machine_state`
- `machine_mutation`
- `machine_writeback`
- `machine_commit`
- `machine_apply`

都还在 `src/machine/runtime/`

因为它们还主要在处理：

- 状态推进
- 副作用分类
- commit / apply 计划

但 `machine_observe` 开始，仓库把它放到：

- `src/machine/observe/machine_observe/`

这说明项目明确把这里当成另一条 sibling 主线：

`execution-internal layers`
vs
`observation/reporting layers`

所以我们在 lesson 里也要把边界讲清楚：

- `machine_apply`
  - 更偏内部 application plan
- `machine_observe`
  - 更偏 external observation artifact

---

## 3. 文件定位

- 接口：`include/machine/observe.h`
- 实现：`src/machine/observe/machine_observe/machine_observe.c`
- 测试：`tests/machine/observe/machine_observe/machine_observe_test.c`
- 规划文档：`docs/backend/MACHINE_OBSERVE_PLAN.md`

这层当前已经有：

- file / report lifecycle
- verifier
- apply -> observe lowering
- current fetch summary
- report / overview artifact
- dump
- 从 `apply / commit / step / machine_ir` 往上游桥接

---

## 4. `include/machine/observe.h`：数据模型

## 4.1 `MachineObserveResolutionKind`

当前 resolution kind：

- `NONE`
- `EXACT_STATE`
- `PREVIEW_STATE`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

这就是整层 lesson 最核心的表。

如果只记一句话，可以记成：

`observe 层在回答：这份 state 到底 exact 还是 preview。`

## 4.2 `MachineObserveKind`

现在 kind 很保守，只有：

- `NONE`
- `STATE`

这说明当前第一版 observe 层没有想做复杂“观察种类” taxonomy，而是先把 state observation 本身立住。

## 4.3 `MachineObserveFile`

可以先记成：

```text
MachineObserveFile =
  MachineApplyFile
  + observe resolution/kind
  + observed_state fields
```

也就是：

- `has_observed_state`
- `observed_status`
- `program_counter`
- `stack_pointer`
- `current_fetch`

## 4.4 `MachineObserveSummary`

这个 summary 会把整条链继续串下来：

- observe resolution / kind
- apply resolution / kind
- commit / writeback / mutation / state / transition / action
- payload bytes / immediate hint
- exact 标记
- observed state
- targets / return immediate

所以这层已经是一个很强的“最终 lesson artifact”：

你看一条 observe dump，几乎能直接追溯整条上游执行链。

---

## 5. build 主线：`apply -> observe`

核心入口是：

- `machine_observe_build_from_machine_apply_file(...)`

逻辑其实非常清爽：

```text
verify apply
clone apply

if apply == APPLIED_STATE:
  observe = EXACT_STATE
  observed_state = applied_state

if apply in pending register/local/global/call:
  observe = PREVIEW_STATE
  observed_state = preview_state

if apply == BLOCKED_ON_CONTROL:
  blocked-on-control

if apply == BLOCKED_UNSUPPORTED:
  blocked-unsupported
```

这段代码的价值恰恰在于它不复杂。

因为它明确表达了当前项目的态度：

- applied-state 才算 exact
- pending family 只能算 preview
- blocked family 不伪造 observed state

---

## 6. verifier 在保什么边界

`machine_observe_verify_file(...)` 非常适合讲“exact 和 preview 不是文字游戏”。

## 6.1 `EXACT_STATE`

如果 resolution 是 `EXACT_STATE`，要求：

- apply resolution 必须是 `APPLIED_STATE`
- `observe_kind = STATE`
- `has_observed_state = 1`
- observed fields 必须和 apply 的 `applied_state` 对齐

所以 exact 的定义非常严格：

`只有上游已经 applied 的 state，才能被观察成 exact-state`

## 6.2 `PREVIEW_STATE`

如果 resolution 是 `PREVIEW_STATE`，要求：

- apply 必须是以下之一：
  - `PENDING_REGISTER_APPLICATION`
  - `PENDING_LOCAL_APPLICATION`
  - `PENDING_GLOBAL_APPLICATION`
  - `PENDING_CALL_APPLICATION`
- `observe_kind = STATE`
- `has_observed_state = 1`
- observed fields 必须和 apply 的 `preview_state` 对齐

所以 preview 也不是拍脑袋猜，而是严格绑定到 apply layer 已经给出的 preview。

## 6.3 blocked families

对于：

- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

要求：

- `observe_kind = NONE`
- `has_observed_state = 0`

这说明当前 observe 层仍然坚持一个原则：

`没有 honest observation，就不要为了好看造一个假的 observed state`

---

## 7. 测试里的关键例子

## 7.1 `load-local`：preview-state

主线测试 dump：

```text
observe: resolution=preview-state
kind=state
apply=pending-register-application
commit=deferred-register-commit
payload=[]
imm=-
exact=no
has-state=yes
status=ready
pc=0x1001
current-byte=0x8a
```

这条非常适合说明：

- 上游还没真正 register apply
- 但观察层已经能给出一份 preview observed state

## 7.2 `return-imm`：exact-state

halt case：

```text
observe: resolution=exact-state
kind=state
apply=applied-state
commit=committed-state
writeback=committed-no-op
payload=[0x17]
imm=7
exact=yes
status=halted
pc=0x1000
return-imm=7
```

这一条能很好地把三层串起来：

- `writeback`: committed-no-op
- `commit`: committed-state
- `apply`: applied-state
- `observe`: exact-state

## 7.3 `jump`：blocked-on-control

测试 dump：

```text
observe: resolution=blocked-on-control
kind=none
payload=[0x01]
imm=1
exact=no
has-state=no
targets=[1]
```

这里 lesson 要强调：

- payload / hint 可以带着
- 但 blocked control 不会得到 observed state

## 7.4 `store-local-imm` / `store-global-imm` / `call-void-imm`

测试分别得到 preview-state：

- `pending-local-application -> preview-state`
- `pending-global-application -> preview-state`
- `pending-call-application -> preview-state`

这些例子特别适合说明 observe 层的核心 boundary：

`只要还没 applied，就只能观察成 preview。`

## 7.5 `test_machine_observe_custom_step_cases`

这组 case 的价值在于，它把观察层最重要的四种结论都锁得很完整：

- `applied-state` -> `exact-state`
- `pending-* application` -> `preview-state`
- `blocked-on-control` -> `blocked-on-control`
- `blocked-unsupported` -> `blocked-unsupported`

也就是说，这组测试几乎就是这层 lesson 的分类总表。

## 7.6 `test_machine_observe_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_observe`
- `i386-preview` 下 `exact / preview / blocked` 这套 observation 结论依然稳定

这很适合提醒读者：

`machine_observe` 当前锁住的是“观察诚实性”，而不是某个特定 ISA 的最终执行细节。`

---

## 8. 这层到底“新增 / 改了 / 翻译了”什么

### 8.1 新增了什么

新增了第一版显式 observation 语义：

- exact observed state
- preview observed state
- blocked observation

### 8.2 改了什么

把 `machine_apply` 里的“已经应用 / 待应用 / preview”结论，改写成“对外可观察时 exact 还是 preview”的结论。

### 8.3 翻译了什么

它把：

- `APPLIED_STATE`
  - 翻译成 `EXACT_STATE`
- `PENDING_*_APPLICATION`
  - 翻译成 `PREVIEW_STATE`
- `BLOCKED_*`
  - 继续保留为 blocked observation

---

## 9. 和上下游的边界

## 9.1 相对 `machine_apply`

- `machine_apply`
  - 关注内部 application plan
- `machine_observe`
  - 关注外部 observation result

## 9.2 相对后面的 `machine_delta`

`machine_observe` 到这里为止还只是：

- 给出一份 exact 或 preview 的 observed state

它还没回答：

- 新旧状态之间的差异是什么
- 哪些字段发生了 delta
- 如何记录 trace / event / outcome

这些才是后面的：

- `machine_delta`
- `machine_trace`
- `machine_event`

开始接的东西。

---

## 10. 和 RISC-V 目标的关系

如果将来真的做 RISC-V：

- register 写回真正发生后
- stack / slot / global memory 真正被改后
- call side effect 真正被应用后

observe 层就会成为“对外看到的 RISC-V state”入口。

而在当前阶段，它先非常诚实地把观察分成：

- exact
- preview

这样就不会把 preview 误当成真实 RISC-V architectural state。

---

## 11. 一段伪代码看完整主线

```text
build_observe(apply):
  verify(apply)

  if apply == applied-state:
    return exact-state {
      observed = applied_state
    }

  if apply in pending register/local/global/call:
    return preview-state {
      observed = preview_state
    }

  if apply == blocked-on-control:
    return blocked-on-control

  if apply == blocked-unsupported:
    return blocked-unsupported
```

---

## 12. 一句话总结

`machine_observe` 的核心价值，是把 `machine_apply` 的内部 application 结论翻译成外部 observation 结论：已经 applied 的才是 `exact-state`，还只是 pending application 的都只能诚实地作为 `preview-state` 观察出来，这也正是当前通往 RISC-V 最终执行模型之前最重要的“不要把 preview 当 exact”边界。  
