# Machine Decode Lesson（compiler_lab）

> 目标：解释 `machine_decode` 为什么会出现在 `machine_step` 之后，它当前到底新增了什么“byte-tag meaning”语义，`src/machine/runtime/machine_decode/` 里真实在做什么，以及这层代码里已经存在的 `op/terminator/none` 分类、tag value、known-name 逻辑是什么。

## 一句话定位

`machine_decode` 是 current fetch byte 第一次被正式解释成“这是什么 tag” 的层。

## 常见误解

- 误解一：decode 已经在做完整指令解释。
  - 当前这层先只回答 tag class / tag value / known-name，不直接给完整动作语义。
- 误解二：step 里其实已经差不多把 decode 做了。
  - step 更偏位置和当前 byte，decode 才正式建立 byte-tag meaning。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_step_lesson.md`
2. `lesson/machine/runtime/machine_launch_lesson.md`

因为 decode 的前提就是：

- 当前 CPU 正站在什么位置、看哪个 byte 已经明确
- 现在才开始解释这个 byte 本身的 tag 含义

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_payload_decode_lesson.md`
2. `lesson/machine/runtime/machine_interp_lesson.md`

因为 decode 之后最自然的问题就是：

- 后面 payload bytes 长什么样
- 合起来最终意味着什么动作

---

## 导学

如果说：

- `machine_step` 回答的是“当前 CPU 正在看哪个字节”

那么：

- `machine_decode` 回答的是“这个字节当前在仓库的 provisional encoding 里代表什么 tag”

所以这层还不是 payload decode，也不是 interpreter。

它更准确的身份是：

`fetch-state -> first explicit byte-tag decode artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_step` 后面还要单独有 `machine_decode`
2. 再看 `include/machine/decode.h`，建立 decode file / tag summary / overview artifact 的整体印象
3. 再看 `src/machine/runtime/machine_decode/machine_decode.c`，理解 current fetch byte 是怎么被分类的
4. 最后带着测试例子去看：
   - 为什么现在只解 tag，不解 payload
   - `op` / `terminator` / `none` 是怎么来的
   - known-name 为什么已经有价值

学完你应该能：

1. 解释 `machine_decode` 和 `machine_step` / `machine_payload_decode` 的边界
2. 说明这层真正新增的是“字节标签意义”，不是完整指令 decode
3. 看懂一个 `machine_step -> machine_decode` 的最小 before/after
4. 说明为什么后面的 payload-decode / interp 需要这层先把 tag class 讲清楚

---

## 1. 为什么需要 `machine_decode`

`machine_step` 已经回答了：

- 当前 `pc`
- 当前 fetch byte
- 当前 segment
- 当前 fetch offset

但它还没有直接回答：

- 这个 byte 现在到底是 op tag、terminator tag，还是 none？
- 如果它是已知 tag，它的 tag value 是多少？
- 它当前有没有已知名字？

所以 `machine_decode` 引入的真正边界是：

`raw fetch byte -> first explicit tag meaning`

这层最重要的新增信息不是更多内存或寄存器，而是：

- tag class
- tag value
- known-name

---

## 2. 为什么不能把它塞回 `machine_step`

`machine_step` 负责的是：

- 当前在哪个字节上
- 当前字节值是多少
- 当前状态是 ready 还是 halted

但它不应该继续直接负责：

- tag classification
- op/terminator naming
- target decode policy

不然 `machine_step` 会同时扮演：

- fetch-state 层
- decode-tag 层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_step`
  - 回答“现在看哪个 byte”
- `machine_decode`
  - 回答“这个 byte 是什么 tag”
- `machine_payload_decode`
  - 再往下回答“这个 tag 后面的 payload bytes 怎么解释”

---

## 3. 文件定位

- 接口：`include/machine/decode.h`
- 实现：`src/machine/runtime/machine_decode/machine_decode.c`
- 测试：`tests/machine/runtime/machine_decode/machine_decode_test.c`
- 规划文档：`docs/backend/MACHINE_DECODE_PLAN.md`

当前这层也是一个 `.c` 文件，但职责已经比较完整：

- raw file lifecycle
- verifier
- step -> decode lowering
- tag naming helper
- report / overview artifact
- dump
- bridge from step / launch / machine_ir

所以它已经不是“顺手加个 enum”。

---

## 4. `include/machine/decode.h`：当前数据模型

### 4.1 `MachineDecodeTagClass`

当前 tag class 只有三类：

- `MACHINE_DECODE_TAG_NONE`
- `MACHINE_DECODE_TAG_OP`
- `MACHINE_DECODE_TAG_TERMINATOR`

这很关键，因为它把 decode 第一版的 scope 说得非常清楚：

- 先只区分字节标签大类
- 还不碰 payload

### 4.2 `MachineDecodeTagSummary`

这是这层最有辨识度的数据结构。

它包含：

- `tag_class`
- `raw_byte`
- `tag_value`
- `is_known`
- `tag_name`

所以这层当前真正新增的 artifact 可以概括成：

`当前字节现在到底有没有 tag 意义，如果有，它叫什么`

### 4.3 `MachineDecodeFile`

可以先把它记成：

```text
MachineDecodeFile =
  MachineStepFile
  + tag_class
  + raw_byte
  + tag_value
  + is_known
```

所以它本质上是：

`step artifact + first decode-tag artifact`

### 4.4 `MachineDecodeTargetPolicySummary`

这层 target policy 目前很小，但也很重要：

- `opcode_tag_base`
- `terminator_tag_base`

这说明当前 decode 层已经不是“把 byte 硬编码匹配一下”，而是：

`承认当前 provisional encoding 本身有 tag policy`

### 4.5 `MachineDecodeReport`

report 会继续缓存：

- runtime launch summary
- initial stack summary
- runtime memory summary
- current segment summary
- fetch summary
- tag summary

这说明 decode 层的 report 不只是关心 decode 自己，而是：

`把当前 tag meaning 嵌回完整的 execution context`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_decode_build_from_machine_step_file(...)`

它大致在做：

1. verify step 输入
2. 复制 `raw_byte`
3. 根据 current byte 判断 tag class
4. 提取 `tag_value`
5. 判断 `is_known`
6. 如果认识，就给出 `tag_name`
7. 再 verify decode file

所以它真正做的是：

`把 fetch byte 变成第一层 decode meaning`

### 5.1 op tag 和 terminator tag 名字怎么来的

`machine_decode.c` 里有两个很 lesson-friendly 的 helper：

- `machine_decode_op_tag_name(...)`
- `machine_decode_terminator_tag_name(...)`

它们当前会把：

- `MachineSelectOpKind`
- `MachineLayoutTerminatorKind`

映射成字符串名字，比如：

- `copy`
- `materialize-imm`
- `alu`
- `call`
- `load-local`
- `return`
- `jump`
- `branch`

这说明 decode 层虽然还没解 payload，但已经开始把“当前 tag 的类型名字”显式化了。

### 5.2 target policy 当前有多强

`machine_decode_get_target_policy_summary(...)` 当前会给：

- `opcode_tag_base = 0x10`
- `terminator_tag_base = 0x80`

也就是说，当前第一版 decode 不是完全 target-specific ISA decode，而是：

`仓库当前 provisional byte encoding 的 tag boundary decode`

这和规划文档的口径是完全一致的。

### 5.3 verifier 在保什么边界

`machine_decode_verify_file(...)` 当前会强检查：

- step 输入必须合法
- `decode_file.raw_byte == fetch_summary.byte_value`
- `tag_summary` 和 raw file 上的分类结果必须一致

这说明 decode 层也不是“算个方便字段”，而是：

`把当前 byte-tag meaning 钉成 verifier-backed contract`

---

## 6. 一个最小例子看“增加了什么”

从 lesson 口径看，最小例子可以压缩成：

```text
step:
  current_byte = 0x10

decode:
  tag_class = op
  tag_value = 0
  is_known = yes
  tag_name = copy
```

这里相对 `machine_step` 真正新增的是：

1. 当前 byte 被归类成了 `op`
2. 其 tag value 被显式提出
3. 它的 known-name 被显式提出

所以 lesson 口径上，这层的核心不是“又把 byte 打印一遍”，而是：

`当前 byte 第一次被赋予标签意义`

---

## 7. 为什么现在只解 tag，不解 payload

这是最重要的边界。

当前 `machine_decode` 故意只做到：

- `op`
- `terminator`
- `none`

以及：

- tag value
- known name

它还**不做**：

- operand decode
- immediate payload decode
- branch target payload decode
- multi-byte instruction decode

因为这些属于下一层：

- `machine_payload_decode`

所以：

- `machine_decode`：先回答“这字节是什么标签”
- `machine_payload_decode`：再回答“标签后面的负载怎么解释”

---

## 8. report 为什么在这层也很自然

到了 decode 层，下游最关心的问题已经像：

- 当前 byte 属于哪一类 tag
- 当前 byte 有没有已知名字
- 当前 decode 状态对应的 fetch/runtime 背景是什么

所以 `MachineDecodeReport` 当前会把：

- fetch summary
- current segment summary
- tag summary

一起缓存出来。

这让后面的 payload decode / interp 不需要再回 step 层自己重算：

- raw byte
- current segment
- current execution context

---

## 9. 测试里最值得读的几个点

`tests/machine/runtime/machine_decode/machine_decode_test.c` 当前最值得读的是三类故事。

### 9.1 recognized tag

最基础的测试会锁：

- 一个 current byte 被识别成 `op` 或 `terminator`
- `tag_value` 和 `tag_name` 正确

当前最适合拿来当课堂正例的，其实就是这条：

```text
raw_byte = 0x1c
tag_class = op
tag_value = 0x0c
tag_name = load-local
```

也就是说，`machine_step` 里“当前字节是 0x1c”，到了 `machine_decode` 里就第一次变成了：

`这不是任意字节，而是一个被当前 provisional encoding 认识的 op tag。`

### 9.2 unknown / none

另一类测试会锁：

- 当前 byte 不属于当前已知标签空间
- 那就应该归成 `none`

这说明 decode 层不是看到什么都硬解释，而是保守分类。

这点很重要，因为它说明这层的默认策略不是：

- “先编一个解释再说”

而是：

- “认识就给 tag meaning，不认识就老老实实返回 `none`”

### 9.3 bridge matrix

和前面几层一样，测试也覆盖：

- `MachineStep*`
- `MachineLaunch*`
- `MachineIrAllocateRewriteReport`

几条桥都能走通。

所以这层不是只能手工从 step file 开始调用。

`test_machine_decode_ir_bridge_and_profile` 还会继续锁：

- `MachineIrAllocateRewriteReport` 可以直接桥到 `machine_decode`
- profile-aware 版本也会把同样的 fetch/decode 语义一路带下来

---

## 10. 这层和前后层的边界

### 10.1 相对 `machine_step`

`machine_step` 只到：

- current byte 是多少
- current segment 是谁

`machine_decode` 则进一步到：

- 这个 byte 是 `op` / `terminator` / `none`
- 它的 tag value 是多少
- 它有没有已知名字

### 10.2 相对 `machine_payload_decode`

`machine_decode` 还没有做：

- operand payload 解码
- immediate / branch offset payload 解析
- richer instruction structure

这些是下一层 `machine_payload_decode` 才开始做的。

所以：

- `machine_decode`：先把标签种类讲清楚
- `machine_payload_decode`：再把负载含义讲清楚

---

## 11. 一句总记

`machine_decode` 不是完整指令 decode，而是在 `machine_step` 之上第一次把“当前 fetch byte 的标签意义”做成 artifact：它先告诉后面的系统，这个字节现在到底是 op、terminator 还是 none，以及它认不认识这个标签名字。`
