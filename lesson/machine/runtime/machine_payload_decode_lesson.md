# Machine Payload Decode Lesson（compiler_lab）

> 目标：解释 `machine_payload_decode` 为什么会出现在 `machine_decode` 之后，它当前到底新增了什么“payload-byte meaning”语义，`src/machine/runtime/machine_payload_decode/` 里真实在做什么，以及这层代码里已经存在的 `total_byte_count` / `payload_byte_count` / `payload_bytes` / `immediate_value` 逻辑是什么。

## 一句话定位

`machine_payload_decode` 是 byte tag 后面的 payload bytes 第一次被正式解释出长度和局部意义的层。

## 常见误解

- 误解一：decode 已经把整条字节序列解释完了。
  - decode 先只回答 tag，payload 的长度、bytes、immediate hint 要到这里才真正出现。
- 误解二：payload_decode 已经在决定最终执行语义。
  - 当前更准确地说，它先把字节和 immediate meaning 摆好，真正动作语义还要到 interp。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_decode_lesson.md`
2. `lesson/machine/runtime/machine_step_lesson.md`

因为 payload_decode 的前提就是：

- 当前 tag 已经分类完成
- 当前 byte 所在位置也已经确定

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_interp_lesson.md`
2. `lesson/machine/runtime/machine_transition_lesson.md`

因为 payload_decode 之后最自然的问题就是：

- 这条 tag+payload 序列到底意味着什么动作
- 这个动作又会怎么转成下一份 fetch-state

---

## 导学

如果说：

- `machine_decode` 回答的是“当前 fetch byte 是 `op` / `terminator` / `none` 哪一类标签”

那么：

- `machine_payload_decode` 回答的是“这个标签后面当前还跟着哪些 payload bytes，它们现在能不能被解释出更具体的 meaning”

所以这层还不是 interpreter，也不是 next-state transition。

它更准确的身份是：

`tag decode -> payload-byte decode artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_decode` 后面还要单独有 `machine_payload_decode`
2. 再看 `include/machine/payload_decode.h`，建立 payload file / payload summary / target policy 的整体印象
3. 再看 `src/machine/runtime/machine_payload_decode/machine_payload_decode.c`，理解 decode tag 是怎么被延伸成 payload decode 的
4. 最后带着测试例子去看：
   - 为什么现在最多只看 3 个 payload bytes
   - `total_byte_count` 和 `payload_byte_count` 在表达什么
   - `immediate_value` 和 `payload_bytes` 为什么要同时出现

学完你应该能：

1. 解释 `machine_payload_decode` 和 `machine_decode` / `machine_interp` 的边界
2. 说明这层真正新增的是“标签后续 payload 的形状”，不是完整执行语义
3. 看懂一个 `machine_decode -> machine_payload_decode` 的最小 before/after
4. 说明为什么后面的 interp 需要这层先把 payload 摊开

---

## 1. 为什么需要 `machine_payload_decode`

`machine_decode` 已经回答了：

- 当前 byte 是 `op` / `terminator` / `none`
- 它的 `tag_value` 是多少
- 它有没有 `tag_name`

但它还没有直接回答：

- 这条东西总共占几个字节？
- tag 后面到底还有没有 payload？
- payload 现在的原始字节是什么？
- 当前有没有一个已经能抽出来的 `immediate_value`？

所以 `machine_payload_decode` 引入的真正边界是：

`byte-tag meaning -> tag + payload shape`

这层最重要的新增信息不是标签名字，而是：

- `total_byte_count`
- `payload_byte_count`
- `payload_bytes[3]`
- `immediate_value`

---

## 2. 为什么不能把它塞回 `machine_decode`

`machine_decode` 负责的是：

- tag class
- tag value
- known-name

但它不应该继续直接负责：

- payload byte extraction
- byte-count accounting
- immediate extraction

不然 `machine_decode` 会同时扮演：

- 标签分类层
- payload 解释层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_decode`
  - 回答“这是什么 tag”
- `machine_payload_decode`
  - 回答“这个 tag 后面跟了什么 payload”
- `machine_interp`
  - 回答“根据 tag + payload，下一步动作是什么”

---

## 3. 文件定位

- 接口：`include/machine/payload_decode.h`
- 实现：`src/machine/runtime/machine_payload_decode/machine_payload_decode.c`
- 测试：`tests/machine/runtime/machine_payload_decode/machine_payload_decode_test.c`
- 规划文档：`docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md`

当前这层也是一个 `.c` 文件，但职责已经完整成型：

- raw file lifecycle
- verifier
- decode -> payload lowering
- payload summary helper
- report / overview artifact
- dump
- bridge from decode / step / machine_ir

所以它已经不只是“在 decode 后面顺手多加几个字段”。

---

## 4. `include/machine/payload_decode.h`：当前数据模型

### 4.1 `MachinePayloadDecodeKind`

当前 kind 仍然保持和 decode 对齐的三类：

- `NONE`
- `OP`
- `TERMINATOR`

这说明这一层没有重新定义 instruction family，而是：

`沿用上一层的 tag 分类，再把 payload 摊开`

### 4.2 `MachinePayloadDecodeSummary`

这是这层最关键的数据结构。

字段包括：

- `kind`
- `raw_byte`
- `tag_value`
- `is_known`
- `tag_name`
- `total_byte_count`
- `payload_byte_count`
- `payload_bytes[3]`
- `immediate_value`

这几个字段的组合很有代表性：

- `total_byte_count`
  - 这条当前 provisional encoding 一共占几个字节
- `payload_byte_count`
  - 除了首字节标签外，后面还跟了几个 payload 字节
- `payload_bytes[3]`
  - 原始 payload bytes 长什么样
- `immediate_value`
  - 当前已经能提取出的一个统一数值视图

也就是说，这层不是只告诉你“后面还有字节”，而是已经把：

`payload 的原始字节视图 + 一个初步抽象值视图`

都给出来了。

### 4.3 `MachinePayloadDecodeTargetPolicySummary`

这层 target policy 目前很小：

- `max_payload_byte_count`

当前值是：

- `3`

所以这层当前的 scope 很明确：

`第一版 payload decode 最多只看 3 个后续字节`

### 4.4 `MachinePayloadDecodeFile`

可以先把它记成：

```text
MachinePayloadDecodeFile =
  MachineDecodeFile
  + kind
  + total_byte_count
  + payload_byte_count
  + payload_bytes
  + immediate_value
```

所以它本质上是：

`decode-tag artifact + payload-shape artifact`

### 4.5 `MachinePayloadDecodeReport`

report 又继续把：

- current segment
- fetch summary
- decode tag summary
- payload summary

一起缓存起来。

这说明这层 report 的重点是：

`把“当前标签 + 当前 payload”嵌回 execution context`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_payload_decode_build_from_machine_decode_file(...)`

它大致在做：

1. verify decode 输入
2. 复制上一层的 tag facts
3. 根据 tag kind 决定当前 payload kind
4. 计算 `total_byte_count`
5. 读取最多 3 个 payload bytes
6. 尝试组装 `immediate_value`
7. 再 verify payload-decode file

所以它真正做的是：

`把“当前字节是什么 tag”推进到“当前这一小段字节整体长什么样”`

### 5.1 verifier 在保什么边界

`machine_payload_decode_verify_file(...)` 当前重点在保：

- decode 输入必须合法
- `kind=none` 时不能伪装出 payload bytes
- `payload_byte_count <= 3`
- `total_byte_count == payload_byte_count + 1`
- raw byte 必须和 fetch / decode 两边一致

这说明这层不是“随手猜几个字节”，而是把 payload 形状正式做成了 verifier-backed contract。

### 5.2 为什么会同时有 `payload_bytes` 和 `immediate_value`

这是这层 lesson 里最值得解释的点之一。

当前实现一方面保留：

- `payload_bytes[3]`

因为后面消费者有时需要原始字节视图。

另一方面又给出：

- `immediate_value`

因为很多后续解释逻辑只想直接拿一个统一数值，而不想每次自己重组 bytes。

所以这层不是二选一，而是故意同时保留：

- raw payload bytes
- one first normalized value view

---

## 6. 一个最小例子看“增加了什么”

测试里的代表性正例会长成大致这样：

```text
machine_payload_decode profile=... status=ready pc=... sp=... current_segment=0 mapped_bytes=8192
payload: kind=op raw=0x1c value=0x0c known=yes name=load-local total-bytes=1 payload-bytes=0 imm=0 bytes=[]
```

这里相对 `machine_decode` 真正新增的是：

1. 这条当前总共占 `1` 个字节
2. 当前没有 payload bytes
3. `immediate_value` 被规范成 `0`

也就是说，即使是“没有额外 payload”的 case，这层仍然有价值，因为它把：

`指令字节总长度`

正式讲清楚了。

---

## 7. report 为什么在这层也很有意义

到了 payload-decode 层，下游最关心的问题已经像：

- 当前标签是什么
- 当前整条东西一共占几个字节
- payload bytes 是哪些
- immediate 解释值是什么

所以 `MachinePayloadDecodeReport` 会把：

- decode tag summary
- payload summary

都缓存下来。

这让后面的 `machine_interp` 不需要再自己回去重组：

- 总长度
- payload bytes
- immediate value

---

## 8. 测试里最值得读的几个点

`tests/machine/runtime/machine_payload_decode/machine_payload_decode_test.c` 当前最值得看的是三类故事。

### 8.1 known op with no payload

最基础的测试会锁：

- `load-local` 这类已知 op tag
- `total_byte_count=1`
- `payload_byte_count=0`

这说明这层即使在最小 case 上也已经有正式含义。

这个 case 最适合直接压成：

```text
raw_byte = 0x1c
tag_name = load-local
total_byte_count = 1
payload_byte_count = 0
payload_bytes = []
immediate_value = 0
```

也就是说，就算“后面没有 payload”，这层也不是白做的，因为它第一次把：

`这一条当前总共占几个字节`

正式讲清楚了。

### 8.2 report overview

测试也会验证：

- `report_overview`
- current segment
- fetch summary
- decode tag summary
- payload summary

这说明这层已经不只是 raw builder，而是有完整 report surface。

这点很重要，因为后面的 `machine_interp` 往往更关心的是：

- 当前 tag 是什么
- 当前总长多少
- 当前 payload 字节是什么

而不是只关心一个孤零零的 raw file。

### 8.3 bridge matrix

和前几层一样，测试还会覆盖：

- `MachineDecode*`
- `MachineStep*`
- `MachineIrAllocateRewriteReport`

所以这层不是只能从一个 decode file 开始手工调用。

`test_machine_payload_decode_ir_bridge_and_profile` 还会继续锁：

- 从 `MachineIrAllocateRewriteReport` 可以直接桥到 payload-decode
- profile-aware 版本也能保持同样的 payload summary 语义

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_decode`

`machine_decode` 只到：

- `op/terminator/none`
- tag value
- known name

`machine_payload_decode` 则进一步到：

- 总字节数
- payload 字节数
- raw payload bytes
- first immediate value

### 9.2 相对 `machine_interp`

`machine_payload_decode` 还没有做：

- 下一步动作是什么
- next status 是什么
- next PC 是什么
- control-transfer target 是什么

这些是下一层 `machine_interp` 才开始做的。

所以：

- `machine_payload_decode`：先把 payload 形状讲清楚
- `machine_interp`：再把“这一条当前意味着什么动作”讲清楚

---

## 10. 一句总记

`machine_payload_decode` 不是在执行指令，而是在 `machine_decode` 之上第一次把“当前标签后面还跟着哪些字节、整条一共多长、能不能抽成一个初步 immediate”做成 artifact，给后面的解释层打地基。`
