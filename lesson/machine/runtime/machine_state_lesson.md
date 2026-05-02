# Machine State Lesson（compiler_lab）

> 目标：解释 `machine_state` 为什么会出现在 `machine_transition` 之后，它到底新增了什么“applied state snapshot”语义，`src/machine/runtime/machine_state/` 里真实在做什么，以及这层代码为什么故意还不去碰完整 register / memory mutation。

## 一句话定位

`machine_state` 是 transition 结果第一次被正式收束成“可继续执行的状态快照”语境的层。

## 常见误解

- 误解一：state 这里已经把完整寄存器和内存都改好了。
  - 当前这层故意还不去碰完整 register/memory mutation。
- 误解二：transition 和 state 差不多，只是多包一层。
  - transition 更偏 next-fetch resolution，state 则更偏 applied state snapshot。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_transition_lesson.md`
2. `lesson/machine/runtime/machine_interp_lesson.md`

因为 state 的前提就是：

- 你已经知道动作语义怎样转成下一份 fetch-state
- 现在才讨论它是否足够稳定成可继续执行的状态快照

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_mutation_lesson.md`
2. `lesson/machine/runtime/machine_writeback_lesson.md`

因为 state 之后最自然的问题就是：

- 还欠哪些非控制副作用
- 哪些 effect 现在已经能算 committed / defer

---

## 导学

如果说：

- `machine_transition` 回答的是“这条指令的结果能不能解析成下一份 fetch-state”

那么：

- `machine_state` 回答的是“这份 transition 结果，能不能进一步变成一份可以拿来继续执行的状态快照”

它不是：

- full architectural state commit
- register writeback
- local/global memory store 生效

它当前只做第一步非常保守的事情：

`transition artifact -> applied state snapshot artifact`

这份讲义建议按下面顺序读：

1. 先理解 `machine_state` 和 `machine_transition` 的边界
2. 再看 `include/machine/state.h`，认识这层 public data model
3. 再看 `src/machine/runtime/machine_state/machine_state.c`，理解 ready / halted / deferred / unsupported 四条主线
4. 最后对照测试里的真实 dump，看这层到底“增加了什么”

学完你应该能：

1. 解释 `machine_state` 为什么不是重复做一遍 `machine_transition`
2. 说明这层新增的是“可应用状态快照”，不是 full mutation
3. 看懂 `MachineTransitionFile -> MachineStateFile` 的最小转换
4. 理解为什么 `machine_mutation` 必须建立在 `machine_state` 之后

---

## 1. 为什么需要 `machine_state`

`machine_transition` 已经告诉我们：

- 当前动作是不是 `advance`
- 是不是 `halt`
- control transfer 现在是不是先 defer
- 有没有 next fetch
- 如果有，下一条 byte 在哪

但它还没有明确回答：

- 现在能不能说“已经有一份 next state 了”
- 这份 next state 的 `status / pc / sp / current fetch` 是什么
- 哪些情况只有 transition meaning，还不能说已经形成 state

所以 `machine_state` 的真实边界是：

`transition resolution -> first applied execution-state snapshot`

这是一个很关键的“翻译动作”：

- `NEXT_FETCH`
  - 翻译成 `READY`
- `HALT`
  - 翻译成 `HALTED`
- `DEFERRED_CONTROL_TRANSFER`
  - 翻译成“还没有 state，只能保留 unresolved”
- `UNSUPPORTED`
  - 翻译成“没有 state，而且明确 unsupported”

换句话说，前一层在说：

`下一步怎么走`

这一层在说：

`下一步的状态是否已经成立`

---

## 2. 这层为什么不能塞回 `machine_transition`

如果把这层塞回 `machine_transition`，那 `machine_transition` 就会同时负责：

- transition 意义
- next fetch 解析
- state snapshot 应用

边界会糊掉。

现在仓库里的分层更清楚：

- `machine_interp`
  - 当前指令动作是什么意思
- `machine_transition`
  - 这个动作能不能解析出下一份 fetch-state
- `machine_state`
  - 这份 transition 结果能不能构成下一份 applied state
- `machine_mutation`
  - 这份 state 还欠哪些非控制副作用

所以 `machine_state` 不是重复，而是把“transition meaning”再推进一层，推进成“state meaning”。

---

## 3. 文件定位

- 接口：`include/machine/state.h`
- 实现：`src/machine/runtime/machine_state/machine_state.c`
- 测试：`tests/machine/runtime/machine_state/machine_state_test.c`
- 规划文档：`docs/backend/MACHINE_STATE_PLAN.md`

当前这层虽然还是单个 `.c` 文件，但职责已经很完整：

- file / report lifecycle
- state verifier
- transition -> state lowering
- current fetch summary
- report artifact
- dump
- 从 `transition / interp / payload / decode / step / machine_ir` 往上游桥接

所以它已经不是“临时 glue”，而是一层真正独立的 runtime sibling。

---

## 4. `include/machine/state.h`：数据模型

## 4.1 `MachineStateResolutionKind`

当前 resolution kind 有：

- `NONE`
- `READY`
- `HALTED`
- `DEFERRED_CONTROL_TRANSFER`
- `UNSUPPORTED`

它们不是随便起的名字，而是在明确回答：

- 这份状态是不是已经能继续执行
- 是不是已经停机
- 是不是还卡在 control target 没落成真实 `pc`
- 是不是根本不支持

## 4.2 `MachineStateFile`

可以先把它记成：

```text
MachineStateFile =
  MachineTransitionFile
  + resolution_kind
  + has_state
  + state_status
  + program_counter
  + stack_pointer
  + current_fetch
```

注意这里最重要的是 `has_state`。

因为这层不是默认认为每个 transition 都已经落成真实状态。

- `READY` / `HALTED`
  - `has_state = 1`
- `DEFERRED_CONTROL_TRANSFER` / `UNSUPPORTED`
  - `has_state = 0`

这就是这层最关键的诚实性。

## 4.3 `MachineStateSummary`

这个 summary 把三层信息揉在一起：

- 本层的 `resolution_kind`
- 上一层 `transition_resolution_kind`
- 更上层的 `action_kind`
- 当前 raw/tag/known/name/instruction bytes
- `state_status / pc / sp / current fetch`
- control targets / return immediate

也就是说，从 lesson 角度你可以把它理解成：

```text
state summary =
  "这条指令是什么"
  + "transition 怎么解析"
  + "state 最终有没有形成"
```

## 4.4 `MachineStateCurrentFetchSummary`

它包含：

- `byte_virtual_address`
- `byte_memory_offset`
- `segment_index`
- `segment_name`
- `byte_value`

这说明 `machine_state` 不只是说“pc 前进了”。

它还会把“当前取指点”变成一份可读 artifact。

## 4.5 `MachineStateTargetPolicySummary`

现在策略非常直接：

- `resolves_ready_state = 1`
- `resolves_halt_state = 1`
- `defers_control_transfer_state = 1`

这几乎就是当前设计的口语版：

- 顺序 next fetch 可以落成 ready state
- halt 可以落成 halted state
- control transfer 还不 pretending 成 ready state

---

## 5. lowering 主线：`transition -> state`

核心入口是：

- `machine_state_build_from_machine_transition_file(...)`

这段代码在 `machine_state.c` 里非常清楚。

它大致做：

```text
verify transition input
clone transition file
state_status = transition_summary.next_status

switch transition resolution:
  NEXT_FETCH:
    state = READY
    has_state = yes
    pc = next_program_counter
    sp = old step.sp
    current_fetch = next fetch fields

  HALT:
    state = HALTED
    has_state = yes
    pc = old step.pc
    sp = old step.sp
    no current fetch

  DEFERRED_CONTROL_TRANSFER:
    state = DEFERRED_CONTROL_TRANSFER
    has_state = no

  UNSUPPORTED:
    state = UNSUPPORTED
    has_state = no

verify output state file
```

你会发现它没有做复杂计算，重点是：

- 把上一层的 transition result 翻译成更严格的 state result
- 把能应用的情况真的物化出来
- 不能应用的情况明确不伪造

---

## 6. verifier 在保什么边界

`machine_state_verify_file(...)` 是这层最值得读的函数之一。

它不是只检查“字段有没有填”，而是在保证：

`state artifact 的诚实性`

### 6.1 `READY`

如果 resolution 是 `READY`，那 verifier 会要求：

- 上一层必须是 `MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH`
- `has_state` 必须为真
- `state_status` 必须是 `READY`
- `program_counter` 必须等于 transition 的 `next_program_counter`
- `stack_pointer` 必须保留原 `step_file->stack_pointer`
- 必须有 `current_fetch`
- `current_segment / current_byte / current_byte_memory_offset`
  - 必须和 transition 里的 next fetch 一致
- 当前 segment 必须存在且可执行
- `current_byte_memory_offset`
  - 必须等于 `program_counter - runtime_memory.base_virtual_address`
- 真的能从 runtime memory 里把 `current_byte` 读出来

这里最重要的不是某一个字段，而是 verifier 把：

- next PC
- memory 映射
- segment executable
- byte value

串成了一条完整合同。

所以 `READY` 不只是“PC 加一”。

它是：

`我真的能在 runtime image 里指出下一条要取的那个字节`

### 6.2 `HALTED`

如果 resolution 是 `HALTED`，verifier 要求：

- transition 必须是 `HALT`
- `has_state = 1`
- `state_status = HALTED`
- `program_counter` 维持原 step 的 PC
- `stack_pointer` 维持原 step 的 SP
- 不能再有 current fetch

所以 halt 的语义是：

`状态存在，但执行停止，不再有“下一条当前字节”`

### 6.3 `DEFERRED_CONTROL_TRANSFER`

verifier 要求：

- transition 必须也是 deferred control transfer
- `has_state = 0`
- `has_current_fetch = 0`
- `state_status` 要和 transition summary 里的 `next_status` 对齐

这说明当前项目明确拒绝做一件事：

`没有 block-target -> runtime-pc 映射时，假装已经得到了下一份 ready state`

### 6.4 `UNSUPPORTED`

要求也很直接：

- transition 必须是 unsupported
- `has_state = 0`
- `has_current_fetch = 0`

还是同一个原则：

`不知道，就别编`

---

## 7. 测试里最值得看的四个例子

## 7.1 例子一：`load-local` 变成 `READY`

`tests/machine/runtime/machine_state/machine_state_test.c` 里的主线例子会得到：

```text
state: resolution=ready
transition=next-fetch
action=advance
name=load-local
pc=0x1001
sp=0x4000
current-segment=0
current-byte=0x8a
```

这里新增了什么？

- 不是只知道这条 `load-local` 会 `advance`
- 而是已经知道下一份状态可以继续跑
- 并且下一条 byte 真的是 `0x8a`

可以把它写成 before / after：

```text
before (transition):
  advance
  next fetch exists
  next pc = 0x1001

after (state):
  resolution = ready
  has_state = yes
  status = ready
  current byte = 0x8a
```

## 7.2 例子二：`return-imm` 变成 `HALTED`

测试里把 step bytes 改成：

- tag `0x81`
- payload `0x17`

最后 dump 会出现：

```text
state: resolution=halted
transition=halt
action=halt
name=return-imm
has-state=yes
status=halted
pc=0x1000
current-segment=-
current-byte=-
return-imm=7
```

这里讲课时要强调：

- `machine_transition` 说“halt 了”
- `machine_state` 进一步说“halt 之后这份状态已经成立”

也就是说：

`halt` 不是没状态，而是 `halted state`

## 7.3 例子三：`jump` 只会得到 `DEFERRED_CONTROL_TRANSFER`

测试把当前字节改成：

- tag `0x84`
- payload `0x01`

结果是：

```text
state: resolution=deferred-control-transfer
transition=deferred-control-transfer
action=control-transfer
has-state=no
targets=[1]
```

这正是当前第一版的边界：

- 已经知道跳到 block `1`
- 但还没有 runtime `pc`
- 所以还不能诚实地说“下一份 state ready 了”

## 7.4 例子四：未知字节变成 `UNSUPPORTED`

把当前字节改成 `0x00` 后，测试期待：

```text
state: resolution=unsupported
transition=unsupported
action=unsupported
has-state=no
```

这说明这层不会去猜。

---

## 8. report 层新增了什么

除了 raw `MachineStateFile`，这层还补了 report：

- `MachineStateReport`
- `MachineStateReportOverviewArtifact`
- `machine_state_build_report_from_*`
- `machine_state_report_refresh(...)`

report 里多出来的是：

- header summary
- target policy summary
- transition report
- state summary
- current fetch summary

测试里的 report dump 也很说明问题：

```text
report_overview:
  origin: status=ready segment=0 mapped-bytes=8192 pc=0x1000 sp=0x4000
  policy: profile=generic-elf32 ready=yes halt=yes deferred-control=yes
  state: resolution=ready has-state=yes status=ready pc=0x1001 targets=[] return-imm=-
```

这说明当前 `machine_state` 已经不只是“能 lowering 出来”，而是已经有：

- 面向 lesson / 调试的 summary
- 面向后续 consumer 的 overview artifact

测试里还有两组特别值得顺手看。

### 8.1 `test_machine_state_custom_step_cases`

这组 case 会把主线以外最关键的几种状态都锁住：

- `return-imm` -> `HALTED`
- `jump` / branch family -> `DEFERRED_CONTROL_TRANSFER`
- 未知字节 -> `UNSUPPORTED`

它的课堂价值在于：

`machine_state` 不是只会处理 happy-path 的 ready next-fetch，它已经把 halt / deferred / unsupported 三个分叉都认真做成了 artifact。`

### 8.2 `test_machine_state_i386_bridge`

这组 case 会锁：

- profile-aware `MachineIrAllocateRewriteReport` 能直接桥到 `machine_state`
- `i386-preview` 下 ready / halted / deferred / unsupported 这套 state-resolution contract 依然成立

所以这一层的 profile-aware bridge 锁住的不是“换个 base 地址”，而是：

`换 profile 后，state 是否成立这件事的判断逻辑依然稳定。`

---

## 9. 这层到底“新增 / 改了 / 翻译了”什么

可以直接总结成三句话。

### 9.1 新增了什么

新增了第一份显式的 applied state artifact：

- `READY`
- `HALTED`
- `DEFERRED_CONTROL_TRANSFER`
- `UNSUPPORTED`

### 9.2 改了什么

把上一层“next fetch resolution”的语义，改造成更严格的“state 是否已经成立”的语义。

### 9.3 翻译了什么

它把：

- `NEXT_FETCH`
  - 翻译成 `READY state`
- `HALT`
  - 翻译成 `HALTED state`
- `DEFERRED_CONTROL_TRANSFER`
  - 翻译成“还没有 state”

---

## 10. 和上下游的边界

## 10.1 相对 `machine_transition`

`machine_transition` 关注：

- 下一步怎么解析
- 有没有 next fetch

`machine_state` 关注：

- 这是不是已经是一份可应用状态

所以一句话区分：

- `transition`：next-step meaning
- `state`：applied-state meaning

## 10.2 相对 `machine_mutation`

`machine_state` 到这里为止，还没有讨论：

- register result 要不要写回
- local/global store 要不要生效
- call effect 怎么办

这些是 `machine_mutation` 才开始回答的问题。

所以：

- `machine_state`
  - 先把控制流层的下一份状态定下来
- `machine_mutation`
  - 再看这条指令还有哪些非控制副作用没处理

---

## 11. 一段伪代码看完整主线

```text
build_state(transition):
  verify(transition)

  if transition.resolution == NEXT_FETCH:
    return READY state {
      pc = transition.next_pc
      sp = old sp
      current_fetch = transition.next_fetch
    }

  if transition.resolution == HALT:
    return HALTED state {
      pc = old pc
      sp = old sp
      no current_fetch
    }

  if transition.resolution == DEFERRED_CONTROL_TRANSFER:
    return unresolved state {
      has_state = no
    }

  if transition.resolution == UNSUPPORTED:
    return unsupported state {
      has_state = no
    }
```

---

## 12. 读代码时最推荐盯的点

如果你自己去读 `src/machine/runtime/machine_state/machine_state.c`，最建议盯这几个函数：

- `machine_state_build_from_machine_transition_file(...)`
  - 看真正的 lowering 分支
- `machine_state_verify_file(...)`
  - 看 ready/halted/deferred/unsupported 的结构合同
- `machine_state_file_get_current_fetch_summary(...)`
  - 看当前 fetch 是怎么回到 runtime segment 上的
- `machine_state_dump_file(...)`
  - 看这层对外到底暴露哪些字段

---

## 13. 一句话总结

`machine_state` 的核心价值，不是“又包装一层”，而是把 `machine_transition` 的结果正式提升成“状态是否已经成立”的结论：ready 和 halted 真的落成 state，control transfer 和 unsupported 则诚实地继续保留 unresolved。  
