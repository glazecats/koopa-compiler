# Machine Encode Lesson（compiler_lab）

> 目标：解释 `machine_encode` 在做什么，为什么它先做 abstract offset，而不是立刻做真实 ISA 编码；它相对 `machine_emit` 到底新增了什么，`src/machine/lowering/machine_encode/` 里各个分片在做什么，以及为什么这一层已经是一个很实用的 offset-aware artifact。

## 一句话定位

`machine_encode` 是 emitted artifact 正式进入稳定 offset/span 视角的层。

## 常见误解

- 误解一：encode 这里已经在做最终目标 ISA 编码。
  - 当前更准确地说，它先建立 abstract offset / span / by-offset lookup。
- 误解二：有 emitted labels 就已经够后面 object 线用了。
  - 后面 bytes/object 线还需要这里先把位置和跨度稳定下来。

## 前置阅读

最推荐先读：

1. `lesson/machine/lowering/machine_emit_lesson.md`
2. `lesson/machine/lowering/machine_layout_lesson.md`

因为 encode 的前提就是：

- 线性顺序已经定下来了
- label-aware emitted artifact 也已经形成

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/object/machine_bytes_lesson.md`
2. `lesson/machine/object/machine_object_lesson.md`

因为 encode 之后最自然的问题就是：

- offset-aware artifact 怎么继续落成真实 bytes
- bytes 又怎么继续进入 object-facing 容器

---

## 最近同步

最近 `machine_encode` 最值得同步的不是“又多几个 offset helper”，而是它现在已经开始承接 **更深一层 preview-bridge compatibility check**。

你现在最好把这层再多记三件事：

1. **不仅算 offset，还会更早筛 preview bytes-range failure**
   比如某些 branch/jump/call 形状如果对当前 preview lane 根本不诚实，现在已经可以在 encode 边界拒绝。

2. **这类 deeper compatibility check 还会继续往上游回流**
   当前 lesson 主线里要理解成：
   - `select/layout/emit/encode`
   正在逐步形成一条更一致的 preview-compatibility chain。

3. **所以 encode 现在不只是“算位置”，还是“bytes 之前最后一层大门”**

一句话压缩就是：

- 以前：`emit -> encode -> bytes`，encode 更偏位置
- 现在：`emit -> encode -> bytes`，encode 也开始承担 preview-lane honesty screening

最近这层还要再补一个直接关系到 `void` 的点：

4. **encode 现在也显式保留 return shape**
   - bare `ret`
   - `reti imm`
   - `ret spill`
   不再被视作“反正都是 return”这种单一 terminator

这条线现在也有直接回归：

- `test_machine_encode_verifier_enforces_return_shapes`

---

## 导学

`machine_emit` 之后，我们已经有：

- laid-out blocks
- stable emitted labels
- by-label lookup

但还没有：

- 每个 block 在代码序列里的位置
- 一个 target label 对应的线性 offset
- “这个 offset 落在哪个 block 里”的查询能力

所以 `machine_encode` 先做一个保守但真实的步骤：

`先计算 abstract code-unit offsets/spans`

这一步让后面的 bytes / fixup / relocation 不用从零开始猜位置。

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_emit` 之后还不能直接跳去 `machine_bytes`
2. 再看 `include/machine/encode.h`，建立 encoded block / span / offset summary 的整体印象
3. 再看 `src/machine/lowering/machine_encode/` 的文件分工，理解 lowering / query / report / verify 是怎么拆的
4. 最后带着最小例子和伪代码去看：
   - offset 是怎么分配的
   - by-offset lookup 为什么在这一层就变重要了

学完你应该能：

1. 解释 `machine_encode` 和 `machine_emit` / `machine_bytes` 的边界
2. 说明这一层真正新增的是“位置语义”，而不是“字节语义”
3. 能看懂一个 `machine_emit -> machine_encode` 的 before/after 例子
4. 能说明为什么这一层已经是一个 offset-aware consumer artifact，而不只是 dump 层

---

## 1. 为什么需要 `machine_encode`

`machine_emit` 已经回答了：

- 每个 block 叫什么
- control target 用什么 stable label 表示

但它还没有回答：

- 每个 block 在代码序列里从哪里开始？
- terminator 自己位于 block 的哪个位置？
- 一个 target block 的 label 对应哪个 offset？

所以 `machine_encode` 引入的真正边界是：

`label-aware emitted program -> offset-aware encoding-prep artifact`

这一层最核心的新增信息，不是 bytes，而是：

`稳定的线性位置语义`

---

## 2. 为什么不能把这层塞回 `machine_emit`

`machine_emit` 负责的是：

- stable emitted label
- by-label navigation
- emitted report / refresh

但它不应该再继续负责：

- offset assignment
- block span 计算
- by-offset lookup
- target label 到 target offset 的解析

不然 `machine_emit` 会变成：

`既是 label artifact，又是 offset assignment engine`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_emit`
  - 回答“这个块叫什么”
- `machine_encode`
  - 回答“这个块在什么位置”
- `machine_bytes`
  - 回答“这个位置最终会落成哪些 bytes”

一句话压缩就是：

- `emit`：起名字
- `encode`：算位置
- `bytes`：落字节

---

## 3. 文件定位

- 接口：`include/machine/encode.h`
- 实现：`src/machine/lowering/machine_encode/`
- 测试：`tests/machine/lowering/machine_encode/`
- 规划文档：`docs/backend/MACHINE_ENCODE_PLAN.md`

当前目录已经是一个真实 split module：

- `machine_encode.c`
- `machine_encode_core.inc`
- `machine_encode_lower.inc`
- `machine_encode_query.inc`
- `machine_encode_report.inc`
- `machine_encode_verify.inc`
- `machine_encode_dump.inc`

所以这层并不是“在 dump 时顺手打印几个 offset”，而是已经有：

- lowering
- verifier
- by-label / by-offset query
- report artifact
- report refresh
- bridge from emit report / machine-ir report

---

## 4. `src/machine/lowering/machine_encode/` 目录怎么读

最简单的读法不是按文件名硬背，而是按四组职责去记。

### 4.1 core / artifact 基础层

- `machine_encode_core.inc`
- `machine_encode_verify.inc`
- `machine_encode_dump.inc`

这几块负责：

- 生命周期
- verifier
- dump

也就是说，它们在回答：

`encoded artifact 怎么被建、被验、被看`

### 4.2 lowering 主线

- `machine_encode_lower.inc`

这块在回答：

- emitted block 怎么复制成 encoded block
- `start_offset / terminator_offset / end_offset` 怎么计算
- emitted label / original ids 怎么保留

所以真正“这层新增的东西”基本都在这里。

### 4.3 raw program query

- `machine_encode_query.inc`

这块负责：

- by-label lookup
- by-function-name-and-offset lookup
- 找“哪个 block 覆盖了这个 offset”
- terminator target summary

这意味着 offset 在这一层已经不是 dump 里能看到的数字，而是正式 query key。

### 4.4 report / bridge 层

- `machine_encode_report.inc`

这一块让 `machine_encode` 不只是 raw program，而是：

- per-function summary
- per-block summary
- functions-with-calls filter
- functions-with-fallthrough filter
- functions-with-branches filter
- report refresh
- build from emit program / emit report / machine-ir program / machine-ir report

所以这一层现在已经很明显是：

`consumer-facing offset-aware artifact layer`

---

## 5. `include/machine/encode.h`：当前数据模型

### 5.1 op / operand / terminator 本体基本继承自 emit

这一层没有重新发明 operand，也没有重新发明 op family。

当前：

- `MachineEncodeBlock` 里面嵌着 `MachineEmitBlock`
- `MachineEncodeTerminatorKind` 其实就是 emit/layout 的 terminator kind

所以 `machine_encode` 的重点不在：

- 再改一轮 op family
- 再改一轮 control family

而在：

- `start_offset`
- `terminator_offset`
- `end_offset`
- `span`

### 5.2 encoded block 的新增字段

`MachineEncodeBlock` 当前最关键的字段是：

- `emit_index`
- `original_layout_index`
- `original_block_id`
- `label_name`
- `start_offset`
- `terminator_offset`
- `end_offset`
- `span`

所以它其实是在保留四层信息：

1. 原始 CFG 身份
2. layout 身份
3. emitted label 身份
4. encoded offset/span 身份

也就是说，这层相对 `machine_emit` 最大的新增不是结构重写，而是：

`每个 block 的线性位置模型`

### 5.3 terminator summary 的新增价值

这一层还专门引入了：

- `MachineEncodeTerminatorSummary`

它会把 terminator 目标统一 surface 成：

- primary target label
- primary target offset
- secondary target label
- secondary target offset
- compare op
- `branch_on_true`

所以这层已经不只知道“target 是谁”，还知道：

`target 在线性位置上在哪里`

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_encode` 负责：

1. 消费 `MachineEmitProgram`
2. 保留 emitted label 和 block 顺序
3. 给每个 block 计算：
   - `start_offset`
   - `terminator_offset`
   - `end_offset`
4. 在 dump / query / report 中同时 surface：
   - target label
   - target offset
5. 提供 verifier / dump / query / report / refresh
6. 提供从 emit report / machine-ir report 继续向下的桥接

### 当前它不是什么

这层不是：

1. final byte encoding
2. ISA-specific binary format
3. relocation artifact
4. real width-sensitive assembler backend

所以当前它更像：

`offset-aware encoding-prep`

不是：

`真正 assembler/code emitter`

---

## 7. 输入契约：它更喜欢吃已经 verifier-safe 的 emitted artifact

这一点和前几层一样重要。

当前最自然的主线不是：

`layout -> 随便生成几个 offset`

而是：

`machine_emit verify -> machine_encode lower`

也就是说，这层默认更喜欢吃：

- emitted label 已稳定
- block 顺序已稳定
- control target 已经是 label-facing artifact

它不会重新决定：

- label 该叫什么
- 哪条边该 fallthrough
- block 顺序该如何改

所以它的 current input contract 是：

`consume stable emitted artifact, do not redesign emit/layout policy`

---

## 8. Lowering 总体策略：它主要是在复制 artifact，并附加 offset/span

如果把 `machine_encode_lower.inc` 的精神压成一段 lesson 级伪代码，大致就是：

```text
copy register bank
copy globals

for each function:
  current_offset = 0

  for each emitted block in order:
    create encoded block
    copy emit/layout/original identities
    copy label

    start_offset = current_offset
    terminator_offset = current_offset + op_count
    end_offset = terminator_offset + 1

    clone emitted ops
    keep emitted terminator unchanged

    current_offset = end_offset

verify encoded program
```

所以这一层最重要的点是：

- 它不是又来一轮 control rewrite
- 它是在保留 emitted artifact 的前提下，附加位置语义

---

## 9. 当前 offset 模型到底意味着什么

这一层当前故意采用了一个非常保守的模型：

- 一条 op 先算一个 abstract code unit
- 一个 terminator 也先算一个 abstract code unit

于是：

```text
start_offset
terminator_offset = start_offset + op_count
end_offset = terminator_offset + 1
```

这个模型的价值不在“模拟真实机器码宽度”，而在：

- 足够稳定
- 足够结构化
- 已经能支持后续 by-offset query / target summary / byte-lowering

所以它是：

`first stable offset contract`

不是：

`final instruction sizing`

---

## 10. 一个最小 before/after 例子：emit -> encode

假设 `machine_emit` 里目前只有 label，没有位置：

```text
F0.L0:
  load_local a.0
  cmpbrift.t ne reg.0, 0, taken=F0.L2, fallthrough=F0.L1
F0.L1:
  ret
F0.L2:
  reti 1
```

到了 `machine_encode`，当前层会先给它分配 abstract offsets，比如：

```text
F0.L0: start=0 term=1 end=2
F0.L1: start=2 term=2 end=3
F0.L2: start=3 term=3 end=4
```

同时 terminator target 也开始同时带：

- target label
- target offset

也就是更接近：

```text
F0.L0:
  cmpbrift.t taken=F0.L2@3 fallthrough=F0.L1@2
```

所以这层真正新增的是：

- 线性位置语义

但它仍然还没生成：

- 真实字节
- fixup record
- object section

---

## 11. 一个更细的例子：为什么 `terminator_offset` 单独存在

如果一个 block 里有 2 条 op：

```text
F0.L0:
  op0
  op1
  jump F0.L1
```

那么当前 offset 模型会更接近：

```text
start_offset = 0
terminator_offset = 2
end_offset = 3
```

这里 `terminator_offset` 的意义不是多余字段，而是：

- 前两格属于 block body
- 最后一格属于 terminator

这对后面几层特别重要，因为它们往往要分清：

- op payload 在哪
- control transfer 自己的位置在哪

所以 `MachineEncodeBlockSpan` 不是纯展示字段，而是后续字节层的结构前提。

---

## 12. verifier：这层到底在锁什么

`machine_encode_verify.inc` 当前锁的重点不是“机器码编码对不对”，而是：

`encoded artifact 的 offset/span contract 是否自洽`

至少包括这些事情：

1. `emit_index` 必须 dense
2. block 必须有 label
3. block 必须有 terminator
4. `start_offset` 必须连续
5. `terminator_offset = start_offset + op_count`
6. `end_offset = terminator_offset + 1`
7. `span` 字段必须和这组三元关系一致
8. terminator target index 必须落在合法 block 范围内

这说明当前 verifier 锁的不是：

- “这是不是某个真实 ISA 的合法编码”

而是：

- “既然你已经进入 `machine_encode`，那 offset/span 模型必须严格成立”

最近还要把它再讲得更完整一点：

- verifier 现在不仅锁 offset/span
- 也会锁 return family 和 return payload shape 要匹配

比如：

- bare `ret` 不能带假 payload
- `reti` 不能没有返回值
- `ret_spill` 也不能拿 immediate 充数

这点非常重要，因为后面 `machine_bytes` 会把它当成位置语义前提。

---

## 13. query：为什么 by-offset lookup 在这一层就变关键了

`machine_encode_query.inc` 最值得抓的一点是：

- offset 不只是 dump 里显示的数字
- offset 已经成了正式导航入口

当前 raw encoded-program side 已经可以做：

- by-label lookup
- by-function-name-and-offset lookup
- 找“哪个 block 覆盖了这个 offset”
- 取 block summary
- 取 terminator target summary

这说明后续 consumer 已经可以直接问：

- `main` 函数里 offset `2` 落在哪个 block？
- `F0.L7` 这个 block 的 `start_offset` 是多少？
- 当前 terminator 的 taken target offset 是多少？

所以 offset 在这一层已经正式从“显示信息”变成：

`artifact-facing navigation key`

---

## 14. report：为什么这层已经是 consumer-facing offset artifact

`machine_encode_report.inc` 让这一层从 raw encoded program，再往上长出一层结构化 artifact。

当前 report side 至少已经有：

- per-function summary
- per-block summary
- functions-with-calls filter
- functions-with-fallthrough filter
- functions-with-branches filter
- block summary by label
- block summary by function-name-and-offset
- terminator summary from report
- report refresh

所以这层不只是：

- encoded program

还是：

- `encoded program + offset-aware summary/report shell`

这件事很关键，因为后续 `machine_bytes` 往往会同时关心：

- offset summary
- target summary
- 某段 offset 落在哪个 block

如果没有 report side，这些都得每次重扫 raw program。

### 14.1 report 里现在到底在统计什么

从 `src/machine/lowering/machine_encode/machine_encode_report.inc` 可以直接看出，当前 report 会为每个 function 统计至少这些方向的信息：

- `block_count`
- `unit_count`
- `op_unit_count`
- `terminator_unit_count`
- `call_count`
- `fallthrough_count`
- `jump_count`
- `branch_count`
- `compare_branch_count`
- `return_count`

而 block summary 这边则会稳定记录：

- `label_name`
- `start_offset`
- `terminator_offset`
- `end_offset`

也就是说，这一层的 report 不只是“顺手把 offset 填进表里”，而是已经形成了一套：

`encoded shape summary + offset summary`

这正是后面 `machine_bytes` 非常喜欢消费的那类信息。

### 14.2 filter 现在也已经是正式能力

当前 report side 已经明确维护了：

- `functions_with_calls`
- `functions_with_fallthrough`
- `functions_with_branches`

所以一个后续 consumer 不需要自己重扫所有 encoded function，才能知道：

- 哪些 function 还有 call site
- 哪些 function 还带 fallthrough
- 哪些 function 还带 branch/compare-branch family

这类 filter 很值得在课上强调，因为它说明：

`machine_encode` 已经不只是“算位置”，也开始替后续层做结构导航`

### 14.3 report lookup 已经和 raw lookup 形成平行能力

当前 report side 现在已经不只是“能 dump”，还已经支持：

- by emitted label 找 block summary
- by `function_index + offset` 找 block summary
- by `function_name + offset` 找 block summary
- 直接取 block terminator summary

所以这一层现在其实有两套平行能力：

- raw encoded program query
- report-side structured query

lesson 口径上，这点很有价值，因为它能提醒读者：

- 如果只是想遍历 artifact，本体就够了
- 如果想做“统计 + 定位 + downstream 过滤”，report side 更顺手

---

## 15. refresh：为什么这是这一层一个很关键的小能力

这一层也有一个容易被忽略但很工程化的能力：

- `machine_encode_report_refresh(...)`

它的意义是：

- 如果 later consumer 在 report-owned encoded program 上做了局部修改
- 不需要回到 `machine_emit` 整体重建
- 可以直接 refresh summary / block-summary / function-filter

lesson 口径上，这点很值得强调，因为它说明：

`machine_encode report 不是一次性导出结果，而是可继续承载本地后处理的 artifact`

---

## 16. 一个更完整的综合例子：emit -> encode

假设 `machine_emit` 里是：

```text
F0.L0:
  store_globali global.0, 9
  fallthrough F0.L1
F0.L1:
  reti 7
```

到了 `machine_encode`，会更接近：

```text
F0.L0 @0..2 term@1
  fallthrough F0.L1 @2
F0.L1 @2..3 term@2
  return
```

所以这一层做的不是：

- “把 label 再换一种写法”

而是：

- “把带 label 的 artifact 变成带 offset/span 的 artifact”

---

## 17. 测试现在在锁什么

当前 `tests/machine/lowering/machine_encode/machine_encode_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_emit -> machine_encode` lowering
2. `machine_ir -> ... -> machine_encode` upstream bridge
3. offset / span 分配
4. by-label lookup
5. by-offset lookup
6. structured terminator target summary
7. report refresh / report dump / report lookup

所以这组测试锁的不是“能不能打印 `@0..2`”，而是：

`offset contract + navigation API + bridge behavior`

一起锁。

### 17.1 最基础 offset-lowering / query case

最适合先看的测试名是：

- `test_machine_encode_lowers_offsets_from_machine_emit`

这一条测试几乎把第一层 encode contract 全锁了：

- `start_offset`
- `terminator_offset`
- `end_offset`
- by-label lookup
- by-function-name-and-offset lookup
- “哪个 block 覆盖了这个 offset”

所以如果要先建立第一印象，这一条就是最直接的入口。

### 17.2 machine-ir bridge 代表 case

下一条很值得顺着看：

- `test_machine_encode_bridge_from_machine_ir`

这条锁的是：

- `machine_ir -> machine_select -> machine_layout -> machine_emit -> machine_encode`
- bridge 下 offset 仍然要稳定
- dump surface 也要稳定

它回答的是一个很实际的问题：

- “这层是不是只能吃已经单独构好的 emitted program？”

答案显然不是，bridge 现在已经是正式公共入口。

### 17.3 structured terminator target 代表 case

这一条也特别值得单独点出来：

- `test_machine_encode_structured_terminator_targets`

它锁的不是一般的 offset，而是：

- target label
- target offset
- compare op
- `branch_on_true`

也就是说，这条测试证明当前 `machine_encode` 已经不仅仅会说：

- “这个 block 在 `@7..10`”

还会说：

- “这个 terminator 的 primary/secondary target 分别是谁、分别在什么 offset”

这正是后面做 bytes/fixup 很关键的前置语义。

### 17.4 report / clone / emit-report bridge 代表 case

这一组测试最能说明这层已经是 artifact 层：

- `test_machine_encode_report_surface`
- `test_machine_encode_clone_and_report_from_program`
- `test_machine_encode_from_machine_emit_report`

它们锁的是：

- report 可以直接从 raw encoded program build
- report 也可以从 emit report 继续桥下来
- clone 之后 report shape 还要稳定

所以这说明 report 不是旁支功能，而是 encode 层现在就已经明确支持的主消费面之一。

### 17.5 machine-ir report bridge 代表 case

再往后很值得看的，是：

- `test_machine_encode_from_machine_ir_report`

这条测试锁的是：

- 不是只有 raw `MachineIrProgram` 能一路桥下来
- `MachineIrAllocateRewriteReport` 这种上游 artifact 也能直接桥成 encode artifact

这很能体现当前这层的工程定位：

`它已经开始吃“上游 report artifact”，不是只吃“上游程序对象”`

### 17.6 verifier 边界代表 case

还有两条很适合课上拿出来讲：

- `test_machine_encode_verifier_rejects_bad_offsets`
- `test_machine_encode_verifier_rejects_bad_targets`

它们锁的是 encode 层最核心的两个 contract：

- offset/span 关系不能乱
- terminator target 也不能越界或错指

所以这一层 verifier 锁的不是：

- “这是不是某个 ISA 的真实机器码”

而是：

- “既然你已经进入 `machine_encode`，位置模型和 target 模型都必须自洽”

---

## 18. 这层和后面 `machine_bytes` 的边界

这是最容易混的地方。

### `machine_encode` 负责

- 计算 abstract offset/span
- 给 label 再附上位置语义
- 让 target 变成 `label + offset`

### `machine_bytes` 负责

- 真正生成 byte array
- 给 offset 再附上 byte-bearing 物化

也就是说：

- `machine_encode` 解决“它在什么位置”
- `machine_bytes` 解决“这个位置最后是什么字节”

### 18.1 和最终 `RISC-V` 方向的关系

根据：

- `docs/ir-conventions.md`
- `docs/NEXT_STEPS.md`

仓库最终目标方向仍然是：

- `RISC-V`

所以虽然当前 `machine_encode` 还没有进入：

- 真实 RISC-V instruction bytes
- 真实 branch-distance relaxation
- 真实 relocation record

但它已经在做一件对未来 RISC-V 很关键的事：

- 先把每个 emitted block 的线性位置固定下来
- 先把 control target 的 label 和 target offset 对齐起来

这一步越稳定，后面真的进入：

- `machine_bytes`
- 更具体的 `riscv32-preview` byte lane
- 乃至未来更真实的 RISC-V encoding/patching

就越不需要反复回头修改“目标到底指向哪里”这层语义。

所以它不是最终 RISC-V lowering 本身，但它确实是：

`未来 RISC-V bytes/fixup 落地之前的必要位置边界`

---

## 19. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_emit` 决定名字，`machine_encode` 决定位置
2. 这一层最大的新增信息是 `start_offset / terminator_offset / end_offset`
3. offset 在这里已经是正式 query key，不只是展示数字
4. report / refresh 让这一层变成真正的 offset-aware consumer artifact
5. 这层还没有进入 byte 语义，它只是把 emitted artifact 稳定地“位置化”

---

## 20. 一句话总结

`machine_encode` 把带 label 的 emitted program 变成带 abstract offset/span 的 encoding-prep artifact；它的关键不只是“算几个数字”，而是把 block 的位置语义、target 的 offset 语义、by-offset navigation、以及 report/refresh 能力一起固定下来，为真正的 byte-bearing 层提供稳定位置边界。
