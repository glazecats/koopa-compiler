# Machine Launch Lesson（compiler_lab）

> 目标：解释 `machine_launch` 为什么会出现在 `machine_runtime` 之后，它当前到底新增了什么“CPU launch state”语义，`src/machine/runtime/machine_launch/` 里真实在做什么，以及这层代码里已经存在的 `pc/sp`、寄存器命名、initial-stack linkage 逻辑是什么。

## 一句话定位

`machine_launch` 是 launch-ready runtime memory 第一次被正式翻译成 CPU 起跑寄存器状态的层。

## 常见误解

- 误解一：runtime 已经有 entry 和 stack，launch 只是把两个数字抄出来。
  - 当前这层还负责 register snapshot、profile-owned register naming、pc/sp linkage。
- 误解二：launch 之后就已经在执行指令。
  - 真正的 fetch-state / current byte 还要到 `machine_step` 才出现。

## 导学

如果说：

- `machine_runtime` 回答的是“现在这份 runtime memory 和 stack 已经准备成什么样”

那么：

- `machine_launch` 回答的是“如果 CPU 现在真的要起跑，它的第一份寄存器状态长什么样”

所以这层不是 stepping，也不是 decode。

它更准确的身份是：

`runtime view -> explicit CPU launch-state artifact`

这份讲义建议按下面顺序读：

1. 先理解为什么 `machine_runtime` 后面还要单独有 `machine_launch`
2. 再看 `include/machine/launch.h`，建立 launch file / register snapshot / overview artifact 的整体印象
3. 再看 `src/machine/runtime/machine_launch/`，理解 runtime file 是怎么被抬成 launch state 的
4. 最后带着测试例子去看：
   - `pc/sp` 为什么会被单独物化
   - 为什么会有 profile-owned register name
   - initial stack image 和 launch state 的关系是怎么显式连起来的

学完你应该能：

1. 解释 `machine_launch` 和 `machine_runtime` / `machine_step` 的边界
2. 说明这层真正新增的是“CPU 启动寄存器快照”，不是再一份 runtime dump
3. 看懂一个 `machine_runtime -> machine_launch` 的最小 before/after
4. 说明为什么后面的 step/decode/interp 需要这层先把 `pc/sp` 定下来

---

## 前置阅读

最推荐先读：

1. `lesson/machine/runtime/machine_runtime_lesson.md`
2. `lesson/machine/runtime/machine_load_lesson.md`

如果你还没先弄清楚：

- runtime memory / stack / initial stack image 已经准备成什么样
- entry virtual address 和 initial stack pointer 是怎么来的

那 launch 这里的 `pc/sp` 和 register snapshot 会像是凭空被物化出来。

## 读完后接哪篇

最自然往下接：

1. `lesson/machine/runtime/machine_step_lesson.md`
2. `lesson/machine/runtime/machine_decode_lesson.md`

因为 launch 的意义就是把：

- “能起跑”

变成：

- “真的开始取第一条 byte”

---

## 1. 为什么需要 `machine_launch`

`machine_runtime` 已经回答了：

- entry virtual address
- initial stack pointer
- synthetic stack segment
- unified runtime memory
- minimal initial-stack image

但它还没有直接回答：

- CPU 当前的寄存器快照是什么？
- 哪个寄存器是 program counter？
- 哪个寄存器是 stack pointer？
- 这些寄存器在当前 target profile 下该叫什么？

所以 `machine_launch` 引入的真正边界是：

`runtime/process-prep view -> explicit CPU launch state`

这层最重要的新增不是更多内存信息，而是：

- `pc`
- `sp`
- launch register table
- runtime launch linkage

一句话压缩：

- `machine_runtime`：把可启动的内存视图准备好
- `machine_launch`：把 CPU 起跑时的寄存器视图准备好

---

## 2. 为什么不能把它塞回 `machine_runtime`

`machine_runtime` 负责的是：

- runtime-owned memory
- stack policy
- launch summary
- initial stack image

但它不应该继续直接负责：

- launch register snapshot
- register naming policy
- raw/report register artifact

不然 `machine_runtime` 会同时扮演：

- 运行前内存准备层
- CPU 启动寄存器层

这样边界会混掉。

项目现在的拆法很清楚：

- `machine_runtime`
  - 回答“运行前的内存和栈长什么样”
- `machine_launch`
  - 回答“CPU 起跑寄存器长什么样”
- `machine_step`
  - 回答“起跑之后，当前执行位置/取指状态长什么样”

---

## 3. 文件定位

- 接口：`include/machine/launch.h`
- 实现入口：`src/machine/runtime/machine_launch/machine_launch.c`
- 拆分实现：
  - `src/machine/runtime/machine_launch/machine_launch_core.inc`
  - `src/machine/runtime/machine_launch/machine_launch_build.inc`
  - `src/machine/runtime/machine_launch/machine_launch_query.inc`
  - `src/machine/runtime/machine_launch/machine_launch_dump.inc`
  - `src/machine/runtime/machine_launch/machine_launch_report.inc`
- 测试：`tests/machine/runtime/machine_launch/machine_launch_test.c`
- 规划文档：`docs/backend/MACHINE_LAUNCH_PLAN.md`

当前这层现在也已经跟着实现拆成了“入口 `.c` + 多个 `.inc` 分片”。

更准确地说，现在是：

- `machine_launch.c` 负责模块入口和聚合
- `machine_launch_core.inc` 放基础生命周期 / 小型公共逻辑
- `machine_launch_build.inc` 放 `runtime -> launch` lowering 主线
- `machine_launch_query.inc` 放寄存器 / target-policy 查询
- `machine_launch_dump.inc` 放 dump
- `machine_launch_report.inc` 放 report / artifact / overview helper

这些文件合起来已经有：

- raw file lifecycle
- verifier
- runtime -> launch lowering
- register query helper
- report / overview / register artifact
- dump
- bridge from runtime / load / machine_ir

也就是说，它已经是一个完整的 launch-state artifact 层。

---

## 4. `include/machine/launch.h`：当前数据模型

### 4.1 `MachineLaunchRegister`

这是这层最重要的数据结构之一。

字段包括：

- `name`
- `value`

当前第一版虽然很简单，但它的重要意义非常大：

`launch state 终于不再只是两个散落字段 pc/sp，而是一个显式 register snapshot`

### 4.2 `MachineLaunchFile`

可以先把它记成：

```text
MachineLaunchFile =
  MachineRuntimeFile
  + program_counter
  + stack_pointer
  + registers[]
  + pc/sp register indices
```

所以它本质上是：

`runtime artifact + first-class launch register artifact`

### 4.3 `MachineLaunchTargetPolicySummary`

这层还有一个很关键的 target policy summary：

- `program_counter_register_name`
- `stack_pointer_register_name`

这说明当前 launch 层已经在处理：

`target-profile register naming`

比如：

- generic / riscv preview
  - `pc` / `sp`
- i386 preview
  - `eip` / `esp`

### 4.4 `MachineLaunchReport`

report 会继续缓存：

- header summary
- target policy summary
- runtime launch summary
- initial stack summary
- runtime memory summary
- register summaries

这说明这层 report 的重点是：

`把 launch register 视图和 runtime 背景信息连在一起`

### 4.5 overview / register artifact

header 里还专门有：

- `MachineLaunchReportOverviewArtifact`
- `MachineLaunchReportRegisterArtifact`

这说明当前调用者不只想问：

- 有几个寄存器

而是已经能直接问：

- 哪个是 PC register
- 哪个是 SP register
- 某个名字对应哪个 register artifact

---

## 5. lowering 主线到底做了什么

核心入口是：

- `machine_launch_build_from_machine_runtime_file(...)`

它大致在做：

1. verify runtime 输入
2. 取出 runtime launch summary
3. 物化 `program_counter` 和 `stack_pointer`
4. 按 target profile 生成对应 register name
5. 建立 `registers[]`
6. 记录 `program_counter_register_index` / `stack_pointer_register_index`
7. 再 verify launch file

所以它真正做的是：

`把 runtime launch facts 变成显式 CPU register state`

### 5.1 register name 是怎么来的

`src/machine/runtime/machine_launch/` 这组实现里有两个非常 lesson-friendly 的 helper：

- `machine_launch_program_counter_register_name(...)`
- `machine_launch_stack_pointer_register_name(...)`

当前规则很清楚：

- `generic-elf32`
  - `pc` / `sp`
- `riscv32-preview`
  - `pc` / `sp`
- `i386-preview`
  - `eip` / `esp`

这说明这层虽然还不是完整 architectural register file，但已经开始显式承认：

`同样的 launch 事实，在不同 target profile 下，寄存器名字会不同`

### 5.2 verifier 在保什么边界

`machine_launch_verify_file(...)` 当前主要在保这些契约：

- runtime 输入必须合法
- `launch_file.program_counter == runtime_launch_summary.entry_virtual_address`
- `launch_file.stack_pointer == runtime_launch_summary.initial_stack_pointer`
- initial stack image 必须正好终止在 `sp`
- `pc/sp` register index 必须合法
- PC/SP 寄存器里的值必须和 `program_counter/stack_pointer` 一致

这说明 launch 层不是“把两个值抄出来”这么简单，而是：

`把 runtime summary 和 launch register state 严格绑死`

---

## 6. 一个最小例子看“增加了什么”

如果 `machine_runtime` 已经能告诉我们：

- `entry_virtual_address = 0x1000`
- `initial_stack_pointer = 0x4000`

到了 `machine_launch`，最关键的新增是：

```text
register pc = 0x1000
register sp = 0x4000
```

对于 `i386-preview`，又会变成更像：

```text
register eip = 0x08048000
register esp = ...
```

所以这层真正新增的不是更多地址，而是：

`地址事实第一次变成 register fact`

---

## 7. report 为什么在这层更像“启动总览”

到了 launch 层，下游常问的已经像：

- 当前 PC 是多少
- 当前 SP 是多少
- 哪个 register 是 PC / SP
- initial stack image 跟 launch state 对不对得上

所以 `MachineLaunchReport` 当前会专门缓存：

- `runtime_launch_summary`
- `initial_stack_summary`
- `runtime_memory_summary`
- `register_summaries`

它的意义不只是“方便打印”，而是：

`让后面的 fetch-state consumer 不用再回 runtime 层自己重推 pc/sp`

---

## 8. 测试里最值得读的几个点

`tests/machine/runtime/machine_launch/machine_launch_test.c` 当前最值得看的是三类故事。

### 8.1 normal generic profile

最基础的测试会锁：

- launch file 能从 runtime file 正常建出来
- `pc/sp` 和 runtime summary 一致
- `pc/sp` register artifact 能直接查
- `register_count == 2`
- `runtime_segments == 2`
- `mapped_bytes == 8192`

从 lesson 角度，这个 case 最适合记成：

```text
lreg.0 pc value=0x1000
lreg.1 sp value=0x4000
```

也就是说，runtime 层里的 entry/SP 事实，到这里第一次被正式钉成了 CPU 寄存器快照。

### 8.2 i386 profile naming

还有一类很有代表性的测试会专门锁：

- `eip`
- `esp`

这说明当前 target-policy naming 不是“锦上添花的 dump 细节”，而是这层正式 API 合同的一部分。

它最值得提醒读者的一点是：

`machine_launch` 现在改的不是地址值本身，而是“同样的 launch 事实，在不同 target profile 下应该叫什么寄存器”。`

### 8.3 bridge matrix

测试还覆盖：

- `MachineRuntime*`
- `MachineLoad*`
- `MachineIrAllocateRewriteReport`

几条桥都能走通。

所以当前这层的 authority 已经不是“只能直接喂 runtime file”，而是：

`launch-state artifact 已经可以在多条 upstream bridge 上稳定构造`

另外，`test_machine_launch_profile_bridge` 也很值得顺手看。

它会继续锁：

- profile-aware `machine_ir` bridge 能直接生成 launch artifact
- `i386-preview` 下命名会切到 `eip/esp`
- report / dump wrapper 也会一起跟着 profile 变化

---

## 9. 这层和前后层的边界

### 9.1 相对 `machine_runtime`

`machine_runtime` 还主要是：

- memory / stack / gap / initial-stack image

`machine_launch` 则进一步明确：

- CPU 起跑时 `pc/sp` 是什么
- 这些值在寄存器表里如何命名和落位

### 9.2 相对 `machine_step`

`machine_launch` 还没有做：

- 当前 segment ownership
- 当前 fetch byte
- ready / halted status

这些是下一层 `machine_step` 才开始做的。

所以：

- `machine_launch`：先把 launch register state 讲清楚
- `machine_step`：再把 execution-position / fetch-state 讲清楚

---

## 10. 一句总记

`machine_launch` 不是在执行程序，而是在 `machine_runtime` 之上第一次把“CPU 起跑状态”做成 artifact：runtime 里的 entry/SP 现在被正式物化成 PC/SP 寄存器及其 target-profile 命名，后面的 fetch/decode 才好接。`
