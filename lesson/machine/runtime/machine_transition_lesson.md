# Machine Transition Lesson（compiler_lab）

> 目标：解释 `machine_transition` 为什么会出现在 `machine_interp` 之后，它当前到底新增了什么“next fetch-state resolution”语义，`src/machine/runtime/machine_transition/` 里真实在做什么，以及这层代码里已经存在的 `next-fetch / halt / deferred-control-transfer / unsupported` 逻辑是什么。

## 一句话定位

`machine_transition` 是动作语义第一次被正式解析成“下一份 fetch-state 会怎样”的层。

## 常见误解

- 误解一：interp 既然知道动作，就已经等于知道下一状态。
  - transition 这里才开始明确 next-fetch / halt / deferred-control-transfer / unsupported 的分辨。
- 误解二：transition 已经在提交所有副作用。
  - 当前更准确地说，它先解析状态迁移方向，真正的状态应用和副作用分类还在后面。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_interp_lesson.md`
2. `lesson/machine/runtime/machine_payload_decode_lesson.md`

因为 transition 的前提就是：

- 当前字节序列已经被解释成动作语义
- 现在要问的是它如何落成下一份 fetch-state

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_state_lesson.md`
2. `lesson/machine/runtime/machine_mutation_lesson.md`

因为 transition 之后最自然的问题就是：

- 这份结果能不能成为一份 applied state snapshot
- 还欠哪些非控制副作用

---

## 导学

如果说：

- `machine_interp` 回答的是“当前这条字节序列意味着什么动作”

那么：

- `machine_transition` 回答的是“这个动作在当前第一版系统里如何被解析成下一份 fetch-state”

所以这层还不是 full state mutation，也不是 commit/apply。

它更准确的身份是：

`action intent -> next fetch-state resolution artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_interp` 后面还要单独有 `machine_transition`
2. 再看 `include/machine/transition.h`，建立 transition file / transition summary / next-fetch summary 的整体印象
3. 再看 `src/machine/runtime/machine_transition/machine_transition.c`，理解 interp 动作是怎么被解析成下一步的
4. 最后带着测试例子去看：
   - `next-fetch`
   - `halt`
   - `deferred-control-transfer`
   - `unsupported`

学完你应该能：

1. 解释 `machine_transition` 和 `machine_interp` / `machine_state` 的边界
2. 说明这层真正新增的是“下一份取指状态如何被解析出来”，不是完整状态提交
3. 看懂一个 `machine_interp -> machine_transition` 的最小 before/after
4. 说明为什么后面的 state/mutation/writeback 需要这层先把 next fetch 解析清楚

---

## 1. 为什么需要 `machine_transition`

`machine_interp` 已经回答了：

- 当前动作是 `advance` / `halt` / `control-transfer` / `unsupported`
- next status 是什么
- 可能的 next PC / target block / return immediate 是什么

但它还没有直接回答：

- 下一份 fetch-state 能不能现在就解析出来？
- 如果能，next byte 是多少？
- next current segment 是谁？
- next current byte 在 runtime memory 里的 offset 是多少？
- 如果是 control transfer，当前第一版要不要 defer？

所以 `machine_transition` 引入的真正边界是：

`action intent -> next-step fetch-state resolution`

这层最重要的新增信息不是动作类别，而是：

- `resolution_kind`
- `has_next_fetch`
- `next_program_counter`
- `next_current_segment_index`
- `next_current_byte`
- `next_current_byte_memory_offset`

---

## 2. 为什么不能把它塞回 `machine_interp`

`machine_interp` 负责的是：

- 当前动作是什么
- 当前动作意图里有哪些 target / immediate

但它不应该继续直接负责：

- 下一份 fetch-state 的具体验证
- next byte / next segment / next offset 的物化
- deferred control transfer policy

不然 `machine_interp` 会同时扮演：

- 动作解释层
- 转换解析层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_interp`
  - 回答“这条意味着什么动作”
- `machine_transition`
  - 回答“这个动作现在能不能解成下一份 fetch-state”
- `machine_state`
  - 再往下把 transition 结果纳入更完整状态视图

---

## 3. 文件定位

- 接口：`include/machine/transition.h`
- 实现：`src/machine/runtime/machine_transition/machine_transition.c`
- 测试：`tests/machine/runtime/machine_transition/machine_transition_test.c`
- 规划文档：`docs/backend/MACHINE_TRANSITION_PLAN.md`

当前这层也是一个 `.c` 文件，但职责很明确：

- raw file lifecycle
- verifier
- interp -> transition lowering
- next-fetch summary helper
- report / overview artifact
- dump
- bridge from interp/payload/decode/step/machine_ir

所以它已经是一个真正的 transition artifact 层。

---

## 4. `include/machine/transition.h`：当前数据模型

### 4.1 `MachineTransitionResolutionKind`

当前 resolution kind 有：

- `NONE`
- `NEXT_FETCH`
- `HALT`
- `DEFERRED_CONTROL_TRANSFER`
- `UNSUPPORTED`

这五类很重要，因为它们把当前第一版 transition 语义说得很清楚：

- 有些动作能直接解成下一份取指状态
- 有些动作会 halt
- control transfer 当前会 defer
- 有些东西仍然 unsupported

### 4.2 `MachineTransitionSummary`

这是这层最关键的数据结构。

它包含：

- `resolution_kind`
- `action_kind`
- `raw_byte`
- `tag_value`
- `is_known`
- `tag_name`
- `instruction_byte_count`
- `next_status`
- `has_next_fetch`
- `next_program_counter`
- `next_current_segment_index`
- `next_current_byte`
- `next_current_byte_memory_offset`
- control target
- return immediate

也就是说，这层把上一层的 action intent 推进成了：

`更接近下一份 step-state 的解析结果`

### 4.3 `MachineTransitionNextFetchSummary`

这层还专门有一个 next-fetch summary：

- `byte_virtual_address`
- `byte_memory_offset`
- `segment_index`
- `segment_name`
- `byte_value`

这说明当 resolution 是 `NEXT_FETCH` 时，当前系统已经不只知道：

- 下一条 PC 是多少

而且还知道：

- 下一条在哪个 segment
- 下一字节是什么

### 4.4 `MachineTransitionTargetPolicySummary`

当前 target policy 明确承认：

- `resolves_linear_next_fetch`
- `resolves_halt_transition`
- `defers_control_transfer_targets`

这三条非常适合 lesson 直接记，因为它们几乎就是当前设计边界的口语化版本。

### 4.5 `MachineTransitionFile`

可以先把它记成：

```text
MachineTransitionFile =
  MachineInterpFile
  + resolution_kind
  + next_status
  + has_next_fetch
  + next PC/segment/byte/offset
```

所以它本质上是：

`interp artifact + next-fetch resolution artifact`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_transition_build_from_machine_interp_file(...)`

它大致在做：

1. verify interp 输入
2. 看当前 `action_kind`
3. 如果是 `advance`
   - 解析出 next fetch
4. 如果是 `halt`
   - 解析成 halt
5. 如果是 `control-transfer`
   - 当前先归成 deferred
6. 如果是不支持
   - 归成 unsupported
7. 再 verify transition file

所以它真正做的是：

`把动作意图推进成“现在能不能落到下一份 step-state”`

### 5.1 verifier 在保什么边界

`machine_transition_verify_file(...)` 当前会按 resolution kind 做强检查：

- `NEXT_FETCH`
  - interp action 必须是 `ADVANCE`
  - `next_status` 必须是 `READY`
  - 必须有 `next_fetch`
  - next PC / next segment / next byte / next offset 必须互相一致
- `HALT`
  - interp action 必须是 `HALT`
  - `next_status` 必须是 `HALTED`
  - 不能再有 next fetch
- `DEFERRED_CONTROL_TRANSFER`
  - interp action 必须是 `CONTROL_TRANSFER`
  - 当前不直接解 next fetch

这说明 transition 层不是“给个标签看着玩”，而是：

`每种 resolution 都有很清晰的结构合同`

### 5.2 为什么 control transfer 现在要 defer

这是这层 lesson 里最值得直接讲的边界。

当前第一版 transition 很明确：

- 线性前进的 next fetch 可以直接算
- halt 可以直接定
- control transfer 先不立刻落成 next fetch，而是 defer

所以这一层没有假装自己已经能把所有控制流都执行完，而是很老实地说：

`control transfer target 目前先保留为 deferred artifact`

这对后面的 state/mutation 层非常重要，因为它们才能决定怎么把 deferred target 真正应用进状态。

---

## 6. 一个最小例子看“增加了什么”

最小线性前进例子可以压缩成：

```text
interp:
  action = advance
  next_program_counter = pc + 1

transition:
  resolution = next-fetch
  has_next_fetch = yes
  next_current_byte = memory[next_pc]
  next_current_segment = .text
```

这里相对 `machine_interp` 真正新增的是：

1. next PC 被推进成 next fetch byte
2. next segment ownership 被显式指出来
3. next byte offset 被显式指出来

而控制转移例子则更像：

```text
interp:
  action = control-transfer
  targets = [1,2]

transition:
  resolution = deferred-control-transfer
```

所以 lesson 口径上，这层的核心不是“再讲一遍动作”，而是：

`动作第一次被解析成下一份 fetch-state，或者被显式 defer`

---

## 7. report 为什么在这层特别有价值

到了 transition 层，下游最关心的问题已经像：

- 这个动作现在能不能直接得到 next fetch
- 如果能，next byte 是什么
- 如果不能，是 halt 还是 deferred transfer

所以 `MachineTransitionReport` 当前专门缓存：

- interp summary
- transition summary
- next fetch summary

这让后面的 `machine_state` / `machine_mutation` 不需要再从 interp 层自己重建下一步。

---

## 8. 测试里最值得读的几个点

`tests/machine/runtime/machine_transition/machine_transition_test.c` 当前最值得看的是三类故事。

### 8.1 next-fetch

最基础的正例会锁：

- `resolution=next-fetch`
- `next_current_byte`
- `next_current_segment`
- `next_current_byte_memory_offset`

这组断言里最有课堂味道的具体形状其实是：

```text
interp:
  action = advance
  next_program_counter = 0x1001

transition:
  resolution = next-fetch
  has_next_fetch = yes
  next_current_segment = .text
  next_current_byte = 0x8a
  next_current_byte_memory_offset = 0x1
```

也就是说，这层第一次把“next PC”推进成了“下一条当前字节真的是什么”。

### 8.2 halt

halt 类测试会锁：

- `resolution=halt`
- 没有 next fetch

这条测试最值得记的一点是：

`halt` 在 transition 层不只是一个标签，而是明确意味着“不再有 next fetch artifact”。`

### 8.3 deferred control transfer

控制流类测试会锁：

- `resolution=deferred-control-transfer`

这很重要，因为它把当前第一版保守边界显式 regression-lock 住了。

这条 case 最适合和前一层连起来看：

```text
interp:
  action = control-transfer
  targets = [1, 2]

transition:
  resolution = deferred-control-transfer
  has_next_fetch = no
```

意思非常明确：

- 已经知道跳转意图
- 但还没有诚实地解出下一条 fetch-state

另外，`test_machine_transition_i386_bridge` 也很值得看。

它会锁：

- profile-aware `MachineIrAllocateRewriteReport` 可以直接桥到 transition
- `next-fetch / halt / deferred-control-transfer / unsupported` 这套 resolution contract 在 `i386-preview` 下也不会变形

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_interp`

`machine_interp` 只到：

- 动作是什么
- 目标和 next PC 意图是什么

`machine_transition` 则进一步到：

- 这个动作现在能不能被解成下一份 fetch-state
- 如果能，下一份 fetch-state 的 byte/segment/offset 是什么

### 9.2 相对 `machine_state`

`machine_transition` 还没有做：

- 更完整的状态对象应用
- deferred effect 分类
- writeback / commit / apply

这些是后面 `machine_state` / `machine_mutation` / `machine_writeback` 才开始接的。

所以：

- `machine_transition`：先把下一份 fetch-state 解析出来或显式 defer
- `machine_state`：再把它吸收到更完整的状态视图里

---

## 10. 一句总记

`machine_transition` 不是完整状态提交器，而是在 `machine_interp` 之上第一次把“当前动作如何解析成下一份 fetch-state”做成 artifact：线性前进会落成 next-fetch，halt 会直接收束，而 control-transfer 当前会被显式保留成 deferred。`
