# Machine Bytes Lesson（compiler_lab）

> 目标：解释 `machine_bytes` 为什么是后端流水线里第一次真正“有字节”的层，它相对 `machine_encode` 到底新增了什么，`src/machine/object/machine_bytes/` 里真实在做什么，以及为什么这一层已经不只是 bytes 本身，而是 reference / fixup / symbol / section 雏形一起长出来的关键层。

## 一句话定位

`machine_bytes` 是 backend 主线里第一次真正拥有 byte image 的层，也是 object-facing raw material 开始成形的地方。

## 常见误解

- 误解一：bytes 这里只是把 offset 翻成 `unsigned char[]`。
  - 当前这层还同时长出了 reference / fixup / symbol / section 雏形。
- 误解二：只要有 encode 就已经几乎等于有 bytes。
  - encode 还只是位置和 span；真正的 byte-bearing artifact 要到这里才成立。

## 导学

`machine_encode` 已经有：

- abstract offset
- block span
- by-label / by-offset lookup

但它还没有真正的：

- `unsigned char[]`
- byte count
- patch byte span
- byte-level target reference

`machine_bytes` 就是第一次把这些结构真正物化成：

`byte-bearing artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_encode` 之后还不能直接跳去 `machine_object`
2. 再看 `include/machine/bytes.h`，建立 byte-bearing block / report / fixup / symbol / section 的整体印象
3. 再看 `src/machine/object/machine_bytes/` 的实现主线，理解字节、reference、fixup、symbol、section 是怎么长出来的
4. 最后带着最小例子和伪代码去看：
   - bytes 是怎么生成的
   - 为什么这一层已经开始变成 object-facing raw material

学完你应该能：

1. 解释 `machine_bytes` 和 `machine_encode` / `machine_object` 的边界
2. 说明这一层真正新增的是“字节语义”，而不只是更多 summary
3. 能看懂一个 `machine_encode -> machine_bytes` 的 before/after 例子
4. 能说明 reference / fixup / symbol / section 为什么会从这一层开始重要起来

---

## 前置阅读

最推荐先读：

1. `lesson/machine/lowering/machine_encode_lesson.md`
2. `lesson/machine/lowering/machine_emit_lesson.md`

因为 bytes 这一层的前提就是：

- 上游已经把 block/layout/control surface 线性化了
- 也已经有稳定的 offset / span / target lookup

如果 encode 这层还没先看，`machine_bytes` 里的“为什么现在终于能生成真实字节”会不够直观。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/object/machine_object_lesson.md`
2. `lesson/machine/object/machine_reloc_lesson.md`

因为 bytes 之后最自然的问题就是：

- 这些字节怎么继续被装进 object-facing 容器
- 哪些地方要继续以 relocation / fixup 的形式保留

---

## 最近同步

如果你之前脑子里还把 `machine_bytes` 记成“把 offset 落成字节”，现在要把它更新成：

- “把 offset 落成字节，并开始承接当前 `riscv32-preview` lane 的真实 honesty boundary”

最近这层最值得记住的新增点有：

1. `target-policy summary`
   - program/report 入口现在能直接查询 preview 能力事实
2. preview register-bank honesty boundary
   - 当前 `riscv32-preview` 只显式承诺 `reg.0..reg.7 -> a0..a7`
3. large local/global slot offset honesty
   - 大 offset 不再依赖 12-bit immediate 运气
4. RV32M-shaped arithmetic slice
   - `mul/div/mod` 的 preview lowering 已经被 regression 锁住
5. global data reference slice
   - `data-addr / data-load / data-store` 已经进入 bytes-side artifact
6. global-object slice
   - `.sbss` / `.sdata` / `global-object` symbols 现在也会在这层开始出现

所以这层现在除了“第一次有 `unsigned char[]`”，还可以理解成：

- “第一层真正开始为直达 `RISC-V` preview asm / object / reloc / elf 提供现实物料的地方”

---

## 1. 为什么需要 `machine_bytes`

`machine_encode` 已经回答了：

- 每个 block 在什么 offset
- target label 对应什么 target offset

但它还没有回答：

- 这些 offset 最后落成什么字节？
- 一条 op 真正占几个字节？
- 哪几个字节以后要被 patch？

所以 `machine_bytes` 引入的真正边界是：

`offset-aware encoding-prep artifact -> byte-bearing artifact`

这一层最核心的新增信息，不是“offset 更精细了”，而是：

`真实 byte image`

---

## 2. 为什么不能把这层塞回 `machine_encode`

`machine_encode` 负责的是：

- stable offset/span
- by-offset lookup
- target offset summary

但它不应该再继续负责：

- 真正生成 `unsigned char[]`
- 统计 byte counts
- byte-level reference / patch span
- object-facing symbol/fixup/section 雏形

不然 `machine_encode` 会变成：

`既是 offset model，又是 byte materializer`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_encode`
  - 回答“它在什么位置”
- `machine_bytes`
  - 回答“这个位置最终是什么字节”
- `machine_object`
  - 回答“这些字节作为对象文件原材料怎么被 section/symbol/fixup 容器化”

一句话压缩就是：

- `encode`：算位置
- `bytes`：落字节
- `object`：装容器

---

## 3. 文件定位

- 接口：`include/machine/bytes.h`
- 实现：`src/machine/object/machine_bytes/`
- 测试：`tests/machine/object/machine_bytes/`
- 规划文档：`docs/backend/MACHINE_BYTES_PLAN.md`

这一层现在也已经跟着实现拆成了目录，而不是只看一个 `machine_bytes.c`：

- `src/machine/object/machine_bytes/machine_bytes.c`
- `src/machine/object/machine_bytes/machine_bytes_core.inc`
- `src/machine/object/machine_bytes/machine_bytes_lower.inc`
- `src/machine/object/machine_bytes/machine_bytes_verify.inc`
- `src/machine/object/machine_bytes/machine_bytes_dump.inc`
- `src/machine/object/machine_bytes/machine_bytes_report.inc`

所以 lesson 口径上更准确地说，现在是：

- `machine_bytes.c` 负责模块入口和聚合
- `machine_bytes_core.inc` 放基础 helper / lifecycle
- `machine_bytes_lower.inc` 放 `encode -> bytes` lowering 主线
- `machine_bytes_verify.inc` 放 verifier
- `machine_bytes_dump.inc` 放 dump
- `machine_bytes_report.inc` 放 report / summary / artifact helper

也就是说，它虽然还是一个紧凑模块，但已经不是旧 lesson 口径里的单文件实现。

---

## 4. `src/machine/object/machine_bytes/` 应该按哪几组职责来读

最容易读懂的方式，不是从头顺着看，而是按四组职责去记。

### 4.1 byte encoding helper

这一组主要在回答：

- 某类 op / terminator 编成什么 opcode-tag
- immediate / compare / target-pair 怎么塞进 payload byte
- 一条 op / terminator 最后占几个字节

典型 helper 包括：

- `machine_bytes_encode_op_kind(...)`
- `machine_bytes_encode_terminator_kind(...)`
- `machine_bytes_encode_operand_descriptor(...)`
- `machine_bytes_encode_target_pair(...)`
- `machine_bytes_encode_compare_flags(...)`
- `machine_bytes_op_encoded_size(...)`
- `machine_bytes_terminator_encoded_size(...)`

这说明当前这层不是“随便塞几个 marker”，而是已经有一套：

`first byte-shape contract`

### 4.2 raw byte program lowering

这一组主要在回答：

- `MachineEncodeBlock` 怎么变成 `MachineBytesBlock`
- byte offsets 怎么从 offset/span 过渡成 byte-offset/span
- `bytes[]` 怎么真正填进去

典型 helper 包括：

- `machine_bytes_clone_block_payload(...)`
- `machine_bytes_write_block_bytes(...)`
- `machine_bytes_lower_program_from_machine_encode(...)`

所以这一组才是真正“把 artifact 物化成字节”的主体。

### 4.3 raw byte query / copy

这一组主要在回答：

- 怎么按 function / block / label / program byte offset 找字节块
- 怎么把整份 program / 一个 function / 一个 block 的字节拷出来

典型能力包括：

- by-label lookup
- by-function-name-and-offset lookup
- by-program-byte-offset lookup
- copy whole program bytes
- copy one function bytes
- copy one block bytes

这说明 bytes 在这一层已经不只是 dump 可见，而是可以被正式复制和导航。

### 4.4 report / object-facing summary

这一组是这层最容易被低估的地方。

当前 report 不只在统计 byte count，它还会继续长出：

- reference summary
- fixup summary
- symbol summary
- section summary

典型 helper 包括：

- `machine_bytes_report_populate_function_references(...)`
- `machine_bytes_report_populate_symbols(...)`
- `machine_bytes_report_populate_fixups(...)`
- `machine_bytes_report_populate_sections(...)`
- `machine_bytes_report_refresh(...)`

所以 `machine_bytes` 不只是“有 bytes”，它已经开始在回答：

`这些 bytes 将来如何进入 object / relocation 语义`

---

## 5. `include/machine/bytes.h`：当前数据模型

### 5.1 raw byte block 的新增字段

`MachineBytesBlock` 当前最关键的字段是：

- `label_name`
- `start_byte_offset`
- `terminator_byte_offset`
- `end_byte_offset`
- `bytes`
- `byte_count`

和 `machine_encode` 对比，当前这层最重要的新增就是：

- `offset` 变成 `byte_offset`
- `abstract span` 变成真实 `byte_count`
- block 终于带上了 `unsigned char *bytes`

所以这层相对上一层最值得抓住的一句就是：

`offset-aware block 终于变成 byte-bearing block`

### 5.2 reference / fixup / symbol / section summary

这一层不只定义 block/program，还定义了一大串非常重要的 summary：

- `MachineBytesReferenceSummary`
- `MachineBytesFixupSummary`
- `MachineBytesSymbolSummary`
- `MachineBytesSectionSummary`

这说明从这一层开始，系统已经在显式区分：

1. 原始 target reference
2. 未来会变成 patch 点的 fixup
3. 可能被 object layer 当成 symbol 的名字
4. 这些内容属于哪个 section

也就是说，`machine_bytes` 不只是“编码完了的 bytes”，还是：

`object-facing raw material`

### 5.3 program/report 两个视角都是真实的

当前 API 同时支持：

- raw `MachineBytesProgram`
- structured `MachineBytesReport`

所以这一层不是“program 是真的，report 只是附带”。

现在两边都已经很重要：

- raw program 负责字节本体和复制
- report 负责把 reference/fixup/symbol/section 组织成更好消费的 artifact

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_bytes` 负责：

1. 消费 `MachineEncodeProgram`
2. 给 block 生成真实 byte array
3. 计算 byte-level 的：
   - `start_byte_offset`
   - `terminator_byte_offset`
   - `end_byte_offset`
4. 提供 raw byte query / copy
5. surface call / control target 的 reference / fixup / symbol / section 雏形
6. 提供 verifier / dump / report / refresh

### 当前它不是什么

这层还不是：

1. target-specific final machine code
2. relocation table artifact
3. object file container
4. ELF/COFF/Mach-O serializer

所以当前它更像：

`byte-bearing pre-object artifact`

不是：

`final object file`

---

## 7. 输入契约：它更喜欢吃已经 verifier-safe 的 encoded artifact

这一点和前几层一样重要。

当前最自然的主线不是：

`offset 模型差不多就随便吐点字节`

而是：

`machine_encode verify -> machine_bytes lower`

也就是说，这层默认更喜欢吃：

- encoded block span 已经稳定
- terminator target index 已经自洽
- label / offset / by-offset query 都已经站稳

它不会重新决定：

- offset/span 该怎么算
- target offset 该怎么解析

所以它的 current input contract 是：

`consume stable encoded artifact, then materialize bytes`

---

## 8. Lowering 总体策略：它先算 byte size，再写 bytes

如果把这层 lowering 的精神压成一段 lesson 级伪代码，大致就是：

```text
copy register bank / globals / function metadata

for each encoded block:
  copy emit/layout/original identity
  copy label

  start_byte_offset = encoded start_offset
  terminator_byte_offset = encoded terminator_offset
  end_byte_offset = encoded end_offset

  byte_count =
    sum(encoded_size(op_i)) + encoded_size(terminator)

  allocate bytes[byte_count]
  write op bytes
  write terminator bytes

verify byte program
```

这里要特别注意两点：

1. 当前 byte offset 仍然和 encode offset 一一对应
2. 但 `byte_count` 已经不再总是 1-unit-per-item

因为有些 op / terminator 已经是 variable-size 的最小模型了。

---

## 9. 当前 byte 模型到底意味着什么

这一层当前不是做真实 ISA 编码，而是一个保守但已经很像编码的模型。

### 9.1 op byte size

当前至少有这些 shape：

- 普通很多 op：1 byte
- `MATERIALIZE_IMM` / `STORE_*_IMM` / `ALU_IMM` / `CMP_IMM`：2 bytes
- call family：`2 + arg_count` bytes

所以这层已经不是：

- “每条 op 永远一个字节”

### 9.2 terminator byte size

terminator 当前也已经分大小：

- `RETURN`：1 byte
- `RETURN_IMM` / `RETURN_SPILL` / `JUMP` / `FALLTHROUGH`：2 bytes
- `BRANCH` / `BRANCH_FALLTHROUGH`：3 bytes
- `COMPARE_BRANCH*`：4 bytes

这意味着：

- control family 已经在字节层体现出更复杂的 shape

### 9.3 payload 不是纯占位符

源码里现在已经把不少结构信息编码进 payload 里了：

- immediate value
- arg count
- operand descriptor
- target pair
- compare flags
- `branch_on_true`

所以这层不是“假的 byte artifact”，而是：

`target-agnostic but structurally meaningful byte artifact`

---

## 10. 一个最小 before/after 例子：encode -> bytes

假设 `machine_encode` 里当前只有位置语义：

```text
F0.L0: start=0 term=2 end=3
F0.L1: start=3 term=3 end=4
F0.L2: start=4 term=4 end=5
```

到了 `machine_bytes`，同样的块会第一次真正带上字节，比如概念上变成：

```text
F0.L0: bytes = [10 21 70 xx]
F0.L1: bytes = [81]
F0.L2: bytes = [82]
```

同时它还会正式带上：

- `start_byte_offset`
- `terminator_byte_offset`
- `end_byte_offset`

也就是说，这层真正新增的是：

- 真实 byte image
- byte-level span

这就是为什么后面的 object/reloc 不再需要从 abstract offset 反推。

---

## 11. 一个更细的例子：call 为什么在这层会很快变复杂

假设有一条 selected call：

```text
calli foo(reg.1, 7)
```

在 `machine_encode` 里，你大概只知道：

- 这是一条 call family op
- 它在某个 offset

但到了 `machine_bytes`，当前模型就会把它编码成更像：

```text
[opcode-tag][arg-count][arg0-desc][arg1-desc]
```

也就是说，call 从这一层开始不再只是“一个位置上的 op”，而是：

- 已经有 payload shape
- 已经有 reference summary
- 未来还会长成 fixup / symbol 关系

这也解释了为什么这层比 `machine_encode` 更接近 object-facing world。

---

## 12. verifier：这层到底在锁什么

`machine_bytes` 这一层的 verifier 重点不只是：

- block index 合不合法

而是：

`byte-bearing artifact 自己的结构是否自洽`

至少包括这些事情：

1. byte offset / byte count / end offset 是否一致
2. block bytes 是否真的存在
3. terminator target index 是否还合法
4. byte-bearing block 的身份字段是否自洽
5. raw bytes 的形状和 block payload 是否匹配

也就是说，这层 verifier 锁的不是：

- “这些字节是不是某个 ISA 的最终合法编码”

而是：

- “既然你已经进入 `machine_bytes`，那 byte-bearing block contract 必须严格成立”

---

## 13. raw program query / copy：为什么 bytes 在这一层已经是正式导航对象

这一层最容易被低估的一点就是：

- bytes 不只是 dump 可见
- bytes 已经能被正式复制和导航

当前 raw program side 已经可以做：

- whole program byte count
- function byte span
- by-label lookup
- by-function-name-and-offset lookup
- by-program-byte-offset lookup
- copy whole program bytes
- copy one function bytes
- copy one block bytes

所以后续 consumer 已经可以直接问：

- 整个 program 一共有多少字节？
- `main` 从程序字节流的哪里开始到哪里结束？
- 程序字节 offset `37` 落在哪个 block？
- 给我 `F0.L7` 的原始 bytes

这说明 bytes 在这一层已经正式从“显示结果”变成：

`artifact-facing navigation object`

---

## 14. reference：为什么这一层先长出 reference summary

这一步非常关键。

当前 `machine_bytes_report_populate_function_references(...)` 会把：

- call site
- jump / fallthrough / branch / compare-branch 的 target

先整理成：

- `MachineBytesReferenceSummary`

这层 reference 先回答的是：

- 谁引用了谁
- owner byte span 在哪
- patch byte span 在哪
- target label / target byte offset 是多少

lesson 口径上可以把它理解成：

`先把“引用关系”显式化，再谈 fixup`

所以这一层并不是直接跳去 relocation，而是先把 raw byte-level reference 摊平。

---

## 15. fixup / symbol / section：为什么这些会从 bytes 层开始长出来

这也是这一层最值得讲的地方。

### 15.1 symbol

当前 report 会先长出三类 symbol：

- function symbol
- block symbol
- external symbol

也就是说，到了 `machine_bytes`，系统已经开始回答：

- 哪些名字是本程序里定义的
- 哪些名字只是外部 call target

### 15.2 fixup

当前 report 还会把 reference 进一步收成：

- call-target fixup
- control-primary fixup
- control-secondary fixup

所以 fixup 在这一层已经开始成为一等信息了。

### 15.3 section

当前 report 还会进一步收成：

- `.text` section summary

所以 object 层不是“凭空发明 section”，而是：

- `machine_bytes` 已经把 section-facing raw material 准备好了

lesson 口径上最好把这一整条讲成：

`bytes -> reference -> fixup -> symbol -> section`

这就是为什么 `machine_bytes` 不只是“有字节”。

---

## 16. report：为什么这层已经是 object-facing raw material

`machine_bytes_report_refresh(...)` 这一块，其实就是把这一层从 raw bytes 推向 object-facing raw material 的关键。

当前 report side 至少已经有：

- function byte summary
- block byte summary
- whole-program byte layout
- reference summaries
- fixup summaries
- symbol summaries
- section summaries
- functions-with-calls / fallthrough / branches filter

所以这层不只是：

- byte-bearing program

还是：

- `byte-bearing program + pre-object structural summary shell`

这会直接让后面的 `machine_object` 轻松很多，因为它已经不用从 raw bytes 自己逆向推这些信息了。

### 16.1 report 里现在到底在统计什么

从 `include/machine/bytes.h` 和 `machine_bytes_report_refresh(...)` 这条实现线可以直接看出，当前 report 至少会稳定持有：

- function byte summaries
- block byte summaries
- whole-program byte layout
- reference summaries
- fixup summaries
- symbol summaries
- section summaries

而且这些 summary 不是孤立存在的，它们之间还通过 offset/slice 彼此挂接：

- `function_reference_offsets`
- `function_fixup_offsets`
- section 的 `symbol_start_index / symbol_count`
- section 的 `fixup_start_index / fixup_count`

也就是说，当前 report 不是一堆并列数组，而是已经开始形成：

`one object-facing indexed artifact`

### 16.2 raw bytes 和 report bytes 现在也是两套平行能力

这一层现在不只支持：

- 从 raw `MachineBytesProgram` 直接 copy whole-program / function / block bytes

还支持：

- 从 `MachineBytesReport` 直接 copy report-owned byte image

所以 lesson 里很值得提醒一句：

- raw program 适合“字节本体遍历”
- report 更适合“字节本体 + 关系摘要 + section/fixup/symbol 一起看”

这也是为什么这一层开始明显有 “artifact shell” 的味道，而不只是 “byte array 容器”。

---

## 17. 一个更完整的综合例子：encode -> bytes -> reference/fixup

假设 `machine_encode` 里有：

```text
F0.L0: start=0 term=2 end=4
  cmpbrift.t ... taken=F0.L2@4 fallthrough=F0.L1@2
F0.L1: start=2 term=2 end=3
F0.L2: start=4 term=4 end=5
```

到了 `machine_bytes`，当前层会更接近：

```text
F0.L0: bytes=[.. .. .. ..]
F0.L1: bytes=[..]
F0.L2: bytes=[..]
```

然后又会在 report 里进一步长出：

```text
reference:
  owner=F0.L0 patch@3 target=F0.L2

fixup:
  kind=control-primary target-block=F0.L2

symbol:
  function main
  block F0.L0
  block F0.L1
  block F0.L2

section:
  .text
```

所以这一层做的不是：

- “把 offset 再换一种写法”

而是：

- “把 offset-bearing artifact 变成 byte-bearing artifact，并且把 object-facing 关系先显式摊平”

---

## 18. 测试现在在锁什么

当前 `tests/machine/object/machine_bytes/machine_bytes_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_encode -> machine_bytes` lowering
2. byte image 物化
3. by-label / by-offset / by-program-offset lookup
4. raw byte copy helpers
5. reference summary
6. fixup summary
7. symbol / section summary
8. `machine_ir -> ... -> machine_bytes` 上游桥接

所以这组测试锁的不是“能不能打印几串十六进制”，而是：

`byte contract + navigation API + pre-object summary behavior`

一起锁。

### 18.1 最基础 byte-lowering / raw query case

最适合先看的测试名是：

- `test_machine_bytes_lowers_from_machine_encode`

这条测试几乎把第一层 bytes contract 全锁了：

- `MachineEncodeProgram -> MachineBytesProgram`
- `byte_count`
- `start_byte_offset / terminator_byte_offset / end_byte_offset`
- by-label / by-program-byte-offset lookup
- whole-program / function / block byte copy

如果要先建立“这一层到底什么时候开始真的有 `unsigned char[]`”的第一印象，这条就是最直接的入口。

### 18.2 report / bridges / pre-object summaries 代表 case

下一条最值得顺着看的是：

- `test_machine_bytes_report_and_bridges`

它锁的不是单一功能，而是一整串对象前材料能力：

- reference summary
- fixup summary
- symbol summary
- `.text` section summary
- report dump
- `MachineIrAllocateRewriteReport -> MachineBytesReport` bridge

这条测试几乎可以当成 “为什么我说这一层已经是 pre-object raw material” 的最直接证据。

### 18.3 profile-aware / `riscv32-preview` 代表 case

这一条非常值得在课上单独拿出来讲：

- `test_machine_bytes_riscv32_preview_profile_changes_text_bytes`

它锁的是一个很关键的事实：

- 当前 bytes 层不只是 generic tag-byte
- profile-aware lowering 已经能让 `riscv32-preview` 真的改 `.text` 字节
- 同时 patch/fixup span 也会跟着 profile 变化

也就是说，到了 `machine_bytes` 这一层，`riscv32-preview` 已经不只是“header policy”或“后面 ELF 再决定的事”，而是：

`真实 byte lane 已经开始分叉`

### 18.4 program-level byte image / byte-offset navigation 代表 case

再往后很值得看的，是：

- `test_machine_bytes_program_level_offsets_and_byte_image`

它锁的是：

- whole-program byte image
- per-function byte span
- program-byte-offset 到 block 的映射

这条测试特别适合解释为什么前面 lesson 要一直强调：

`bytes 在这一层已经是正式导航对象，不只是 dump 结果`

### 18.5 emit/encode artifact bridge 与 payload 代表 case

最后两条也很值得顺着看：

- `test_machine_bytes_from_machine_emit_artifacts`
- `test_machine_bytes_encodes_call_payloads`

它们锁的是：

- 不是只有 raw `MachineEncodeProgram` 能继续桥下来
- `MachineEmitProgram` / `MachineEmitLowerReport` 那类上游 artifact 也可以桥成 bytes
- call payload 在这一层已经真正进入 byte contract，而不是纯占位符

这能很好地体现当前 bytes 层的工程定位：

`它已经开始吃多种上游 artifact，并把 payload 物化成真正字节`

---

## 19. 这层和后面 `machine_object` 的边界

这是最容易混的地方。

### `machine_bytes` 负责

- 生成真实 byte array
- 记录 reference
- 记录 fixup/symbol/section 雏形

### `machine_object` 负责

- 把这些原材料真正装进 object-facing section/symbol/fixup 容器

也就是说：

- `machine_bytes` 解决“字节和引用关系长什么样”
- `machine_object` 解决“这些关系作为对象文件语义怎么组织”

### 19.1 和最终 `RISC-V` 方向的关系

根据：

- `docs/ir-conventions.md`
- `docs/NEXT_STEPS.md`

仓库最终目标方向仍然是：

- `RISC-V`

所以虽然当前 `machine_bytes` 还没有进入：

- 真正最终版 RISC-V object emission
- 完整 relocation ABI
- 完整 ELF container

但它已经在做一件对未来 RISC-V 非常关键的事：

- 真正把控制流 target / call site / compare branch family 落成字节
- 并且允许 `riscv32-preview` 在 bytes 层就开始改变 `.text` 内容和 patch span

这一步越清楚，后面的：

- `machine_object`
- `machine_reloc`
- `machine_container`
- `machine_elf`

就越不需要再回头发明“这些引用关系在字节里到底体现在哪里”。

所以它不是最终 RISC-V object layer 本身，但它确实是：

`未来 RISC-V object/reloc 落地之前的关键 byte-bearing 边界`

---

## 20. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_encode` 决定位置，`machine_bytes` 决定字节
2. 这一层最大的新增信息是 `bytes + byte_count + byte_offset`
3. reference/fixup/symbol/section 会从这一层开始一起长出来
4. raw byte query/copy 让 bytes 在这里就已经成为正式 artifact
5. 这层还不是 object file，但已经是非常明显的 object-facing raw material

---

## 21. 一句话总结

`machine_bytes` 是整个后端流水线里第一次真正拥有 byte array 的层；它不只是把 encode 的 offset-aware artifact 落成字节，还把 reference、fixup、symbol、section 这些后续 object/relocation 需要的结构前提一起长出来，因此它已经是一个非常明确的 pre-object、byte-bearing artifact 边界。
