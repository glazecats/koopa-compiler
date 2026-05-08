# Machine Step Lesson（compiler_lab）

> 目标：解释 `machine_step` 为什么会出现在 `machine_launch` 之后，它当前到底新增了什么“fetch-state / execution-position”语义，`src/machine/runtime/machine_step/` 里真实在做什么，以及这层代码里已经存在的 current segment / fetch byte / memory offset / ready-vs-halted 逻辑是什么。

## 一句话定位

`machine_step` 是 CPU launch state 第一次真正进入“当前正站在哪、正在看哪个 byte”的执行位置语境的层。

## 常见误解

- 误解一：launch 之后就已经等于 current step。
  - launch 还只是寄存器起点，step 才正式带来 current segment / offset / fetch byte。
- 误解二：step 已经在解释 opcode 语义。
  - 真正的 tag 分类和 payload meaning 还要到 decode / payload_decode 才出现。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_launch_lesson.md`
2. `lesson/machine/runtime/machine_runtime_lesson.md`

因为 step 的前提就是：

- 起跑寄存器状态已经建立
- runtime memory / stack / entry 地址也已经讲清楚

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_decode_lesson.md`
2. `lesson/machine/runtime/machine_payload_decode_lesson.md`

因为 step 之后最自然的问题就是：

- 当前 fetch byte 属于什么 tag
- 它后面 payload bytes 又意味着什么

---

## 最近同步

最近这层最值得同步的是：`machine_step` 现在更明确地成为了 launch 之后整条 decode/interp/state 主线的正式起点。

当前最好再多记两点：

1. **step 不再只是“拿到 current byte”**
   它现在更像后面 decode/payload/interp/transition 的统一 fetch-state authority。

2. **它在真实 `compiler -riscv / -perf` 主链里的位置更稳了**
   现在要把它理解成“launch 之后真正进入执行位置语境的第一层”，而不只是测试过渡态；它属于真实 backend 的执行侧主线，但不参与 preview asm 文本生成本身。

---

## 导学

如果说：

- `machine_launch` 回答的是“CPU 起跑寄存器状态长什么样”

那么：

- `machine_step` 回答的是“起跑之后，此刻 CPU 正站在什么执行位置、正在看哪个字节”

所以这层还不是 decode，也不是 interp。

它更准确的身份是：

`launch-state -> explicit fetch-state artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_launch` 后面还要单独有 `machine_step`
2. 再看 `include/machine/step.h`，建立 step file / fetch summary / current segment 的整体印象
3. 再看 `src/machine/runtime/machine_step/machine_step.c`，理解 launch state 是怎么被抬成 fetch-state 的
4. 最后带着测试例子去看：
   - 当前 byte 是怎么来的
   - current segment 为什么必须显式化
   - `ready` / `halted` 为什么会在这层出现

学完你应该能：

1. 解释 `machine_step` 和 `machine_launch` / `machine_decode` 的边界
2. 说明这层真正新增的是“执行位置和取指状态”，不是再一份 launch report
3. 看懂一个 `machine_launch -> machine_step` 的最小 before/after
4. 说明为什么后面的 decode/interp 需要这层先把 current byte 定出来

---

## 1. 为什么需要 `machine_step`

`machine_launch` 已经回答了：

- 当前 `pc`
- 当前 `sp`
- launch register snapshot

但它还没有直接回答：

- 当前 `pc` 落在哪个 runtime segment 里？
- 当前看到的 fetch byte 是多少？
- 这个状态现在是 `ready` 还是 `halted`？
- 这个 fetch byte 在 flat runtime memory 里的 offset 是多少？

所以 `machine_step` 引入的真正边界是：

`launch register state -> explicit execution-position / fetch-state`

这层最重要的新增信息不是更多 register，而是：

- step status
- current segment
- current byte
- current byte memory offset

---

## 2. 为什么不能把它塞回 `machine_launch`

`machine_launch` 负责的是：

- PC/SP 物化
- launch register snapshot

但它不应该继续直接负责：

- fetch byte
- execution status
- current segment ownership
- fetch offset

不然 `machine_launch` 会同时扮演：

- CPU 起跑层
- 首步执行位置层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_launch`
  - 回答“怎么起跑”
- `machine_step`
  - 回答“起跑后当前站在哪个字节上”
- `machine_decode`
  - 回答“这个字节的 tag 意义是什么”

---

## 3. 文件定位

- 接口：`include/machine/step.h`
- 实现：`src/machine/runtime/machine_step/machine_step.c`
- 测试：`tests/machine/runtime/machine_step/machine_step_test.c`
- 规划文档：`docs/backend/MACHINE_STEP_PLAN.md`

这层当前也集中在一个 `.c` 文件里，但职责很清楚：

- raw file lifecycle
- verifier
- launch -> step lowering
- fetch summary helper
- report / overview artifact
- dump
- bridge from launch / runtime / load / machine_ir

所以它已经是一个真正的 fetch-state artifact 层。

---

## 4. `include/machine/step.h`：当前数据模型

### 4.1 `MachineStepStatus`

当前 status 只有两类：

- `MACHINE_STEP_STATUS_READY`
- `MACHINE_STEP_STATUS_HALTED`

这说明 step 层当前最保守但最关键的新增之一就是：

`执行位置现在是不是还能继续取指`

虽然现在还没 stepping semantics，但状态至少已经开始显式化。

### 4.2 `MachineStepFetchSummary`

这是这层最有辨识度的数据结构之一。

它包含：

- `byte_virtual_address`
- `byte_memory_offset`
- `segment_index`
- `segment_name`
- `byte_value`

也就是说，这层第一次把“当前正在看的那个字节”正式做成 artifact。

### 4.3 `MachineStepFile`

可以先把它记成：

```text
MachineStepFile =
  MachineLaunchFile
  + status
  + current_segment_index
  + current_byte
  + current_byte_memory_offset
```

所以它本质上是：

`launch-state artifact + first explicit fetch-state snapshot`

### 4.4 `MachineStepReport`

report 继续缓存：

- header summary
- target policy summary
- runtime launch summary
- initial stack summary
- runtime memory summary
- current segment summary
- fetch summary

所以这层 report 的重点是：

`把当前执行位置和它背后的 runtime 上下文一起交给后续 decode consumer`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_step_build_from_machine_launch_file(...)`

它大致在做：

1. verify launch 输入
2. 把 `program_counter` / `stack_pointer` 直接继承下来
3. 设定 `status`
4. 根据当前 `pc` 找到所属 runtime segment
5. 读取 `pc` 对应的 mapped byte
6. 计算这个 byte 在 unified memory 里的 offset
7. 再 verify step file

所以它真正做的是：

`把 launch PC 变成一个带 segment + byte + offset 的显式 fetch-state`

### 5.1 verifier 在保什么边界

`machine_step_verify_file(...)` 当前会强检查：

- launch 输入必须合法
- `pc/sp` 必须和 launch file 一致
- status 必须是 `ready` 或 `halted`
- 如果是 `ready`
  - 当前 `pc` 必须被某个 mapped segment 覆盖
  - 当前 segment 必须是 executable
  - `current_segment_index` 必须和查出来的一致
  - `current_byte` 必须等于 runtime memory 里该地址上的 byte
  - `current_byte_memory_offset` 必须等于 `pc - runtime_memory_base`

这说明这层不是“给 decode 提供一个大概意思”，而是：

`把当前取指位置的事实正式钉成 verifier-backed contract`

### 5.2 halted 状态为什么要在这层出现

虽然现在第一版仍然很窄，但 `halted` 的存在已经很有意义了。

因为这说明 step 层在设计上已经承认：

- execution-position 不一定总能继续 fetch

只是当前还没有丰富的 halt 原因分类，先保守地只区分：

- 能继续取指
- 不能继续取指

---

## 6. 一个最小例子看“增加了什么”

从 lesson 口径看，最小例子可以压缩成：

```text
launch:
  pc = 0x1000
  sp = 0x4000

step:
  status = ready
  current_segment = .text
  current_byte = <byte at 0x1000>
  current_byte_memory_offset = 0
```

这里相对 `machine_launch` 真正新增的是：

1. 当前正在看的 segment 被显式指出来了
2. 当前 fetch byte 被显式指出来了
3. 当前字节在 unified memory 里的 offset 被显式指出来了

所以 lesson 口径上，这层的核心不是“launch 再打印一遍”，而是：

`launch PC 第一次被提升成 fetch-state`

---

## 7. report 为什么在这层特别自然

到了 step 层，下游最关心的问题已经像：

- 当前在执行哪一段
- 当前 byte 是什么
- 它在内存镜像里的 offset 是多少

所以 `MachineStepReport` 现在把：

- current segment summary
- fetch summary

都缓存了出来。

这使得后面的 decode 层不需要再从 launch/runtime 里重推：

- segment ownership
- raw fetch byte

---

## 8. 测试里最值得读的几个点

`tests/machine/runtime/machine_step/machine_step_test.c` 当前最值得读的是三类故事。

### 8.1 normal ready state

最基础的测试会锁：

- `status=ready`
- 当前 segment 是 `.text`
- 当前 fetch byte 被正确读出
- offset 被正确算出

这组断言里最有辨识度的具体数字其实是：

- `pc = 0x1000`
- `sp = 0x4000`
- `current_segment_index = 0`
- `fetch byte = 0x1c`
- `byte_memory_offset = 0`

所以这层 lesson 最适合记的一句话是：

`launch` 层只知道 PC/SP；到了 `step` 层，PC 第一次真正对应到了“.text 里的第一个字节 0x1c”。`

### 8.2 bridge matrix

测试会覆盖：

- `MachineLaunch*`
- `MachineRuntime*`
- `MachineLoad*`
- `MachineIrAllocateRewriteReport`

几条上游桥都能走通。

这说明 step 层不是只能从一个 launch file 手工接。

`test_machine_step_ir_bridge_and_profile` 还会继续锁：

- `MachineIrAllocateRewriteReport` 可以直接一路桥到 `machine_step`
- profile-aware 版本也会一起把 base / `pc` / `sp` / fetch 视图带下来

### 8.3 report / dump wrapper

和前面几层一样，测试还会锁：

- raw dump
- report dump
- direct bridge dump

所以这层当前也已经有比较完整的 consumer surface。

换句话说，这层不是“只有一个内部状态 struct”，而是：

`raw state + report state + wrapper dump`

都已经被测试认真锁住了。

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_launch`

`machine_launch` 只到：

- `pc/sp`
- register snapshot

`machine_step` 则进一步到：

- 当前 segment
- 当前 fetch byte
- current memory offset
- ready/halted

### 9.2 相对 `machine_decode`

`machine_step` 还没有做：

- 当前 byte 是 op 还是 terminator
- tag value 是多少
- known tag name 是什么

这些是下一层 `machine_decode` 才开始做的。

所以：

- `machine_step`：先把“看到哪个 byte”讲清楚
- `machine_decode`：再把“这个 byte 代表什么 tag”讲清楚

---

## 10. 一句总记

`machine_step` 不是在执行指令，而是在 `machine_launch` 之上第一次把“当前取指状态”做成 artifact：PC 现在对应哪个 segment、哪个 byte、哪个 flat-memory offset，以及当前还能不能继续 fetch，都会被显式讲清楚。`
