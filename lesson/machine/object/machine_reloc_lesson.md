# Machine Reloc Lesson（compiler_lab）

> 目标：解释 `machine_reloc` 为什么还要单独存在，它相对 `machine_object` 到底新增了什么，`src/machine/object/machine_reloc/` 里真实在做什么，以及为什么这一层是“relocation 语义第一次正式落地”的地方。

## 一句话定位

`machine_reloc` 是 object artifact 里 fixup 真正被提升成 relocation 语义的层。

## 常见误解

- 误解一：fixup 和 relocation 只是两种叫法。
  - fixup 还更像待修补点，relocation 则已经是正式对象文件/链接语义。
- 误解二：有了 relocation 之后就已经直接是 ELF。
  - 后面还要先经过 container，再落到标准 ELF 边界。

## 前置阅读

最推荐先读：

1. `lesson/machine/object/machine_object_lesson.md`
2. `lesson/machine/object/machine_bytes_lesson.md`

因为 reloc 的前提就是：

- object 层已经把 bytes 组织成 section/symbol/fixup artifact
- 现在要把待修补点正式翻译成 relocation world

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/object/machine_container_lesson.md`
2. `lesson/machine/object/machine_elf_lesson.md`

因为 relocation 之后最自然的问题就是：

- 整份 artifact 怎么先做内部稳定序列化
- 然后再怎么投影成标准 ELF

---

## 最近同步

最近这层已经不只是“有 relocation rows”，还开始有：

1. `relocation-family summary`
   - 直接统计 `call / primary-control / secondary-control / data-addr / data-load / data-store`
2. structured report artifact
   - 不再只有 raw relocation file
3. preview addend honesty
   - verifier 会检查已知 internal target 的 addend 漂移
4. zero-patch fallthrough honesty
   - direct preview artifact 不再伪造 semantic fallthrough 的第二条假 relocation
5. data relocation family
   - 不再只有 call/control，global data 访问相关族也已经进来了
6. section-local relocation-summary surface
   - report 现在可以直接按 relocation section 取那一段 relocation summaries
7. explicit profile-name dump
   - dump/report-dump 里现在会打印
     - `generic`
     - `riscv32-preview`
     - `i386-preview`
     这类显式 profile 名，而不是只给一个数字枚举值

所以这层现在除了：

- `fixup -> relocation record`

还要再加一句：

- `relocation record -> 可直接被 ELF / consumer 查询的 relocation-family surface`

---

## 导学

`machine_object` 已经有：

- section
- symbol
- fixup

但 fixup 还只是“对象里的待修补点”。

后面如果要做：

- final serialization
- ELF `.rel.*`
- relocation table / relocation record 输出

我们需要把这些 fixup 正式提升成：

`relocation artifact`

这就是 `machine_reloc` 的职责。

这份讲义建议按下面顺序读：

1. 先理解为什么 object 层之后还要再单独落一层 relocation artifact
2. 再看 `include/machine/reloc.h`，建立 relocation section / relocation record / embedded object 的整体印象
3. 再看 `src/machine/object/machine_reloc/machine_reloc.c` 的实现主线，理解 object 怎么被抬成 relocation artifact
4. 最后带着最小例子和伪代码去看：
   - fixup 是怎么变成 relocation record 的
   - 为什么 section-local relocation grouping 在这一层才算正式成立

学完你应该能：

1. 解释 `machine_reloc` 和 `machine_object` / `machine_container` 的边界
2. 说明这一层真正新增的是“显式 relocation 容器”，而不只是更多 fixup 字段
3. 能看懂一个 `machine_object -> machine_reloc` 的 before/after 例子
4. 能说明为什么 `relocation table` 在这一层才算真正落地

---

## 1. 为什么需要 `machine_reloc`

`machine_object` 已经回答了：

- 哪些 bytes 属于哪个 section
- 哪些 symbol 挂在哪个 section 上
- 哪些 fixup 指向哪个 target

但它还没有回答：

- relocation records 作为独立 artifact 长什么样
- relocation 是怎么按 section 分组的
- later serializer 要怎么直接消费 relocation，而不是重新解释 fixup

也就是说，`machine_object` 的 fixup 仍然更像：

- object-facing patch metadata

还不是：

- relocation-facing table/record artifact

所以 `machine_reloc` 引入的真正边界是：

`object-facing fixup container -> explicit relocation artifact`

这一层最核心的新增信息，不是 bytes，也不是 symbol，而是：

`relocation grouping and relocation ownership`

---

## 2. 为什么不能把这层塞回 `machine_object`

`machine_object` 负责的是：

- section container
- symbol container
- fixup container

但它不应该再继续负责：

- relocation table container
- relocation record container
- relocation-section query
- object fixup 到 relocation record 的正式提升

不然 `machine_object` 会变成：

`既是 object artifact，又是 relocation artifact`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_object`
  - 回答“object 里的 section/symbol/fixup 怎么组织”
- `machine_reloc`
  - 回答“fixup 怎么被提升成 relocation table/record”
- `machine_container`
  - 回答“relocation artifact 怎么被序列化成最终容器字节”

一句话压缩就是：

- `object`：有 fixup
- `reloc`：有 relocation table/record
- `container`：把 relocation artifact 打包出去

这句话最好记牢：

`object 里有 fixup，不等于已经有 relocation table`

---

## 3. 文件定位

- 接口：`include/machine/reloc.h`
- 实现：`src/machine/object/machine_reloc/`
- 测试：`tests/machine/object/machine_reloc/`
- 规划文档：`docs/backend/MACHINE_RELOC_PLAN.md`

这一层目前实现几乎集中在：

- `src/machine/object/machine_reloc/machine_reloc.c`

所以 lesson 口径上要注意：

- 文件数少
- 但内容已经很清楚地分成：
  - embedded object clone
  - relocation-section build
  - relocation-record build
  - query
  - verifier
  - dump

---

## 4. `machine_reloc.c` 应该按哪几组职责来读

最容易读懂的方式，不是从头顺着看，而是按四组职责去记。

### 4.1 embedded object clone

这一组主要在回答：

- 输入 object file 怎么被完整复制进 relocation artifact

典型 helper 是：

- `machine_reloc_clone_object_file(...)`

这很重要，因为它说明 `machine_reloc` 不是扔掉 object 再重建，而是：

- 保留一份 embedded object artifact
- 再在它上面叠出 relocation 层

所以当前 `MachineRelocFile` 不是“只有 relocations”，而是：

- `object_file + relocation sections + relocation records`

### 4.2 relocation section build

这一组主要在回答：

- 当前 object 里的每个 section，怎么对应一个 relocation section
- relocation section 里该记录哪些 span / count / 起始偏移

这部分最值得抓的点是：

- `MachineRelocSection` 不是重新造 bytes
- 它是在 object section 之上，建立“这一段 section 对应多少 relocation”的 grouping

### 4.3 relocation record build

这一组主要在回答：

- object fixup 怎么变成 `MachineRelocation`
- 哪些 fixup kind 映射成哪类 relocation kind
- target symbol / target section / target byte offset 怎么被带进 relocation record

这一部分才是本层真正“fixup -> relocation”的核心。

### 4.4 query / verifier / dump

这一组主要在回答：

- 怎么按 section 名找 relocation section
- 怎么取某个 relocation record
- 怎么拿 section-local relocation slice
- relocation artifact 自己是否合法
- 怎么把 relocation sections + records 打成一个 relocation-facing snapshot

所以这层已经很明显不是“中间过程里的一个临时数组”，而是正式 artifact。

---

## 5. `include/machine/reloc.h`：当前数据模型

### 5.1 `MachineRelocFile`

当前最关键的数据结构是：

```text
MachineRelocFile =
  embedded MachineObjectFile
  + relocation sections
  + relocation records
```

这一点非常重要，因为它说明 relocation 层不是把 object 扔掉重来，而是：

- object artifact 继续保留
- relocation artifact 叠在它上面

### 5.2 relocation section

`MachineRelocSection` 当前主要携带：

- `object_section_index`
- `name`
- `start_byte_offset`
- `end_byte_offset`
- `byte_count`
- `relocation_start_index`
- `relocation_count`

所以这一层真正新增的是：

`section-local relocation grouping`

也就是说，这一层终于正式回答了：

- `.text` 这一节对应哪段 relocation records

### 5.3 relocation record

`MachineRelocation` 当前主要携带：

- `kind`
- `target_kind`
- `object_section_index`
- `source_label_name`
- `owner_byte_offset`
- `owner_byte_count`
- `patch_byte_offset`
- `patch_byte_count`
- `target_name`
- optional `target_symbol_index`
- optional `target_section_index`
- optional `target_byte_offset`
- `addend`

所以这层相对 `MachineObjectFixup` 最大的新增，不是“字段名不一样”，而是：

- 它现在已经被认定为 relocation record
- 它已经显式属于一个 relocation artifact

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_reloc` 负责：

1. 消费 `MachineObjectFile`
2. 保留嵌入的 object artifact
3. 给每个 object section 建 relocation grouping
4. 建立显式 relocation record 数组
5. 保留 patch span / target symbol / target byte offset / addend
6. 提供 verifier / dump / relocation query

### 当前它不是什么

这一层还不是：

1. target-specific relocation opcode encoding
2. final object-file serialization
3. ELF/COFF/Mach-O writer
4. linker / archive semantics

所以当前它更像：

`minimal relocation-facing artifact`

不是：

`final relocation file format`

---

## 7. 输入契约：它更喜欢吃已经 verifier-safe 的 object artifact

这一点和前几层一样重要。

当前最自然的主线不是：

`从 bytes 里临时推导 relocation`

而是：

`machine_object verify -> machine_reloc build`

也就是说，这层默认更喜欢吃：

- object sections 已经稳定
- symbols 已经稳定
- fixups 已经显式挂在 object artifact 上

它不会重新决定：

- section 叫什么
- symbol 属于谁
- fixup 是什么 kind

所以它的 current input contract 是：

`consume stable object artifact, then lift fixups into relocation artifact`

---

## 8. Lowering 总体策略：它先复制 object，再建立 relocation sections 和 records

如果把 `machine_reloc_build_from_machine_object_file(...)` 的精神压成一段 lesson 级伪代码，大致就是：

```text
clone object file into embedded object_file

for each object section:
  create relocation section
  keep object_section_index / name / byte span
  set relocation_start_index from object fixup slice
  set relocation_count from object fixup count

for each object fixup:
  create relocation record
  map fixup kind -> relocation kind
  copy patch span / owner span / target name
  preserve target symbol / section / byte offset when available
  set addend = 0

verify reloc file
```

所以这一层最重要的点是：

- 它不是重新分析 object bytes
- 它是在 object artifact 的基础上，把 fixup 正式抬升成 relocation record

---

## 9. fixup 和 relocation 的真正区别

这里最好不要只停在一句“relocation 更正式”。

可以这样区分：

### fixup

更偏：

- 这里以后要 patch
- patch 点现在挂在 object artifact 上

### relocation

更偏：

- 这是 relocation artifact 里的正式 record
- 它属于哪个 relocation section
- 它关联哪个 target symbol
- 它的 patch span / addend / target location 是什么

所以当前这层做的不是：

- “给 fixup 换个名字”

而是：

- `fixup -> relocation record inside relocation artifact`

---

## 10. 一个最小 before/after 例子：object -> reloc

假设 `machine_object` 里还只是 fixup 语义：

```text
section .text
fixup:
  source=.text + 3
  target_symbol=F0.L2
```

到了 `machine_reloc`，当前层会把它提升成 relocation record：

```text
relocation table for .text:
  reloc[0]:
    kind=control-primary
    patch=.text + 3
    target_symbol=F0.L2
    addend=0
```

这里最关键的变化不是“内容完全换了”，而是：

- 原来的 fixup 被归入 relocation section
- patch span / target symbol / addend 变成正式 relocation record 字段

所以这层做的是：

`fixup -> relocation`

而不是：

`bytes -> new bytes`

---

## 11. 一个更细的例子：external call 为什么在这一层会变成 symbol-target relocation

假设 object 层里有一个 external call fixup：

```text
fixup:
  kind=call-target
  target_name=sink
  target_symbol_index = sym.external("sink")
```

到了 relocation 层，它会更接近：

```text
reloc:
  kind=call-target
  target_kind=symbol
  target_name=sink
  target_symbol_index = sym.external("sink")
```

这件事的关键不是“信息完全新增了”，而是：

- target 已经不再只是 object fixup 里的一个名字
- 它已经变成 relocation record 的正式 target

所以当前 relocation 层一个很重要的 lesson 口径是：

`symbol-targeted patch semantics 在这一层才真正变成 relocation-facing contract`

---

## 12. verifier：这层到底在锁什么

这一层的 verifier 重点不再只是 object 容器自洽，而是：

`relocation artifact 是否真的自洽`

至少包括这些事情：

1. embedded object file 本身必须是合法的
2. relocation section 范围要合法
3. relocation record 数组范围要合法
4. section-local relocation slice 要和 section metadata 对得上
5. target symbol / target section / target byte offset 这些可选字段要自洽

这说明 relocation verifier 锁的不是：

- “这是最终 ELF relocation 吗”

而是：

- “这份 relocation artifact 自己是不是一个合法、可继续序列化的中间结果”

这点很重要，因为后面的 `machine_container` 会直接建立在它的 relocation section / record 结构上。

---

## 13. query：为什么这一层已经很像 relocation API

`machine_reloc` 这一层的 query 也很像一个迷你 relocation API。

当前至少已经支持：

- 取 embedded object file
- by-name 找 relocation section
- 取某个 section 下挂着的 relocations
- 按 index 找 relocation

所以后续 consumer 已经可以直接问：

- `.text` 这段有多少 relocation？
- 第 0 个 relocation 指向谁？
- 这个 relocation 的 patch span 是多少？
- 这是 block-target relocation 还是 symbol-target relocation？

这说明 relocation 层不只是 dump readable，而是已经具备：

`relocation-facing navigation surface`

再说得更直白一点：

- 在 `machine_object` 层，你更像是在问“fixup 还挂在哪个 object section 下面”
- 到了 `machine_reloc` 层，你已经是在问“这个 section 对应哪张 relocation table、这张表里第几个 relocation record 指向谁”

所以 query 的关注点也发生了切换：

`从 object fixup 所属关系，切到 relocation section / relocation record 的正式导航关系`

---

## 14. dump：为什么这一层第一次真正把 relocation sections 和 relocation records 并排摆出来

这一层 dump 的意义很大，因为它第一次把：

- relocation sections
- relocation records

真正并排打印成一个 relocation-facing snapshot。

在 object 层时，我们看到的还是：

- sections
- symbols
- fixups

到了 relocation 层，重点已经切成：

- 这些 fixups 如何被 section-local relocation tables 接住

所以 relocation dump 的意义不是“再换一种文本”，而是：

`让 relocation semantics 第一次作为一个整体出现`

---

## 15. 这一层和后面 `machine_container` 的边界

这是最容易混的地方。

### `machine_reloc` 负责

- relocation section
- relocation record
- target symbol / section / byte offset metadata

### `machine_container` 负责

- 把 relocation artifact 序列化成最终容器字节

也就是说：

- `machine_reloc` 仍然是结构化 artifact
- `machine_container` 才开始做最终字节容器打包

所以这层解决的是：

- “relocation 作为数据结构长什么样”

不是：

- “relocation 在文件里最终怎么编码”

如果再把三层压成一句顺口的话：

- `machine_object`：把 fixup 挂进 object
- `machine_reloc`：把 fixup 提升成 relocation tables / records
- `machine_container`：把 relocation artifact 真正打包成可落盘容器

这也是为什么 lesson 里要反复强调：

`reloc 解决的是“结构化 relocation artifact”，container 才解决“最终文件字节布局”`

---

## 16. 测试现在在锁什么

当前 `tests/machine/object/machine_reloc/machine_reloc_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_object -> machine_reloc`
2. `machine_ir report -> ... -> machine_reloc` upstream bridge
3. relocation section grouping
4. relocation record construction
5. external symbol target relocation
6. target symbol / target section / target byte offset metadata
7. relocation query / dump surface

所以这组测试锁的不是“能不能打印 `reloc.0`”，而是：

`relocation ownership + relocation query + bridge behavior`

一起锁。

### 16.1 `test_machine_reloc_builds_from_machine_ir_report`

这个 case 很适合你拿来理解：

`machine_ir report -> machine_object -> machine_reloc`

整条桥到底在保什么。

这个测试里，`main` 只有一个 `.text` section，但会产生两个控制流 relocation：

- `ctrl-primary`
- `ctrl-secondary`

测试锁住的关键事实包括：

1. 最终 `MachineRelocFile` 里只有一个 relocation section，也就是 `.text`
2. 这个 `.text` relocation section 下面挂了两个 relocation records
3. 第一个 relocation 指向 `F0.L2`
4. 第二个 relocation 指向 `F0.L1`
5. 两个 relocation 都带着：
   - `target_symbol_index`
   - `target_section_index`
   - `target_byte_offset`

也就是说，这个 case 不是只在证明“桥能跑通”，而是在证明：

`内部 block target relocation 不但存在，而且已经能被正式定位到 symbol / section / byte offset`

如果把这个 case 的 lesson 级伪代码压一下，可以把它理解成：

```text
machine_ir branch
  -> machine_select/layout/emit/encode/bytes/object
  -> object .text fixups
  -> reloc .text relocation table
  -> each control target becomes explicit relocation record
```

这个测试里的 dump 也很值得读，因为它把这一层最重要的“成品样子”锁住了：

- `machine_reloc total_bytes=9 object_sections=1 symbols=4 relocation_sections=1 relocations=2`
- `relsec.0 .text span=0..9 bytes=9 relocations=2`
- `reloc.0 ctrl-primary ...`
- `reloc.1 ctrl-secondary ...`

lesson 里你可以把它理解成：

`一条分支在 object 层还是 fixup，到 reloc 层就已经变成两条正式 control relocation records 了`

### 16.2 `test_machine_reloc_builds_from_machine_object_with_external_call`

这个 case 重点不是内部跳转，而是：

`external symbol relocation`

测试先手工搭了一个很小的 `emit -> encode -> bytes -> object` 主线，其中唯一重要的 op 是：

```text
call sink(5)
```

然后它锁住：

1. `machine_reloc` 可以直接从 `MachineObjectFile` 建出来
2. 第 0 个 relocation 的 kind 是 `MACHINE_BYTES_FIXUP_CALL_TARGET`
3. 这个 relocation 有 `target_symbol_index`
4. 但它没有：
   - `target_section_index`
   - `target_byte_offset`
5. `target_name` 仍然是 `"sink"`

这个区别非常关键。

内部 label target 的时候，当前 artifact 往往能同时知道：

- 它是哪个 symbol
- 它落在哪个 section
- 它在 section 里的 byte offset

但 external symbol target 的时候，当前层只知道：

- 它要去找哪个外部符号

却还不知道：

- 它在本 object 内部到底落在哪个 section / byte offset

所以这个测试实际上在教你分清两种 relocation：

1. internal target relocation
2. external symbol relocation

前者常常能带 section/offset，后者当前只带 symbol attachment 更自然。

---

## 17. 这一层和最终 `RISC-V` 方向、后续文件格式层的关系

前面用户已经专门提醒过一个总方向：

`这个仓库最后想去的是 RISC-V`

所以读 `machine_reloc` 时，最好不要把它理解成“永远泛化的一层 metadata”。

更准确地说，它现在是在做两件过渡性的关键工作：

1. 把上游通用 fixup 语义整理成显式 relocation artifact
2. 给后面的 `machine_container` / `machine_elf` 提供一个更像真实目标机工具链会消费的中间层

但它目前还没有走到：

- 最终 `RISC-V` relocation opcode 编码
- 最终 ELF relocation type number
- 最终 object-file relocation entry layout

所以你可以把它理解成：

`target-facing relocation semantics 开始变真，但 final file-format encoding 还没到这里`

这也是为什么当前 public API 里会有：

- `machine_reloc_build_from_machine_ir_report(...)`
- `machine_reloc_build_from_machine_ir_report_with_profile(...)`

它说明这层已经开始允许“目标 profile”往下传，但它仍然保持了一个很克制的边界：

- 现在先把 relocation artifact 建对
- 以后再由 `machine_container` / `machine_elf` 去决定真正的目标文件表达

放在整个 backend lesson 主线上，这层最值得记的一句话是：

`machine_reloc` 让 relocation 先作为 artifact 成形；而真正把它写成 `RISC-V` 兼容 object/ELF relocation entry，则是后面几层的事。

---

## 18. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_object` 里有 fixup，`machine_reloc` 里有 relocation record
2. 这一层最大的新增信息是 section-local relocation grouping
3. relocation record 在这一层第一次真正变成 artifact 本体
4. external symbol / block target / byte offset target 在这一层开始进入 relocation contract
5. 这层还不是最终文件格式，但已经是非常明确的 relocation-facing artifact

---

## 19. 一句话总结

`machine_reloc` 的职责是把 `machine_object` 里的 fixup 正式提升成 relocation sections 和 relocation records；它的关键不只是“把 fixup 换个名字”，而是让 relocation grouping、target attachment、patch span 和查询接口第一次真正作为 relocation artifact 成形，因此它是后面 container 和 ELF 输出的 relocation 语义起点。
