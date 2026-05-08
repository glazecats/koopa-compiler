# Machine Load Lesson（compiler_lab）

> 目标：解释 `machine_load` 为什么会出现在 `machine_exec` 之后，它当前到底新增了什么“显式 load map”语义，`src/machine/runtime/machine_load/` 里真实在做什么，以及这层代码里已经存在的 file-bytes / memory-bytes / zero-fill / entry-segment 映射逻辑是什么。

## 一句话定位

`machine_load` 是 executable candidate 第一次正式进入“显式 loader map”语境的层。

## 常见误解

- 误解一：exec 已经把该回答的都回答完了，load 只是拷贝字节。
  - load 真正新增的是 file-vs-memory byte accounting、zero-fill、entry-segment mapping。
- 误解二：load 之后就已经等于 runtime state。
  - runtime 还要继续补 synthetic stack、initial SP、unified memory 等启动前视图。

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_exec_lesson.md`
2. `lesson/machine/runtime/machine_image_lesson.md`

因为 load 的前提就是：

- image 视角已经成立
- exec 已经决定它是不是 executable-ready enough

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_runtime_lesson.md`
2. `lesson/machine/runtime/machine_launch_lesson.md`

因为 load 之后最自然的问题就是：

- 映射好的内存如何继续变成 launch-ready runtime view
- 再怎么变成 CPU 起跑寄存器状态

---

## 最近同步

最近这层最值得同步的，是它现在不只是在 runtime 前做一张 load map，而是更明确地成为：

- executable candidate
- 到 process-prep / launch-ready runtime

之间的正式桥。

结合最近的主线变化，当前这层最好再多记两点：

1. **load 现在更明确服务于真实 `compiler -riscv / -perf` 导出链**
   也就是说，它不只是内部结构，而是在一条已经和真实 backend/export 主线接上的下游 runtime 链中。

2. **上游 image 的 provenance-aware 语义现在更值得往下游保留地理解**
   lesson 里最好把 load 想成：
   - 不只是 map bytes
   - 也是后面 runtime/launch/observe 能继续建立 artifact provenance 语境的中间层

所以这层现在除了：

- file bytes
- memory bytes
- zero-fill

还要再多记一句：

- `machine_load` 现在更像 “exec 之后第一份真正可被 runtime 消费的内存布局权威面”

---

## 导学

如果说：

- `machine_exec` 回答的是“这份 image 现在是不是 executable-ready enough”

那么：

- `machine_load` 回答的是“如果一个最小 loader 去映射它，当前 load map 长什么样”

所以这层不是 runtime，也不是 process state。

它更准确的身份是：

`exec candidate -> explicit loader-map artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_exec` 后面还要单独有 `machine_load`
2. 再看 `include/machine/load.h`，建立 load segment / memory summary / target policy 的整体印象
3. 再看 `src/machine/runtime/machine_load/`，理解 exec segment 怎么变成 byte-bearing load segment
4. 最后带着测试例子去看：
   - 为什么 `file_byte_count` 和 `memory_byte_count` 不再相等
   - zero-fill 和 segment alignment 为什么要在这层出现

学完你应该能：

1. 解释 `machine_load` 和 `machine_exec` / `machine_runtime` 的边界
2. 说明这层真正新增的是“显式 load mapping”，不是再一份 exec dump
3. 看懂一个 `machine_exec -> machine_load` 的最小 before/after
4. 说明为什么后面的 `machine_runtime` 需要这层先把映射内存表述清楚

---

## 1. 为什么需要 `machine_load`

`machine_exec` 已经回答了：

- entry 存在
- unresolved relocation 为 0
- 哪些 segment 是 executable

但它还没有直接回答：

- 哪些 segment 变成了显式 load mapping？
- 这些 mapping 在内存里到底占多少字节？
- file bytes 之外的那一段是不是要补零？
- entry PC 落在哪个 mapped segment 里？

所以 `machine_load` 引入的真正边界是：

`executable candidate -> explicit load map`

这层最重要的新增信息不是“再多一张 segment 表”，而是：

- `file_byte_count`
- `memory_byte_count`
- alignment
- zero-fill
- flat memory image

---

## 2. 为什么不能把它塞回 `machine_exec`

`machine_exec` 负责的是：

- executable readiness
- entry
- executable segment permission

但它不应该继续直接负责：

- byte-bearing load segment payload
- file-byte vs memory-byte accounting
- zero-fill policy
- explicit loader-map memory view

不然 `machine_exec` 会同时扮演：

- 可执行候选层
- loader 映射层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_exec`
  - 回答“能不能执行”
- `machine_load`
  - 回答“如果要映射，当前 map 长什么样”
- `machine_runtime`
  - 回答“如果已经映射并加了栈，launch-ready runtime view 长什么样”

---

## 3. 文件定位

- 接口：`include/machine/load.h`
- 实现入口：`src/machine/runtime/machine_load/machine_load.c`
- 拆分实现：
  - `src/machine/runtime/machine_load/machine_load_core.inc`
  - `src/machine/runtime/machine_load/machine_load_build.inc`
  - `src/machine/runtime/machine_load/machine_load_query.inc`
  - `src/machine/runtime/machine_load/machine_load_dump.inc`
  - `src/machine/runtime/machine_load/machine_load_report.inc`
- 测试：`tests/machine/runtime/machine_load/machine_load_test.c`
- 规划文档：`docs/backend/MACHINE_LOAD_PLAN.md`

这层最近也跟着 implementor 的目录整理做了拆分，所以 lesson 里最好不要再把它理解成：

- “一个 `machine_load.c` 全包”

更准确地说，现在是：

- `machine_load.c` 负责模块入口和聚合
- `machine_load_core.inc` 放基础生命周期 / 小型公共逻辑
- `machine_load_build.inc` 放 `exec -> load` lowering 主线
- `machine_load_query.inc` 放地址查询和 flat memory helper
- `machine_load_dump.inc` 放 dump
- `machine_load_report.inc` 放 report / artifact / filter helper

这些文件合起来，功能已经很宽：

- raw file lifecycle
- target policy helper
- exec -> load lowering
- verifier
- dump
- report
- flat memory copy / byte lookup
- bridge from exec / image / ELF / machine_ir

也就是说，它已经不是“给 exec 多包一层 struct”。

---

## 4. `include/machine/load.h`：当前数据模型

### 4.1 `MachineLoadSegment`

这是这层最关键的数据结构。

字段包括：

- `name`
- `exec_segment_index`
- `virtual_address`
- `file_byte_count`
- `memory_byte_count`
- `bytes`
- `readable`
- `writable`
- `executable`

这里最值得注意的是：

- `file_byte_count`
- `memory_byte_count`

这两个字段一出现，就说明这层已经开始回答：

`文件里有多少字节`
`映射到内存后占多少字节`

### 4.2 `MachineLoadTargetPolicySummary`

这一层还有一个很重要的 summary：

- `target_profile`
- `base_virtual_address`
- `segment_alignment`

这说明当前 load mapping 已经不是“隐式跟着 exec 走”，而是：

`开始有自己的 target policy`

### 4.3 `MachineLoadMemorySummary`

它会汇总：

- `base_virtual_address`
- `end_virtual_address`
- `memory_byte_count`
- `entry_offset`

这说明当前 load 层已经开始从“segment 列表”往上提升成：

`整体映射内存视图`

### 4.4 `MachineLoadFile`

可以先把它记成：

```text
MachineLoadFile =
  MachineExecFile
  + load segments
  + total_memory_byte_count
```

所以它本质上是：

`exec artifact + explicit mapped-memory artifact`

### 4.5 `MachineLoadReport`

report 会继续缓存：

- `header_summary`
- `target_policy_summary`
- `memory_summary`
- `segment_summaries`
- executable / non-executable filter

所以这层 report 的重点不再只是 entry/executable，而是：

`整体 load map 的可查询视图`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_load_build_from_machine_exec_file(...)`

它大致在做：

1. verify exec 输入
2. 为每个 exec segment 建 load segment
3. 复制 file bytes
4. 按 target alignment 计算 memory span
5. 对 file bytes 之后的内存尾部做 zero-fill
6. 汇总 total mapped byte count
7. 重新 verify load file

所以它真正做的是：

`把 executable segment 变成 loader 看得懂的 byte-bearing memory segment`

### 5.1 为什么 `memory_byte_count` 不再等于 `file_byte_count`

这是这层最重要的新增。

测试里的 `.text` 例子会出现：

```text
file-bytes=9
mem-bytes=4096
zero-fill=4087
align=4096
```

这说明当前 load 层已经开始表达：

- 文件里只有 9 字节
- 但实际映射内存按策略要占 4096 字节
- 后面的 4087 字节要补零

也就是说，这层第一次把：

`文件内容`

和

`内存映射跨度`

区分开了。

### 5.2 alignment policy 为什么在这层出现

当前实现里：

- `machine_load_segment_alignment(profile)` 返回 `0x1000`

也就是说，现在还很保守：

- 所有 profile 都先按 4096 做段对齐

但 lesson 里最重要的不是具体数值，而是：

`segment alignment policy 从这层开始变成显式 public fact`

### 5.3 zero-fill 为什么很重要

当前实现会把 file bytes 之后直到 `memory_byte_count` 的区域显式补零。

这一步很关键，因为它意味着：

`mapped memory image` 已经可以作为完整内存视图来读，而不是“只有文件里那几字节有定义”

这正是后面 `machine_runtime` 能继续叠栈和 launch summary 的前提。

---

## 6. 一个最小例子看“增加了什么”

测试里的经典 dump 之一是：

```text
machine_load profile=generic-elf32 base=0x1000 entry=0x1000 segments=1 file_bytes=9 memory_bytes=4096
entry_segment: lseg.0 .text perms=r-x
segments:
  lseg.0 .text xseg=0 vaddr=0x1000 file-bytes=9 mem-bytes=4096 zero-fill=4087 align=4096 perms=r-x
```

这里相对 `machine_exec` 真正新增的是：

1. `file_bytes` 和 `memory_bytes` 被分开
2. segment 有了真实的 byte-bearing mapped payload
3. `zero-fill` 被显式表述
4. `align` 被显式表述

所以 lesson 口径上，这层的核心不是“再确认 entry”，而是：

`显式把 exec segment 讲成 loader-mapped memory segment`

---

## 7. report 为什么在这层更有价值

到了 load 层，下游最常问的已经很像：

- 哪个 segment 覆盖某个虚拟地址？
- entry 落在哪个 load segment？
- 当前整个 flat mapped memory 是多少字节？
- 某个虚拟地址上的 byte 是多少？

所以 `MachineLoadReport` 当前会额外缓存：

- target policy summary
- memory summary
- segment filter

而 raw file 也直接暴露了：

- `machine_load_file_get_memory_byte_at_virtual_address(...)`
- `machine_load_file_copy_flat_memory_image(...)`

这说明这层开始真正服务：

`later runtime memory consumer`

而不只是服务人工读 segment dump。

---

## 8. 测试里最值得读的几个点

### 8.1 normal resolved path

`tests/machine/runtime/machine_load/machine_load_test.c` 里最核心的 normal case 会验证：

- profile / base / entry 都保留
- `.text` 被映射成一个 load segment
- `file_byte_count == 9`
- `memory_byte_count == 4096`
- `zero-fill == 4087`

这条测试最适合当 lesson 里的“最小正例”。

### 8.2 report overview / filter / artifact

测试还会锁：

- `report_overview`
- `entry_segment`
- `executable[1]`
- `non-executable[0]`

也就是说，这层 report 不只是给个总数，而是真的开始有：

`overview + filter + artifact`

三层消费面。

### 8.3 profile-aware bridge

测试里还有 `i386-preview` 相关 case，会检查：

- `base=0x8048000`

这说明当前 load 层已经支持：

`profile-aware loader mapping`

而不是只有 repository default。

---

### 8.4 `test_machine_load_rejects_non_executable_entry_segment`

这个 case 很值得专门记一下，因为它锁住了一个很容易误解的边界：

`machine_exec` 说 entry 要在 executable segment 里，到了 `machine_load` 这层这个约束也不能被放松。`

也就是说，如果上游 exec artifact 自己就不满足：

- entry segment is executable

那当前 load build/verify 也应该拒绝继续向下传播。

这说明 `machine_load` 不是“只负责复制 bytes 的机械层”，它仍然建立在前一层的 exec 合法性契约上。

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_exec`

`machine_exec` 关心：

- entry exists?
- unresolved relocation gone?
- executable segment 哪些是 code?

`machine_load` 关心：

- code segment 被映射成多少 memory bytes
- alignment 是多少
- zero-fill 是多少
- flat mapped memory 长什么样

### 9.2 相对 `machine_runtime`

`machine_load` 还没有做：

- synthetic stack
- initial stack pointer
- unified runtime memory over load + stack
- launch summary

这些是下一层 `machine_runtime` 才开始做的。

所以：

- `machine_load`：先把 loader map 讲清楚
- `machine_runtime`：再把 process-prep/runtime launch 视角讲清楚

---

## 10. 一句总记

`machine_load` 不是再判断能不能执行，而是第一次把 executable candidate 变成显式 loader map：segment 现在不只是 file bytes，而是带 alignment、zero-fill、memory span 和 flat memory image 的映射内存对象。`
