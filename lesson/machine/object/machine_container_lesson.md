# Machine Container Lesson（compiler_lab）

> 目标：解释 `machine_container` 为什么不是 ELF，而是仓库内部自定义的最终容器层；它相对 `machine_reloc` 到底新增了什么，`src/machine/object/machine_container/` 里真实在做什么，以及为什么这一层是“结构化 artifact 第一次被整体序列化成稳定文件字节”的地方。

## 一句话定位

`machine_container` 是 relocation/object artifact 第一次被整体打包成仓库内部稳定文件字节的层。

## 常见误解

- 误解一：container 就是自定义版 ELF。
  - 它更准确地说是内部最终容器，不是对外标准对象格式。
- 误解二：有了 container 就没必要再单独做 ELF。
  - 后面 `machine_elf` 仍然要负责标准 header/section/symbol/relocation 语义。

## 前置阅读

最推荐先读：

1. `lesson/machine/object/machine_reloc_lesson.md`
2. `lesson/machine/object/machine_object_lesson.md`

因为 container 的前提就是：

- object / relocation artifact 已经稳定
- 现在要解决的是“如何把整份结构化 artifact 一次性序列化”

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/object/machine_elf_lesson.md`
2. `lesson/machine/runtime/machine_image_lesson.md`

因为 container 之后最自然的问题就是：

- 如何进入标准 ELF 边界
- 然后从加载视角看这份 ELF 已经知道哪些地址事实

---

## 最近同步

最近这层最值得同步的，不是“container 格式换了”，而是它现在更明确地处在：

- object / reloc
- 到 ELF / image

之间的稳定过渡位置。

结合最近提交，当前这层最好再多记两点：

1. **container 已经不只是 `.text`-only 内部壳**
   随着上游 object/reloc 开始承接 `.sbss` / `.sdata` / `global-object` / data-side relocation，这层也更应该被理解成“会承接这些结构化 section family 的内部最终容器”。

2. **它现在更像 provenance 保持链的一环**
   后面的 ELF / image lesson 已经开始讲 artifact summary / origin profile / source provenance，所以 container 现在最好别再理解成“随手序列化一下”，而是：
   - 直达标准 ELF 之前的内部最终收口层

一句话压缩就是：

- 以前：`reloc -> container -> elf`，container 更像内部文件格式
- 现在：`reloc -> container -> elf`，container 也开始承担“保住结构化 artifact 不丢层”的职责

---

## 导学

到了 `machine_reloc`，我们已经有：

- object artifact
- relocation sections
- relocation records
- symbol / section / patch metadata

这时已经可以做一种稳定的最终序列化了，但还没必要立刻跳去：

- ELF
- COFF
- Mach-O

所以项目先引入一层中间目标：

`machine_container`

它的作用不是“再发明一套对象语义”，而是：

`把 relocation artifact 序列化成仓库内部稳定的最终容器字节`

这份讲义建议按下面顺序读：

1. 先理解为什么 relocation artifact 之后还要单独落一层 container
2. 再看 `include/machine/container.h`，建立 header / tables / string table / payload 的整体印象
3. 再看 `src/machine/object/machine_container/machine_container.c` 的实现主线，理解 relocation artifact 怎么变成一个稳定 byte image
4. 最后带着最小例子和伪代码去看：
   - 哪些表会进入 container
   - 为什么 layout summary 在这一层开始变成关键 surface

学完你应该能：

1. 解释 `machine_container` 和 `machine_reloc` / `machine_elf` 的边界
2. 说明这一层真正新增的是“最终序列化布局”，而不是新的语义层
3. 能看懂一个 `machine_reloc -> machine_container` 的 before/after 例子
4. 能说明为什么 header / table / string / payload 关系在这一层才算真正落地

---

## 1. 为什么需要 `machine_container`

`machine_reloc` 已经回答了：

- 哪些 relocation 属于哪个 section
- relocation record 长什么样
- target symbol / target section / patch span 是什么

但它还没有回答：

- 这一整份 relocation-facing artifact 最终怎么变成一段完整字节流？
- header、section table、symbol table、relocation table、string table、payload 分别摆在哪里？
- later consumer 要怎么拿到一个稳定的、可复制的最终容器镜像？

所以 `machine_container` 引入的真正边界是：

`structured relocation artifact -> stable serialized container byte image`

这一层最核心的新增信息，不是 relocation 语义，而是：

`file layout ownership`

也就是：

- header 在哪里
- tables 在哪里
- string table 在哪里
- payload 在哪里

---

## 2. 为什么不能把这层塞回 `machine_reloc`

`machine_reloc` 负责的是：

- relocation sections
- relocation records
- relocation query

但它不应该再继续负责：

- 最终 byte image 分区布局
- header/table/string/payload offset 分配
- 容器字节复制
- 容器布局摘要

不然 `machine_reloc` 会变成：

`既是 relocation artifact，又是 final serialized container`

这样会把边界揉坏。

项目现在的拆法很清楚：

- `machine_reloc`
  - 回答“relocation 作为结构化 artifact 长什么样”
- `machine_container`
  - 回答“这份 artifact 作为最终内部容器字节长什么样”
- `machine_elf`
  - 回答“如何把内部容器投影成标准 ELF artifact”

一句话压缩就是：

- `reloc`：有结构
- `container`：有最终内部字节布局
- `elf`：有标准对象格式布局

---

## 3. 文件定位

- 接口：`include/machine/container.h`
- 实现：`src/machine/object/machine_container/`
- 测试：`tests/machine/object/machine_container/`
- 规划文档：`docs/backend/MACHINE_CONTAINER_PLAN.md`

这一层目前实现几乎集中在：

- `src/machine/object/machine_container/machine_container.c`

所以 lesson 口径上要注意：

- 文件数少
- 但逻辑已经很明确地分成：
  - embedded reloc clone
  - string table / entry size helper
  - header/table offset layout
  - final byte image serialization
  - layout/query/dump

---

## 4. `machine_container.c` 应该按哪几组职责来读

最容易读懂的方式，不是从头顺着看，而是按四组职责去记。

### 4.1 embedded reloc clone

这一组主要在回答：

- 输入 `MachineRelocFile` 怎么被完整复制进 container artifact

典型 helper 是：

- `machine_container_clone_reloc_file(...)`

这很重要，因为它说明 container 层不是扔掉 reloc 再重建，而是：

- 保留一份 embedded relocation artifact
- 再在它上面附加 serialized container bytes

所以当前 `MachineContainerFile` 不是“只有 bytes”，而是：

- `reloc_file + container sections + serialized bytes`

### 4.2 string/table layout helper

这一组主要在回答：

- string table 要多大
- 某个字符串写到哪里
- header entry / section entry / symbol entry / relocation entry 的大小是多少

典型 helper 包括：

- `machine_container_measure_string(...)`
- `machine_container_append_string(...)`
- `machine_container_write_u32_le(...)`

这说明当前 container 层不是“随便拼一块内存”，而是已经有：

`明确的 binary layout contract`

### 4.3 final serialization 主线

这一组主要在回答：

- header offset 怎么定
- section table offset 怎么定
- symbol table offset 怎么定
- relocation table offset 怎么定
- string table offset 怎么定
- payload offset 怎么定
- 最终 bytes 怎么一次性写出来

这一部分才是本层真正“artifact -> final internal byte image”的核心。

### 4.4 query / verifier / dump

这一组主要在回答：

- container 自己是否结构合法
- layout summary 怎么拿
- section lookup 怎么做
- container bytes 怎么拷出来
- dump 怎么把 layout 打成一个容器快照

所以这层已经很明显不是“序列化完就结束”，而是已经有正式 consumer surface。

---

## 5. `include/machine/container.h`：当前数据模型

### 5.1 `MachineContainerFile`

当前最关键的数据结构是：

```text
MachineContainerFile =
  embedded MachineRelocFile
  + container sections
  + serialized bytes
  + header/table/string/payload offsets
```

这一点非常重要，因为它说明 container 层不是只保留最终 byte image，而是：

- relocation artifact 继续保留
- serialized 容器信息叠在它上面

### 5.2 layout summary

这一层专门有一个：

- `MachineContainerLayoutSummary`

它会明确记录：

- `header_offset`
- `header_size`
- `section_table_offset`
- `symbol_table_offset`
- `relocation_table_offset`
- `string_table_offset`
- `string_table_size`
- `payload_offset`
- `payload_size`
- `total_byte_count`

这说明从这一层开始，系统第一次显式回答：

`最终内部容器的整体布局长什么样`

另外，这一层还有一个很适合课堂上顺手记住的小事实：

- header 不是“模糊存在”
- 它在实现里就是固定大小的结构头
- 当前测试里你能直接看到 `header_size == 44`

也就是说，这层已经不是“概念上的有个头”，而是：

`header 本身已经变成可验证、可查询、可打印的精确布局数字`

### 5.3 container section

`MachineContainerSection` 当前主要携带：

- `reloc_section_index`
- `object_section_index`
- `name`
- `logical_start_byte_offset`
- `byte_count`
- `file_offset`
- `relocation_start_index`
- `relocation_count`

这说明当前层相对 `machine_reloc` 最大的新增不是“又有 section”，而是：

- 同一个 section 现在同时有：
  - 逻辑偏移
  - 文件偏移

这正是 container 层最重要的新增信息之一。

---

## 6. 这层现在是什么，不是什么

### 当前它是什么

当前 `machine_container` 负责：

1. 消费 `MachineRelocFile`
2. 保留嵌入的 relocation artifact
3. 生成一个稳定 serialized byte image
4. 在这个 byte image 里保留：
   - header
   - section table
   - symbol table
   - relocation table
   - string table
   - payload area
5. 提供 verifier / dump / layout summary / byte copy / section lookup

### 当前它不是什么

这层现在还不是：

1. ELF/COFF/Mach-O compatibility
2. target-specific relocation opcode encoding
3. linker/archive semantics

所以它更准确地说是：

`internal final serialized container`

不是：

`standard object-file format`

---

## 7. 输入契约：它更喜欢吃已经 verifier-safe 的 relocation artifact

这一点和前几层一样重要。

当前最自然的主线不是：

`从 object/fixup 临时拼 container`

而是：

`machine_reloc verify -> machine_container build`

也就是说，这层默认更喜欢吃：

- relocation sections 已经稳定
- relocation records 已经稳定
- embedded object / symbol / fixup attachment 已经稳定

它不会重新决定：

- 哪个 relocation 属于哪个 section
- target symbol 是谁
- patch span 是多少

所以它的 current input contract 是：

`consume stable relocation artifact, then serialize it into one final internal byte image`

---

## 8. Lowering 总体策略：它先算 layout，再把所有表和 payload 写进 bytes

如果把 `machine_container_build_from_machine_reloc_file(...)` 的精神压成一段 lesson 级伪代码，大致就是：

```text
clone reloc file into embedded reloc_file

measure:
  header size
  section table size
  symbol table size
  relocation table size
  string table size
  payload size

assign offsets:
  header_offset = 0
  section_table_offset = header_end
  symbol_table_offset = section_table_end
  relocation_table_offset = symbol_table_end
  string_table_offset = relocation_table_end
  payload_offset = string_table_end

allocate final bytes[total_byte_count]

write header
write section entries
write symbol entries
write relocation entries
write string table
copy payload bytes

verify container file
```

所以这一层最重要的点是：

- 它不是重新理解 relocation 语义
- 它是在 relocation artifact 的基础上，建立最终内部容器布局

---

## 9. 一个最小 before/after 例子：reloc -> container

假设 `machine_reloc` 已经有：

```text
sections:
  .text
symbols:
  main, F0.L0, F0.L1, F0.L2
relocations:
  .text + 3 -> F0.L2
```

到了 `machine_container`，这些结构会被打包成一个内部稳定的字节镜像，大致概念上像：

```text
[header]
[section table]
[symbol table]
[relocation table]
[string table]
[payload bytes]
```

也就是说，这层真正新增的是：

- 一个完整 serialized byte image
- 每张表和 payload 在文件里的 offset/layout

但它仍然还不是：

- ELF section header layout
- ELF symbol entry layout

所以它是“内部最终容器”，不是“标准对象文件格式”。

---

## 10. 一个更细的例子：为什么同一个 section 在这一层同时有逻辑 offset 和 file offset

假设 relocation 层里有：

```text
reloc section .text:
  logical bytes = 9
```

到了 container 层，它会更接近：

```text
container section .text:
  logical_start_byte_offset = 0
  byte_count = 9
  file_offset = 373
```

这里最关键的变化不是“数字不同”，而是：

- relocation 层的 section 只知道逻辑内容
- container 层的 section 还知道它在最终文件镜像里的具体落点

所以当前 container 层一个很重要的 lesson 口径是：

`逻辑 section span 和文件内布局 span 在这一层第一次被同时固定下来`

---

## 11. verifier：这层到底在锁什么

这一层的 verifier 重点不再只是 relocation artifact 自洽，而是：

`最终内部容器布局是否自洽`

至少包括这些事情：

1. header / table / string / payload offset 是否彼此对得上
2. 各区段 span 是否连续、不重叠
3. section 的 `file_offset` / `byte_count` 是否合理
4. container bytes 是否真的存在并匹配 layout summary
5. embedded relocation artifact 本身也必须合法

这说明 container verifier 锁的不是：

- “这是最终 ELF 吗”

而是：

- “这份内部容器镜像自己是不是一份合法、可复制、可继续投影的最终中间结果”

这点很重要，因为下一层 `machine_elf` 会直接建立在它的 byte image 和 layout 之上。

---

## 12. query：为什么 layout summary 在这一层就变关键了

`machine_container` 这一层最值得抓的一点是：

- bytes 不只是能拷出来
- layout 也已经能被正式查询

当前至少已经支持：

- 取 embedded reloc file
- 取 layout summary
- by-name 找 container section
- 拷整个 container bytes

所以后续 consumer 已经可以直接问：

- header 从哪里到哪里？
- string table 从哪里开始多大？
- payload 从哪里开始多大？
- `.text` 在容器文件里的 `file_offset` 是多少？

这说明 container 层不只是“有最终字节”，而且有：

`final internal file-layout query surface`

再说得更直白一点：

- 在 `machine_reloc` 层，你更像是在问“哪条 relocation 属于哪个 section”
- 到了 `machine_container` 层，你已经是在问“这些 section / symbol / relocation 表最终在文件里排到了哪里”

所以 query 的关注点也发生了切换：

`从 relocation record 导航，切到 final container layout 导航`

---

## 13. dump：为什么这一层第一次真正把 layout 作为第一等输出

这一层 dump 的意义很大，因为它第一次把：

- header 区域
- section table 区域
- symbol table 区域
- relocation table 区域
- string table 区域
- payload 区域

明确地并排打印成一个 container-facing snapshot。

在 relocation 层时，我们看到的还是：

- relocation sections
- relocation records

到了 container 层，重点已经切成：

- 这些结构最终如何排进一个完整文件镜像

所以 container dump 的意义不是“再换一种文本”，而是：

`让 final serialized layout 第一次作为一个整体出现`

---

## 14. 这一层和后面 `machine_elf` 的边界

这是最容易混的地方。

### `machine_container` 负责

- internal final serialized byte image
- header/table/string/payload layout
- container section 的 file offset

### `machine_elf` 负责

- 把这套内部布局进一步投影成标准 ELF object format

也就是说：

- `machine_container` 解决“内部最终镜像怎么组织”
- `machine_elf` 解决“标准对象格式怎么组织”

所以 container 层解决的是：

- “我们自己的最终容器长什么样”

不是：

- “标准 ELF 文件长什么样”

---

## 15. 测试现在在锁什么

当前 `tests/machine/object/machine_container/machine_container_test.c` 这条线，lesson 口径上至少应该理解成在锁这些 authority：

1. `machine_reloc -> machine_container`
2. `machine_ir report -> ... -> machine_container` upstream bridge
3. header/table/string/payload layout
4. layout summary
5. section lookup
6. final byte image copy
7. custom container magic / version / top-level byte layout

所以这组测试锁的不是“能不能输出一串 bytes”，而是：

`container layout contract + query surface + bridge behavior`

一起锁。

---

### 15.1 `test_machine_container_builds_from_machine_ir_report`

这个 case 很适合你拿来理解：

`machine_ir report -> ... -> machine_reloc -> machine_container`

整条桥在 container 层到底落成了什么。

测试里最值得记的不是“桥跑通了”，而是下面这些精确事实：

1. 上游 object bytes 是 `9`
2. section 数是 `1`
3. symbol 数是 `4`
4. relocation 数是 `2`
5. 最终 container bytes 是 `382`
6. header 大小是 `44`
7. payload 起点是 `373`
8. payload 大小还是 `9`
9. `.text` 的 `file_offset` 也是 `373`
10. container 开头四个字节是：
    - `M`
    - `C`
    - `N`
    - `1`

这几个数字连起来其实就在说明一件事：

`前面的 373 个字节已经被 header / section table / symbol table / relocation table / string table 吃掉了，真正的 section payload 是从 373 才开始落盘。`

这个 case 的 dump 也很值得直接拿来当 lesson 例子：

- `machine_container container_bytes=382 object_bytes=9 sections=1 symbols=4 relocations=2`
- `layout header=0..44 section_table=44 symbol_table=72 relocation_table=216 string_table=320..373 payload=373..382`
- `csec.0 .text file@373 bytes=9 logical=0 relocations=2`

lesson 里你可以把它理解成：

`上游 9 字节的 .text，在 container 层没有消失，但它前面多出了完整的容器头和多张表，因此最终文件大小变成了 382。`

如果把这个 case 再压成 lesson 级伪代码，可以理解成：

```text
machine_ir report
  -> reloc artifact
  -> measure all table sizes
  -> assign file offsets
  -> write MCN1 header
  -> write section/symbol/relocation/string tables
  -> append .text payload bytes at payload_offset
```

### 15.2 `test_machine_container_builds_from_machine_reloc_with_external_call`

这个 case 的重点不是分支，而是：

`external-call relocation 进入 container 后，布局仍然必须自洽`

测试先手工搭了一条很小的主线：

```text
call sink(5)
ret 0
```

然后走：

`emit -> encode -> bytes -> object -> reloc -> container`

它锁住的关键事实包括：

1. 这条线可以直接从 `MachineRelocFile` 序列化成 `MachineContainerFile`
2. `object_byte_count == 5`
3. `section_count == 1`
4. `symbol_count == 3`
5. `relocation_count == 1`
6. `payload_size == 5`
7. `total_byte_count > object_byte_count`

这里最想教给读者的一点是：

`container 层并不关心 relocation target 是内部 label 还是外部 symbol 才决定“要不要布局”；只要 relocation artifact 已经稳定，它就负责把它整体排进最终内部镜像。`

也就是说，对 external call 来说，这层新增的不是“又理解了一遍 sink 是谁”，而是：

- 它把带着外部符号 relocation 的 artifact 一样纳入了统一容器布局
- 并保证 layout summary、payload span、总字节数这些 container 语义仍然成立

这个 case 很适合和上一节对照着看：

- 上一个 case 重点是“layout 数字被精确锁住”
- 这个 case 重点是“即使 target 是外部 symbol，container serialization contract 也一样成立”

---

## 16. 这一层和最终 `RISC-V` 方向、后续 `ELF` 层的关系

前面已经反复强调过总方向：

`这个仓库最后要去的是 RISC-V`

所以读 `machine_container` 时，最好不要把它理解成“和目标机无关的一次性自定义格式”。

更准确地说，它现在承担的是一个过渡但非常关键的角色：

1. 把已经稳定的 object/reloc artifact 收束成一份稳定内部 byte image
2. 给后面的 `machine_elf` 提供一个更容易消费的“最终内部序列化基底”

但它当前还没有走到：

- 最终 `RISC-V` ELF relocation entry 编码
- 最终 ELF section header / symbol entry 格式
- 最终标准对象文件兼容布局

所以你可以把它理解成：

`目标相关的 payload 和 relocation 已经可以逐步变真，但“标准文件格式的真相”还没到这里。`

这也正好对应当前仓库的阶段感：

- 上游 `machine_bytes` 已经可能带有 `RISC-V` preview 风格的 `.text` bytes
- `machine_reloc` 已经把 patch/fixup 提升成显式 relocation artifact
- `machine_container` 负责把这些东西装进稳定内部容器
- `machine_elf` 才继续把这套内部容器往标准 ELF 世界投影

所以 lesson 里最值得记的一句话是：

`machine_container` 不是最终 `RISC-V ELF`，但它是把 RISC-V 方向上的 payload/relocation 结果收束成统一内部文件镜像的关键一步。

---

## 17. 你现在读这层时最该抓住的主线

建议抓这五句：

1. `machine_reloc` 负责结构，`machine_container` 负责最终内部字节布局
2. 这一层最大的新增信息是 header/table/string/payload 的 file layout
3. 同一个 section 在这一层第一次同时拥有 logical span 和 file offset
4. layout summary 在这一层已经是非常关键的 consumer surface
5. 这层还不是 ELF，但已经是明确的 internal final serialized artifact

---

## 18. 一句话总结

`machine_container` 把 relocation-facing artifact 序列化成仓库内部稳定的最终容器字节；它的关键不只是“拼出一串 bytes”，而是让 header/table/string/payload 的整体文件布局、container section 的 file offset，以及最终 byte image 的查询接口第一次真正作为 internal final-serialization artifact 成形，因此它是走向真实目标文件格式之前的内部最终容器边界。
