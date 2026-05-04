# Machine Image Lesson（compiler_lab）

> 目标：解释 `machine_image` 为什么会出现在 `machine_elf` 之后，它当前到底新增了什么“load image 视角”，`src/machine/runtime/machine_image/` 里真实在做什么，以及这层代码里已经存在的地址、入口、relocation resolved/unresolved 逻辑是什么。

## 一句话定位

`machine_image` 是标准 ELF object 第一次被切换到“如果从加载视角看它”这一语境的层。

## 常见误解

- 误解一：ELF 里已经有地址和段，image 只是重复一遍。
  - image 真正新增的是 load-image view 下的 base address、entry、resolved/unresolved relocation 语义。
- 误解二：image 已经等于 executable candidate。
  - executable readiness 还要到 `machine_exec` 才正式判断。

## 前置阅读

最推荐先读：

1. `lesson/machine/object/machine_elf_lesson.md`
2. `lesson/machine/object/machine_container_lesson.md`

因为 image 的前提就是：

- 你已经知道这份 artifact 先是怎么成为标准 ELF object 的
- 现在才切换到加载视角

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_exec_lesson.md`
2. `lesson/machine/runtime/machine_load_lesson.md`

因为 image 之后最自然的问题就是：

- 它什么时候才算 executable candidate
- 如果真的去映射，load map 长什么样

---

## 最近同步

最近这层最值得记住的新增点是：

`machine_image` 不再只是“从 ELF 看地址”，它现在还会把上游 ELF provenance 一路带下来。`

具体来说：

1. `source_elf_artifact_summary`
   - image raw file / report / dump 现在都会承接上游 `MachineElfArtifactSummary`
2. direct build vs imported/reprofiled 区分继续保留
   - 后面的 load/runtime/observe 不用回到 `machine_elf`，也能知道 image 的来源语义
3. image 现在不只是一层地址视图，也开始承接 provenance-aware consumer surface

所以现在这层除了：

- base virtual address
- entry symbol
- resolved / unresolved relocation

还要再加一句：

- `image` 现在也开始承接“这份东西是怎么从 ELF 来的”这一层 artifact provenance

---

## 导学

如果说：

- `machine_elf` 回答的是“这份 backend artifact 现在是不是一个标准 ELF object”

那么：

- `machine_image` 回答的是“如果把这份 ELF object 从加载视角再看一眼，当前已经能知道哪些虚拟地址、入口点、段覆盖范围、已解/未解重定位信息”

所以这层不是再做一种 object format。

它的定位更准确地说是：

`post-ELF load-image preparation layer`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_elf` 后面还要再单独落一层 `machine_image`
2. 再看 `include/machine/image.h`，建立 image file / report / address query 的整体印象
3. 再看 `src/machine/runtime/machine_image/`，理解 ELF `.text`、symbol、relocation 怎么投影成 image 视角
4. 最后带着最小例子和伪代码去看：
   - base virtual address 是怎么来的
   - entry symbol 怎么选
   - relocation 为什么会分成 resolved / unresolved

学完你应该能：

1. 解释 `machine_image` 和 `machine_elf` 的职责边界
2. 说明这层真正新增的是“加载视角下的地址语义”，不是又一种内部字节格式
3. 看懂一个 `machine_elf -> machine_image` 的 before/after 例子
4. 说明为什么 report/query 在这层会天然变得比前面更偏“按地址导航”

---

## 1. 为什么需要 `machine_image`

`machine_elf` 已经回答了：

- ELF header / section / symbol / relocation 是不是标准的
- 目标 profile 对应什么 `e_machine` / relocation type
- 外部工具能不能把它当成 relocatable ELF object 理解

但它还没有直接回答：

- 现在这份对象里的 `.text` 如果当成要被装载的第一段，它的虚拟地址区间是什么？
- 哪些符号已经能映射到 image 内虚拟地址？
- 哪些 relocation site 已经能在“单对象、单 text image”范围里找到目标地址？
- 哪个函数目前最像 entry？

所以 `machine_image` 引入的真正边界是：

`standard object view -> first load-image view`

它不是 linker，也不是 executable finalizer。

它只是先把当前已经拥有的信息整理成：

- 一个 base virtual address
- 一个或多个 image segment
- 一批已经带虚拟地址的 image symbol
- 一批 site_virtual_address 已知的 image relocation
- resolved / unresolved 的最初分类

一句话压缩：

- `machine_elf`：这是不是一个标准对象文件
- `machine_image`：如果把它当成要被装载的代码镜像，现在已经知道哪些地址事实

---

## 2. 为什么不能把这层塞回 `machine_elf`

`machine_elf` 负责的是：

- object format
- section/table/symbol/relocation 的 ELF 语义
- parse / normalize / round-trip

但它不应该继续直接负责：

- image base address policy
- load segment coverage
- entry virtual address
- relocation site 在 image 里的地址投影
- “这个 relocation 现在能不能在 image 内就地解析到目标地址”

不然 `machine_elf` 会同时扮演：

- 对象格式层
- 加载视角层

这样边界会揉在一起。

项目现在的拆法非常清楚：

- `machine_elf`
  - 回答“ELF object 长什么样”
- `machine_image`
  - 回答“把这个 object 看成第一份 load image 时，地址事实长什么样”

这也是为什么 `docs/NEXT_STEPS.md` 现在把当前 backend active slice 放在 `docs/backend/MACHINE_IMAGE_PLAN.md`，而不是再停在 `machine_elf`。

---

## 3. 文件定位

- 接口：`include/machine/image.h`
- 实现入口：`src/machine/runtime/machine_image/machine_image.c`
- 拆分实现：
  - `src/machine/runtime/machine_image/machine_image_core.inc`
  - `src/machine/runtime/machine_image/machine_image_build.inc`
  - `src/machine/runtime/machine_image/machine_image_query_dump.inc`
  - `src/machine/runtime/machine_image/machine_image_verify.inc`
- 测试：`tests/machine/runtime/machine_image/machine_image_test.c`
- 规划文档：`docs/backend/MACHINE_IMAGE_PLAN.md`

这层最近已经跟着 implementor 的目录整理一起做了第一轮拆分。

所以 lesson 口径上最好不要再把它理解成：

- “只有一个 `machine_image.c`”

更准确地说，现在是：

- `machine_image.c` 负责模块入口和聚合
- `machine_image_core.inc` 放基础生命周期 / 小型公共逻辑
- `machine_image_build.inc` 放 `ELF -> image` lowering 主线
- `machine_image_query_dump.inc` 放 query / dump / report-facing helper
- `machine_image_verify.inc` 放 verifier

但不要因为入口文件还叫一个 `.c`，就把它当成薄层。

当前这组文件合起来已经同时包含了：

- raw file 生命周期
- raw query helper
- ELF -> image lowering
- verifier
- dump
- report build
- report query helper
- `machine_ir -> ... -> machine_elf -> machine_image` bridge

也就是说，这层已经不只是“转一下名字再 dump”。

---

## 4. `src/machine/runtime/machine_image/` 应该按哪几组职责来读

最容易读懂的方式，不是从头顺着扫，而是先按五组职责记。

### 4.1 基础 helper

这组主要包括：

- `machine_image_set_error(...)`
- `machine_image_strdup(...)`
- `machine_image_append_format(...)`

它们负责：

- 错误消息
- 文本拷贝
- dump string builder

这一组本身不复杂，但 lesson 里值得记一个点：

`machine_image` 现在是一个完整 artifact 层，不是只能返回 int 的黑盒子，所以它天然需要稳定 error / dump surface。

### 4.2 profile -> 地址策略

这一组很关键：

- `machine_image_base_virtual_address(...)`
- `machine_image_segment_alignment(...)`

当前代码里，base virtual address 已经是按 `MachineElfTargetProfile` 决定的：

- `MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32`
  - `0x1000`
- `MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW`
  - `0x10000`
- `MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW`
  - `0x08048000`

而 segment alignment 当前统一给：

- `0x1000`

这说明一个非常重要的现实：

`machine_image` 不是只复制 ELF bytes，它已经开始承载 target-flavored load address policy。`

### 4.3 raw image query helper

这组包括：

- `machine_image_file_get_segment(...)`
- `machine_image_file_find_segment_by_name(...)`
- `machine_image_file_find_segment_covering_virtual_address(...)`
- `machine_image_file_get_symbol(...)`
- `machine_image_file_find_symbol_by_name(...)`
- `machine_image_file_find_symbol_by_virtual_address(...)`
- `machine_image_file_get_entry_symbol(...)`
- `machine_image_file_get_relocation(...)`
- `machine_image_file_find_relocation_by_site_virtual_address(...)`
- `machine_image_file_get_resolved_relocation_count(...)`
- `machine_image_file_get_unresolved_relocation_count(...)`
- `machine_image_file_get_resolved_relocation_by_index(...)`
- `machine_image_file_get_unresolved_relocation_by_index(...)`

这一组体现出这层最典型的消费模式：

`address-oriented navigation`

前面像 `machine_select` / `machine_layout` / `machine_emit` / `machine_encode`，很多查询还是围绕：

- function
- block
- label
- offset

到了 `machine_image`，查询重心进一步变成：

- 某个虚拟地址落在哪个 segment
- 某个虚拟地址对应哪个 symbol
- 某个 relocation site 的虚拟地址是什么
- resolved / unresolved relocation 各有多少

这就是“object 视角”和“image 视角”的差别。

### 4.4 ELF -> image lowering 主线

核心入口是：

- `machine_image_build_from_machine_elf_file(...)`

这条主线真正在做的事情大致是：

1. verify ELF 输入
2. 找到 `.text` section
3. 复制 `.text` bytes 成为 image byte payload
4. 建一个单独的 image segment，名字固定为 `.text`
5. 给这个 segment 赋 base virtual address
6. 把 text-defined ELF symbols 转成 image-defined symbols
7. 选择 entry symbol / entry virtual address
8. 把 ELF relocation 转成 image relocation，并判断 resolved 还是 unresolved
9. 对结果再跑一遍 `machine_image_verify_file(...)`

所以它不是：

`ELF bytes 原样搬家`

而是：

`从 object metadata 中提取第一份 load-image 语义`

### 4.5 report / verifier / dump / bridge

这组包括：

- `machine_image_build_report_from_file(...)`
- `machine_image_verify_file(...)`
- `machine_image_dump_file(...)`
- `machine_image_build_from_machine_elf_report(...)`
- `machine_image_build_from_machine_elf_bytes(...)`
- `machine_image_build_from_machine_ir_report(...)`
- `machine_image_build_report_from_machine_elf_file(...)`
- `machine_image_build_report_from_machine_elf_report(...)`
- `machine_image_build_report_from_machine_elf_bytes(...)`
- `machine_image_build_report_from_machine_ir_report(...)`

这一组说明：

- 这层可以从 raw ELF file 来
- 也可以从 ELF report 来
- 也可以直接从 ELF bytes parse 再来
- 还可以一路从 `machine_ir` bridge 过来

而且 report 不是可有可无的包装壳，它已经是正式 API surface。

---

## 5. `include/machine/image.h`：当前数据模型

### 5.1 `MachineImageFile`

这是最核心的 raw artifact。

可以先把它记成：

```text
MachineImageFile =
  target_profile
  + base_virtual_address
  + entry info
  + segments
  + symbols
  + relocations
  + owned image bytes
```

这里很重要的一点是：

- 它已经不是 section-based object view
- 它已经是 segment/symbol/relocation/image-bytes view

也就是说，中心名词已经从：

- section

慢慢转成：

- segment
- virtual_address
- site_virtual_address

### 5.2 `MachineImageSegment`

字段包括：

- `name`
- `image_offset`
- `virtual_address`
- `byte_count`
- `align`

当前最直观的理解方式是：

`这段 image bytes 在整个 image payload 里的切片，以及它被装载后从哪个虚拟地址开始`

现在第一版 lowering 很保守，只生成一个 `.text` segment，所以很多测试里你会看到：

- `segment_count == 1`
- `segments[0].name == ".text"`
- `segments[0].image_offset == 0`

### 5.3 `MachineImageSymbol`

字段包括：

- `name`
- `binding`
- `type`
- `is_defined`
- `segment_index`
- `value_offset`
- `virtual_address`

这说明 image symbol 当前关注的是：

- 它是不是 image 内已定义符号
- 如果是，它位于哪个 segment
- 在 segment 里的 offset 是多少
- 它的最终 image virtual address 是多少

这里最值得记的公式就是：

```text
symbol.virtual_address
= segment.virtual_address + symbol.value_offset
```

### 5.4 `MachineImageRelocation`

字段包括：

- `source_kind`
- `segment_index`
- `segment_offset`
- `site_virtual_address`
- `is_resolved`
- `target_virtual_address`
- `type`
- `symbol_index`
- `symbol_name`

这个结构正好体现了 `machine_image` 最有辨识度的新增语义：

`一个 relocation site 现在到底只是“还欠外部解析”，还是已经能映射到 image 内部某个目标地址`

所以它不是只存：

- 原始 relocation type
- 原始 symbol reference

它还显式存：

- `site_virtual_address`
- `is_resolved`
- `target_virtual_address`

这就是 image 层和 object 层最不一样的地方。

### 5.5 `MachineImageReport`

report 会在 raw file 之上再缓存：

- `header_summary`
- `entry_symbol_summary`
- `segment_summaries`
- `symbol_summaries`
- `relocation_summaries`
- `resolved_relocation_indices`
- `unresolved_relocation_indices`

也就是说，report 的价值不是“再拷一份数据”，而是：

`把最常被消费的 image inspection 视角提前整理好`

尤其是这两组索引：

- `resolved_relocation_indices`
- `unresolved_relocation_indices`

它们让下游不用每次自己扫整张 relocation 表。

---

## 6. lowering 主线到底做了什么

这里直接按 `machine_image_build_from_machine_elf_file(...)` 来讲。

### 6.1 第一步：先验证 ELF 输入

函数一开始先做：

- `machine_elf_verify_file(...)`
- `machine_elf_file_get_target_profile(...)`
- `machine_elf_file_get_text_section(...)`

如果：

- ELF 本身不合法
- 找不到 `.text`
- `.text` 的文件范围越界

就直接失败。

这说明当前 image 层明确依赖：

`verifier-legal machine_elf input`

它不是想容忍随便一份坏 ELF bytes 再凑合猜。

### 6.2 第二步：把 `.text` 投影成第一个 image segment

当前 lowering 很保守：

- 只看 `.text`
- 只建一个 segment
- `segment.name = ".text"`
- `segment.image_offset = 0`
- `segment.virtual_address = base_virtual_address`
- `segment.byte_count = text_section->byte_count`
- `segment.align = machine_image_segment_alignment(profile)`

这一步等价于说：

`先别谈多 segment、data segment、loader program header，我们先把当前 text image 单独站稳。`

### 6.3 第三步：复制 `.text` bytes

当前 raw image payload 就是：

```text
image.bytes = copy(elf.bytes[text_section.file_offset .. text_section.file_offset + byte_count))
```

也就是说，这层暂时还没有去：

- patch relocation
- 改写 ISA bytes
- 合并多个 section

它只是先把：

`未来将被装载的第一段代码字节`

抽出来，绑定上虚拟地址。

### 6.4 第四步：把 text-defined symbols 变成 image symbols

当前规则很清楚：

- 每个 ELF symbol 都会拷到 image symbol 表
- 只有“定义在 `.text` section 里的符号”才会成为 image-defined symbol

也就是源码里的条件：

- `elf_symbol->is_defined`
- `elf_symbol->section_index == text_section_index`

如果满足，就设置：

- `is_defined = 1`
- `segment_index = 0`
- `value_offset = elf_symbol->value`
- `virtual_address = base_virtual_address + elf_symbol->value`

如果不满足，就保持：

- `is_defined = 0`
- `segment_index = (size_t)-1`
- `virtual_address = 0`

所以这层一个很重要的翻译就是：

`ELF symbol value offset -> image virtual address`

### 6.5 第五步：选择 entry

当前 entry 选择是一个两段式策略。

第一优先级：

- 找名字叫 `main`
- `binding == MACHINE_ELF_SYMBOL_GLOBAL`
- `type == MACHINE_ELF_SYMBOL_FUNC`
- 且它是 text-defined

如果找到了，就：

- `has_entry = 1`
- `entry_symbol_index = symbol_index`
- `entry_virtual_address = symbol.virtual_address`

如果没找到，再退化到：

- 第一个满足
  - global
  - func
  - defined in text
  的符号

这个策略很保守，但很适合当前阶段：

- 既优先支持常见 `main`
- 又不强行要求所有输入都必须恰好有 `main`

### 6.6 第六步：把 ELF relocation 变成 image relocation

这一段是整个 `machine_image` 最应该认真看的地方。

当前每个 ELF relocation 都会变成一个 `MachineImageRelocation`。

关键字段翻译大致是：

```text
image_relocation.segment_index = 0
image_relocation.segment_offset = elf_relocation.offset
image_relocation.site_virtual_address = base_virtual_address + elf_relocation.offset
image_relocation.symbol_index = elf_relocation.symbol_index
image_relocation.symbol_name = elf_relocation.symbol_name
image_relocation.type = elf_relocation.type
```

而 resolved / unresolved 的判断逻辑非常直接：

```text
if target_symbol is defined and target_symbol is in segment 0:
    is_resolved = 1
    target_virtual_address = target_symbol.virtual_address
else:
    is_resolved = 0
    target_virtual_address = 0
```

也就是说，这一层新增的不是“把 relocation 消灭掉”，而是：

`先把 relocation site 放进 image 地址空间，再判断目标地址在当前 image 里是否已经可知`

这就是当前计划文档里说的：

- resolved in-image target
- unresolved external obligation

---

## 7. 用最小例子看“改了什么 / 增加了什么”

这一节专门回答你一直要求的那类问题：

- 这一层到底改了什么？
- 新增了什么信息？
- 从上一层翻译了什么？

### 7.1 例子一：一个内部分支 relocation 被解析成 resolved

假设 `machine_elf` 这一层大致有：

```text
.text bytes
symbol:
  F0.L1 defined in .text, value=0x5
  F0.L2 defined in .text, value=0x7
  main  defined in .text, value=0x0
relocation:
  site offset = 0x4
  target symbol = F0.L2
```

到 `machine_image` 之后，新增出来的信息就变成：

```text
base_virtual_address = 0x1000
segment .text:
  image_offset = 0
  virtual_address = 0x1000

symbol F0.L2:
  value_offset = 0x7
  virtual_address = 0x1007

relocation:
  segment_offset = 0x4
  site_virtual_address = 0x1004
  is_resolved = yes
  target_virtual_address = 0x1007
```

这里真正“增加”的内容不是 symbol 表数量变了，而是：

1. site 有了虚拟地址
2. target 有了虚拟地址
3. relocation 被分类成 “已在 image 内可解析”

### 7.2 例子二：外部调用保留为 unresolved

`tests/machine/runtime/machine_image/machine_image_test.c` 里有一个很典型的例子：

- `main` 里做 `CALL_VOID_IMM sink`
- `sink` 是外部符号，不在当前 image 定义
- profile 选 `MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW`

于是 build 出来的 image 会有：

```text
base_virtual_address = 0x08048000
entry = main
symbol sink:
  is_defined = 0

relocation at site 0x08048000:
  kind = call
  type = R_386_PLT32
  is_resolved = no
  target_virtual_address = 0
  symbol_name = sink
```

这里最关键的 lesson 是：

`machine_image` 没有假装“所有 relocation 到了 image 层都已经完成解析”。

它老老实实地把这类情况表达成：

- site 已知
- target symbol 名字已知
- relocation type 已知
- 但目标虚拟地址当前未知

这正是“第一份 load-image 视角”该做的事。

### 7.3 例子三：entry 不是新字节，而是新增的地址语义

在很多例子里，`main` 本来在 ELF symbol 里就存在。

但到了 `machine_image`，新增的是：

```text
has_entry = 1
entry_symbol_index = index(main)
entry_virtual_address = base_virtual_address + main.value_offset
```

所以 entry 不是平白凭空造了个新 symbol。

它真正新增的是：

`谁是入口，以及入口在 image 地址空间里是多少`

---

## 8. 一段伪代码看懂 lowering 主线

把核心逻辑压缩一下，大致就是：

```text
build_image_from_elf(elf):
    verify(elf)
    profile = elf.target_profile
    text = elf.text_section

    image.base_virtual_address = base_address_for(profile)
    image.segment[0] = {
        name = ".text",
        image_offset = 0,
        virtual_address = image.base_virtual_address,
        byte_count = text.byte_count,
        align = 0x1000
    }
    image.bytes = copy(text.bytes)

    for each elf_symbol:
        clone name/binding/type
        if symbol is defined in .text:
            mark defined in segment 0
            value_offset = elf_symbol.value
            virtual_address = base_virtual_address + value_offset

    entry = prefer global func named "main"
    if not found:
        entry = first defined global func in text

    for each elf_relocation:
        target = image.symbols[elf_relocation.symbol_index]
        site_virtual_address = base_virtual_address + elf_relocation.offset
        if target is defined in segment 0:
            mark resolved
            target_virtual_address = target.virtual_address
        else:
            mark unresolved

    verify(image)
    return image
```

这段伪代码里最值得记住的三件事是：

1. `.text` 被投影成第一个 segment
2. symbol value 被投影成 virtual address
3. relocation site/target 被投影成 resolved-or-unresolved image fact

---

## 9. verifier 在保什么边界

`machine_image_verify_file(...)` 这一层并不花哨，但它非常关键，因为 image 层现在已经开始处理：

- bytes
- segment
- symbol
- relocation
- entry
- virtual address consistency

当前 verifier 主要在保这些约束。

### 9.1 segment 约束

- 至少要有一个 segment
- `byte_count > 0` 时必须有 `bytes`
- 每个 segment 都要有名字
- `image_offset + byte_count` 不能越界
- `align` 不能是 0

也就是说，segment 不是只挂个名字就行，它必须真能在 image bytes 里切出合法区间。

### 9.2 symbol 约束

对于 defined symbol：

- `segment_index` 必须有效
- `value_offset` 不能超出对应 segment
- `virtual_address` 必须等于 `segment.virtual_address + value_offset`

对于 undefined symbol：

- `segment_index` 必须是 `(size_t)-1`
- `virtual_address` 必须是 0

这说明“已定义 / 未定义”的语义在 image 层是强约束，不是提示字段。

### 9.3 entry 约束

如果 `has_entry == 1`，那么：

- `entry_symbol_index` 必须在范围内
- 这个 entry symbol 必须是 defined
- 它的 `virtual_address` 必须等于 `entry_virtual_address`

所以 entry 不是独立悬空字段，它必须和 symbol 表相互一致。

### 9.4 relocation 约束

每个 relocation 都要满足：

- `symbol_name` 非空
- `segment_index` 合法
- `segment_offset` 在 segment 范围内
- `site_virtual_address == segment.virtual_address + segment_offset`
- `symbol_index` 合法

额外还要求：

- 如果 `is_resolved == 1`，那 `target_virtual_address != 0`
- 如果 `is_resolved == 0`，那 `target_virtual_address == 0`

这就把 resolved/unresolved 从“说明文案”变成了 verifier-backed contract。

---

## 10. report 为什么在这层尤其重要

到了 `machine_image`，report 的意义会比前面更强。

原因很简单：

这层的消费者很多时候想问的不是：

- 第 N 个 block 是谁

而是：

- 某个地址属于哪个 segment
- 入口符号是谁
- 某个 relocation site 的目标已不已知
- unresolved relocation 一共有多少

所以 `MachineImageReport` 目前明确缓存了：

- header summary
- entry symbol summary
- segment summaries
- symbol summaries
- relocation summaries
- resolved/unresolved relocation index sets

这让下游可以直接做：

- `machine_image_report_find_segment_summary_covering_virtual_address(...)`
- `machine_image_report_find_symbol_summary_by_virtual_address(...)`
- `machine_image_report_find_relocation_summary_by_site_virtual_address(...)`
- `machine_image_report_get_resolved_relocation_summary_by_index(...)`
- `machine_image_report_get_unresolved_relocation_summary_by_index(...)`

一句话说，report 让 image 层从：

- “有原始结构”

变成：

- “有面向地址语义的消费接口”

---

## 11. dump 文本现在在表达什么

`machine_image_dump_file(...)` 现在打印的骨架大致是：

```text
machine_image profile=... base=... bytes=... segments=... symbols=... relocations=... entry=...
entry_address: ...
segments:
  iseg.N ...
symbols:
  isym.N ...
relocations:
  irel.N ...
```

这里最值得注意的是 relocation 行。

resolved 时会类似：

```text
irel.0 kind=ctrl-primary seg=0 off=0x4 site=0x1004 resolved=yes target=0x1007 sym=3 type=2 name=F0.L2
```

unresolved 时会类似：

```text
irel.0 kind=call seg=0 off=0x0 site=0x08048000 resolved=no target=- sym=... type=... name=sink
```

也就是说，dump 已经不再只是“列出 relocation 条目”，而是直接把：

- site address
- resolved 状态
- target address

全都打出来了。

这很适合 teaching，因为一眼就能看出这层新增的语义到底是什么。

---

## 12. 测试里最值得读的几个点

`tests/machine/runtime/machine_image/machine_image_test.c` 当前很适合当 lesson 附带阅读材料，因为它覆盖了三类最关键的故事。

### 12.1 从 `machine_ir` 一路 bridge 到 `machine_image`

测试 `test_machine_image_builds_from_machine_ir_report` 做的是：

- 手工构造 `MachineIrAllocateRewriteReport`
- 走 `machine_image_build_from_machine_ir_report(...)`
- 检查：
  - segment count
  - symbol count
  - relocation count
  - base virtual address
  - entry virtual address
  - symbol virtual address
  - relocation resolved / target_virtual_address

这说明当前主线已经不是“只能单测这层内部 helper”。

它已经能走通：

`machine_ir -> ... -> machine_elf -> machine_image`

### 12.2 unresolved external call

测试 `test_machine_image_preserves_unresolved_external_call_site` 专门验证：

- 外部 `sink` 调用在 image 层仍然是 unresolved
- 但是 site virtual address 已经是明确的
- 对于 `i386-preview`，还能看到具体 relocation type 变成 `R_386_PLT32`

这条测试特别重要，因为它证明：

`machine_image` 不是把所有未完成事项都抹平，而是把“已知到哪一步”表达出来。`

### 12.3 report helper / bytes helper

还有一些测试覆盖：

- `machine_image_build_report_from_machine_elf_report(...)`
- `machine_image_build_from_machine_elf_bytes(...)`
- `machine_image_build_report_from_machine_elf_bytes(...)`
- `machine_image_dump_report(...)`

这说明这一层已经不只是 raw builder。

它还在认真维护：

- report consumer surface
- from-bytes reimport surface
- dump/report round-trip inspection surface

### 12.4 profile / base-address helper

`test_machine_image_profile_and_ir_dump_helpers` 这类 case 也很值得看。

它们会直接锁：

- `generic-elf32` 的 base 是 `0x1000`
- `riscv32-preview` 的 base 是 `0x10000`
- `i386-preview` 的 base 是 `0x08048000`

这组测试很重要，因为它说明：

`machine_image` 的 profile 差异不是 dump 文本标签，而是真的会改变 image 视角里的虚拟地址起点。`

---

## 13. 当前这层的边界是什么

这个 lesson 最后一定要把边界说清楚，不然很容易误会成“已经快是 linker 了”。

### 13.1 当前已经有的

- profile-owned base virtual address
- `.text` -> one first load segment
- image-defined text symbol virtual addresses
- entry selection
- relocation site virtual addresses
- resolved vs unresolved relocation classification
- raw/report address-oriented query helper
- from-ELF / from-bytes / from-machine-ir bridge

### 13.2 当前还没有故意去做的

- 多 object 链接
- 跨 object 符号解析
- relocation patch 回最终 ISA executable bytes
- 真正 loader/program-header/executable image 语义
- 完整 data/bss/image layout mainline

所以它当前更准确的身份不是：

`mini linker`

而是：

`first explicit post-ELF load-image artifact`

---

## 14. 一句总记

如果你最后只想记一句话，那就是：

`machine_elf` 解决“这是一个什么对象文件”，`machine_image` 解决“把它当成第一份可装载代码镜像时，现在已经知道哪些地址、入口和 relocation 事实”。`
