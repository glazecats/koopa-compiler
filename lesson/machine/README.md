# Machine Lessons

这套 `lesson/machine/` 不是一堆彼此独立的笔记，而是在讲同一条 machine backend 主线：

`machine_ir -> machine_select -> machine_layout -> machine_emit -> machine_encode -> machine_bytes -> machine_object -> machine_reloc -> machine_container -> machine_elf -> machine_image -> machine_exec -> machine_load -> machine_runtime -> machine_launch -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome -> machine_history -> machine_timeline -> machine_log -> machine_journal`

当前最终目标方向仍然是 `RISC-V`。

最近这条 machine 主线还要额外记住一个同步点：

- 仓库现在已经有真实 `compiler` CLI 入口：
  - `compiler -riscv input.sy -o output`
  - `compiler -perf input.sy -o output`
- 当前导出的不再只是内部 dump，而是 assembler-accepted 的 `RISC-V` preview asm
- 并且当前全局对象已经会显式落到 `.sbss` / `.sdata`

同时也要记住当前边界：

- `-koopa` 现在仍然是刻意不支持的
- 当前 machine 主线优先级是 **RISC-V correctness before backend optimization**

如果按最近提交再多记一条当前焦点，那就是：

- `machine_select` 的 CFG live-out-aware cleanup 已经不再只是实验性 fallback，而是更接近 checkpoint 的 selected-side cleanup 主线

lesson 里会经常出现：

- `generic-elf32`
- `i386-preview`
- `riscv32-preview`

这些 profile 主要是 staging / bridge / 验证 surface，不代表最终目标改成了别的 ISA。

---

## 1. 目录怎么分

- `lowering/`
  - 讲 Value-SSA / machine register model 往 machine-facing lowering 落下来的前半段
- `object/`
  - 讲 layout/encode 之后怎么变成 byte、object、reloc、container、ELF
- `runtime/`
  - 讲 ELF/image 再往“可加载、可运行、可单步解释”的运行前后状态怎么长出来
- `observe/`
  - 讲执行结果怎么继续收束成 observe/delta/trace/event/outcome/history/timeline/log/journal

如果你把它想成四段，会更容易记：

1. `lowering`
   - “代码选成什么样”
2. `object`
   - “最终字节和目标文件怎么长”
3. `runtime`
   - “如果真的加载/起跑/执行，这个机器状态怎么长”
4. `observe`
   - “执行之后，怎么把结果整理成可消费记录”

如果你现在想快速补最近 machine 新增的内容，最值得优先回看的几篇是：

1. `object/machine_bytes_lesson.md`
2. `object/machine_object_lesson.md`
3. `object/machine_reloc_lesson.md`
4. `object/machine_elf_lesson.md`
5. `runtime/machine_image_lesson.md`

因为最近最明显的新东西就在：

- preview bytes honesty
- object/fixup/relocation summary surface
- ELF provenance / artifact summary
- image 对上游 ELF provenance 的承接

如果你最近主要在补 object/runtime 这条收尾线，还可以再用一句话把它们串起来：

- `machine_bytes -> machine_object -> machine_reloc -> machine_container -> machine_elf -> machine_image`
  现在已经不只是“越走越下游”
  - 也是一条越来越完整的
    - profile
    - policy
    - provenance
    承接链

---

## 2. 最推荐的阅读顺序

如果你第一次读，最推荐按下面顺序，不要来回跳：

1. `lowering/machine_ir_lesson.md`
2. `lowering/machine_select_lesson.md`
3. `lowering/machine_layout_lesson.md`
4. `lowering/machine_emit_lesson.md`
5. `lowering/machine_encode_lesson.md`
6. `object/machine_bytes_lesson.md`
7. `object/machine_object_lesson.md`
8. `object/machine_reloc_lesson.md`
9. `object/machine_container_lesson.md`
10. `object/machine_elf_lesson.md`
11. `runtime/machine_image_lesson.md`
12. `runtime/machine_exec_lesson.md`
13. `runtime/machine_load_lesson.md`
14. `runtime/machine_runtime_lesson.md`
15. `runtime/machine_launch_lesson.md`
16. `runtime/machine_step_lesson.md`
17. `runtime/machine_decode_lesson.md`
18. `runtime/machine_payload_decode_lesson.md`
19. `runtime/machine_interp_lesson.md`
20. `runtime/machine_transition_lesson.md`
21. `runtime/machine_state_lesson.md`
22. `runtime/machine_mutation_lesson.md`
23. `runtime/machine_writeback_lesson.md`
24. `runtime/machine_commit_lesson.md`
25. `runtime/machine_apply_lesson.md`
26. `observe/machine_observe_lesson.md`
27. `observe/machine_delta_lesson.md`
28. `observe/machine_trace_lesson.md`
29. `observe/machine_event_lesson.md`
30. `observe/machine_outcome_lesson.md`
31. `observe/machine_history_lesson.md`
32. `observe/machine_timeline_lesson.md`
33. `observe/machine_log_lesson.md`
34. `observe/machine_journal_lesson.md`

如果你不想一次读完整条链，下面有“按问题选读”。

---

## 3. 按问题选读

### 3.1 想知道 “select 到底在选什么”

先读：

1. `lowering/machine_ir_lesson.md`
2. `lowering/machine_select_lesson.md`
3. `lowering/machine_layout_lesson.md`

这三篇会回答：

- 值已经住到哪了
- generic machine op 怎么选成 selected op
- block 怎么变成线性布局

### 3.2 想知道 “字节到底从哪来”

先读：

1. `lowering/machine_emit_lesson.md`
2. `lowering/machine_encode_lesson.md`
3. `object/machine_bytes_lesson.md`
4. `object/machine_object_lesson.md`
5. `object/machine_elf_lesson.md`

这条线会回答：

- label 怎么显式化
- offset 怎么算
- 指令最终是什么 bytes
- bytes 怎么收束进 object / ELF

### 3.3 想知道 “如果真的执行，会发生什么”

先读：

1. `runtime/machine_image_lesson.md`
2. `runtime/machine_exec_lesson.md`
3. `runtime/machine_load_lesson.md`
4. `runtime/machine_runtime_lesson.md`
5. `runtime/machine_launch_lesson.md`
6. `runtime/machine_step_lesson.md`
7. `runtime/machine_decode_lesson.md`
8. `runtime/machine_payload_decode_lesson.md`
9. `runtime/machine_interp_lesson.md`
10. `runtime/machine_transition_lesson.md`
11. `runtime/machine_state_lesson.md`

这条线会回答：

- ELF/image 如何变成 executable candidate
- 怎么 load
- launch state 是什么
- 当前 byte/tag/payload/action/next-state 是怎么来的

### 3.4 想知道 “结果最后怎么整理成记录”

先读：

1. `observe/machine_observe_lesson.md`
2. `observe/machine_delta_lesson.md`
3. `observe/machine_trace_lesson.md`
4. `observe/machine_event_lesson.md`
5. `observe/machine_outcome_lesson.md`
6. `observe/machine_history_lesson.md`
7. `observe/machine_timeline_lesson.md`
8. `observe/machine_log_lesson.md`
9. `observe/machine_journal_lesson.md`

这条线会回答：

- before/after state 怎么变成 observe
- change 怎么变成 delta/trace
- trace 怎么收束成 event/outcome
- outcome 怎么继续变成 history/timeline/log/journal

---

## 4. 每个目录里重点先看哪几篇

### 4.1 `lowering/`

第一优先：

- `machine_ir_lesson.md`
- `machine_select_lesson.md`

第二优先：

- `machine_layout_lesson.md`
- `machine_emit_lesson.md`
- `machine_encode_lesson.md`

### 4.2 `object/`

第一优先：

- `machine_bytes_lesson.md`
- `machine_elf_lesson.md`

第二优先：

- `machine_object_lesson.md`
- `machine_reloc_lesson.md`
- `machine_container_lesson.md`

### 4.3 `runtime/`

第一优先：

- `machine_image_lesson.md`
- `machine_exec_lesson.md`
- `machine_load_lesson.md`
- `machine_runtime_lesson.md`
- `machine_launch_lesson.md`

第二优先：

- `machine_step_lesson.md`
- `machine_decode_lesson.md`
- `machine_payload_decode_lesson.md`
- `machine_interp_lesson.md`
- `machine_transition_lesson.md`
- `machine_state_lesson.md`

第三优先：

- `machine_mutation_lesson.md`
- `machine_writeback_lesson.md`
- `machine_commit_lesson.md`
- `machine_apply_lesson.md`

### 4.4 `observe/`

第一优先：

- `machine_outcome_lesson.md`
- `machine_history_lesson.md`
- `machine_timeline_lesson.md`
- `machine_log_lesson.md`
- `machine_journal_lesson.md`

第二优先：

- `machine_observe_lesson.md`
- `machine_delta_lesson.md`
- `machine_trace_lesson.md`
- `machine_event_lesson.md`

---

## 5. lesson 里常见的四种信息

大多数 machine lesson 现在都尽量按同一口径写：

1. 为什么需要这一层
2. 它和前后层的边界是什么
3. 真实代码在哪
4. 测试例子到底锁了什么

很多 lesson 还会专门讲：

- `新增了什么`
- `改了什么`
- `翻译了什么`
- 伪代码主线
- 真实 test name 对应的行为

所以如果你只想快速扫某一篇，最先看这几个段落就够了。

---

## 6. 代码路径怎么对照

machine lesson 现在最好按下面这套路径记：

- 公共头文件：
  - `include/machine/*.h`
- lowering 实现：
  - `src/machine/lowering/`
- object 实现：
  - `src/machine/object/`
- runtime 实现：
  - `src/machine/runtime/`
- observe 实现：
  - `src/machine/observe/`
- 测试：
  - `tests/machine/lowering/`
  - `tests/machine/object/`
  - `tests/machine/runtime/`
  - `tests/machine/observe/`
- 规划文档：
  - `docs/backend/MACHINE_*_PLAN.md`

另外现在要注意：

- 有些模块已经拆成 `目录 + 入口 .c + 多个 .inc`
- 有些模块目前还是真正的单 `.c`

所以 lesson 里不能一律写成“都是单文件”，也不能一律写成“都拆分了很多分片”。

---

## 7. 目前同步状态怎么理解

当前 machine lesson 的路径同步可以这样理解：

- 那些实现已经明确拆目录分片的重点模块，lesson 现在基本都已经同步到位了
  - 例如 `machine_select`、`machine_bytes`、`machine_elf`、`machine_image`、`machine_exec`、`machine_load`、`machine_runtime`、`machine_launch`
- 那些 lesson 里还写单 `.c` 的模块，很多并不是漏改，而是实现目前确实还是单文件
  - 例如 `machine_state`、`machine_interp`、`machine_transition`、`machine_decode`、`machine_step`、`machine_mutation`、`machine_apply`、`machine_commit`、`machine_writeback`
  - 以及 observe 里的 `machine_observe`、`machine_delta`、`machine_trace`、`machine_event`、`machine_outcome`

所以当前更准确的说法是：

- `不是“所有 lesson 都绝对最终同步完毕了”`
- `而是“已经把目前最容易误导读者的拆文件路径问题基本同步了一轮”`

后面如果 implementor 继续拆文件，还需要继续跟进 lesson。

---

## 8. 后面怎么继续长

如果后面继续补课，最建议的维护方式是：

1. 先补对应 lesson 正文
2. 再同步本目录 README
3. 如果某一层目录拆了文件，就顺手改 lesson 里的代码路径说明
4. 保持 lesson 里的阅读口径一致：
   - 为什么有这层
   - 边界是什么
   - 改了什么
   - 测试锁了什么

这样以后不会再回到“md 太多但没有总地图”的状态。
