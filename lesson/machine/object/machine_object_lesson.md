# Machine Object Lesson（compiler_lab）

> 目标：解释 `machine_object` 为什么要从 `machine_bytes` 独立出来，它相对 `machine_bytes` 到底新增了什么，`src/machine/object/machine_object/` 里真实在做什么，以及为什么这一层是“对象文件语义开始成形”的地方。

## 一句话定位

`machine_object` 是 byte-bearing raw material 第一次被正式组织成 object-facing section/symbol/fixup artifact 的层。

## 常见误解

- 误解一：有了 bytes 就已经差不多等于 object file 了。
  - 这里真正新增的是 section、symbol、fixup 这些对象文件语义骨架。
- 误解二：object 之后就已经等于 relocation fully landed。
  - relocation 语义还要到后面的 `machine_reloc` 再正式收束。

## 前置阅读

最推荐先读：

1. `lesson/machine/object/machine_bytes_lesson.md`
2. `lesson/machine/lowering/machine_encode_lesson.md`

因为 object 的前提就是：

- 真实 bytes 已经出现
- reference/fixup/symbol/section 雏形已经从 bytes 层开始长出来

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/object/machine_reloc_lesson.md`
2. `lesson/machine/object/machine_container_lesson.md`

因为 object 之后最自然的问题就是：

- fixup 怎么继续升格成正式 relocation 语义
- 整份对象 artifact 怎么继续被整体序列化

---

## 最近同步

最近这层最值得更新的，不是“又多了些 helper”，而是：

`machine_object` 现在已经更像 downstream consumer 可以直接拿来问问题的 artifact 层。`

具体可以记成四点：

1. `target-policy summary`
   - bytes 侧 preview honesty facts 现在会一路带到 object
2. `fixup-family summary`
   - 可以直接区分 `call / primary-control / secondary-control / data-side`
3. structured report artifact
   - 不再只有 raw object file
4. global-object / data-fixup slice
   - `.sbss` / `.sdata` / `global-object` / data-side fixup 也已经在这层保留
5. explicit profile-name dump
   - dump/report-dump 现在会直接打印
     - `generic`
     - `riscv32-preview`
     - `i386-preview`
     这种 profile 名，而不是只露出一个数字枚举值

所以现在这层除了“section/symbol/fixup 正式容器化”，还要记成：

- “consumer-facing summary/query surface 也开始成形”

---

## 导学

`machine_bytes` 已经有：

- bytes
- byte spans
- reference summary
- fixup summary
- symbol summary
- section summary

但它还不是对象文件意义上的 artifact。

对象文件层真正关心的是：

- section
- symbol
- fixup

以及它们之间的**归属关系和所有权**。

所以 `machine_object` 的作用不是再多做一轮 bytes，而是：

`把 byte-bearing raw material 容器化成 object-facing artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_bytes` 之后还要单独落一层 `machine_object`
2. 再看 `include/machine/object.h`，建立 section/symbol/fixup 容器模型
3. 再看 `src/machine/object/machine_object/machine_object.c` 的实现主线，理解 object file 是怎么从 bytes report/program 建出来的
4. 最后带着最小例子和伪代码去看：
   - `.text` section 是怎么形成的
   - symbol / fixup 为什么在这一层变成真正的 artifact 本体

学完你应该能：

1. 解释 `machine_object` 和 `machine_bytes` / `machine_reloc` 的边界
2. 说明这一层真正新增的是“对象文件语义容器”，而不是“更多字节”
3. 能看懂一个 `machine_bytes -> machine_object` 的 before/after 例子
4. 能说明为什么 section / symbol / fixup 在这一层才算真正“落地”

---

## 1. 为什么需要 `machine_object`

`machine_bytes` 已经回答了：

- 每个 block 有哪些字节
- 哪个 byte span 对应哪个 target
- 哪些 reference 将来会变成 fixup
- 哪些名字像 function/block/external symbol

但 `machine_bytes` 里的这些 still 更像：

- summary
- raw material
- pre-object facts

而不是：

- 真正拥有 section 数组的 object artifact
- 真正拥有 symbol 数组的 object artifact
- 真正拥有 fixup 数组的 object artifact

所以 `machine_object` 引入的真正边界是：

`byte-bearing raw material -> object-facing container artifact`

这一层最核心的新增信息，不是 bytes 本身，而是：

`ownership and grouping`

也就是：

- 这些 bytes 属于哪个 section
- 这些 symbols 挂在哪个 section 上
- 这些 fixups 挂在哪个 section 上

---

## 2. 为什么不能把这层塞回 `machine_bytes`

`machine_bytes` 负责的是：

- raw bytes
- byte offsets
- reference / fixup / symbol / section summary

但它不应该再继续负责：

- 真正持有 section 容器
- 真正持有 symbol 容器
- 真正持有 fixup 容器
- section-local symbol/fixup 查询

不然 `machine_bytes` 会变成：

`既是 byte materializer，又是 object artifact owner`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_bytes`
  - 回答“字节和 object-facing raw facts 长什么样”
- `machine_object`
  - 回答“这些 raw facts 作为 object artifact 怎么被正式容器化”
- `machine_reloc`
  - 回答“object 里的 fixup 如何再提升成 relocation artifact”

一句话压缩就是：

- `bytes`：有字节，也有 object-facing 雏形
- `object`：把雏形装进正式容器
- `reloc`：把 fixup 再提升成 relocation record/table

最近这条 object line 还可以再压成一句更贴近当前实现的话：

- `machine_object` 现在已经不只是“把东西装起来”
- 它还会把当前 preview profile/policy 以更适合 consumer 直接阅读的形式带出来

---

## 3. 文件定位

- 接口：`include/machine/object.h`
- 实现：`src/machine/object/machine_object/`
- 测试：`tests/machine/object/machine_object/`
- 规划文档：`docs/backend/MACHINE_OBJECT_PLAN.md`

这一层目前实现几乎集中在：

- `src/machine/object/machine_object/machine_object.c`

所以 lesson 口径上要注意：

- 文件数少
- 但内容已经不薄

它至少已经包含：

- object file lifecycle
- section/symbol/fixup query
- bytes report/program -> object lowering
- verifier
- dump

如果你要按最近这轮未提交改动去看源码，最值得直接盯一眼的两个变化点是：

- `machine_object_target_profile_name(...)`
  - 说明 dump/report 已经开始把 profile 当成“给人和 consumer 看懂的名字”
- `machine_object_dump_report(...)`
  - 说明 report 层不只是缓存 summary，也在承担更稳定的 consumer-facing文本 contract

---

## 4. `machine_object.c` 应该按哪几组职责来读

最容易读懂的方式，不是从头顺着看，而是按四组职责去记。

### 4.1 object file lifecycle

这一组主要在回答：

- `MachineObjectFile` 怎么 init/free
- section / symbol / fixup 的 owned memory 怎么释放

典型入口包括：

- `machine_object_file_init(...)`
- `machine_object_file_free(...)`

这说明这一层和前面的 summary 层最大的不同之一就是：

`它真的拥有这些容器`

### 4.2 section/symbol/fixup query

这一组主要在回答：

- 怎么按 section name 找 section
- 怎么找 symbol
- 怎么找 fixup
- 怎么取某个 section 下挂着的 symbols/fixups
- 怎么把 section bytes 拷出来

典型入口包括：

- `machine_object_file_find_section_by_name(...)`
- `machine_object_file_get_section_symbols(...)`
- `machine_object_file_get_section_fixups(...)`
- `machine_object_file_find_symbol_by_name(...)`
- `machine_object_file_get_fixup(...)`

这说明 object 层不是只“能 dump”，而是已经有比较像对象文件 API 的导航能力。

### 4.3 bytes -> object lowering

这一组主要在回答：

- `MachineBytesReport` 怎么变成 `MachineObjectFile`
- `MachineBytesProgram` 怎么变成 `MachineObjectFile`
- `.text` section 怎么形成
- symbol / fixup 怎么从 bytes-side summary 复制成真正容器

典型入口包括：

- `machine_object_build_from_machine_bytes_report(...)`
- `machine_object_build_from_machine_bytes_program(...)`
- `machine_object_build_from_machine_ir_report(...)`

这里才是“这一层的核心工作”。

### 4.4 verifier / dump

这一组主要在回答：

- object artifact 自己是否结构合法
- 怎么把 sections / symbols / fixups 作为一个完整 object-facing snapshot 打出来

所以这一层不是“只存结构”，而是已经有完整可读 surface。

---

## 5. `include/machine/object.h`：当前数据模型

### 5.1 三类本体容器

这一层最核心的三类 owned object 是：

- `MachineObjectSection`
- `MachineObjectSymbol`
- `MachineObjectFixup`

这和 `machine_bytes` 的 summary 层有本质区别。

在 bytes 层，我们更多是在说：

- “有一个 symbol summary”
- “有一个 fixup summary”

而在 object 层，我们已经在说：

- “这个 object file 拥有一个 symbol 数组”
- “这个 object file 拥有一个 fixup 数组”

也就是说，这一层新增的是：

`first-class ownership`

### 5.2 section 真正变成容器本体

`MachineObjectSection` 当前不只是名字和 span，它已经 owned：

- `name`
- `bytes`
- `byte_count`
- `symbol_start_index`
- `symbol_count`
- `fixup_start_index`
- `fixup_count`

这说明 `.text` 在这层已经不只是 summary 项，而是：

`真正持有 payload 的 section container`

### 5.3 symbol 真正变成容器本体

`MachineObjectSymbol` 当前会持有：

- `kind`
- `name`
- `is_defined`
- `section_index`
- `byte_offset_in_section`
- `byte_count`
- `incoming_fixup_count`
- optional `function_index`
- optional `emit_index`

这说明从这一层开始，symbol 已经不只是名字列表，而是：

`真实带位置信息的 object symbol`

### 5.4 fixup 真正变成容器本体

`MachineObjectFixup` 当前会持有：

- `kind`
- `target_kind`
- `section_index`
- `source_label_name`
- `owner_byte_offset`
- `owner_byte_count`
- `patch_byte_offset`
- `patch_byte_count`
- `target_name`
- optional target function/emit/byte offset
- optional `target_symbol_index`

这说明 fixup 在这一层已经从 bytes-side summary 变成：

`真正挂在 object artifact 里的 patch record`

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_object` 负责：

1. 消费 `MachineBytesReport` 或 `MachineBytesProgram`
2. 建立 object section 数组
3. 建立 object symbol 数组
4. 建立 object fixup 数组
5. 把 `.text` bytes 作为 owned section payload 挂起来
6. 提供 verifier / dump / section-local query / byte copy

### 当前它不是什么

这一层还不是：

1. relocation table artifact
2. target-specific relocation encoding
3. final file serialization
4. ELF/COFF/Mach-O output

所以当前它更像：

`minimal object-facing artifact`

不是：

`final object format`

---

## 7. 输入契约：它更喜欢吃已经整理好的 bytes artifact

这一点和前几层一样重要。

当前最自然的主线不是：

`随便拿一堆 bytes 就开始发明 object 结构`

而是：

`machine_bytes report/program -> machine_object`

也就是说，这层默认更喜欢吃：

- bytes 已经稳定
- reference/fixup/symbol/section summary 已经齐
- `.text` span / symbol / fixup 关系已经显式化

它不会重新决定：

- reference 是谁
- fixup patch span 是哪
- symbol 名字来自哪里

所以它的 current input contract 是：

`consume stable byte-bearing raw material, then containerize it`

---

## 8. Lowering 总体策略：它主要是在把 bytes-side summary 变成 owned object containers

如果把 `machine_object_build_from_machine_bytes_report(...)` 的精神压成一段 lesson 级伪代码，大致就是：

```text
flatten whole-program bytes

allocate object sections
allocate object symbols
allocate object fixups

for each section summary:
  create owned section
  copy section name
  copy section bytes slice
  copy section span / counts / symbol/fixup slice metadata

for each symbol summary:
  create owned symbol
  copy name / kind / section attachment / offsets

for each fixup summary:
  create owned fixup
  copy kind / target kind / patch span / target name / target symbol index

compute total_byte_count
verify object file
```

所以这一层最重要的点是：

- 它不是重新分析 bytes
- 它是在 bytes-side facts 的基础上，建立 owned object containers

---

## 9. 一个最小 before/after 例子：bytes -> object

假设 `machine_bytes` 里我们只有：

```text
section-like bytes:
  F0.L0 -> [10 21 70 xx]
  F0.L1 -> [81]
  F0.L2 -> [82]

fixup:
  patch_byte_offset=3
  target=F0.L2
```

到了 `machine_object`，这些东西会被重新装进对象文件语义容器里：

```text
section .text:
  bytes = [10 21 70 xx 81 82]

symbols:
  main
  F0.L0
  F0.L1
  F0.L2

fixups:
  .text + 3 -> symbol F0.L2
```

所以这一层不是在新增“更多字节”，而是在新增：

- section owner
- symbol owner
- fixup owner

这就是它相对 `machine_bytes` 最大的变化。

---

## 10. 一个更细的例子：external call symbol 为什么在 object 层更像“真的符号”

假设 bytes 层有一个 external call target：

```text
call sink(5)
```

在 `machine_bytes` 层，你可能已经有：

- 一个 external symbol summary 叫 `sink`
- 一个 call-target fixup summary 指向 `sink`

但到了 `machine_object`，这件事会更接近：

```text
symbol:
  kind = external
  name = sink
  defined = false

fixup:
  kind = call-target
  target_symbol = sink
```

这里真正变化的不是“名字一样没变”，而是：

- 它们已经变成 object artifact 里的正式 entries

所以 object 层的重点是：

`summary fact -> owned artifact entry`

---

## 11. verifier：这层到底在锁什么

这一层的 verifier 重点不再只是 span 自洽，而是：

`object-facing container contract 是否成立`

至少包括这些事情：

1. section / symbol / fixup 表存在且范围合法
2. section bytes 是否和 section span 自洽
3. symbol 的 section attachment / byte offset 是否合理
4. fixup 的 target symbol / target kind 是否合理
5. total byte count 是否和 section payload 统计匹配

这说明 object verifier 锁的不是：

- “这是某个最终 object 格式吗”

而是：

- “这份 object artifact 自己是不是一份自洽的容器化中间结果”

这点很重要，因为下一层 `machine_reloc` 会直接建立在它的 fixup/symbol/section 关系之上。

---

## 12. query：为什么这一层已经很像 object API

`machine_object` 这一层的 query 特别像一个迷你对象文件 API。

当前至少已经支持：

- by-name 找 section
- 拷某个 section 的 bytes
- 取某个 section 下挂着的 symbols
- 取某个 section 下挂着的 fixups
- by-name 找 symbol
- 按 index 找 fixup

所以后续 consumer 已经可以直接问：

- `.text` 这段 payload 是什么？
- 这个 section 下有哪些 defined symbols？
- `sink` 这个 symbol 在 object 里是什么 kind？
- 第 0 个 fixup 指向谁？

这说明 object 层不只是 dump readable，而是已经具备：

`object-facing navigation surface`

### 12.1 这一层的 query 已经开始带“归属关系”了

这一点非常值得在 lesson 里点出来。

前面 `machine_bytes` 层虽然也能问很多问题，但更多是在问：

- 这个 byte span 是什么
- 这个 reference/fixup summary 长什么样

而到了 `machine_object`，query 明显开始转向“归属关系”：

- 某个 section 下面挂了哪些 symbols
- 某个 section 下面挂了哪些 fixups
- 某个 symbol 属于哪个 section
- 某个 fixup 的 target symbol 是谁

也就是说，这一层 query 的关键词已经不是：

- span / summary / raw facts

而是：

- `ownership`
- `attachment`
- `container membership`

这正是 object 层和 bytes 层最值得区分的一点。

---

## 13. dump：为什么这层第一次真正把三件事并排摆出来

这层 dump 的意义很大，因为它第一次把：

- sections
- symbols
- fixups

真正并排打印成一个 object-facing snapshot。

lesson 口径上，这一步很关键，因为在前面的 bytes 层，这几类东西更多还是：

- raw facts
- summary lists

而在 object 层，它们第一次被正式放进一个共同的 artifact 视图里。

所以 object dump 的意义不是“更好看”，而是：

`让 object semantics 第一次作为一个整体出现`

---

## 14. 这一层和后面 `machine_reloc` 的边界

这是最容易混的地方。

### `machine_object` 负责

- section container
- symbol container
- fixup container

### `machine_reloc` 负责

- relocation table
- relocation record
- section-local relocation grouping

也就是说：

- `machine_object` 仍然保留的是 fixup/object 语义
- `machine_reloc` 才把它们进一步升级成 relocation artifact

这句话最好记牢：

`object 里有 fixup，不等于已经有 relocation table`

---

## 15. 测试现在在锁什么

当前 `tests/machine/object/machine_object/machine_object_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_bytes report -> machine_object`
2. `machine_bytes program -> machine_object`
3. `machine_ir report -> ... -> machine_object` upstream bridge
4. `.text` section payload
5. defined function/block symbol containerization
6. external symbol containerization
7. fixup -> target symbol attachment
8. section-local symbol/fixup query

所以这组测试锁的不是“能不能打印 `.text`”，而是：

`object container ownership + query surface + bridge behavior`

一起锁。

### 15.1 defined object / `.text` / internal block symbol 代表 case

最值得先看的测试名是：

- `test_machine_object_builds_from_machine_ir_report`

这条测试几乎把 object 层的第一条主线全锁了：

- `MachineIrAllocateRewriteReport -> ... -> MachineObjectFile`
- 一个 `.text` section
- function symbol
- block symbols
- control fixups
- section-local symbol/fixup 查询
- section bytes copy

如果要先建立第一印象，这条测试就是最直接的入口，因为它把：

- section
- symbol
- fixup

第一次真的并排锁成一份 object artifact。

### 15.2 external symbol / call-target fixup 代表 case

下一条很值得顺着看的是：

- `test_machine_object_builds_from_machine_bytes_program_with_external_symbol`

它锁的是另一个非常关键的故事：

- 上游 bytes 层已经有 external symbol summary
- object 层要把它变成真正的 external symbol entry
- call-target fixup 要真正挂到这个 symbol 上

也就是说，这条测试锁的不是“名字还是叫 `sink`”，而是：

- `sink` 在 object file 里已经是一个正式 external symbol
- 调用 fixup 也已经正式指向它

这条测试特别适合解释前面 lesson 里说的：

`summary fact -> owned artifact entry`

不是空话，而是代码里真的这么落。

### 15.3 这两条测试其实正好对应 object 层的两大职责

你可以把当前测试矩阵非常粗暴但有效地分成两类：

1. `defined object`
   - 内部 `.text`
   - defined function/block symbol
   - control fixup
2. `external object`
   - external symbol
   - call-target fixup

这也说明当前 `machine_object` 虽然规模还小，但它已经不是“只有一条 happy path 的玩具容器层”。

---

## 16. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_bytes` 给你 raw material，`machine_object` 把它装成容器
2. 这一层最大的新增信息是 owned section / symbol / fixup arrays
3. `.text` 在这一层第一次真正变成 payload-owning section
4. external / function / block symbol 在这一层第一次真正变成 object symbol
5. 这层还不是 relocation/object-file serialization，但已经是非常明确的 object-facing artifact

---

## 17. 和最终 `RISC-V` 方向以及后续文件格式层的关系

根据：

- `docs/ir-conventions.md`
- `docs/NEXT_STEPS.md`

仓库最终目标方向仍然是：

- `RISC-V`

所以虽然当前 `machine_object` 还没有进入：

- target-specific relocation encoding
- ELF-specific record layout
- final object-file serialization

但它已经在做一件对未来 RISC-V 很关键的事：

- 把 `.text` bytes、target symbol、fixup attachment 这些 object-facing 语义固定下来

这一步越稳定，后面的：

- `machine_reloc`
- `machine_container`
- `machine_elf`

就越不需要再回头发明：

- 哪些 bytes 属于哪个 section
- 哪个 fixup 挂在哪个 symbol
- 内部 block label 和外部 call target 在 object 里分别怎么出现

所以它不是最终 RISC-V object format 本身，但它确实是：

`未来 RISC-V relocation / object-file 落地之前的第一层正式 object 容器边界`

---

## 18. 一句话总结

`machine_object` 把 `machine_bytes` 里的字节、symbol 雏形和 fixup 雏形正式组织成 section/symbol/fixup 容器；它的关键不只是“把 bytes 装起来”，而是让 object-facing ownership、attachment 和 query surface 第一次真正成形，因此它是后面 relocation 和 final serialization 的对象语义起点。
