# Lesson Index

这份 `lesson/README.md` 的目标，不是只列文件名，而是给整个 `lesson/` 目录一张总地图。

现在这套 lesson 基本可以按三条主线理解：

1. `core`
   - 源程序怎么被理解成前中端 IR
2. `ssa`
   - lower IR 之后，SSA / allocator / machine-register-model 怎么长
3. `machine`
   - machine/backend 主线怎么从 machine IR 一路走到最终 journal

如果把整条仓库学习路径压成一句话，可以先记成：

`source text -> lexer -> parser -> AST -> semantic -> canonical IR -> IR passes -> lower IR -> SSA -> machine lowering -> object -> runtime -> observe -> journal`

当前最终 backend 目标方向仍然是 `RISC-V`。

lesson 里会出现：

- `generic-elf32`
- `i386-preview`
- `riscv32-preview`

这些更多是 staging / bridge / 验证 surface，不代表最终目标改成别的 ISA。

---

## 1. 目录怎么分

- `core/`
  - 前端与基础 IR 主线
- `ssa/`
  - `value_ssa` / `memory_ssa` / allocator / machine-register-model / SSA execution support
- `machine/`
  - backend 主线总目录

其中 `machine/` 又继续分成：

- `machine/lowering/`
  - `machine_ir -> machine_select -> machine_layout -> machine_emit -> machine_encode`
- `machine/object/`
  - `machine_bytes -> machine_object -> machine_reloc -> machine_container -> machine_elf`
- `machine/runtime/`
  - `machine_image -> machine_exec -> machine_load -> machine_runtime -> machine_launch -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply`
- `machine/observe/`
  - `machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome -> machine_history -> machine_timeline -> machine_log -> machine_journal`

所以最粗的理解是：

1. `core`
   - 先把程序读懂
2. `ssa`
   - 再把 lower IR 整理成更适合后端的 SSA / allocator 形态
3. `machine`
   - 最后把它一路推进成 machine/backend artifact

---

## 2. 最推荐的完整阅读顺序

如果你第一次完整读这套讲义，最推荐按下面顺序：

1. `core/README.md`
2. `core/lexer_lesson.md`
3. `core/parser_lesson.md`
4. `core/ast_lesson.md`
5. `core/semantic_lesson.md`
6. `core/ir_lesson.md`
7. `core/ir_pass_lesson.md`
8. `core/lower_ir_lesson.md`
9. `core/tests_lesson.md`
10. `ssa/README.md`
11. `ssa/value_ssa_lesson.md`
12. `ssa/value_ssa_pass_lesson.md`
13. `ssa/value_ssa_alloc_lesson.md`
14. `ssa/value_ssa_machine_lesson.md`
15. `ssa/value_ssa_interp_lesson.md`
16. `ssa/memory_ssa_lesson.md`
17. `ssa/memory_ssa_pass_lesson.md`
18. `machine/README.md`
19. `machine/lowering/machine_ir_lesson.md`
20. `machine/lowering/machine_select_lesson.md`
21. `machine/lowering/machine_layout_lesson.md`
22. `machine/lowering/machine_emit_lesson.md`
23. `machine/lowering/machine_encode_lesson.md`
24. `machine/object/machine_bytes_lesson.md`
25. `machine/object/machine_object_lesson.md`
26. `machine/object/machine_reloc_lesson.md`
27. `machine/object/machine_container_lesson.md`
28. `machine/object/machine_elf_lesson.md`
29. `machine/runtime/machine_image_lesson.md`
30. `machine/runtime/machine_exec_lesson.md`
31. `machine/runtime/machine_load_lesson.md`
32. `machine/runtime/machine_runtime_lesson.md`
33. `machine/runtime/machine_launch_lesson.md`
34. `machine/runtime/machine_step_lesson.md`
35. `machine/runtime/machine_decode_lesson.md`
36. `machine/runtime/machine_payload_decode_lesson.md`
37. `machine/runtime/machine_interp_lesson.md`
38. `machine/runtime/machine_transition_lesson.md`
39. `machine/runtime/machine_state_lesson.md`
40. `machine/runtime/machine_mutation_lesson.md`
41. `machine/runtime/machine_writeback_lesson.md`
42. `machine/runtime/machine_commit_lesson.md`
43. `machine/runtime/machine_apply_lesson.md`
44. `machine/observe/machine_observe_lesson.md`
45. `machine/observe/machine_delta_lesson.md`
46. `machine/observe/machine_trace_lesson.md`
47. `machine/observe/machine_event_lesson.md`
48. `machine/observe/machine_outcome_lesson.md`
49. `machine/observe/machine_history_lesson.md`
50. `machine/observe/machine_timeline_lesson.md`
51. `machine/observe/machine_log_lesson.md`
52. `machine/observe/machine_journal_lesson.md`

这个顺序的好处是：

- 先把前端和 IR 地基搭好
- 再把 SSA / allocator / machine register model 看明白
- 最后再读 machine/backend 主线

---

## 3. 三种入口

### 3.1 初学者路线

如果你现在最需要的是：

- 先建立整体感
- 不想一上来掉进 backend 细节
- 想先知道“这个编译器大概是怎么一路走下来的”

最推荐这条短路线：

1. `core/README.md`
2. `core/lexer_lesson.md`
3. `core/parser_lesson.md`
4. `core/ast_lesson.md`
5. `core/semantic_lesson.md`
6. `core/ir_lesson.md`
7. `core/lower_ir_lesson.md`
8. `ssa/README.md`
9. `ssa/value_ssa_lesson.md`
10. `ssa/value_ssa_machine_lesson.md`
11. `machine/README.md`
12. `machine/lowering/machine_ir_lesson.md`
13. `machine/lowering/machine_select_lesson.md`
14. `machine/object/machine_elf_lesson.md`
15. `machine/runtime/machine_launch_lesson.md`
16. `machine/observe/machine_outcome_lesson.md`
17. `machine/observe/machine_journal_lesson.md`

这条路线的目标不是把所有细节一次读完，而是先建立这几个大图景：

- 前端怎么把源码读懂
- IR 和 lower IR 是什么边界
- SSA 为什么存在
- machine backend 怎么一路走到最终记录

### 3.2 实现者路线

如果你现在更像是在做实现、改代码、跟踪真实模块边界，最推荐：

1. `core/README.md`
2. `core/ir_lesson.md`
3. `core/ir_pass_lesson.md`
4. `core/lower_ir_lesson.md`
5. `core/tests_lesson.md`
6. `ssa/README.md`
7. `ssa/value_ssa_lesson.md`
8. `ssa/value_ssa_pass_lesson.md`
9. `ssa/value_ssa_alloc_lesson.md`
10. `ssa/value_ssa_machine_lesson.md`
11. `ssa/memory_ssa_lesson.md`
12. `ssa/memory_ssa_pass_lesson.md`
13. `machine/README.md`
14. 然后按你正在改的那条 machine 子线继续下钻

这条路线的重点不是“从最简单开始”，而是：

- 先把真正影响实现边界的 IR / lower IR / SSA / tests 搞清楚
- 再顺着 machine 子线对照真实目录、测试、计划文档去读

### 3.3 查模块路线

如果你不是系统学习，而是临时卡在某个模块，最好的方法不是从头读完，而是：

1. 先看顶层这篇 `lesson/README.md`
2. 再进对应分线的 `README.md`
3. 最后只读那个模块前后 1 到 3 篇 lesson

最常见的几种查法：

- 想查前端：
  - `core/README.md`
  - 对应的 `lexer / parser / ast / semantic`
- 想查 IR：
  - `core/ir_lesson.md`
  - `core/ir_pass_lesson.md`
  - `core/lower_ir_lesson.md`
- 想查 SSA：
  - `ssa/README.md`
  - 对应的 `value_ssa_*` 或 `memory_ssa_*`
- 想查 backend lowering：
  - `machine/lowering/README.md`
  - 对应模块及其前后一篇
- 想查 object/ELF：
  - `machine/object/README.md`
  - `machine_bytes -> ... -> machine_elf`
- 想查 runtime：
  - `machine/runtime/README.md`
  - 对应模块及其上游/下游
- 想查 observe/final record：
  - `machine/observe/README.md`
  - `machine_outcome -> machine_history -> machine_timeline -> machine_log -> machine_journal`

---

## 4. 如果你不想一次读完整套

### 4.1 想先把“前端到 IR”看懂

先读：

1. `core/README.md`
2. `core/lexer_lesson.md`
3. `core/parser_lesson.md`
4. `core/ast_lesson.md`
5. `core/semantic_lesson.md`
6. `core/ir_lesson.md`
7. `core/ir_pass_lesson.md`
8. `core/lower_ir_lesson.md`

### 4.2 想先把“SSA 到 machine 入口”看懂

先读：

1. `core/lower_ir_lesson.md`
2. `ssa/README.md`
3. `ssa/value_ssa_lesson.md`
4. `ssa/value_ssa_pass_lesson.md`
5. `ssa/value_ssa_alloc_lesson.md`
6. `ssa/value_ssa_machine_lesson.md`

这条线会回答：

- lower IR 为什么存在
- value SSA 怎么建立
- allocator 怎么做
- machine register model 怎么把它接到 backend

### 4.3 想先把“machine/backend 主线”看懂

先读：

1. `machine/README.md`
2. 然后按 `machine` README 里的顺序往下

### 4.4 想先搞明白“测试到底在锁什么”

先读：

1. `core/tests_lesson.md`
2. 然后回去看你正在读的那篇 lesson 里提到的 test 名字

---

## 5. 三条主线分别在讲什么

### 5.1 `core/`

这条线主要回答：

- 源码怎么被切成 token
- token 怎么变成 AST
- semantic 怎么补足事实
- canonical IR 是什么
- IR pass 怎么整理 IR
- lower IR 为什么是后端前的重要边界

更详细导读看：

- `core/README.md`

### 5.2 `ssa/`

这条线主要回答：

- lower IR 为什么还要继续变成 SSA
- `value_ssa` 和 `memory_ssa` 分别在解决什么
- SSA pass、allocator、machine register model 怎么搭桥
- SSA 层能做哪些解释/分析/执行相关 support

更详细导读看：

- `ssa/README.md`

### 5.3 `machine/`

这条线主要回答：

- machine IR / select / layout / emit / encode 怎么长
- bytes / object / reloc / container / ELF 怎么收束
- image / exec / load / runtime / launch / step / decode / interp / state 怎么推进
- observe / delta / trace / event / outcome / history / timeline / log / journal 怎么把结果收束成最终记录

更详细导读看：

- `machine/README.md`

---

## 6. 最常用的几个衔接点

如果你容易在目录之间迷路，最值得记住的衔接点其实就几个：

1. `core/ir_lesson.md`
   - semantic 之后，程序第一次稳定落成 IR
2. `core/lower_ir_lesson.md`
   - canonical IR 继续往 SSA / backend 入口收束
3. `ssa/value_ssa_machine_lesson.md`
   - SSA / allocator / machine register model 开始和 machine backend 接上
4. `machine/lowering/machine_ir_lesson.md`
   - machine/backend 正式起跑
5. `machine/object/machine_elf_lesson.md`
   - 内部 object/container 终于落到标准 ELF 边界
6. `machine/runtime/machine_launch_lesson.md`
   - “能加载”进一步变成“能起跑”
7. `machine/observe/machine_outcome_lesson.md`
   - 执行语义开始收束成结果语言
8. `machine/observe/machine_journal_lesson.md`
   - 当前最下游的记录 artifact

---

## 7. 最常见的几种“我现在该读哪篇”

### 我只想知道这仓库大概在干嘛

读：

1. `core/README.md`
2. `ssa/README.md`
3. `machine/README.md`

### 我只想知道 backend 现在做到哪里

读：

1. `machine/README.md`
2. `machine/observe/machine_journal_lesson.md`

然后再结合：

- `docs/backend/MACHINE_JOURNAL_PLAN.md`

### 我只想知道为什么最后目标是 RISC-V

读：

1. `ssa/value_ssa_machine_lesson.md`
2. `machine/lowering/machine_ir_lesson.md`
3. `machine/lowering/machine_select_lesson.md`
4. `machine/object/machine_elf_lesson.md`
5. `machine/runtime/machine_image_lesson.md`

### 我只想顺着一条真实执行主线读

读：

1. `machine/runtime/README.md`
2. `machine/runtime/machine_image_lesson.md`
3. `machine/runtime/machine_exec_lesson.md`
4. `machine/runtime/machine_load_lesson.md`
5. `machine/runtime/machine_runtime_lesson.md`
6. `machine/runtime/machine_launch_lesson.md`
7. `machine/runtime/machine_step_lesson.md`
8. `machine/runtime/machine_decode_lesson.md`
9. `machine/runtime/machine_payload_decode_lesson.md`
10. `machine/runtime/machine_interp_lesson.md`
11. `machine/runtime/machine_transition_lesson.md`
12. `machine/runtime/machine_state_lesson.md`

### 我只想看“结果最后怎么变成记录”

读：

1. `machine/observe/machine_outcome_lesson.md`
2. `machine/observe/machine_history_lesson.md`
3. `machine/observe/machine_timeline_lesson.md`
4. `machine/observe/machine_log_lesson.md`
5. `machine/observe/machine_journal_lesson.md`

---

## 8. 现在这个 README 该怎么用

如果你只是找某一篇，直接进对应目录就行。

但如果你想真的建立整体感，最建议：

1. 先读这篇 `lesson/README.md`
2. 再读对应分线的 `README.md`
3. 最后按分线顺序进入具体 lesson

最自然的节奏通常是：

1. `core/README.md`
2. `ssa/README.md`
3. `machine/README.md`

然后再下钻正文。

---

## 9. 当前状态怎么理解

这套 lesson 现在已经基本从“平铺很多 md”整理成了“按主线分目录 + README 导读”的结构。

不过它仍然是会继续演化的：

- implementor 如果继续拆文件
- backend 如果继续往下游推进
- SSA / memory SSA 如果继续扩

lesson 和 README 都还需要继续同步。

所以当前更准确的说法是：

- `已经有一张能用的总地图`
- `但它仍然是会跟着仓库继续长的`
