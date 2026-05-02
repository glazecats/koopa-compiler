# Machine Runtime Lesson（compiler_lab）

> 目标：解释 `machine_runtime` 为什么会出现在 `machine_load` 之后，它当前到底新增了什么“launch-ready runtime view”语义，`src/machine/runtime/machine_runtime/` 里真实在做什么，以及这层代码里已经存在的 synthetic stack / initial SP / unified runtime memory / initial-stack image 逻辑是什么。

## 一句话定位

`machine_runtime` 是 load map 之后第一次把“能映射”正式推进成“快能起跑”的层。

## 常见误解

- 误解一：runtime 这里只是把 load segment 再包一层。
  - 当前真正新增的是 stack、initial SP、launch summary、unified runtime memory。
- 误解二：只要有 load map 就已经可以直接 step。
  - 后面的 launch/step 依赖的 `pc/sp`、runtime region、initial stack image 都还要靠这层先立起来。

## 导学

如果说：

- `machine_load` 回答的是“显式 load map 长什么样”

那么：

- `machine_runtime` 回答的是“在这个 load map 之上，最小 launch-ready runtime 视图长什么样”

所以这层不是 instruction step，也不是 decode / interp。

它更准确的身份是：

`load map -> conservative process-preparation artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_load` 后面还要单独有 `machine_runtime`
2. 再看 `include/machine/runtime.h`，建立 runtime segment / stack / launch / region 的整体印象
3. 再看 `src/machine/runtime/machine_runtime/`，理解 load map 怎么被抬成 runtime view
4. 最后带着测试例子去看：
   - synthetic stack 是怎么来的
   - initial SP 为什么是那样
   - unified runtime memory / gap / initial-stack image 在表达什么

学完你应该能：

1. 解释 `machine_runtime` 和 `machine_load` / `machine_launch` 的边界
2. 说明这层真正新增的是“运行前准备好的内存与启动视图”，不是又一份 load dump
3. 看懂一个 `machine_load -> machine_runtime` 的最小 before/after
4. 说明为什么后面的 launch/step/decode 需要这层先把 PC、SP、stack 和 unified memory 讲清楚

---

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_load_lesson.md`
2. `lesson/machine/runtime/machine_exec_lesson.md`
3. `lesson/machine/runtime/machine_image_lesson.md`

因为 runtime 这里默认前提就是：

- image 已经建立了地址视角
- exec 已经建立了 executable candidate 视角
- load 已经建立了显式映射

如果上游这三层没先读，runtime 里的 stack / gap / unified region 会比较难判断它到底是在“新增什么”。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_launch_lesson.md`
2. `lesson/machine/runtime/machine_step_lesson.md`

最常见的读法是：

- 先接 `machine_launch`
  - 看 runtime memory 怎么继续变成 CPU launch state
- 再接 `machine_step`
  - 看 launch state 怎么开始真的取第一条 byte

---

## 1. 为什么需要 `machine_runtime`

`machine_load` 已经回答了：

- 哪些 load segment 被映射
- 每个 segment 的 file bytes / memory bytes 是多少
- flat mapped memory 长什么样

但它还没有直接回答：

- 启动时 PC 是多少？
- 启动时 SP 是多少？
- 栈段在哪？
- load map 和 stack 合起来的 unified runtime memory 长什么样？
- 最小初始栈镜像长什么样？

所以 `machine_runtime` 引入的真正边界是：

`loader map -> launch-ready runtime view`

这层最重要的新增信息不是更多 segment，而是：

- synthetic stack segment
- initial stack pointer
- gap
- launch summary
- initial-stack summary
- runtime region model

---

## 2. 为什么不能把它塞回 `machine_load`

`machine_load` 负责的是：

- load-backed segment mapping
- file-byte / memory-byte accounting
- zero-fill

但它不应该继续直接负责：

- synthetic stack
- launch-ready `pc/sp`
- runtime gap
- initial stack image
- unified runtime regions

不然 `machine_load` 会同时扮演：

- loader 映射层
- 进程准备层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_load`
  - 回答“怎么映射”
- `machine_runtime`
  - 回答“映射好了以后，一个最小 runtime/process-prep 视图长什么样”
- `machine_launch`
  - 再往下接启动状态

---

## 3. 文件定位

- 接口：`include/machine/runtime.h`
- 实现入口：`src/machine/runtime/machine_runtime/machine_runtime.c`
- 拆分实现：
  - `src/machine/runtime/machine_runtime/machine_runtime_core.inc`
  - `src/machine/runtime/machine_runtime/machine_runtime_build.inc`
  - `src/machine/runtime/machine_runtime/machine_runtime_query.inc`
  - `src/machine/runtime/machine_runtime/machine_runtime_dump.inc`
  - `src/machine/runtime/machine_runtime/machine_runtime_report.inc`
- 测试：`tests/machine/runtime/machine_runtime/machine_runtime_test.c`
- 规划文档：`docs/backend/MACHINE_RUNTIME_PLAN.md`

这层最近同样已经跟着实现拆分一起做了同步，所以 lesson 口径最好也更新成：

- `machine_runtime.c` 负责模块入口和聚合
- `machine_runtime_core.inc` 放基础生命周期 / 公共逻辑
- `machine_runtime_build.inc` 放 `load -> runtime` lowering 主线
- `machine_runtime_query.inc` 放 memory / stack / region 查询
- `machine_runtime_dump.inc` 放 dump
- `machine_runtime_report.inc` 放 report / overview / artifact helper

也就是说，这层虽然入口还是一个 `.c`，但实际职责已经明确分摊到多份实现文件里。

这些文件合起来已经有很多很明确的职责：

- raw file lifecycle
- runtime target policy
- load -> runtime lowering
- verifier
- dump
- report / overview / artifact helper
- unified region helper
- bridge from load / exec / image / ELF / machine_ir

所以它已经明显不是一个薄壳 wrapper。

---

## 4. `include/machine/runtime.h`：当前数据模型

### 4.1 `MachineRuntimeSegment`

当前 runtime segment 除了名字、地址、字节，还额外有：

- `kind`
- `load_segment_index`

其中 kind 现在分两类：

- `MACHINE_RUNTIME_SEGMENT_KIND_LOAD`
- `MACHINE_RUNTIME_SEGMENT_KIND_STACK`

这说明 runtime 层第一次把：

- 原来来自 load map 的 segment
- 后来合成出来的 stack segment

放进了同一个统一模型里。

### 4.2 `MachineRuntimeHeaderSummary`

这一层 header summary 里最重要的新增就是：

- `entry_virtual_address`
- `initial_stack_pointer`

也就是说，到了 runtime 层，我们不再只讲 entry，还开始讲：

`launch-ready SP`

### 4.3 `MachineRuntimeTargetPolicySummary`

这层 target policy 目前包括：

- `stack_alignment`
- `stack_byte_count`
- `stack_gap_byte_count`

这说明当前 synthetic stack 不是拍脑袋生成的，而是：

`profile-owned runtime policy`

### 4.4 `MachineRuntimeStackSummary` / `GapSummary` / `LaunchSummary`

这几个 summary 很能体现 runtime 层的身份。

它们分别在表达：

- 栈段在哪、大小多少、SP 落在哪里
- load 区和栈区之间的 gap 是多少
- 启动时 `pc/sp/stack-segment` 是什么

所以这层已经不是单纯 memory image，而是开始显式表达：

`launch state`

### 4.5 `MachineRuntimeInitialStackSummary`

这是这层很有辨识度的新增 artifact。

它会总结：

- `argc`
- `argv` terminator
- `envp` terminator
- `auxv` terminator
- initial stack image 的 base/end/word-size

当前第一版还很保守：

- `argc=0`
- 只给一个极小、保守的初始栈镜像

但这已经足够把“栈不是空白概念”这件事正式做出来。

### 4.6 `MachineRuntimeRegionSummary`

这层还额外有一个统一 region 模型：

- `LOAD`
- `GAP`
- `STACK`

这说明 runtime 层当前已经不只是 segment 视角，而是有了：

`一张统一的 runtime address-space 划分表`

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_runtime_build_from_machine_load_file(...)`

它大致在做：

1. verify load 输入
2. 复制现有 load-backed segments
3. 按 policy 计算 stack alignment / stack size / stack gap
4. 添加 synthetic `.stack` segment
5. 计算 initial stack pointer
6. 汇总 unified runtime mapped byte count
7. 暴露 gap / launch / initial-stack summary
8. 再 verify runtime file

所以它真正做的是：

`在 load map 之上，构造一个最小 launch-ready runtime memory view`

### 5.1 synthetic stack 是怎么来的

当前实现里的关键 helper 包括：

- `machine_runtime_stack_alignment(...)`
- `machine_runtime_stack_byte_count(...)`
- `machine_runtime_stack_gap_byte_count(...)`

当前第一版很保守，基本都是：

- `0x1000`

所以 lesson 口径上最重要的不是具体值，而是：

`stack segment 不是原输入里自带的，而是 runtime 层按策略合成的`

### 5.2 initial SP 是怎么来的

测试里的 `generic-elf32` 例子很直观：

- base = `0x1000`
- `.text` mapped 4096 bytes
- gap = 4096 bytes
- stack base = `0x3000`
- initial SP = `0x4000`

也就是说：

`SP` 当前就是栈段顶端

这很适合作为第一版 launch-ready policy。

### 5.3 unified runtime memory 为什么是新边界

到了 runtime 层，你已经不能只看 load segment 了。

因为现在有：

- load segment
- gap
- stack segment

这些东西组合起来才是：

`runtime-visible address space`

所以 runtime 层当前最重要的新增不是“又多了一个 segment”，而是：

`统一看待 load + gap + stack`

---

## 6. 一个最小例子看“增加了什么”

测试里的经典 dump 是：

```text
machine_runtime profile=generic-elf32 base=0x1000 entry=0x1000 sp=0x4000 segments=2 mapped_bytes=8192
entry_segment: rseg.0 .text perms=r-x
stack_segment: rseg.1 .stack perms=rw-
segments:
  rseg.0 .text kind=load lseg=0 vaddr=0x1000 bytes=4096 perms=r-x
  rseg.1 .stack kind=stack vaddr=0x3000 bytes=4096 perms=rw-
```

这里相对 `machine_load` 真正新增的是：

1. `sp=0x4000`
2. `.stack` segment 被正式加进来了
3. runtime segment 出现了 `kind=load` / `kind=stack`
4. 总 mapped bytes 现在是 load + stack 的统一总量

所以 lesson 口径上，这层的核心不是“多了个栈”，而是：

`load-backed memory 第一次被提升成 launch-ready runtime memory`

---

## 7. report 为什么在这层更重要

runtime 层的调用者最常问的通常已经像：

- entry segment 是谁
- stack segment 是谁
- SP 落在哪
- gap 有多大
- initial stack image 长什么样
- 某个 virtual address / flat offset 落在哪个 region

所以 `MachineRuntimeReport` 当前会额外缓存：

- `stack_summary`
- `gap_summary`
- `launch_summary`
- `initial_stack_summary`
- executable / non-executable / stack filter

也就是说，这层 report 已经明显不只是“把 raw file 再打印一遍”，而是在构造：

`launch-oriented runtime overview`

---

## 8. initial stack image 当前在表达什么

这部分很值得单独讲，因为它是 runtime 层最有辨识度的新增之一。

测试里的 report dump 会直接给出：

```text
initial-stack: base=... end=... bytes=20 word=4 argc=0 argv-null=... env-null=... auxv-null=...
```

这说明当前 runtime 层已经不再把“栈里应该有点什么”完全留空。

它当前至少显式表达了一个非常保守的最小镜像：

- `argc = 0`
- `argv` null terminator
- `envp` null terminator
- `auxv` terminator pair

lesson 口径上，最好的理解方式是：

`这还不是完整 OS ABI 栈，但它已经是 first explicit initial-stack artifact`

---

## 9. region 模型为什么很重要

runtime 层当前不只保留 segment 视角，还专门加了：

- `MachineRuntimeRegionSummary`
- `MachineRuntimeReportRegionArtifact`
- region filter

region 当前分：

- load
- gap
- stack

这说明当前下游如果只想问：

- “某个地址处于 load、gap 还是 stack？”

它不需要再自己从 segment + stack + gap summary 里拼。

这一步的 lesson 价值很大，因为它说明 runtime 层已经开始从：

- “若干 separate artifact”

提升成：

- “统一运行态地址空间模型”

---

## 10. 测试里最值得读的几个点

### 10.1 normal generic profile

最基础的测试会锁：

- `.text` load segment 仍然存在
- synthetic `.stack` 被正确加入
- `sp=0x4000`
- mapped bytes 变成 8192

这是 lesson 最适合拿来讲的正例。

### 10.2 report overview / launch / stack / region

测试里还会验证：

- report overview
- stack summary
- gap summary
- launch summary
- initial stack summary
- region lookup / filter

这说明 runtime 层现在的 consumer surface 已经很完整，不只是“有个 dump”。

### 10.3 profile-aware bridge

测试里也覆盖了 `i386-preview` 路线，验证：

- base 变成 `0x08048000`
- 对应 stack / launch / report 也全部跟着 profile 走

这说明 runtime 层不是只有 repository default 的一条死路。

### 10.4 `test_machine_runtime_bridge_matrix`

这个 case 也很值得专门提出来，因为它锁的不是某一个数字，而是：

`bridge 面是不是整条矩阵都通。`

它会从很多不同上游入口继续 build 到 `machine_runtime`，包括：

- `MachineLoadFile`
- `MachineLoadReport`
- `MachineExecFile`
- `MachineExecReport`
- `MachineImageFile`
- `MachineImageReport`
- `MachineElfFile`
- `MachineElfReport`
- ELF bytes
- profile-aware `MachineIrAllocateRewriteReport`

lesson 里这条测试最值得传达的一点是：

`machine_runtime` 已经不是某一条 happy path 专用层，而是整个 runtime-prep 入口矩阵都被认真锁住了。`

### 10.5 `test_machine_runtime_profiled_wrappers` / `test_machine_runtime_profile_bridge`

这两组 case 也很有代表性。

它们会验证：

- profile-aware wrapper 不只是能 build
- 而是 base / stack / launch / dump / report 都会一起跟着 profile 变化

所以当前 runtime 层的 profile 不是一段注释，而是会真正改变：

- load-backed 区域起点
- synthetic stack 区域
- `pc/sp`
- 整体 report / dump 输出

---

## 11. 这层和前后层的边界

### 11.1 相对 `machine_load`

`machine_load` 还只关心：

- load segment
- memory bytes
- zero-fill

`machine_runtime` 进一步关心：

- synthetic stack
- launch-ready `pc/sp`
- unified runtime memory
- gap
- initial stack image
- region model

### 11.2 相对 `machine_launch`

`machine_runtime` 还没有真正进入：

- launch-state artifact
- first fetch state
- decode / interp 语义

这些是后面的 `machine_launch` / `machine_step` / `machine_decode` 才开始接的。

所以：

- `machine_runtime`：先把 runtime memory 和 launch-ready summary 讲清楚
- `machine_launch`：再把“真正启动时的状态对象”讲清楚

---

## 12. 一句总记

`machine_runtime` 不是在执行程序，而是在 load map 之上第一次把“可启动的运行前状态”做成 artifact：它补出 synthetic stack、initial SP、gap、launch summary、initial-stack image 和统一 runtime region 视图，给后面的 launch/step/decode 线打地基。`
