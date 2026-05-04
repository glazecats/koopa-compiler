# Machine ELF Lesson（compiler_lab）

> 目标：解释 `machine_elf` 在这条流水线里的最终位置，它相对 `machine_container` 到底新增了什么，`src/machine/object/machine_elf/` 里真实在做什么，以及为什么这一层是“真实目标格式边界”而不是再一个内部 IR。

## 一句话定位

`machine_elf` 是这条 object 线第一次真正落到标准外部对象格式边界的层。

## 常见误解

- 误解一：container 和 ELF 其实差不多，只是名字不同。
  - container 还是仓库内部最终容器，ELF 才是外部工具能按标准理解的 object format。
- 误解二：ELF 这里只负责一次性导出 bytes。
  - 当前这层还承担 parse、round-trip、normalize、target-policy 等结构化能力。

## 导学

`machine_container` 虽然已经能给出稳定最终字节，但它还是：

- 仓库内部格式
- 不是标准对象文件格式

所以如果要迈到一个更真实的目标文件边界，我们就需要：

`machine_elf`

它的职责不是再发明一层内部容器，而是：

`把前面的 section/symbol/relocation/container 信息投影成真实 ELF relocatable object skeleton`

这份讲义建议按下面顺序读：

1. 先理解为什么 container 之后还要单独落一层 ELF
2. 再看 `include/machine/elf.h`，建立 ELF file / report / target profile 的整体印象
3. 再看 `src/machine/object/machine_elf/` 的实现主线，理解 container 怎么被投影成 ELF
4. 最后带着最小例子和伪代码去看：
   - container 信息是怎么变成 `.text/.symtab/.rel.text` 的
   - 为什么 parse / round-trip / normalize 是这一层的重点能力

学完你应该能：

1. 解释 `machine_elf` 和 `machine_container` 的边界
2. 说明这一层真正新增的是“标准对象格式语义”，而不是又一种内部容器
3. 能看懂一个 `machine_container -> machine_elf` 的 before/after 例子
4. 能说明为什么 target profile、parse、round-trip、normalize 会在这一层变得重要

---

## 前置阅读

最推荐先读：

1. `lesson/machine/object/machine_container_lesson.md`
2. `lesson/machine/object/machine_reloc_lesson.md`
3. `lesson/machine/object/machine_bytes_lesson.md`

如果你还没先知道：

- 内部容器字节是怎么组织的
- relocation / symbol / section 信息前面是怎么收束的

那 ELF 这里的 `.text / .symtab / .rel.text / .shstrtab` 投影会显得比较突然。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_image_lesson.md`
2. `lesson/machine/runtime/machine_exec_lesson.md`

因为 ELF object 这层之后，最自然的问题就是：

- 如果从加载视角看，这份 ELF 现在已经知道哪些地址事实
- 它什么时候才算 executable candidate

---

## 最近同步

最近 `machine_elf` 最值得同步到 lesson 里的，是它现在已经开始显式表达：

- 这份 ELF 是 direct preview build 来的
- 还是 imported / reparsed / reprofiled 来的

几个关键新增点如下：

1. `MachineElfArtifactSummary`
   - raw file 和 report artifact 现在都能区分 `direct-patch-spans` 与 `imported-relocation-table`
2. `origin_profile`
   - 除了当前 `target_profile`，现在还有显式 `origin_profile`
3. relocation-family summary
   - ELF 这层也能直接给出 `call / primary-control / secondary-control` family 计数
4. verifier-backed direct-vs-imported honesty
   - canonical direct preview ELF artifact 不允许偷偷带不该存在的 secondary control relocation
5. global-object section slice
   - `.sbss` / `.sdata` 与 `STT_OBJECT` globals 现在也能一路保留下来

所以现在对这层的印象最好从：

- “标准 ELF object”

更新成：

- “带 provenance / relocation-family / global-object slice 的标准 ELF object”

---

## 1. 为什么需要 `machine_elf`

`machine_container` 已经回答了：

- header/table/string/payload 的内部布局长什么样
- 内部最终 byte image 怎么组织

但它还没有回答：

- 这份 byte image 如何成为真正 ELF32 little-endian relocatable object？
- `.text`、`.strtab`、`.symtab`、`.rel.text`、`.shstrtab` 分别怎么进入标准 ELF section 语义？
- later 外部工具如果看到这份 artifact，能不能按 ELF 规则理解它？

所以 `machine_elf` 引入的真正边界是：

`internal final serialized container -> standard ELF artifact`

这一层最核心的新增信息，不是“又有一份 bytes”，而是：

`standard object-format semantics`

---

## 2. 为什么不能把这层塞回 `machine_container`

`machine_container` 负责的是：

- 仓库内部最终容器布局
- header/table/string/payload 作为内部约定长什么样

但它不应该再继续负责：

- ELF header 语义
- ELF section header 语义
- ELF symbol table 语义
- ELF relocation entry 语义
- target-profile-specific `e_machine` / relocation opcode policy

不然 `machine_container` 会变成：

`既是内部容器，又是外部标准对象格式`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_container`
  - 回答“我们自己的最终内部镜像长什么样”
- `machine_elf`
  - 回答“标准 ELF object 长什么样”

一句话压缩就是：

- `container`：内部最终容器
- `elf`：外部标准对象格式

---

## 3. 文件定位

- 接口：`include/machine/elf.h`
- 实现：`src/machine/object/machine_elf/`
- 测试：`tests/machine/object/machine_elf/`
- 规划文档：`docs/backend/MACHINE_ELF_PLAN.md`

这一层现在也已经跟着实现拆成了一个目录，而不是只盯着 `machine_elf.c`：

- `src/machine/object/machine_elf/machine_elf.c`
- `src/machine/object/machine_elf/machine_elf_core.inc`
- `src/machine/object/machine_elf/machine_elf_build.inc`
- `src/machine/object/machine_elf/machine_elf_parse.inc`
- `src/machine/object/machine_elf/machine_elf_verify.inc`
- `src/machine/object/machine_elf/machine_elf_query.inc`
- `src/machine/object/machine_elf/machine_elf_dump.inc`

lesson 口径上更准确地说，现在是：

- `machine_elf.c` 负责模块入口和聚合
- `machine_elf_core.inc` 放基础 byte/helper / lifecycle 逻辑
- `machine_elf_build.inc` 放 `container -> elf` forward build / refresh 主线
- `machine_elf_parse.inc` 放 parse / import / normalize / round-trip 相关逻辑
- `machine_elf_verify.inc` 放 verifier
- `machine_elf_query.inc` 放 query / report-facing helper
- `machine_elf_dump.inc` 放 dump

也就是说，它已经不是一个“只会 build 一次 ELF bytes”的薄层。

---

## 4. `src/machine/object/machine_elf/` 应该按哪几组职责来读

最容易读懂的方式，不是从头顺着看，而是按五组职责去记。

### 4.1 ELF binary encoding helper

这一组主要在回答：

- ELF header / section header / symbol / relocation entry 的字段怎么写
- little-endian 的 `u16/u32` 怎么写和读
- section/header 对齐怎么算

典型 helper 包括：

- `machine_elf_write_u16_le(...)`
- `machine_elf_write_u32_le(...)`
- `machine_elf_read_u16_le(...)`
- `machine_elf_read_u32_le(...)`
- `machine_elf_align_up(...)`

这说明当前 ELF 层不是“抽象结构体就够了”，而是已经在真实处理：

`ELF byte-level layout mechanics`

### 4.2 target profile / relocation policy

这一组主要在回答：

- `generic-elf32`
- `riscv32-preview`
- `i386-preview`

这些 profile 各自对应：

- `e_machine`
- `e_flags`
- call / control relocation type mapping

典型 helper 包括：

- `machine_elf_get_target_policy(...)`
- `machine_elf_map_relocation_type(...)`
- `machine_elf_relocation_kind_from_type(...)`
- `machine_elf_profile_from_header(...)`

所以 target profile 不是附加配置，而是这层 current design 的一等公民。

### 4.3 forward build / refresh

这一组主要在回答：

- `MachineContainerFile` 怎么变成 `MachineElfFile`
- `MachineElfFile` 改过之后怎么 refresh 回 canonical ELF bytes

也就是说，这层不只是 forward serializer，还有：

- editable structured file
- refresh back to bytes

### 4.4 parse / normalize / round-trip

这一组主要在回答：

- 现有 ELF bytes 怎么 parse 回 `MachineElfFile`
- 输入如果 section/symbol/relocation 顺序不 canonical，怎么 normalize 回 canonical output

这部分正是当前 ELF 层和前面所有层最大的区别之一：

- 它已经要处理“外部格式导入”

### 4.5 report / query / verifier / dump

这一组主要在回答：

- section / symbol / relocation / header 怎么被 query
- target policy 怎么被 query
- dump 怎么把 ELF 结构完整打出来
- verifier 怎么重新从字节图像交叉检查结构视图

所以这层已经很明显不是“最终输出就结束”，而是一个可读、可查、可 round-trip 的 ELF artifact。

---

## 5. `include/machine/elf.h`：当前数据模型

### 5.1 `MachineElfFile`

当前最关键的数据结构是：

```text
MachineElfFile =
  target_profile
  + sections
  + symbols
  + relocations
  + owned ELF bytes
  + canonical section indices
```

这里很重要的一点是：

- 它既有 structured arrays
- 也有 owned byte image

所以它不是“只是一份 parse 结果”，也不是“只是一份 bytes”。

它是：

`structured ELF view + owned ELF bytes`

### 5.2 header / target policy summary

这一层还有两个很关键的 summary：

- `MachineElfHeaderSummary`
- `MachineElfTargetPolicySummary`

这说明当前 ELF 层不仅关心：

- 文件里写了什么

还关心：

- 这些 header / relocation policy 在 profile 上意味着什么

### 5.3 report artifact

`MachineElfReport` 当前会额外持有：

- file
- header summary
- target policy summary
- section summaries
- symbol summaries
- relocation summaries

所以 ELF 层不仅能给你 raw file，还能给你一份“面向 inspection 的总结壳”。

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_elf` 负责：

1. 消费 `MachineContainerFile`
2. 生成 ELF32 little-endian relocatable object skeleton
3. 保留：
   - `.text`
   - `.strtab`
   - `.symtab`
   - `.rel.text`
   - `.shstrtab`
4. 提供 section / symbol / relocation / header / target-policy query
5. 提供 verifier / dump / byte-copy helper
6. 提供 parse / round-trip / canonicalize / reprofile 能力

### 当前它不是什么

这一层现在还不是：

1. full platform-ABI relocation semantics
2. executable/program-header image
3. COFF or Mach-O support

所以当前它更像：

`real but narrow ELF relocatable artifact`

不是：

`full production system linker/assembler frontend`

---

## 7. 输入契约：它更喜欢吃已经稳定的 internal container

这一点和前几层一样重要。

当前最自然的主线不是：

`随便一段 bytes -> 当成 ELF`

而是：

`machine_container -> machine_elf`

也就是说，这层 forward build 默认更喜欢吃：

- 内部 section / symbol / relocation 关系已经稳定
- internal serialized layout 已经稳定

然后再把它们投影到标准 ELF 语义。

所以它的 current forward input contract 是：

`consume stable internal container, then project to standard ELF`

当然这层和前面不同的是：它也已经支持反向 parse/import。

---

## 8. Forward build 总体策略：container 信息被投影成 ELF section/symbol/relocation 布局

如果把 forward build 的精神压成一段 lesson 级伪代码，大致就是：

```text
read container artifact
choose target profile

decide ELF header fields:
  class / data / type / machine / flags

build section list:
  NULL
  .text
  .strtab
  .symtab
  .rel.text
  .shstrtab

build symbol list:
  null symbol
  local block symbols
  global function symbols
  external symbols

build relocation entries:
  map internal relocation kinds to ELF relocation types
  point relocations at ELF symbol indices

serialize bytes:
  ELF header
  section payloads
  section headers

verify by rereading bytes
```

所以这一层最重要的点是：

- 它不是重新解释业务语义
- 它是在 container artifact 的基础上，投影出标准 ELF 布局

---

## 9. 一个最小 before/after 例子：container -> ELF

假设 `machine_container` 已经有一份内部容器：

```text
header/table/string/payload
sections: .text
symbols: main, F0.L0, F0.L1, F0.L2
relocations: .text + 3 -> F0.L2
```

到了 `machine_elf`，当前层会把这些信息投影成真正 ELF 视角的结构，比如：

```text
ELF header
section headers:
  NULL
  .text
  .strtab
  .symtab
  .rel.text
  .shstrtab
symbols:
  null symbol
  local block symbols
  global function symbol main
relocations:
  .rel.text entry -> symbol F0.L2
```

所以这层做的不是“再自定义一种容器”，而是：

- 把内部 section/symbol/relocation 信息翻译进 ELF 的 section/symbol/relocation 布局

因此这层新增的是：

`标准对象格式语义`

---

## 10. target profile：为什么它在这一层这么重要

当前至少有三个 profile：

- `generic-elf32`
- `riscv32-preview`
- `i386-preview`

它们会直接控制：

- `e_machine`
- `e_flags`
- relocation opcode mapping

举个最小 lesson 级对比：

同一个 internal relocation kind：

```text
call-target
```

在不同 profile 下，最终可能会被映射到不同 ELF relocation type。

所以 profile 在这一层不是“额外元数据”，而是：

`标准对象格式投影策略`

这也是为什么当前 lesson 不能把 ELF 层讲成“只是 generic ELF bytes”。

更准确的口径是：

`profile-aware ELF artifact`

---

## 11. verifier：这层为什么已经很“真”

这一层 verifier 最值得讲的点是：

- 它不只看内存里的结构体
- 它还会重新从 owned byte image 里把 header / section header / symbol / relocation 读回来交叉检查

也就是说，当前 verifier 锁的不是：

- “这些数组看起来像 ELF”

而是：

- “这份 structured ELF view 和它自己的 byte image 必须互相一致”

这就是为什么这层在 plan 里被称为：

- more real than metadata-only checking

这也是它和前面大多数内部层的一个重要区别。

---

## 12. parse / round-trip / normalize：为什么这是这一层真正的重点能力

前面的层大多是：

- build forward

而到了 ELF 层，情况就不一样了。

当前它已经有：

- parse from bytes
- rebuild into `MachineElfFile`
- refresh back into canonical ELF bytes
- normalize one accepted input ELF into canonical output

这说明这一层的重点已经不只是：

- “我们能生成 ELF”

还包括：

- “我们能理解、校验、重建、规范化 ELF”

lesson 口径上，这一层一定要强调：

`parse / round-trip / normalize 是 ELF 层成为真实外部格式边界的关键`

### 一个最小 round-trip 例子

概念上当前已经支持：

```text
MachineContainerFile
  -> MachineElfFile
  -> bytes
  -> parse back into MachineElfFile
  -> verify
  -> normalize back into canonical bytes
```

这条链成立，说明 ELF 层已经不是单向导出器，而是：

`bidirectional format boundary`

---

## 13. query / report：为什么这层已经是 ELF-facing inspection API

当前 query/report 已经覆盖：

- by-name 找 section
- 取 `.text/.strtab/.symtab/.rel.text/.shstrtab`
- by-name 找 symbol
- 取 relocation
- 取 first global symbol index
- 取 header summary
- 取 target policy summary
- build report from file / bytes / container / machine-ir

所以后续 consumer 已经可以直接问：

- 这份 ELF 的 `.symtab` 在哪？
- 第一项 global symbol 从哪个 index 开始？
- 当前 profile 对 `call-target` 用的 relocation opcode 是多少？
- 第 0 个 relocation 指向哪个 symbol？

这说明 ELF 层不只是 bytes output，而是已经具备：

`ELF-facing structured inspection surface`

再说得更直白一点：

- 在 `machine_container` 层，你更像是在问“payload 和各种内部表摆在文件里的哪里”
- 到了 `machine_elf` 层，你已经是在问“这些内容在 ELF 语义里分别是哪一个 section / symbol / relocation entry”

所以 query 的关注点也发生了切换：

`从内部布局导航，切到标准 ELF 语义导航`

---

## 14. dump：为什么这一层第一次真正让“外部标准格式语义”整体出现

这一层 dump 的意义很大，因为它第一次把：

- ELF header
- section headers
- symbol table
- relocation table

真正并排打印成一个标准对象格式快照。

在 container 层时，我们看到的是：

- header/table/string/payload 的内部布局

而在 ELF 层时，我们看到的是：

- `.text/.symtab/.rel.text` 等标准 ELF 语义

所以 ELF dump 的意义不是“再换一种文本”，而是：

`让标准对象格式语义第一次作为一个整体出现`

---

## 15. 这一层和前一层 `machine_container` 的边界

- `machine_container`
  - 内部稳定容器格式
- `machine_elf`
  - 真实 ELF object format

这个边界很重要，因为它把：

- 仓库内部 artifact

和

- 外部可识别标准格式

明确分开了。

所以这一层解决的是：

- “标准 ELF 文件长什么样”

而不是：

- “内部容器如何排布”

---

## 16. 测试现在在锁什么

当前 `tests/machine/object/machine_elf/machine_elf_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_container -> machine_elf`
2. `machine_ir report -> ... -> machine_elf` upstream bridge
3. `.text / .strtab / .symtab / .rel.text / .shstrtab` 的存在和布局
4. header summary / target profile summary
5. profile-specific `e_machine` / relocation opcode policy
6. parse / round-trip / normalize
7. malformed-header / malformed-link / malformed-relocation rejection

所以这组测试锁的不是“能不能打印 ELF header”，而是：

`standard-format projection + profile policy + round-trip/import contract`

一起锁。

---

### 16.1 `test_machine_elf_builds_from_machine_ir_report`

这个 case 很适合你拿来理解：

`machine_ir report -> ... -> machine_container -> machine_elf`

在最普通的 `generic-elf32` 下，最终到底生成了什么。

测试里最值得记的不是“能生成 ELF”，而是下面这些精确事实：

1. section 数是 `6`
2. symbol 数是 `5`
3. relocation 数是 `2`
4. 总 ELF 字节数是 `468`
5. `shoff` 也就是 section header table 起点是 `228`
6. `.text` 在 `file@52`，大小 `9`
7. `.strtab` 在 `file@61`，大小 `24`
8. `.symtab` 在 `file@88`，大小 `80`
9. `.rel.text` 在 `file@168`，大小 `16`
10. `.shstrtab` 在 `file@184`，大小 `43`
11. 第一项 global symbol index 是 `4`
12. 前四个 magic 字节是 `0x7F 'E' 'L' 'F'`

这些数字连起来，其实就在说明一件事：

`这层已经不只是“有一个看起来像 ELF 的结构体”，而是把 ELF header、各 section 内容、section header table 位置都精确落成了真实 ELF byte image。`

这个测试里的 dump 也非常适合 lesson 直接引用理解：

- `machine_elf profile=generic-elf32 bytes=468 sections=6 symbols=5 relocations=2 shoff=228`
- `esec.1 .text type=1 file@52 bytes=9`
- `esec.3 .symtab type=2 file@88 bytes=80 link=2 info=4`
- `esec.4 .rel.text type=9 file@168 bytes=16 link=3 info=1`

再往细一点看，它还锁住了 ELF 里一个很关键的排序事实：

- `F0.L0/F0.L1/F0.L2` 这些 block label symbol 是 local
- `main` 是 global
- local 在前，global 在后

也就是说，这个 case 不只是在证明“section 在”，也在证明：

`ELF symbol ordering 和 relocation 指向的 symbol index 已经开始像真实对象文件那样工作。`

### 16.2 `test_machine_elf_builds_from_machine_container_with_external_call`

这个 case 的重点不是内部控制流，而是：

`external symbol relocation 进入 ELF 之后会变成什么样`

测试搭的是一条很小的线：

```text
call sink(5)
ret 0
```

然后一路走：

`emit -> encode -> bytes -> object -> reloc -> container -> elf`

它锁住的关键事实包括：

1. build 时显式选了 `MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW`
2. 最终 `header.machine == EM_RISCV`
3. `.rel.text` 大小是 `8`
4. 第一项 global symbol index 是 `2`
5. `sink` 能在 ELF symbol table 里被找到
6. `sink` 是 global，但 `is_defined == false`
7. relocation 指向的 symbol index 是 `3`
8. relocation type 是 `R_RISCV_CALL`

这几条合起来说明的不是“外部调用没丢”，而是：

`外部未定义符号已经能以真实 ELF 的 undefined global symbol + relocation entry 的形式出现。`

也就是说，在 `machine_reloc` 层它还是“target_name=sink 的 relocation artifact”，到了 `machine_elf` 层它已经更接近：

- `.symtab` 里有一个 undefined global symbol `sink`
- `.rel.text` 里有一条引用它的 relocation entry

这正是标准对象格式边界最重要的味道之一。

### 16.3 `test_machine_elf_builds_riscv32_preview_profile` / `test_machine_elf_builds_i386_preview_profile`

这两个 case 很适合一起看，因为它们正好在教你：

`target profile 不是标签，而是真的会改 ELF header 和 relocation policy。`

`riscv32-preview` 这边，测试锁住了：

1. `header.machine == EM_RISCV`
2. `.text` 大小从 generic 的 `9` 变成了 `28`
3. `.text` 的 preview bytes 和 generic bytes 必须不同
4. 第一条控制流 relocation 变成 `R_RISCV_BRANCH`
5. 第二条控制流 relocation 变成 `R_RISCV_JAL`

这说明 `riscv32-preview` 已经不只是“header 写个 RISC-V 名字”，而是：

- `.text` 真变了
- relocation opcode 也真变了

`i386-preview` 这边，测试锁住了：

1. `header.machine == EM_386`
2. primary control relocation 变成 `R_386_PC32`
3. secondary control relocation 变成 `R_386_32PLT`

所以 lesson 里最值得强调的一句话是：

`profile` 决定的不是注释，而是“同一份上游 artifact 被投影成哪一种 ELF 机器语义”。

### 16.4 parse / normalize 代表 case

`machine_elf` 这一层和前面几层最不一样的地方，是它已经开始接受“外面来的 ELF bytes 不一定正好长成我们 canonical 样子”。

当前测试里，至少有几类非常有代表性的 parse/normalize case：

1. `test_machine_elf_parse_accepts_reordered_null_section_header`
   - 说明 section header 顺序不完全 canonical 也能被接受，再 normalize 回标准顺序
2. `test_machine_elf_parse_accepts_missing_rel_text_section`
   - 说明输入缺一个 canonical `.rel.text`，也能在 normalize 时补回去
3. `test_machine_elf_parse_synthesizes_missing_null_symbol`
   - 说明输入缺失 null symbol 时，可以自动补齐
4. `test_machine_elf_normalize_drops_unused_undefined_global_symbols`
   - 说明 normalize 不只是重排，还会做“删掉无用符号”的 canonical cleanup
5. `test_machine_elf_normalize_bytes_with_profile_retargets_header_and_relocs`
   - 说明同一份 generic ELF bytes 可以直接 normalize 成 `riscv32-preview` 或 `i386-preview`

这几组测试合起来在证明：

`machine_elf` 已经不是单向导出器，而是一个能导入、修正、规范化、重投影的格式边界。`

---

## 17. 这一层和最终 `RISC-V` 方向的关系

前面用户已经明确提醒过一件事：

`这个仓库最终目标是 RISC-V`

所以读 `machine_elf` 时，最好不要把三个 profile 理解成“项目目标还没定”。

更准确地说，当前三条线的课堂口径应该是：

- `generic-elf32`
  - 保守、过渡、默认安全基线
- `riscv32-preview`
  - 和最终目标方向对齐的主预览线
- `i386-preview`
  - staging / compatibility 用的对照线，不代表目标方向改变

这也是为什么当前 `riscv32-preview` 的测试特别关键。

它锁住的不是一句抽象的“支持 RISC-V”，而是三件很实的事：

1. `e_machine` 真的切到了 `EM_RISCV`
2. `.text` bytes 真的变成了 preview RISC-V 风格
3. relocation type 真的切到了 `R_RISCV_*`

所以 lesson 里最值得记住的一句话是：

`machine_elf` 是当前后端里第一层把“RISC-V 是最终目标”这件事直接投影进标准对象格式语义里的地方。`

当然，这一层也还没有走到“完整 RISC-V ABI 已完成”。

它现在做到的是：

- profile-aware ELF header
- profile-aware relocation policy
- profile-aware preview `.text` bytes

而还没有做到的是：

- 完整 ABI 级别的所有 relocation family
- 完整 toolchain 兼容承诺
- 可执行文件 / program header 那一层

所以你可以把当前阶段理解成：

`RISC-V 方向已经进入真实 ELF 语义，但还停在 preview / checkpoint-ready 的对象文件层。`

---

## 18. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_container` 负责内部最终镜像，`machine_elf` 负责标准对象格式投影
2. 这一层最大的新增信息是 ELF header/section/symbol/relocation 的标准语义
3. target profile 在这一层是第一等设计点，不是附加配置
4. parse / round-trip / normalize 是这一层成为真实外部格式边界的关键能力
5. 这层已经不是单向导出器，而是可验证、可查询、可重建的 ELF artifact

---

## 19. 一句话总结

`machine_elf` 把前面内部流水线积累的 section/symbol/relocation/container 信息投影成真实 ELF relocatable object；它的关键不只是“生成 ELF bytes”，而是让 target profile、标准 ELF header/section/symbol/relocation 语义，以及 parse/round-trip/normalize 这些外部格式能力第一次真正作为后端的最终标准格式边界成形。
