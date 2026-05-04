# Machine Delta Lesson（compiler_lab）

> 目标：解释 `machine_delta` 为什么会出现在 `machine_observe` 之后，它当前到底新增了什么“before/after delta”语义，`src/machine/observe/machine_delta/` 里真实在做什么，以及这层为什么只给 coarse change flags，不会假装已经知道完整 register/memory 字节级 diff。

## 一句话定位

`machine_delta` 是 observe 主线里第一次把 origin 和 observed state 正式摆成 before/after 变化对的层。

## 常见误解

- 误解一：delta 这里已经有完整寄存器/内存字节级 diff。
  - 当前这层故意只给 coarse change flags。
- 误解二：observe 和 delta 的差别只是名字不同。
  - observe 更偏“现在看到的是什么状态”，delta 更偏“它和起点相比变了什么”。

## 前置阅读

最推荐先读：

1. `lesson/machine/observe/machine_observe_lesson.md`
2. `lesson/machine/runtime/machine_apply_lesson.md`

因为 delta 的前提就是：

- observed state 已经建立
- 你也已经知道 origin/apply 侧有哪些 preview/exact 边界

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/observe/machine_trace_lesson.md`
2. `lesson/machine/observe/machine_event_lesson.md`

因为 delta 之后最自然的问题就是：

- 这份变化如何被讲成一步 execution record
- 再怎么继续归类成事件族

---

## 最近同步

最近这层最值得同步的，是它现在更明确地成为：

- observe exact/preview state
- 到 execution record / event / outcome / history

之间的固定变化层。

当前最好再多记两点：

1. **delta 不再只是 observe 的附属 summary**
   它现在更像后面 trace/event/outcome 整条语义收束链的标准 before/after 输入。

2. **coarse change flags 的克制反而更像主动边界**
   不是“暂时没做细 diff”，而是当前主线刻意只承诺 coarse delta。

---

## 导学

如果说：

- `machine_observe` 回答的是“现在对外看到的是 exact state 还是 preview state”

那么：

- `machine_delta` 回答的是“把 origin step 和 observed state 摆在一起看，变化到底是什么”

它的关键词是：

- `exact-state-delta`
- `preview-state-delta`
- `blocked-on-control`
- `blocked-unsupported`

同时这层第一次显式给出 change flags：

- `status_changed`
- `program_counter_changed`
- `stack_pointer_changed`
- `fetch_changed`

所以它不再只是“看到一个结果”，而是开始回答：

`从 before 到 after，到底变了哪里`

同样继续记住总方向：

- 当前 `generic-elf32` / `i386-preview` 只是阶段性 surface
- 最终目的地仍然是 `RISC-V`

所以这层可以理解成：

`在真正 RISC-V execution/mutation engine 完整落地前，先给出一个 honest coarse delta artifact`

---

## 1. 为什么需要 `machine_delta`

`machine_observe` 已经告诉我们：

- 这是 exact-state 还是 preview-state
- 当前 observed state 是什么

但它还没有直接回答：

- 和 origin step 相比，status 有没有变
- `pc` 有没有变
- `sp` 有没有变
- current fetch 有没有变

所以 `machine_delta` 的边界是：

`observed state -> origin-vs-observed delta`

这层的意义很大，因为后面所有：

- trace
- event
- outcome

其实都要建立在“变化是什么”这个事实上。

---

## 2. 这层和 `machine_observe` 的区别

一句话区分：

- `machine_observe`
  - 关心“现在看到的状态是什么”
- `machine_delta`
  - 关心“和原始 step 相比发生了什么变化”

所以 delta 层不是单纯复制 observed state，而是给 observed state 配上 origin 基线。

---

## 3. 文件定位

- 接口：`include/machine/delta.h`
- 实现：`src/machine/observe/machine_delta/machine_delta.c`
- 测试：`tests/machine/observe/machine_delta/machine_delta_test.c`
- 规划文档：`docs/backend/MACHINE_DELTA_PLAN.md`

这层当前已经有：

- file / report lifecycle
- verifier
- observe -> delta lowering
- origin-vs-observed summary
- coarse change flags
- report / overview artifact
- dump
- 从 `observe / apply / commit / step / machine_ir` 往上游桥接

---

## 4. `include/machine/delta.h`：数据模型

## 4.1 `MachineDeltaResolutionKind`

当前 resolution kind：

- `NONE`
- `EXACT_STATE_DELTA`
- `PREVIEW_STATE_DELTA`
- `BLOCKED_ON_CONTROL`
- `BLOCKED_UNSUPPORTED`

这里很适合直接对应上一层：

- `EXACT_STATE`
  - 往下变成 `EXACT_STATE_DELTA`
- `PREVIEW_STATE`
  - 往下变成 `PREVIEW_STATE_DELTA`

## 4.2 `MachineDeltaKind`

目前 kind 还是很保守：

- `NONE`
- `STATE`

说明第一版 delta 层不做细粒度子类扩张，而是先把“状态变化”这个主 artifact 建好。

## 4.3 `MachineDeltaFile`

可以先记成：

```text
MachineDeltaFile =
  MachineObserveFile
  + delta resolution/kind
  + observed state fields
```

注意这层虽然名字叫 delta，但 raw file 里没有真的单独存一份“大 diff 列表”。

它存的是：

- observed state
- 然后在 summary 里通过 origin step header 计算 change flags

## 4.4 `MachineDeltaSummary`

这层的 summary 才是真正重点：

- 上游 observe/apply/commit/... 整条链
- `origin_status / origin_pc / origin_sp / origin_fetch`
- `observed_status / observed_pc / observed_sp / observed_fetch`
- `status_changed`
- `program_counter_changed`
- `stack_pointer_changed`
- `fetch_changed`

所以从 lesson 角度，可以直接把它理解成：

```text
delta summary =
  before
  + after
  + 变化标志
```

---

## 5. build 主线：`observe -> delta`

核心入口是：

- `machine_delta_build_from_machine_observe_file(...)`

逻辑很直接：

```text
verify observe
clone observe

if observe == EXACT_STATE:
  resolution = EXACT_STATE_DELTA
  observed_state = observe.observed_state

if observe == PREVIEW_STATE:
  resolution = PREVIEW_STATE_DELTA
  observed_state = observe.observed_state

if observe == BLOCKED_ON_CONTROL:
  blocked-on-control

if observe == BLOCKED_UNSUPPORTED:
  blocked-unsupported
```

真正的“delta 感”不是在 build 分支里，而是在 summary 计算里。

---

## 6. 真正的 delta 是怎么计算出来的

`machine_delta_file_get_delta_summary(...)` 很关键。

它会把：

- step header 里的 origin 信息
- 当前 observed state

拼起来，并计算：

- `status_changed`
- `program_counter_changed`
- `stack_pointer_changed`
- `fetch_changed`

例如：

```text
status_changed =
  origin status != observed status

program_counter_changed =
  origin pc != observed pc

stack_pointer_changed =
  origin sp != observed sp

fetch_changed =
  是否有 fetch 的状态变了
  或者 segment/byte/offset 变了
```

这里很值得讲的一点是：

- 它只做 coarse flag
- 不做 bytewise memory diff
- 不做 register-by-register diff

这正是当前层的诚实边界。

---

## 7. verifier 在保什么边界

`machine_delta_verify_file(...)` 很适合拿来讲“exact delta / preview delta”的区别。

## 7.1 `EXACT_STATE_DELTA`

如果 resolution 是 `EXACT_STATE_DELTA`，要求：

- 上游 observe 必须是 `EXACT_STATE`
- `delta_kind = STATE`
- `has_observed_state = 1`
- observed fields 必须和 observe summary 对齐
- `is_exact_delta = true`
- 四个 coarse change flags 都必须存在

注意这里不是说四个变化都一定是 `yes`，而是：

- 都必须有判断结果

## 7.2 `PREVIEW_STATE_DELTA`

要求：

- 上游 observe 必须是 `PREVIEW_STATE`
- `delta_kind = STATE`
- `has_observed_state = 1`
- observed fields 必须和 observe 对齐
- `is_exact_delta = false`
- 四个 coarse change flags 同样必须存在

也就是说 preview delta 不是“不知道”，而是：

`我知道一个 preview before/after 变化`

## 7.3 blocked families

对于 blocked control / unsupported：

- `has_observed_state = 0`
- 各个 change flag 都不存在

所以 blocked case 的 lesson 边界非常清楚：

`没有 honest after-state，就没有 honest delta`

---

## 8. 测试里的关键例子

## 8.1 `load-local`

主线测试 dump：

```text
delta: resolution=preview-state-delta
kind=state
observe=preview-state
name=load-local
exact=no
origin-pc=0x1000
origin-byte=0x1c
observed-pc=0x1001
observed-byte=0x8a
status-changed=no
pc-changed=yes
fetch-changed=yes
```

这条很适合讲：

- 当前还是 preview delta
- 但 `pc` 和 fetch 已经能明确看出变化

## 8.2 `return-imm`

halt case：

```text
delta: resolution=exact-state-delta
kind=state
observe=exact-state
name=return-imm
exact=yes
origin-status=ready
observed-status=halted
status-changed=yes
pc-changed=no
fetch-changed=yes
return-imm=7
```

这里非常适合讲三件事：

1. exact observe 才能得到 exact delta
2. halt 会导致 `status_changed=yes`
3. `pc` 不一定变，但 fetch 还是会变

## 8.3 `jump`

测试 dump：

```text
delta: resolution=blocked-on-control
kind=none
exact=no
observed-status=-
status-changed=-
pc-changed=-
fetch-changed=-
targets=[1]
```

这说明 blocked control 的 delta 层不会伪造任何 after-state comparison。

## 8.4 `store-local-imm` / `store-global-imm` / `call-void-imm`

这些测试都得到：

- `preview-state-delta`
- `pc-changed=yes`
- `fetch-changed=yes`

它们特别适合说明：

`当前 delta 层已经能统一表达这些 pending family 的“前后变化轮廓”，但还不声称自己知道真实 architectural memory/register diff。`

## 8.5 `test_machine_delta_custom_step_cases`

这组 case 的课堂价值很高，因为它把几种最关键的分支都锁住了：

- `return-imm` -> `exact-state-delta`
- `jump` -> `blocked-on-control`
- `store-local/global/call` -> `preview-state-delta`

也就是说，这组测试几乎就在替 lesson 说明：

`observe` 层分成的 exact / preview / blocked 三大类，到 delta 层以后会怎样变成 before/after change artifact。`

## 8.6 `test_machine_delta_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_delta`
- `i386-preview` 下 `exact / preview / blocked` 这套 delta contract 不会因为目标 profile 变化而失真

这说明这层真正被锁住的是：

`change honesty`

而不是某个特定 ISA 的最终细粒度差异表达。

---

## 9. 这层到底“新增 / 改了 / 翻译了”什么

### 9.1 新增了什么

新增了第一版显式 delta 语义：

- exact delta
- preview delta
- coarse change flags

### 9.2 改了什么

把 `machine_observe` 的 observed state 结论，改写成带有 before/after 对照的 change artifact。

### 9.3 翻译了什么

它把：

- `EXACT_STATE`
  - 翻译成 `EXACT_STATE_DELTA`
- `PREVIEW_STATE`
  - 翻译成 `PREVIEW_STATE_DELTA`
- `BLOCKED_*`
  - 保持成 blocked delta

同时把 origin step 信息引入进来。

---

## 10. 和上下游的边界

## 10.1 相对 `machine_observe`

- `machine_observe`
  - 现在看到的 after-state 是什么
- `machine_delta`
  - 相对 origin，到底变了什么

## 10.2 相对 `machine_trace`

`machine_delta` 到这里还没把变化解释成“执行记录”。

后面的 `machine_trace` 才会进一步说：

- 这是 exact-trace 还是 preview-trace
- 变化属于哪种 coarse trace change class

---

## 11. 和 RISC-V 目标的关系

真正做到 RISC-V 时，delta 层理论上可以更丰富：

- 寄存器哪些位变了
- 内存哪些地址变了
- ABI 相关状态哪里变了

但当前仓库非常克制，只给：

- status / pc / sp / fetch 这类 coarse delta

这种做法是对的，因为它避免把 preview surface 错讲成已经知道完整 RISC-V architectural diff。

---

## 12. 一段伪代码看完整主线

```text
build_delta(observe):
  verify(observe)

  if observe == exact-state:
    delta = exact-state-delta
    after = observed state

  if observe == preview-state:
    delta = preview-state-delta
    after = observed state

  compare origin step vs after:
    status_changed
    pc_changed
    sp_changed
    fetch_changed

  if blocked:
    no observed state
    no change flags
```

---

## 13. 一句话总结

`machine_delta` 的核心价值，是在 `machine_observe` 之后第一次把 before/after 对照正式做成 artifact：它不去伪造完整 register/memory diff，只诚实地给出 exact/preview delta 和 coarse change flags，为后面的 trace/event 铺路。  
