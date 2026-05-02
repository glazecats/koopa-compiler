# Machine Exec Lesson（compiler_lab）

> 目标：解释 `machine_exec` 为什么会出现在 `machine_image` 之后，它当前到底新增了什么“可执行候选”语义，`src/machine/runtime/machine_exec/` 里真实在做什么，以及这层代码里已经存在的 entry / segment permission / unresolved-relocation rejection 逻辑是什么。

## 一句话定位

`machine_exec` 是 image 线第一次正式回答“这份东西现在够不够像 executable candidate”的层。

## 常见误解

- 误解一：有了 image 地址视角，就已经差不多能执行了。
  - 当前还必须先回答 entry 是否存在、relocation 是否都 resolved、哪些 segment 是 executable。
- 误解二：exec 这里只是 image 再包一层 report。
  - 它已经在做明确的 executable-readiness 筛选和 permission surfacing。

## 导学

如果说：

- `machine_image` 回答的是“当前 image 里已经知道哪些虚拟地址、段、符号、relocation site”

那么：

- `machine_exec` 回答的是“这份 image 现在够不够像一个可执行候选物”

所以这层不是 linker，也不是 runtime。

它更准确的身份是：

`image -> executable-candidate authority`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_image` 后面还要单独有 `machine_exec`
2. 再看 `include/machine/exec.h`，建立 exec file / report / executable segment 的整体印象
3. 再看 `src/machine/runtime/machine_exec/`，理解 image 是怎么被抬成 exec candidate 的
4. 最后带着测试例子去看：
   - entry 为什么必须存在
   - unresolved relocation 为什么必须被拒绝
   - executable segment 为什么要被单独标出来

学完你应该能：

1. 解释 `machine_exec` 和 `machine_image` / `machine_load` 的边界
2. 说明这层真正新增的是“可执行就绪性判断”，不是又一种 image dump
3. 看懂一个 `machine_image -> machine_exec` 的最小 before/after
4. 说明为什么后面的 loader/runtime 需要这层先把 entry 和 executable segment 讲清楚

---

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_image_lesson.md`
2. `lesson/machine/object/machine_elf_lesson.md`

因为 exec 这里默认前提就是：

- ELF 已经被投影成 image 视角
- 地址、段、符号、relocation site 已经有了第一版 load-image 语义

如果上游两层没先读，`entry`、`resolved/unresolved relocation`、`executable segment` 这些判断条件会像平地冒出来。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_load_lesson.md`
2. `lesson/machine/runtime/machine_runtime_lesson.md`

因为 exec 之后最自然的问题就是：

- 这份 executable candidate 如果真的要映射，load map 长什么样
- 映射之后，launch-ready runtime view 又长什么样

---

## 1. 为什么需要 `machine_exec`

`machine_image` 已经回答了：

- `.text` 映射到了哪个 base virtual address
- image-defined symbol 的 virtual address 是什么
- relocation site 哪些已经 resolved，哪些还 unresolved

但它还没有直接回答：

- 当前这份 image 到底有没有合法 entry？
- 现在还有没有 unresolved relocation obligation？
- 哪些 image segment 应该被当成 executable-ready code segment？

所以 `machine_exec` 引入的真正边界是：

`load-image view -> executable-candidate view`

这层最重要的新增信息不是更多 bytes，而是：

- entry-required
- unresolved-relocation rejection
- executable permission surfacing

一句话压缩：

- `machine_image`：先把地址讲清楚
- `machine_exec`：再判断“这份东西现在能不能算作 executable candidate”

---

## 2. 为什么不能把它塞回 `machine_image`

`machine_image` 负责的是：

- image segment
- symbol virtual address
- relocation site resolved / unresolved

但它不应该继续直接负责：

- executable readiness
- entry-required validation
- executable segment filter
- later loader-facing exec artifact

不然 `machine_image` 会同时扮演：

- 地址投影层
- 可执行候选层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_image`
  - 回答“当前 image 长什么样”
- `machine_exec`
  - 回答“这个 image 现在是不是 executable-ready enough”
- `machine_load`
  - 回答“如果要被 loader 映射，显式 load map 长什么样”

---

## 3. 文件定位

- 接口：`include/machine/exec.h`
- 实现入口：`src/machine/runtime/machine_exec/machine_exec.c`
- 拆分实现：
  - `src/machine/runtime/machine_exec/machine_exec_core.inc`
  - `src/machine/runtime/machine_exec/machine_exec_build.inc`
  - `src/machine/runtime/machine_exec/machine_exec_query.inc`
  - `src/machine/runtime/machine_exec/machine_exec_dump.inc`
  - `src/machine/runtime/machine_exec/machine_exec_report.inc`
- 测试：`tests/machine/runtime/machine_exec/machine_exec_test.c`
- 规划文档：`docs/backend/MACHINE_EXEC_PLAN.md`

这层现在也已经跟着实现拆成了“入口 `.c` + 多个 `.inc` 分片”，所以 lesson 口径最好同步成：

- `machine_exec.c` 负责模块入口和聚合
- `machine_exec_core.inc` 放基础生命周期 / 公共逻辑
- `machine_exec_build.inc` 放 `image -> exec` lowering 主线
- `machine_exec_query.inc` 放 query / segment helper
- `machine_exec_dump.inc` 放 dump
- `machine_exec_report.inc` 放 report / artifact helper

这些文件合起来已经有：

- raw file lifecycle
- raw query helper
- image -> exec lowering
- verifier
- dump
- report build
- report filter / artifact helper
- bridge from image / ELF / machine_ir

也就是说，它现在已经是一个拆分后的完整 artifact 层，而不是旧 lesson 口径里的单文件实现。

---

## 4. `include/machine/exec.h`：当前数据模型

### 4.1 `MachineExecSegment`

这是这层最关键的结构之一。

字段包括：

- `name`
- `image_segment_index`
- `virtual_address`
- `byte_count`
- `readable`
- `writable`
- `executable`

这说明 `machine_exec` 当前最重要的新增不是内容重写，而是：

`segment permission view`

也就是当前 image segment 到了 exec 层以后，要被明确讲成：

- `r-x`
- `rw-`
- 等等

### 4.2 `MachineExecFile`

当前 raw artifact 可以先记成：

```text
MachineExecFile =
  MachineImageFile
  + entry_virtual_address
  + exec segments
```

所以它本质上是：

`image artifact + executable-candidate shaping`

### 4.3 `MachineExecReport`

report 又在 raw file 之上缓存了：

- `header_summary`
- `segment_summaries`
- `executable_segment_indices`
- `non_executable_segment_indices`

这说明 report 的重点是：

`把 executable / non-executable 这类 later consumer 最常问的问题提前整理好`

### 4.4 overview / segment artifact

header 里还专门有：

- `MachineExecReportOverviewArtifact`
- `MachineExecReportSegmentArtifact`
- `MachineExecReportSegmentFilterKind`

这说明这层当前已经不只是“报告一张整表”，而是已经开始支持：

- 从 report 总览直接拿 entry segment
- 按 executable / non-executable / entry 过滤 segment
- 对单个 segment 做 artifact-style 查询

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_exec_build_from_machine_image_file(...)`

这条主线大致在做：

1. verify image 输入
2. 检查 image 必须有 entry
3. 检查 unresolved relocation 必须为 0
4. 按 image segment 建立 exec segment
5. 给 segment 挂上 permission policy
6. 重新验证 exec file

所以它不是“复制 image 再改个名字”，而是：

`用一套更严格的可执行候选契约筛一遍 image`

### 5.1 为什么 entry 必须存在

当前 `machine_exec` 的第一条核心规则就是：

`没有 entry，就不是 executable candidate`

这和 `machine_image` 很不一样。

在 image 层，你可以只是说：

- “当前 image 有一批 symbol 和 relocation”

但到了 exec 层，必须进一步回答：

- “从哪里开始执行”

### 5.2 为什么 unresolved relocation 必须拒绝

当前第二条核心规则就是：

`如果 image 里还有 unresolved relocation，就不能进入 exec`

这也很合理，因为 exec 层此时想表达的是：

- 这份东西至少已经不欠外部目标地址了

它还不是完整 OS executable，但它已经不能继续保留“还有外部 relocation 没定”的状态。

### 5.3 executable permission 是怎么来的

当前实现里，segment permission 规则仍然很保守。

从测试和 dump 看，典型 `.text` 会变成：

```text
perms=r-x
```

所以这层当前最重要的 permission surfacing 是：

- 可执行代码段要明确被标成 executable
- 后面 loader/runtime 就不用再从 image 层自己猜

---

## 6. 一个最小例子看“增加了什么”

测试里当前最经典的一份 dump 是：

```text
machine_exec profile=generic-elf32 base=0x1000 entry=0x1000 segments=1 bytes=9
entry_segment: xseg.0 .text perms=r-x
segments:
  xseg.0 .text img-seg=0 vaddr=0x1000 bytes=9 perms=r-x
```

这里相对 `machine_image` 真正新增的是三件事：

1. `entry=0x1000` 被正式提升成 exec header 事实
2. `entry_segment` 被显式指出来
3. 段权限被正式写成 `r-x`

所以 lesson 口径上，不该讲成：

`image 又打印了一遍`

而应该讲成：

`image 里的地址事实被提升成 executable-candidate 事实`

---

## 7. unresolved relocation 为什么要被拒绝

测试里有专门构造 unresolved external call 的 image，再尝试 build exec。

lesson 级别最该记住的规则就是：

```text
if image has unresolved relocations:
    reject exec build
```

也就是说，`machine_exec` 不会把 unresolved external obligation 带到下一层去。

这一步是从：

- “地址视角下的未决 obligation”

进入到：

- “可执行候选层必须已经没有未决 obligation”

的关键边界。

---

## 8. report 为什么在这层更好用

到了 exec 层，很多调用者最想问的已经不是：

- 第 N 个 symbol 是谁

而是：

- 哪个 segment 是 executable
- 哪个 segment 是 entry segment
- 当前有没有 non-executable segment

所以 `MachineExecReport` 当前会直接缓存：

- executable segment index set
- non-executable segment index set

测试里的 report dump 也会直接给：

```text
report_segment_filters:
  executable[1] xseg.0(.text)
  non-executable[0]
  entry[1] xseg.0(.text)
```

这说明当前 report 已经很明显在服务：

`later loader/runtime consumer`

而不是只服务人工读 dump。

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_image`

`machine_image` 还允许：

- unresolved relocation
- 更中性的 image segment 视图

`machine_exec` 则要求：

- 有 entry
- 没 unresolved relocation
- executable segment 已明确

### 9.2 相对 `machine_load`

`machine_exec` 还没有做：

- byte-bearing load segment payload
- file-byte vs memory-byte mapping
- zero-fill / load alignment policy

这些是下一层 `machine_load` 才开始正式做的。

所以：

- `machine_exec`：判断能不能执行
- `machine_load`：决定如果要装载，怎么映射

---

## 10. 测试 authority 现在在锁什么

`tests/machine/runtime/machine_exec/machine_exec_test.c` 当前很值得当 lesson 附带阅读材料，因为它至少在锁这些东西：

1. 正常 resolved image 能成功 build 成 exec
2. unresolved image 会被拒绝
3. entry segment / executable filter / report overview 都可查
4. `MachineImage*` / `MachineElf*` / ELF bytes / `MachineIrAllocateRewriteReport` 这几条 bridge 都能走通
5. profile-aware bridge 也能走通

所以当前这层的测试口径已经不是：

`能不能建一个 exec struct`

而是：

`build + verify + filter + report + upstream bridge`

一起锁。

---

### 10.1 `test_machine_exec_builds_from_machine_ir_and_image_artifacts`

这个 case 是最适合当正例来读的。

它锁住的不是一句抽象的“build 成功”，而是：

1. `machine_ir -> ... -> machine_image -> machine_exec` 可以走通
2. 最终只有一个 exec segment
3. 总字节数还是 `9`
4. entry 就是 `0x1000`
5. 这个 entry 落在 `.text`
6. `.text` 的权限是 `r-x`

lesson 里最适合把它压成一句话：

`image` 层只知道地址和 relocation；到了 `exec` 层，这些信息第一次被收紧成“一个 entry 在可执行段里的可执行候选物”。`

### 10.2 `test_machine_exec_rejects_entry_outside_executable_segment`

这个 case 很重要，因为它说明 verifier 不是只看：

- “有没有 entry 地址”

而是更严格地看：

- “这个 entry 地址是不是落在 executable segment 里”

也就是说，下面这种情况当前会被拒绝：

```text
entry = 0x...
but entry segment is not executable
```

这正好体现 `machine_exec` 和 `machine_image` 的边界：

- `machine_image` 只负责把 entry 地址说出来
- `machine_exec` 负责判断这个 entry 是否真的具备可执行资格

### 10.3 `test_machine_exec_rejects_unresolved_relocations`

这个 case 是这层最该抓住的“反例”。

它专门锁：

- unresolved relocation 仍然存在时
- `machine_exec_build_from_machine_image_file(...)` 必须失败

这条测试的 lesson 价值非常高，因为它把这层最核心的 stop condition 说得很干净：

`有 entry 还不够；如果还有外部 relocation obligation 没清掉，也不能进入 exec。`

换句话说，这一层不是在给未决 image 发通行证，而是在做第一道真正的“可执行就绪性闸门”。

### 10.4 bridge / profile-aware helper case

`test_machine_exec_elf_bridge_helpers` 以及 profile-aware bridge 相关 case 也很值得看。

它们会锁：

- 从 `MachineElfFile`
- 从 `MachineElfReport`
- 从 ELF bytes
- 从 `MachineIrAllocateRewriteReport`

都能继续 build / report / dump 到 `machine_exec`。

所以这层的 public surface 已经不是：

- “只有一个 raw builder”

而是：

`raw artifact + report artifact + dump helper + profile-aware bridge`

一起成体系。

---

## 11. 一句总记

`machine_exec` 不是再做一份 image，而是把 image 提升成“可执行候选” artifact：必须有 entry、必须没有 unresolved relocation、必须把 executable segment 说清楚，后面的 loader/runtime 才好继续往下接。`
