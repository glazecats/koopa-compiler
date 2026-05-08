# Machine Emit Lesson（compiler_lab）

> 目标：解释 `machine_emit` 为什么不是“真正发机器码”，而是一个 label-aware emission-prep 层；它相对 `machine_layout` 到底新增了什么，`src/machine/lowering/machine_emit/` 里各个分片在做什么，以及为什么这一层已经是一个很实用的 consumer-facing artifact。

## 一句话定位

`machine_emit` 是 layout 之后第一次把线性 block 序列正式变成“带稳定 label 的 emitted artifact”的层。

## 常见误解

- 误解一：emit 就是在真正发机器码。
  - 当前这层还是 emission-prep，不是最终 ISA byte encoding。
- 误解二：layout 之后可以直接跳到 encode。
  - encode 依赖这里先把 label、emitted block、by-label lookup 这些消费面立起来。

## 前置阅读

最推荐先读：

1. `lesson/machine/lowering/machine_layout_lesson.md`
2. `lesson/machine/lowering/machine_select_lesson.md`

因为 emit 的前提就是：

- selected CFG 已经被 layout 成线性 block 顺序
- 现在要把这些 block 变成带 label 的 emitted artifact

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/lowering/machine_encode_lesson.md`
2. `lesson/machine/object/machine_bytes_lesson.md`

因为 emit 之后最自然的问题就是：

- 这些 emitted label/block 最终怎么变成稳定 offset
- 然后再怎么真正落成字节

---

## 最近同步

最近这层最值得同步的是：`machine_emit` 已经不只是“起 label 名字”，它也开始承接 preview-lane compatibility facts。

你现在最好把这层额外记成：

1. **emit report 现在会继续带 policy/compatibility surface**
   所以下游不需要每次回到 `machine_select`，也能知道当前 preview-lane 依赖哪些前置事实。

2. **`riscv32-preview` verifier 现在也已经延伸到 emit 层**
   这意味着某些 compatibility failure 不会再拖到 encode/bytes 才发现。

3. **它已经从“标签分配层”变成“带稳定 emitted identity + policy carry-through 的 artifact 层”**

所以现在对这层的理解最好从：

- `layout -> emitted labels`

更新成：

- `layout -> emitted labels + downstream carry-through surface`

最近这层再补一个最容易漏讲的点会更完整：

4. **emit 不会洗掉 bare `ret`**
   - 如果 layout block 本来就是 void-return block
   - emit 只会给它加 label / emitted identity
   - 不会把它改造成 `reti 0`

---

## 导学

`machine_layout` 已经决定了：

- block 顺序
- fallthrough 关系
- layout-aware terminator family

但后面如果要继续做：

- dump
- offset 计算
- bytes
- fixup / relocation

我们还需要一个稳定的名字体系，让块不再只靠 `layout index` 被引用。

这就是 `machine_emit` 的作用：

`给 laid-out block 分配稳定 emitted label`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_layout` 之后还不能直接进 `machine_encode`
2. 再看 `include/machine/emit.h`，建立 emitted block / label / report 的整体印象
3. 再看 `src/machine/lowering/machine_emit/` 的文件分工，理解 lowering / query / report / verify 是怎么拆的
4. 最后带着最小例子和伪代码去看：
   - label 是怎么分配的
   - 为什么 query/report 在这一层已经很重要

学完你应该能：

1. 解释 `machine_emit` 和 `machine_layout` / `machine_encode` 的边界
2. 说明这一层真正新增的是“稳定 emitted identity”，而不是新指令族
3. 能看懂一个 `machine_layout -> machine_emit` 的 before/after 例子
4. 能说明为什么这一层已经是一个很实用的后端 artifact，而不只是打印文本

---

## 1. 为什么需要 `machine_emit`

`machine_layout` 已经回答了：

- 这些 block 最后按什么顺序排
- 哪些边可以 fallthrough
- 哪些边仍然是显式 jump / branch

但它还没有回答：

- 每个 laid-out block 最后叫什么？
- 后续 consumer 如果想指一个 block，用什么稳定名字指？
- dump 里怎么把 control target 从 `layout.N` 变成更适合后续层消费的 artifact identity？

所以 `machine_emit` 引入的真正边界是：

`linearized layout program -> label-aware emitted program`

这一层最核心的新增信息，不是 bytes，也不是 offsets，而是：

`稳定的 emitted block identity`

---

## 2. 为什么不能把这层塞回 `machine_layout`

`machine_layout` 负责的是：

- layout order
- fallthrough-aware control presentation

但它不应该再继续负责：

- globally unique label naming
- label-based block lookup
- emitted artifact summary / report / refresh

不然 `machine_layout` 会变成：

`既是 block-order engine，又是 emitted artifact manager`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_layout`
  - 回答“块按什么顺序排、控制流怎么呈现”
- `machine_emit`
  - 回答“这些 laid-out block 用什么稳定名字出现”
- `machine_encode`
  - 回答“这些稳定命名的 block 最后占据哪些 offset/span”

一句话压缩就是：

- `layout`：排顺序
- `emit`：起名字
- `encode`：算位置

---

## 3. 文件定位

- 接口：`include/machine/emit.h`
- 实现：`src/machine/lowering/machine_emit/`
- 测试：`tests/machine/lowering/machine_emit/`
- 规划文档：`docs/backend/MACHINE_EMIT_PLAN.md`

当前目录已经是一个真实 split module：

- `machine_emit.c`
- `machine_emit_core.inc`
- `machine_emit_lower.inc`
- `machine_emit_query.inc`
- `machine_emit_report.inc`
- `machine_emit_verify.inc`
- `machine_emit_dump.inc`

所以这层并不是“起个标签然后在 dump 里替换一下字符串”那么简单，它已经有：

- lowering
- verifier
- raw program query
- report artifact
- report refresh
- bridge from upstream report

---

## 4. `src/machine/lowering/machine_emit/` 目录怎么读

最简单的读法不是按文件名硬背，而是按四组职责去记。

### 4.1 core / artifact 基础层

- `machine_emit_core.inc`
- `machine_emit_verify.inc`
- `machine_emit_dump.inc`

这几块负责：

- 生命周期
- verifier
- dump

也就是说，它们在回答：

`emitted artifact 怎么被建、被验、被看`

### 4.2 lowering 主线

- `machine_emit_lower.inc`

这块在回答：

- layout block 怎么复制成 emit block
- `layout_index` / `original_block_id` 怎么保留
- label 名字怎么生成

所以真正“这层新增的东西”基本就在这里。

### 4.3 raw program query

- `machine_emit_query.inc`

这块负责：

- whole-program summary
- function lookup by name
- block lookup by label
- block summary

这意味着 emitted label 不只是 dump 里能看到，它已经成为正式 query key。

### 4.4 report / bridge 层

- `machine_emit_report.inc`

这一块特别重要，它让 `machine_emit` 不只是 raw program，而是：

- per-function shape summaries
- per-block shape summaries
- functions-with-calls filter
- functions-with-fallthrough filter
- functions-with-branches filter
- report refresh
- direct dump from report
- bridge from `MachineIrAllocateRewriteReport`

所以这一层现在已经是很明显的：

`consumer-facing artifact layer`

---

## 5. `include/machine/emit.h`：当前数据模型

### 5.1 op / operand / terminator 本体基本继承自 layout

当前这层没有重新发明 operand，也没有重新发明 op family。

它沿用的是：

- `MachineLayoutOperand`
- `MachineLayoutOp`
- `MachineLayoutTerminatorKind`

所以 `machine_emit` 的重点不在：

- 再改一轮 op family
- 再改一轮 fallthrough family

而在：

- `block identity`
- `artifact navigation`

### 5.2 emit block 的新增字段

`MachineEmitBlock` 当前最关键的字段是：

- `emit_index`
- `original_layout_index`
- `original_block_id`
- `label_name`

这四个字段连起来看很重要：

- `emit_index`
  - 说明它在 emitted program 里的顺序
- `original_layout_index`
  - 说明它来自哪个 layout block
- `original_block_id`
  - 说明它最早来自哪个原始 CFG block
- `label_name`
  - 说明它现在的稳定 artifact identity

所以这层其实是在同时保留三种历史：

- 原始 block 身份
- layout 身份
- emitted 身份

### 5.3 report 侧新增了什么

`MachineEmitLowerReport` 不只是把 program 包起来，它还额外持有：

- `function_shape_summaries`
- `function_block_summary_offsets`
- `block_shape_summaries`
- `functions_with_calls`
- `functions_with_fallthrough`
- `functions_with_branches`

所以 emitted report 不只是“再存一份 dump 文本”，而是：

`one structured summary artifact above the raw emitted program`

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_emit` 负责：

1. 消费 `MachineLayoutProgram`
2. 保留 layout block 顺序
3. 给每个 block 分配 emitted label
4. 用 label 而不是 raw layout index 来 surface control targets
5. 提供 verifier / dump / query / report
6. 提供 report refresh / clone / upstream report bridge

### 当前它不是什么

这层现在还不是：

1. final encoding
2. address assignment
3. relocation layer
4. byte-bearing artifact

因此这里的 “emit” 更准确地理解成：

`emission-prep`

不是：

`assembler output`

---

## 7. 输入契约：它更喜欢吃已经 verifier-safe 的 layout artifact

这一点和前面两层很像，也很重要。

当前最自然的主线不是：

`selected cfg -> 自己随便起标签`

而是：

`machine_layout verify -> machine_emit lower`

也就是说，这层当前的 input contract 更接近：

- block order 已经稳定
- terminator family 已经是 fallthrough-aware 的 layout program

它不会重新决定：

- 哪一边该 fallthrough
- block 顺序该怎么排

这些都被视为上一层已经决定好的事实。

所以它的 current input contract 是：

`consume stable layout artifact, do not redesign layout policy`

---

## 8. Lowering 总体策略：它主要是在复制 artifact，并附加 label

如果把 `machine_emit_lower.inc` 的精神压成一段 lesson 级伪代码，大致就是：

```text
copy register bank
copy globals

for each function:
  copy function metadata

  for each layout block in order:
    create emitted block
    emit_index = block_index
    original_layout_index = src.layout_index
    original_block_id = src.original_block_id
    label_name = make_label(function_index, emit_index)

    clone selected ops
    keep layout terminator unchanged

verify emitted program
```

所以这一层最重要的点是：

- 它不是又来一轮 control rewrite
- 它是在保留 layout 结果的前提下，附加 emitted identity

这里“保留 layout 结果”现在最好明确到 return family：

- `ret`
- `reti imm`
- `ret spill`

都会被原样保留下来，只是从 `layout.N` 世界进入 `F0.Ln` 世界。

---

## 9. 一个最小 before/after 例子：plain fallthrough block

假设 `machine_layout` 里我们还只能按 layout index 看控制流：

```text
layout.0:
  store_globali global.0, 9
  fallthrough layout.1
layout.1:
  reti 7
```

到了 `machine_emit`，块本体和顺序基本不变，但会新增稳定标签：

```text
F0.L0:
  store_globali global.0, 9
  fallthrough F0.L1
F0.L1:
  reti 7
```

这里真正“增加”的东西非常具体：

- `layout.N` 变成稳定 label
- 控制流目标开始用 label name surface 出来

但它“还没增加”的是：

- 地址
- offset
- bytes

同时它“也还没改”的还有：

- return 是否带值

所以如果上游是：

```text
layout.1:
  ret
```

那么 emit 之后应该理解成：

```text
F0.L1:
  ret
```

而不是：

```text
F0.L1:
  reti 0
```

---

## 10. 一个更完整的例子：为什么要同时保留三种 block 身份

在 dump 或 query 视角里，当前 emitted block 其实会同时携带：

```text
emit.0 -> layout.0 -> bb.0
```

这意味着同一个 block 现在可以被三种方式理解：

1. 它在 emitted artifact 里是 `emit.0`
2. 它在 layout 阶段是 `layout.0`
3. 它最早来自原始 CFG 的 `bb.0`

这件事非常重要，因为后续 consumer 往往会同时需要：

- 当前 emitted 身份
- 它来自哪里

所以 `machine_emit` 不是单纯“改名”，而是：

`给 block 建立 stable emitted identity，同时保留 provenance`

---

## 11. verifier：这层到底在锁什么

`machine_emit_verify.inc` 当前锁的重点不是机器码正确性，而是：

`emitted artifact 自己的结构合法性`

至少包括这些事情：

1. `emit_index` 必须 dense
2. block 必须有 emitted label
3. emitted labels 必须在全程序范围内全局唯一
4. operand / spill / slot 引用必须合法
5. selected op family 传下来的形状不能被 emit 层弄坏
6. layout terminator family 的 payload shape 必须仍然匹配

这里最值得单独讲的是：

`全局唯一 emitted label`

因为这正是这一层存在的核心价值之一。

举个最小例子：

如果两个 block 都被命名成：

```text
F0.L1
```

那么这已经不是“dump 难看一点”，而是 emitted artifact identity 被破坏，verifier 会直接拒绝。

所以这层 verifier 在锁的不是：

- “控制流大概还能跑”

而是：

- “emitted artifact 是否已经有稳定、唯一、可导航的 block identity”

---

## 12. query：为什么 by-label lookup 在这一层就变重要了

`machine_emit_query.inc` 最值得抓的一点是：

- label 不只是展示文本
- label 已经成了正式导航入口

当前 raw emitted-program side 已经可以做：

- whole-program summary
- function lookup by name
- block lookup by emitted label
- block summary
- function summary

这说明后续 consumer 完全可以直接问：

- `F0.L7` 这个 block 是谁？
- 它来自哪个 function？
- 它原来是哪个 `layout.N` / `bb.N`？

所以 emitted label 在这一层的地位，已经不是“为了 dump 好看”。

它已经是：

`first stable artifact-facing block name`

---

## 13. report：为什么这层已经是 consumer-facing artifact

`machine_emit_report.inc` 让这一层从 raw emitted program，再往上长出一层结构化 artifact。

当前 report side 至少已经有：

- per-function shape summary
- per-block shape summary
- functions-with-calls filter
- functions-with-fallthrough filter
- functions-with-branches filter
- report refresh
- report dump
- build from raw program
- build from layout program
- build from machine-ir program
- build from machine-ir report

所以这层现在不只是：

- emitted program

还是：

- `emitted program + reusable analysis/report shell`

### 一个典型 consumer 问题

后续 consumer 可能想问：

- 哪些 emitted function 带 fallthrough？
- 某个 label 对应的 block terminator kind 是什么？
- 某个 function 里 block summary 的偏移切片是什么？

这些事情如果没有 report side，就得每次自己重扫 raw program。

而现在这一层已经把这些“后续常问问题”变成了正式 API。

这也是为什么 `machine_emit` 已经明显不是一层“薄包装”。

### 13.1 report 里现在到底在统计什么

从 `src/machine/lowering/machine_emit/machine_emit_report.inc` 里可以直接看出，当前 report 会为每个 function 统计至少这些东西：

- `block_count`
- `op_count`
- `call_count`
- `fallthrough_count`
- `jump_count`
- `branch_count`
- `branch_fallthrough_count`
- `compare_branch_count`
- `compare_branch_imm_count`
- `compare_branch_fallthrough_count`
- `compare_branch_imm_fallthrough_count`
- `return_count`
- `return_imm_count`
- `return_spill_count`

也就是说，这一层的 report 已经不只是“块名列表”，而是：

`emitted control-shape summary`

这对后面的 `machine_encode` 很重要，因为它关心的已经不是“块是不是有名字”，而是：

- 哪些函数有 fallthrough
- 哪些函数还有 branch family
- 哪些 emitted block 的 terminator 需要继续 downstream 消费

### 13.2 filter 现在也已经是正式能力

现在这层 report side 已经明确维护了：

- `functions_with_calls`
- `functions_with_fallthrough`
- `functions_with_branches`

所以一个后续 consumer 不需要自己重扫所有 function 才知道：

- 哪些 emitted function 值得重点看
- 哪些函数根本没有 control complexity
- 哪些函数已经进入“只剩 label + return”这种简单形状

这类 filter 在课上很值得强调，因为它说明：

`machine_emit` 已经开始替后续层做结构导航，而不是只做文本展示`

### 13.3 upstream bridge 也已经不只是 raw program bridge

当前 report build 已经不只支持：

- from raw emitted program
- from layout program
- from machine-ir program

还支持：

- from `MachineIrAllocateRewriteReport`

这说明当前 bridge 能力已经不只是：

- “从上游再 lower 一遍”

而是：

- “从上游现成 artifact 继续往下搭 emitted artifact”

lesson 口径上，这一点很重要，因为它意味着这层已经开始参与真正的 backend artifact pipeline 组装，而不是单独漂着的一个小模块。

---

## 14. refresh：为什么这是这一层一个很关键的小能力

这层有一个很容易被忽略，但实际上很工程化的能力：

- `machine_emit_lower_report_refresh_shape(...)`

它的意义是：

- 如果 later consumer 在 report-owned emitted program 上做了局部修改
- 不需要回到 `machine_layout` 整体重建
- 可以直接 refresh report summary

lesson 口径上，这件事非常值得强调，因为它说明：

`machine_emit report 不是一次性导出结果，而是可继续承载本地后处理的 artifact`

这会直接影响后面很多实验层怎么搭。

---

## 15. 一个更完整的综合例子：layout -> emit

假设 `machine_layout` 里当前是：

```text
layout.0 -> bb.0:
  cmpbrift.t ne reg.0, 0, taken=layout.2, fallthrough=layout.1
layout.1 -> bb.2:
  reti 2
layout.2 -> bb.1:
  reti 1
```

到了 `machine_emit`，当前层不会再改 branch family，而是更接近：

```text
F0.L0: ; emit.0 -> layout.0 -> bb.0
  cmpbrift.t ne reg.0, 0, taken=F0.L2, fallthrough=F0.L1
F0.L1: ; emit.1 -> layout.1 -> bb.2
  reti 2
F0.L2: ; emit.2 -> layout.2 -> bb.1
  reti 1
```

所以这一层做的不是：

- “再 lower 一轮 control”

而是：

- “把 layout 稳定结果翻译成 label-facing emitted artifact”

---

## 16. 测试现在在锁什么

当前 `tests/machine/lowering/machine_emit/machine_emit_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_layout -> machine_emit` lowering
2. emitted label 生成
3. by-label lookup
4. function / block summary
5. report refresh / report dump
6. `machine_ir -> machine_layout -> machine_emit` upstream bridge

所以这组测试锁的不是“能不能打印出 `F0.L0`”，而是：

`label identity + consumer API + bridge behavior`

一起锁。

### 16.1 最基础 label-lowering / summary case

最适合先看的测试名是：

- `test_machine_emit_lowers_labels_from_machine_layout`
- `test_machine_emit_summary_surface`

这两条测试锁的是最基础的 emitted contract：

- `layout.N -> F0.LN`
- `emit_index / original_layout_index / original_block_id / label_name`
- function summary
- block summary
- by-label lookup

如果要先建立第一印象，这两条就够了。

### 16.2 machine-ir bridge 代表 case

下一组推荐直接看：

- `test_machine_emit_bridge_surfaces_labels_from_machine_ir`
- `test_machine_emit_lower_dump_from_machine_ir`

这组测试锁的是：

- `machine_ir -> machine_select -> machine_layout -> machine_emit`
- bridge 过来之后 label 还要稳定
- dump surface 也要稳定

所以这能直接回答一个常见问题：

- “这层是不是只能吃已经单独构好的 layout program？”

答案显然不是，bridge 现在已经是正式能力。

### 16.3 report / refresh / clone 代表 case

这组最能说明这层已经是 artifact 层：

- `test_machine_emit_report_surface_from_machine_layout`
- `test_machine_emit_clone_and_report_from_program`
- `test_machine_emit_report_dump_from_machine_layout`

它们锁的是：

- report 可以直接从 layout build
- report 也可以从 raw emit program clone/build
- report dump 是正式 contract

这说明 report 不是附属调试工具，而是公共 surface。

### 16.4 machine-ir report bridge 代表 case

再往后最值得看的，是：

- `test_machine_emit_report_bridge_from_machine_ir_report`
- `test_machine_emit_report_dump_from_machine_ir_report`

这组测试锁的是：

- 不是只有 raw `MachineIrProgram` 能桥接下来
- `MachineIrAllocateRewriteReport` 这种上游 artifact 也能直接桥下来
- 而且 report dump contract 也要稳

这能很好地体现当前这层的工程定位：

`它已经开始吃“上游 artifact”，不是只吃“上游程序结构”`

### 16.5 verifier 边界代表 case

还有一条很值得课堂上提出来：

- `test_machine_emit_verifier_rejects_duplicate_labels`

它锁的是一个非常核心的 emitted contract：

- label 必须稳定
- label 也必须唯一

也就是说，这一层 verifier 锁的不是“名字看起来像不像 `F0.L0`”，而是：

`emitted label 作为 artifact identity，不能冲突`

---

## 17. 这层和后面 `machine_encode` 的边界

这是最容易混的地方。

### `machine_emit` 负责

- 给 block 稳定命名
- 保留 layout order
- 用 label-facing 方式 surface control target
- 给后续层提供 by-label navigation / report artifact

### `machine_encode` 负责

- 计算 abstract offset/span
- 给 label 再附上位置语义

也就是说：

- `machine_emit` 解决“这个块叫什么”
- `machine_encode` 解决“这个块在什么位置”

### 17.1 和最终 `RISC-V` 方向的关系

根据：

- `docs/ir-conventions.md`
- `docs/NEXT_STEPS.md`

仓库最终目标方向仍然是：

- `RISC-V`

所以虽然当前 `machine_emit` 还没有进入：

- 真实 RISC-V instruction encoding
- 真实 branch offset assignment
- 真实 relocation payload

但它已经在做一件对未来 RISC-V 很关键的事：

- 先把 laid-out block 的身份固定成稳定 label
- 让后续 offset / bytes / fixup 层不必再用脆弱的 `layout.N` 直接说话

这一步越稳定，后面真的进入：

- `machine_encode`
- `machine_bytes`
- 乃至更具体的 RISC-V byte lane

时，target references 就越不容易反复换表示。

所以它不是最终 RISC-V lowering 本身，但它确实是：

`未来 RISC-V backend 落到稳定 block reference 之前的必要名字边界`

---

## 18. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_layout` 决定顺序，`machine_emit` 决定名字
2. 这一层最大的新增信息是 `label_name`
3. emitted label 在这里已经是正式 query key，不是展示文本
4. report / refresh 让这一层变成真正的 consumer-facing artifact
5. 这层还没有进入 offset/bytes，它只是把线性布局稳定地“命名化”

---

## 19. 一句话总结

`machine_emit` 是把 layout 后的 block 变成“带稳定 emitted label 的后端 artifact”的那一层；它不只是给 dump 换个名字，而是把 block identity、by-label navigation、shape report 和 upstream bridge 一起固定下来，为后面的 offset、bytes、fixup 和 object work 提供稳定名字边界。
