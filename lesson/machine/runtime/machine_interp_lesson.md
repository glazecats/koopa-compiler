# Machine Interp Lesson（compiler_lab）

> 目标：解释 `machine_interp` 为什么会出现在 `machine_payload_decode` 之后，它当前到底新增了什么“这条当前字节序列意味着什么动作”语义，`src/machine/runtime/machine_interp/` 里真实在做什么，以及这层代码里已经存在的 `advance / halt / control-transfer / unsupported` 逻辑是什么。

## 一句话定位

`machine_interp` 是当前字节序列第一次被正式翻译成动作语义的层。

## 常见误解

- 误解一：interp 已经在做完整架构执行。
  - 当前这层更偏动作分类和第一版执行语义，不是完整 register/memory simulator。
- 误解二：payload_decode 已经差不多等于 interp。
  - payload_decode 还偏字节 meaning，interp 才真正回答 `advance / halt / control-transfer / unsupported`。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_payload_decode_lesson.md`
2. `lesson/machine/runtime/machine_decode_lesson.md`

因为 interp 的前提就是：

- tag 和 payload meaning 已经摆好
- 现在才进入“这东西在当前仓库里意味着什么动作”这个问题

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_transition_lesson.md`
2. `lesson/machine/runtime/machine_state_lesson.md`

因为 interp 之后最自然的问题就是：

- 这个动作怎么被解析成下一份 fetch-state
- 再怎么变成可继续执行的状态快照

---

## 最近同步

最近这层最值得同步的，是它现在更明确地成为了 transition/state/mutation 那条链的动作语义入口。

当前最好再多记两点：

1. **interp 不是 payload 层的附属解释注释**
   它现在更像 runtime 主线里一个正式的 action authority。

2. **它和后面的 transition/state 分工更清楚了**
   interp 定动作类目，transition 解下一 fetch-state，state/mutation 再分别处理可继续执行的快照和副作用族。

---

## 导学

如果说：

- `machine_payload_decode` 回答的是“当前标签和 payload 长什么样”

那么：

- `machine_interp` 回答的是“按当前仓库的 provisional 执行语义，这条东西现在意味着什么动作”

所以这层还不是 next-state transition，也不是 state mutation。

它更准确的身份是：

`payload-decode -> first execution-intent artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_payload_decode` 后面还要单独有 `machine_interp`
2. 再看 `include/machine/interp.h`，建立 interp file / action summary / target policy 的整体印象
3. 再看 `src/machine/runtime/machine_interp/machine_interp.c`，理解 payload facts 是怎么被抬成动作语义的
4. 最后带着测试例子去看：
   - `advance` / `halt` / `control-transfer` / `unsupported` 分别在表达什么
   - target block index / return immediate 为什么会在这层出现

学完你应该能：

1. 解释 `machine_interp` 和 `machine_payload_decode` / `machine_transition` 的边界
2. 说明这层真正新增的是“动作意图”，不是完整状态更新
3. 看懂一个 `machine_payload_decode -> machine_interp` 的最小 before/after
4. 说明为什么后面的 transition/mutation 需要这层先把动作类目定下来

---

## 1. 为什么需要 `machine_interp`

`machine_payload_decode` 已经回答了：

- tag 是什么
- payload bytes 是什么
- immediate value 是什么
- 当前一共占多少字节

但它还没有直接回答：

- 这一条现在是应该线性前进？
- 还是应该 halt？
- 还是应该做 control transfer？
- 如果是 control transfer，目标 block 是哪些？
- 如果是 return immediate，值是多少？

所以 `machine_interp` 引入的真正边界是：

`payload shape -> action intent`

这层最重要的新增信息不是更多 bytes，而是：

- `action_kind`
- `next_status`
- `has_next_program_counter`
- `primary/secondary target block`
- `return_immediate`

---

## 2. 为什么不能把它塞回 `machine_payload_decode`

`machine_payload_decode` 负责的是：

- 字节长度
- payload 原始值
- immediate 视图

但它不应该继续直接负责：

- 动作分类
- control target 语义
- halt / unsupported 判断

不然 `machine_payload_decode` 会同时扮演：

- 字节解析层
- 解释语义层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_payload_decode`
  - 回答“字节怎么拆”
- `machine_interp`
  - 回答“拆完以后代表什么动作”
- `machine_transition`
  - 回答“这个动作如何变成下一份 fetch-state”

---

## 3. 文件定位

- 接口：`include/machine/interp.h`
- 实现：`src/machine/runtime/machine_interp/machine_interp.c`
- 测试：`tests/machine/runtime/machine_interp/machine_interp_test.c`
- 规划文档：`docs/backend/MACHINE_INTERP_PLAN.md`

当前这层也是一个 `.c` 文件，但职责已经很明确：

- raw file lifecycle
- verifier
- payload-decode -> interp lowering
- control target helper
- report / overview artifact
- dump
- bridge from payload/decode/step/machine_ir

所以它已经不只是“多了个 enum 表示动作”。

---

## 4. `include/machine/interp.h`：当前数据模型

### 4.1 `MachineInterpActionKind`

当前 action kind 有：

- `NONE`
- `ADVANCE`
- `HALT`
- `CONTROL_TRANSFER`
- `UNSUPPORTED`

这五类非常重要，因为它们把当前第一版解释语义说得很清楚：

- 不是所有已知 tag 都一定是线性前进
- 也不是一上来就做完整状态更新

### 4.2 `MachineInterpSummary`

这是这层最关键的数据结构。

它包含：

- `action_kind`
- `raw_byte`
- `tag_value`
- `is_known`
- `tag_name`
- `instruction_byte_count`
- `next_status`
- `has_next_program_counter`
- `next_program_counter`
- `has_primary_target_block`
- `primary_target_block_index`
- `has_secondary_target_block`
- `secondary_target_block_index`
- `has_return_immediate`
- `return_immediate`

这说明 interp 层当前已经不只是“给动作类型”，而是开始回答：

`这个动作可能会需要哪些 next-step inputs`

### 4.3 `MachineInterpTargetPolicySummary`

当前 target policy 目前包括：

- `max_control_target_count`
- `resolves_linear_next_program_counter`
- `resolves_control_targets_as_block_indices`

这说明当前 interp 层并不是在 pretending 做 final ISA semantics，而是明确承认：

`第一版解释语义只做到 block-index 级别的 control target`

### 4.4 `MachineInterpFile`

可以先把它记成：

```text
MachineInterpFile =
  MachinePayloadDecodeFile
  + action_kind
  + next_status
  + next_program_counter?
  + primary/secondary targets?
  + return_immediate?
```

所以它本质上是：

`payload artifact + action-intent artifact`

### 4.5 `MachineInterpReport`

report 会继续缓存：

- fetch summary
- decode tag summary
- payload summary
- interp summary

这说明这层 report 的重点是：

`把“当前这条字节序列意味着什么动作”嵌回完整上下文`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_interp_build_from_machine_payload_decode_file(...)`

它大致在做：

1. verify payload-decode 输入
2. 看当前 kind / tag / payload
3. 决定 action kind
4. 决定 next status
5. 如有必要，填 next PC / target block / return immediate
6. 再 verify interp file

所以它真正做的是：

`把“字节长什么样”推进到“这条现在该干什么”`

### 5.1 target pair 是怎么来的

当前实现里有一个很关键的 helper：

- `machine_interp_decode_target_pair(...)`

它会把一个编码后的 pair byte 解成：

- primary target block
- secondary target block

这说明当前 control-transfer 语义还很保守：

- 不直接讲真实机器地址
- 先讲成 block-index 级别的目标

### 5.2 verifier 在保什么边界

`machine_interp_verify_file(...)` 当前会按 action kind 做不同约束：

- `ADVANCE`
  - 必须 `next_status == READY`
  - 必须有 `next_program_counter`
  - 并且 next PC 至少要前进过当前指令长度
- `HALT`
  - 必须 `next_status == HALTED`
  - 不能再带 next PC 或 control target
- `CONTROL_TRANSFER`
  - 必须是 `READY`
  - 目标字段要成体系

这说明 interp 层并不是“拍个动作名字”，而是：

`每种动作都开始带自己的结构契约`

---

## 6. 一个最小例子看“增加了什么”

从 lesson 口径看，最小线性前进的例子可以压缩成：

```text
payload-decode:
  total-byte-count = 1
  tag = load-local

interp:
  action = advance
  next_status = ready
  next_program_counter = pc + 1
```

这里相对 `machine_payload_decode` 真正新增的是：

1. 当前这条被判断成线性前进
2. 下一条 PC 应该是多少被显式化了

而 control-transfer 的例子则更像：

```text
interp:
  action = control-transfer
  targets = [1,2]
```

return-immediate 的例子则会出现：

```text
interp:
  action = halt
  return-imm = 1
```

所以 lesson 口径上，这层的核心不是“payload 再解释一遍”，而是：

`当前字节序列第一次被赋予执行动作语义`

---

## 7. report 为什么在这层特别值钱

到了 interp 层，下游最关心的问题已经像：

- 当前动作是什么
- 如果前进，next PC 是多少
- 如果转移，目标 block 是哪些
- 如果返回，return immediate 是多少

所以 `MachineInterpReport` 当前专门缓存：

- payload summary
- interp summary

这让后面的 transition 层不需要再从 payload/decode 层自己拼装动作。

---

## 8. 测试里最值得读的几个点

`tests/machine/runtime/machine_interp/machine_interp_test.c` 当前最值得看的是四类故事。

### 8.1 advance

最基础的正例会锁：

- `action=advance`
- `next_status=ready`
- `next_program_counter` 正确前进

当前最适合直接记住的课堂正例其实是：

```text
raw/tag = 0x1c / load-local
instruction_byte_count = 1
action = advance
next_status = ready
next_program_counter = pc + 1
```

也就是说，前一层只知道“这是 `load-local`，长度 1”；到了这一层，系统第一次明确说：

`这条东西的执行意图是线性前进到下一条。`

### 8.2 halt

还有一类测试会锁：

- `action=halt`
- 不能再带 next PC

这里最值得讲的具体例子其实是 `return-imm`。

lesson 口径上可以把它压成：

```text
tag = return-imm
action = halt
next_status = halted
return_immediate = 1 or 7
has_next_program_counter = no
```

这说明 `machine_interp` 到这里已经开始承认：

- 有些指令不是前进
- 它们会直接把当前执行结果归结成 halt

### 8.3 control transfer

控制流类的测试会锁：

- `action=control-transfer`
- `targets=[primary,secondary]`

这类 case 很适合提醒读者：

`machine_interp` 当前对 control-flow 的第一版理解，不是“真实机器地址跳到哪”，而是“先把 block target index 说出来”。`

所以这一层虽然已经开始理解控制流，但仍然故意停在：

- `primary_target_block_index`
- `secondary_target_block_index`

### 8.4 unsupported

这类测试的意义在于：

- 当前 provisional interp 不会强装自己全都懂
- 遇到当前不支持的形状，会显式给出 `unsupported`

这说明这层是保守解释，而不是激进过拟合。

另外，`test_machine_interp_i386_bridge` 也很值得顺手看。

它会锁：

- `MachineIrAllocateRewriteReport` 可以直接 profile-aware 地桥到 interp
- `i386-preview` 路线下同样能得到一致的 `advance/halt/control-transfer/unsupported` 分类

也就是说，这层的 profile-aware bridge 锁的是：

`上游 profile 变化不会把动作分类面打坏。`

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_payload_decode`

`machine_payload_decode` 只到：

- 字节长度
- payload bytes
- immediate view

`machine_interp` 则进一步到：

- 动作类别
- next status
- next PC
- control targets
- return immediate

### 9.2 相对 `machine_transition`

`machine_interp` 还没有做：

- 真正构造下一份 fetch-state
- next current byte
- next current segment
- next current memory offset

这些是下一层 `machine_transition` 才开始做的。

所以：

- `machine_interp`：先把动作意图讲清楚
- `machine_transition`：再把动作变成下一份 step-state

---

## 10. 一句总记

`machine_interp` 不是完整执行器，而是在 `machine_payload_decode` 之上第一次把“这条当前字节序列意味着前进、停机、转移还是不支持”做成 artifact，并把 next-PC/target/return-imm 这些动作意图显式抬出来。`
